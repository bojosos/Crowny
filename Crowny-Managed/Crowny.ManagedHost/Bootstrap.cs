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
    private static readonly Queue<object> Diagnostics = new();
    private static readonly object DiagnosticLock = new();
    private static readonly byte[] ManagedLogCode = "managed.log"u8.ToArray();
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
            CaptureState = &CaptureState,
            ApplyState = &ApplyState,
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
            host->GetEntityParent == null || host->SetEntityParent == null || host->DestroyEntity == null ||
            host->InvokeHostBinding == null || !host->HasCompleteBindings())
            return NativeStatus.AbiMismatch;
        if (_initialized)
            return NativeStatus.InvalidArgument;
        _host = *host;
        _initialized = true;
        ManagedRuntimeContext.SetLogHandler(ForwardLog);
        ManagedRuntimeContext.SetEntityHandlers(GetEntityName, SetEntityName, FindEntityByName, GetEntityParent, SetEntityParent,
                                                DestroyEntity);
        ManagedRuntimeContext.SetHostBindingHandler(InvokeHostBinding);
        ManagedRuntimeContext.SetNativeHostApi(*(ManagedNativeHostApi*)host);
        ManagedRuntimeContext.SetScriptResolver(Program.ResolveScriptComponent);
        ManagedAotRoots.Preserve();
        return NativeStatus.Ok;
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
        ManagedRuntimeContext.SetLogHandler(null);
        ManagedRuntimeContext.SetEntityHandlers(null, null, null, null, null, null);
        ManagedRuntimeContext.SetHostBindingHandler(null);
        ManagedRuntimeContext.SetNativeHostApi(default);
        ManagedRuntimeContext.SetScriptResolver(null);
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
        if ((uint)eventData->Kind > (uint)NativeEventKind.TriggerExit3D ||
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
    private static NativeStatus CollectDiagnostics(NativeBlobWriter* output)
    {
        if (!_initialized)
            return NativeStatus.NotInitialized;
        try
        {
            object[] diagnostics;
            lock (DiagnosticLock)
            {
                diagnostics = Diagnostics.ToArray();
                Diagnostics.Clear();
            }
            byte[] json = JsonSerializer.SerializeToUtf8Bytes(diagnostics);
            Write(output, json);
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
                Diagnostics.Enqueue(new { Severity = "Error", Code = "managed.exception", Message = error.Message, Stack = error.ToString() });
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
                Diagnostics.Enqueue(new { Severity = "Error", Code = code, Message = message, Stack = string.Empty });
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

    private static Guid Decode(NativeUuid value)
    {
        byte* bytes = value.Bytes;
        return new Guid(new ReadOnlySpan<byte>(bytes, 16), bigEndian: true);
    }

    private static ReadOnlySpan<byte> AsSpan(NativeBlob value) =>
        value.Length == 0 ? ReadOnlySpan<byte>.Empty : new ReadOnlySpan<byte>(value.Data, (int)value.Length);

    private static void Write(NativeBlobWriter* writer, byte[] data)
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

    private static void ForwardLog(int severity, string value)
    {
        try
        {
            if (_host.Log == null)
                return;
            byte[] message = Encoding.UTF8.GetBytes(value ?? string.Empty);
            fixed (byte* codeBytes = ManagedLogCode)
            fixed (byte* messageBytes = message)
            {
                _host.Log(_host.Context, (uint)severity, new NativeStringView(codeBytes, (uint)ManagedLogCode.Length),
                          new NativeStringView(messageBytes, (uint)message.Length), new NativeStringView(null, 0));
            }
        }
        catch
        {
            // Logging must not throw back into gameplay code.
        }
    }

    private static string GetEntityName(UUID entity)
    {
        NativeStringView name = default;
        EnsureHostStatus(_host.GetEntityName(_host.Context, Encode(entity), &name), "get an entity name");
        if (!IsValid(name))
            throw new InvalidOperationException("The native host returned an invalid entity name.");
        return Decode(name);
    }

    private static void SetEntityName(UUID entity, string value)
    {
        byte[] encoded = Encoding.UTF8.GetBytes(value ?? string.Empty);
        fixed (byte* bytes = encoded)
            EnsureHostStatus(_host.SetEntityName(_host.Context, Encode(entity), new NativeStringView(bytes, (uint)encoded.Length)),
                             "set an entity name");
    }

    private static UUID FindEntityByName(string value)
    {
        byte[] encoded = Encoding.UTF8.GetBytes(value ?? string.Empty);
        NativeUuid entity = default;
        fixed (byte* bytes = encoded)
            EnsureHostStatus(_host.FindEntityByName(_host.Context, new NativeStringView(bytes, (uint)encoded.Length), &entity),
                             "find an entity by name");
        return DecodeCrownyUuid(entity);
    }

    private static UUID GetEntityParent(UUID entity)
    {
        NativeUuid parent = default;
        EnsureHostStatus(_host.GetEntityParent(_host.Context, Encode(entity), &parent), "get an entity parent");
        return DecodeCrownyUuid(parent);
    }

    private static void SetEntityParent(UUID entity, UUID parent)
    {
        EnsureHostStatus(_host.SetEntityParent(_host.Context, Encode(entity), Encode(parent)), "set an entity parent");
    }

    private static void DestroyEntity(UUID entity)
    {
        EnsureHostStatus(_host.DestroyEntity(_host.Context, Encode(entity)), "destroy an entity");
    }

    private static byte[] InvokeHostBinding(ManagedBindingId binding, UUID entity, byte[] input)
    {
        input ??= Array.Empty<byte>();
        NativeBlob output = default;
        fixed (byte* inputBytes = input)
        {
            NativeBlob inputBlob = new(inputBytes, (ulong)input.Length);
            EnsureHostStatus(_host.InvokeHostBinding(_host.Context, (NativeHostBinding)binding, Encode(entity), inputBlob, &output),
                             $"invoke managed host binding {binding}");
        }
        if (output.Length > int.MaxValue || (output.Data == null && output.Length != 0))
            throw new InvalidOperationException("The native host returned an invalid binding result.");
        return output.Length == 0 ? Array.Empty<byte>() : new ReadOnlySpan<byte>(output.Data, (int)output.Length).ToArray();
    }

    private static void EnsureHostStatus(NativeStatus status, string operation)
    {
        if (status != NativeStatus.Ok)
            throw new InvalidOperationException($"The native host could not {operation}. Status {status}.");
    }

    private static NativeUuid Encode(UUID value)
    {
        NativeUuid result = default;
        byte* bytes = result.Bytes;
        for (int wordIndex = 0; wordIndex < 4; ++wordIndex)
        {
            uint word = wordIndex switch { 0 => value.d0, 1 => value.d1, 2 => value.d2, _ => value.d3 };
            int offset = wordIndex * 4;
            bytes[offset] = (byte)(word >> 24);
            bytes[offset + 1] = (byte)(word >> 16);
            bytes[offset + 2] = (byte)(word >> 8);
            bytes[offset + 3] = (byte)word;
        }
        return result;
    }

    private static UUID DecodeCrownyUuid(NativeUuid value)
    {
        byte* bytes = value.Bytes;
        return new UUID(ReadUuidWord(bytes, 0), ReadUuidWord(bytes, 4), ReadUuidWord(bytes, 8), ReadUuidWord(bytes, 12));
    }

    private static uint ReadUuidWord(byte* value, int offset) =>
        (uint)value[offset] << 24 | (uint)value[offset + 1] << 16 | (uint)value[offset + 2] << 8 | value[offset + 3];
}
