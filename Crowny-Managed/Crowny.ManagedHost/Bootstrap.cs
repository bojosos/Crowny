using System.Buffers;
using System.Runtime.CompilerServices;
using System.Runtime.InteropServices;
using System.Reflection;
using System.Text;
using System.Text.Json;
using Crowny.ManagedHost.Interop;

namespace Crowny.ManagedHost;

public static unsafe class Bootstrap
{
    private static readonly ManagedProgram Program = new();
    private readonly record struct Diagnostic(string Code, string Message, string Stack);
    private static readonly Queue<Diagnostic> Diagnostics = new();
    private static readonly object DiagnosticLock = new();
    private static NativeHostApi _host;
    private static bool _initialized;

    [UnmanagedCallersOnly(EntryPoint = "cw_managed_get_api", CallConvs = [typeof(CallConvCdecl)])]
    public static NativeStatus GetApi(NativeProgramApi* api, uint apiSize)
    {
        if (api is null || apiSize < (uint)sizeof(NativeProgramApi))
            return NativeStatus.InvalidArgument;
        *api = new NativeProgramApi
        {
            Size = (uint)sizeof(NativeProgramApi),
            AbiVersion = NativeAbi.Version,
            Initialize = &Initialize,
            Shutdown = &Shutdown,
            LoadProgram = &LoadProgram,
            UnloadProgram = &UnloadProgram,
            GetCatalog = &GetCatalog,
            CreateScript = &CreateScript,
            DestroyScript = &DestroyScript,
            Dispatch = &Dispatch,
            NotifySceneEvent = &NotifySceneEvent,
            CaptureState = &CaptureState,
            ApplyState = &ApplyState,
            InvokeButton = &InvokeButton,
            CollectDiagnostics = &CollectDiagnostics
        };
        return NativeStatus.Ok;
    }

    [UnmanagedCallersOnly(CallConvs = [typeof(CallConvCdecl)])]
    private static NativeStatus Initialize(NativeHostApi* host)
    {
        if (host is null || host->Size < (uint)sizeof(NativeHostApi) || host->AbiVersion != NativeAbi.Version)
            return NativeStatus.AbiMismatch;
        if (host->Log == null || host->GetEntityName == null || host->SetEntityName == null || host->FindEntityByName == null ||
            host->GetEntityParent == null || host->SetEntityParent == null || host->DestroyEntity == null || !host->HasCompleteBindings())
            return NativeStatus.AbiMismatch;
        if (_initialized)
            return NativeStatus.InvalidArgument;
        try
        {
            _host = *host;
            ManagedRuntimeContext.SetNativeHostApi(*(ManagedNativeHostApi*)host);
            ManagedRuntimeContext.SetScriptResolver(Program.ResolveScriptComponent);
            ManagedAotRoots.Preserve();
            _initialized = true;
            return NativeStatus.Ok;
        }
        catch (Exception error)
        {
            ManagedRuntimeContext.ClearNativeHostApi();
            _host = default;
            return Record(error);
        }
    }

    [UnmanagedCallersOnly(CallConvs = [typeof(CallConvCdecl)])]
    private static void Shutdown()
    {
        try
        {
            if (_initialized)
                Program.Unload();
        }
        catch
        {
            // Native shutdown cannot propagate a managed exception.
        }
        _initialized = false;
        ManagedRuntimeContext.ClearNativeHostApi();
        _host = default;
        lock (DiagnosticLock)
            Diagnostics.Clear();
    }

    [UnmanagedCallersOnly(CallConvs = [typeof(CallConvCdecl)])]
    private static NativeStatus LoadProgram(NativeStringView assemblyPath, ulong generation)
    {
        if (!_initialized)
            return NativeStatus.NotInitialized;
        if (!IsValid(assemblyPath) || assemblyPath.Length == 0 || generation == 0)
            return NativeStatus.InvalidArgument;
        try
        {
            Program.Load(Decode(assemblyPath), generation);
            return NativeStatus.Ok;
        }
        catch (Exception error)
        {
            return Record(error, NativeStatus.ProgramLoadFailed);
        }
    }

    [UnmanagedCallersOnly(CallConvs = [typeof(CallConvCdecl)])]
    private static NativeStatus UnloadProgram()
    {
        if (!_initialized)
            return NativeStatus.NotInitialized;
        try
        {
            return Program.Unload()
                     ? NativeStatus.Ok
                     : RecordDiagnostic("managed.reload.context_leak",
                                        "The collectible game context is still alive. Check game-owned threads, static event subscriptions, " +
                                        "native callbacks, and strong or pinned handles.", NativeStatus.ReloadLeak);
        }
        catch (Exception error)
        {
            return Record(error);
        }
    }

    [UnmanagedCallersOnly(CallConvs = [typeof(CallConvCdecl)])]
    private static NativeStatus GetCatalog(NativeBlobWriter* output)
    {
        if (!_initialized)
            return NativeStatus.NotInitialized;
        try
        {
            Write(output, Encoding.UTF8.GetBytes(Program.GetCatalogJson()));
            return NativeStatus.Ok;
        }
        catch (Exception error)
        {
            return Record(error);
        }
    }

    [UnmanagedCallersOnly(CallConvs = [typeof(CallConvCdecl)])]
    private static NativeStatus CreateScript(NativeStringView assemblyName, NativeStringView typeNamespace, NativeStringView typeName,
                                             NativeUuid entity, NativeBlob initialState, ulong* instance)
    {
        if (!_initialized)
            return NativeStatus.NotInitialized;
        if (instance is null)
            return NativeStatus.InvalidArgument;
        if (!IsValid(assemblyName) || assemblyName.Length == 0 || !IsValid(typeNamespace) || !IsValid(typeName) ||
            typeName.Length == 0 || !IsValid(initialState))
            return NativeStatus.InvalidArgument;
        try
        {
            *instance = Program.Create(Decode(assemblyName), Decode(typeNamespace), Decode(typeName), Decode(entity), AsSpan(initialState));
            return NativeStatus.Ok;
        }
        catch (Exception error)
        {
            return Record(error, error is TypeLoadException ? NativeStatus.ScriptTypeMissing : NativeStatus.ManagedException);
        }
    }

    [UnmanagedCallersOnly(CallConvs = [typeof(CallConvCdecl)])]
    private static NativeStatus DestroyScript(ulong instance)
    {
        if (!_initialized)
            return NativeStatus.NotInitialized;
        try
        {
            Program.Destroy(instance);
            return NativeStatus.Ok;
        }
        catch (Exception error)
        {
            return Record(error, error is KeyNotFoundException ? NativeStatus.StaleHandle : NativeStatus.ManagedException);
        }
    }

    [UnmanagedCallersOnly(CallConvs = [typeof(CallConvCdecl)])]
    private static NativeStatus Dispatch(ulong instance, NativeEvent* eventData)
    {
        if (!_initialized)
            return NativeStatus.NotInitialized;
        if (eventData is null || eventData->Size < (uint)sizeof(NativeEvent))
            return NativeStatus.InvalidArgument;
        if (!Enum.IsDefined(eventData->Kind) ||
            eventData->Payload.Length % (ulong)sizeof(NativeContactPoint) != 0 ||
            (eventData->Payload.Data is null && eventData->Payload.Length != 0) ||
            eventData->Payload.Length / (ulong)sizeof(NativeContactPoint) > int.MaxValue)
            return NativeStatus.InvalidArgument;
        try
        {
            int count = (int)(eventData->Payload.Length / (ulong)sizeof(NativeContactPoint));
            var contacts = new ReadOnlySpan<NativeContactPoint>(eventData->Payload.Data, count);
            Program.Dispatch(instance, eventData->Kind, eventData->DeltaTime, Decode(eventData->OtherEntity), contacts);
            return NativeStatus.Ok;
        }
        catch (Exception error)
        {
            return Record(error, error is KeyNotFoundException ? NativeStatus.StaleHandle : NativeStatus.ManagedException);
        }
    }

    [UnmanagedCallersOnly(CallConvs = [typeof(CallConvCdecl)])]
    private static NativeStatus NotifySceneEvent(uint eventType, NativeUuid scene, uint executionState)
    {
        if (!_initialized)
            return NativeStatus.NotInitialized;
        if (eventType > (uint)SceneLifecycleEventType.ExecutionStateChanged ||
            executionState > (uint)SceneExecutionState.Simulate)
            return NativeStatus.InvalidArgument;
        try
        {
            SceneManager.NotifySceneEvent((SceneLifecycleEventType)eventType,
                                          Decode(scene),
                                          (SceneExecutionState)executionState);
            return NativeStatus.Ok;
        }
        catch (Exception error)
        {
            return Record(error);
        }
    }

    [UnmanagedCallersOnly(CallConvs = [typeof(CallConvCdecl)])]
    private static NativeStatus CaptureState(ulong instance, NativeBlobWriter* output)
    {
        if (!_initialized)
            return NativeStatus.NotInitialized;
        try
        {
            Write(output, Program.CaptureState(instance));
            return NativeStatus.Ok;
        }
        catch (Exception error)
        {
            return Record(error, error is KeyNotFoundException ? NativeStatus.StaleHandle : NativeStatus.ManagedException);
        }
    }

    [UnmanagedCallersOnly(CallConvs = [typeof(CallConvCdecl)])]
    private static NativeStatus ApplyState(ulong instance, NativeBlob state)
    {
        if (!_initialized)
            return NativeStatus.NotInitialized;
        if (!IsValid(state))
            return NativeStatus.InvalidArgument;
        try
        {
            Program.ApplyState(instance, AsSpan(state));
            return NativeStatus.Ok;
        }
        catch (Exception error)
        {
            return Record(error, error is KeyNotFoundException ? NativeStatus.StaleHandle : NativeStatus.ManagedException);
        }
    }

    [UnmanagedCallersOnly(CallConvs = [typeof(CallConvCdecl)])]
    private static NativeStatus InvokeButton(ulong instance, ulong methodId, NativeBlob arguments, NativeBlobWriter* output)
    {
        if (!_initialized)
            return NativeStatus.NotInitialized;
        if (!IsValid(arguments) || output is null)
            return NativeStatus.InvalidArgument;
        try
        {
            Write(output, Program.InvokeButton(instance, methodId, AsSpan(arguments)));
            return NativeStatus.Ok;
        }
        catch (Exception error)
        {
            return Record(error, error is KeyNotFoundException ? NativeStatus.StaleHandle : NativeStatus.ManagedException);
        }
    }

    [UnmanagedCallersOnly(CallConvs = [typeof(CallConvCdecl)])]
    private static NativeStatus CollectDiagnostics(NativeBlobWriter* output)
    {
        if (!_initialized)
            return NativeStatus.NotInitialized;
        try
        {
            Diagnostic[] diagnostics;
            lock (DiagnosticLock)
            {
                diagnostics = Diagnostics.ToArray();
                Diagnostics.Clear();
            }
            if (diagnostics.Length == 0)
            {
                Write(output, "[]"u8);
                return NativeStatus.Ok;
            }
            var buffer = new ArrayBufferWriter<byte>();
            using (var writer = new Utf8JsonWriter(buffer))
            {
                writer.WriteStartArray();
                foreach (Diagnostic diagnostic in diagnostics)
                {
                    writer.WriteStartObject();
                    writer.WriteString("Severity", "Error");
                    writer.WriteString("Code", diagnostic.Code);
                    writer.WriteString("Message", diagnostic.Message);
                    writer.WriteString("Stack", diagnostic.Stack);
                    writer.WriteEndObject();
                }
                writer.WriteEndArray();
            }
            Write(output, buffer.WrittenSpan);
            return NativeStatus.Ok;
        }
        catch (Exception error)
        {
            return Record(error);
        }
    }

    private static NativeStatus Record(Exception error, NativeStatus failure = NativeStatus.ManagedException)
    {
        if (error is TargetInvocationException { InnerException: not null } invocation)
            error = invocation.InnerException!;
        try
        {
            lock (DiagnosticLock)
                Diagnostics.Enqueue(new Diagnostic("managed.exception", error.Message, error.ToString()));
        }
        catch
        {
            // Preserve the ABI status even when diagnostic allocation fails.
        }
        return error is BlobWriteException ? NativeStatus.BufferWriteFailed : failure;
    }

    private static NativeStatus RecordDiagnostic(string code, string message, NativeStatus failure)
    {
        try
        {
            lock (DiagnosticLock)
                Diagnostics.Enqueue(new Diagnostic(code, message, string.Empty));
        }
        catch
        {
            // Preserve the ABI status even when diagnostic allocation fails.
        }
        return failure;
    }

    private static bool IsValid(NativeStringView value) =>
        (value.Data is not null || value.Length == 0) && value.Length <= int.MaxValue;

    private static bool IsValid(NativeBlob value) =>
        (value.Data is not null || value.Length == 0) && value.Length <= int.MaxValue;

    private static string Decode(NativeStringView value) =>
        value.Data is null || value.Length == 0 ? string.Empty : Encoding.UTF8.GetString(new ReadOnlySpan<byte>(value.Data, checked((int)value.Length)));

    private static UUID Decode(NativeUuid value)
    {
        byte* bytes = value.Bytes;
        return UUID.FromBytes(bytes);
    }

    private static ReadOnlySpan<byte> AsSpan(NativeBlob value) =>
        value.Length == 0 ? ReadOnlySpan<byte>.Empty : new ReadOnlySpan<byte>(value.Data, (int)value.Length);

    private static void Write(NativeBlobWriter* writer, ReadOnlySpan<byte> data)
    {
        if (writer is null || writer->Size < (uint)sizeof(NativeBlobWriter) || writer->Write == null)
            throw new ArgumentException("Invalid native blob writer.");
        fixed (byte* bytes = data)
        {
            NativeStatus status = writer->Write(writer->Context, bytes, (ulong)data.Length);
            if (status != NativeStatus.Ok)
                throw new BlobWriteException($"Native blob writer failed with status {status}.");
        }
    }

    private sealed class BlobWriteException(string message) : Exception(message);

}
