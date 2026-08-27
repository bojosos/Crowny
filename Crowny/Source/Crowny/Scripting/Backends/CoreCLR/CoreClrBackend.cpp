#include "cwpch.h"

#include "Crowny/Common/UTF8.h"
#include "Crowny/Common/Time.h"
#include "Crowny/Ecs/Components.h"
#include "Crowny/Input/Input.h"
#include "Crowny/Scene/SceneManager.h"
#include "Crowny/Scripting/Managed/Internal/ManagedBackend.h"
#include "Crowny/Scripting/Managed/Interop/CrownyManagedAbi.h"
#include "Crowny/Scripting/Managed/Interop/ManagedAbiValidation.h"
#include "Crowny/Scripting/Managed/Interop/ManagedJson.h"

#include <cstring>
#include <limits>

#include <glm/gtc/matrix_inverse.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtx/quaternion.hpp>

#if defined(CW_PLATFORM_WIN32) && !defined(CW_EMSCRIPTEN)
#include <Windows.h>
#elif !defined(CW_EMSCRIPTEN)
#include <dlfcn.h>
#endif

namespace Crowny
{
#ifdef CW_EMSCRIPTEN
    Scope<ManagedBackend> CreateCoreClrBackend() { return nullptr; }
#else
    namespace
    {
#ifdef CW_PLATFORM_WIN32
        using HostChar = wchar_t;
#define CW_NETHOST_CALL __stdcall
#define CW_HOSTFXR_CALL __cdecl
#define CW_CORECLR_DELEGATE_CALL __stdcall
#else
        using HostChar = char;
#define CW_NETHOST_CALL
#define CW_HOSTFXR_CALL
#define CW_CORECLR_DELEGATE_CALL
#endif

        constexpr int32_t HOSTFXR_SUCCESS = 0;
        constexpr int32_t HOSTFXR_SUCCESS_ALREADY_INITIALIZED = 1;
        constexpr int32_t HOSTFXR_SUCCESS_DIFFERENT_PROPERTIES = 2;
        constexpr int32_t NETHOST_BUFFER_TOO_SMALL = static_cast<int32_t>(0x80008098u);

        struct GetHostFxrParameters
        {
            size_t Size;
            const HostChar* AssemblyPath;
            const HostChar* DotnetRoot;
        };

        struct HostFxrInitializeParameters
        {
            size_t Size;
            const HostChar* HostPath;
            const HostChar* DotnetRoot;
        };

        enum class HostFxrDelegateType : int32_t
        {
            LoadAssemblyAndGetFunctionPointer = 5
        };

        using GetHostFxrPathFn = int32_t(CW_NETHOST_CALL*)(HostChar*, size_t*, const GetHostFxrParameters*);
        using HostFxrHandle = void*;
        using HostFxrInitializeForRuntimeConfigFn =
          int32_t(CW_HOSTFXR_CALL*)(const HostChar*, const HostFxrInitializeParameters*, HostFxrHandle*);
        using HostFxrGetRuntimeDelegateFn = int32_t(CW_HOSTFXR_CALL*)(HostFxrHandle, HostFxrDelegateType, void**);
        using HostFxrCloseFn = int32_t(CW_HOSTFXR_CALL*)(HostFxrHandle);
        using LoadAssemblyAndGetFunctionPointerFn = int32_t(CW_CORECLR_DELEGATE_CALL*)(
          const HostChar*, const HostChar*, const HostChar*, const HostChar*, void*, void**);

        Path AbsolutePath(const Path& path)
        {
            std::error_code error;
            Path result = fs::absolute(path, error);
            return error ? Path() : result;
        }

        bool IsRegularFile(const Path& path)
        {
            std::error_code error;
            return fs::is_regular_file(path, error) && !error;
        }

        bool IsDirectory(const Path& path)
        {
            std::error_code error;
            return fs::is_directory(path, error) && !error;
        }

        class DynamicLibrary
        {
        public:
            bool Load(const Path& path)
            {
                const Path absolute = AbsolutePath(path);
                if (absolute.empty())
                    return false;
#ifdef CW_PLATFORM_WIN32
                m_Handle = LoadLibraryW(absolute.wstring().c_str());
#else
                m_Handle = dlopen(absolute.string().c_str(), RTLD_LAZY | RTLD_LOCAL);
#endif
                return m_Handle != nullptr;
            }

            void* Find(const char* name) const
            {
#ifdef CW_PLATFORM_WIN32
                return m_Handle != nullptr ? reinterpret_cast<void*>(GetProcAddress(static_cast<HMODULE>(m_Handle), name)) : nullptr;
#else
                return m_Handle != nullptr ? dlsym(m_Handle, name) : nullptr;
#endif
            }

        private:
            void* m_Handle = nullptr;
        };

#ifdef CW_PLATFORM_WIN32
        std::wstring ToHostString(const Path& path) { return AbsolutePath(path).wstring(); }
        std::wstring ToHostString(StringView value) { return UTF8::ToWide(String(value)); }
        String ToUtf8Path(const Path& path) { return UTF8::FromWide(AbsolutePath(path).wstring()); }
#else
        String ToHostString(const Path& path) { return AbsolutePath(path).string(); }
        String ToHostString(StringView value) { return String(value); }
        String ToUtf8Path(const Path& path) { return AbsolutePath(path).string(); }
#endif

        bool IsUsableHostFxrInitialization(int32_t status)
        {
            return status == HOSTFXR_SUCCESS || status == HOSTFXR_SUCCESS_ALREADY_INITIALIZED;
        }

        const ManagedProgramArtifact* FindArtifact(const ManagedProgramDefinition& program, ManagedProgramArtifactKind kind,
                                                   StringView logicalName = {})
        {
            const auto artifact = std::find_if(program.Artifacts.begin(), program.Artifacts.end(), [&](const ManagedProgramArtifact& value) {
                return value.Kind == kind && (logicalName.empty() || value.LogicalName == logicalName);
            });
            return artifact == program.Artifacts.end() ? nullptr : &*artifact;
        }

        struct AbiString
        {
            explicit AbiString(String value) : Storage(std::move(value))
            {
                View.data = reinterpret_cast<const uint8_t*>(Storage.data());
                View.length = static_cast<uint32_t>(Storage.size());
            }

            String Storage;
            cw_managed_string_view View{};
        };

        cw_managed_uuid ToAbiUuid(const UUID& uuid)
        {
            cw_managed_uuid result{};
            const String text = uuid.ToString();
            uint32_t output = 0;
            uint8_t high = 0;
            bool haveHigh = false;
            for (const char character : text)
            {
                if (character == '-')
                    continue;
                const uint8_t value = character >= '0' && character <= '9' ? static_cast<uint8_t>(character - '0')
                                      : character >= 'a' && character <= 'f' ? static_cast<uint8_t>(character - 'a' + 10)
                                                                            : static_cast<uint8_t>(character - 'A' + 10);
                if (!haveHigh)
                {
                    high = value;
                    haveHigh = true;
                }
                else if (output < 16)
                {
                    result.bytes[output++] = static_cast<uint8_t>((high << 4u) | value);
                    haveHigh = false;
                }
            }
            return result;
        }

        UUID FromAbiUuid(const cw_managed_uuid& uuid)
        {
            auto word = [&](uint32_t offset) {
                return static_cast<uint32_t>(uuid.bytes[offset]) << 24u | static_cast<uint32_t>(uuid.bytes[offset + 1]) << 16u |
                       static_cast<uint32_t>(uuid.bytes[offset + 2]) << 8u | static_cast<uint32_t>(uuid.bytes[offset + 3]);
            };
            return UUID(word(0), word(4), word(8), word(12));
        }

        cw_managed_status CW_MANAGED_CALL WriteBlob(void* context, const uint8_t* data, uint64_t length)
        {
            if (context == nullptr || (data == nullptr && length != 0) || length > std::numeric_limits<size_t>::max())
                return CW_MANAGED_STATUS_INVALID_ARGUMENT;
            if (length == 0)
                return CW_MANAGED_STATUS_OK;
            try
            {
                Vector<uint8_t>& output = *static_cast<Vector<uint8_t>*>(context);
                output.insert(output.end(), data, data + static_cast<size_t>(length));
                return CW_MANAGED_STATUS_OK;
            }
            catch (...)
            {
                return CW_MANAGED_STATUS_BUFFER_WRITE_FAILED;
            }
        }

        bool ReadBindingFloats(cw_managed_blob input, float* output, size_t count)
        {
            if ((input.data == nullptr && input.length != 0) || input.length != count * sizeof(float))
                return false;
            std::memcpy(output, input.data, static_cast<size_t>(input.length));
            return true;
        }

        cw_managed_status WriteBindingResult(cw_managed_blob* output, const void* data, size_t size)
        {
            if (output == nullptr || (data == nullptr && size != 0) || size > 64)
                return CW_MANAGED_STATUS_INVALID_ARGUMENT;
            thread_local Array<uint8_t, 64> storage{};
            if (size != 0)
                std::memcpy(storage.data(), data, size);
            output->data = size == 0 ? nullptr : storage.data();
            output->length = size;
            return CW_MANAGED_STATUS_OK;
        }

        template <typename T> cw_managed_status WriteBindingResult(cw_managed_blob* output, const T& value)
        {
            return WriteBindingResult(output, &value, sizeof(value));
        }

        bool HasComponent(Entity entity, StringView name)
        {
            if (name == "Crowny.Transform")
                return entity.HasComponent<TransformComponent>();
#define CW_HAS_MANAGED_COMPONENT(managedName, nativeType)                                                                            \
    if (name == managedName)                                                                                                         \
        return entity.HasComponent<nativeType>()
            CW_HAS_MANAGED_COMPONENT("Crowny.AudioListener", AudioListenerComponent);
            CW_HAS_MANAGED_COMPONENT("Crowny.AudioSource", AudioSourceComponent);
            CW_HAS_MANAGED_COMPONENT("Crowny.Camera", CameraComponent);
            CW_HAS_MANAGED_COMPONENT("Crowny.LightComponent", LightComponent);
            CW_HAS_MANAGED_COMPONENT("Crowny.Text", TextComponent);
            CW_HAS_MANAGED_COMPONENT("Crowny.SpriteRendererComponent", SpriteRendererComponent);
            CW_HAS_MANAGED_COMPONENT("Crowny.MeshRenderer", MeshRendererComponent);
            CW_HAS_MANAGED_COMPONENT("Crowny.Rigidbody2D", Rigidbody2DComponent);
            CW_HAS_MANAGED_COMPONENT("Crowny.BoxCollider2D", BoxCollider2DComponent);
            CW_HAS_MANAGED_COMPONENT("Crowny.CircleCollider2D", CircleCollider2DComponent);
            CW_HAS_MANAGED_COMPONENT("Crowny.Rigidbody3D", Rigidbody3DComponent);
            CW_HAS_MANAGED_COMPONENT("Crowny.BoxCollider3D", BoxCollider3DComponent);
            CW_HAS_MANAGED_COMPONENT("Crowny.SphereCollider3D", SphereCollider3DComponent);
            CW_HAS_MANAGED_COMPONENT("Crowny.CapsuleCollider3D", CapsuleCollider3DComponent);
#undef CW_HAS_MANAGED_COMPONENT
            return false;
        }

        bool AddComponent(Entity entity, StringView name)
        {
            if (name == "Crowny.Transform")
                return entity.HasComponent<TransformComponent>();
#define CW_ADD_MANAGED_COMPONENT(managedName, nativeType)                                                                            \
    if (name == managedName)                                                                                                         \
    {                                                                                                                                \
        entity.AddOrGetComponent<nativeType>();                                                                                      \
        return true;                                                                                                                 \
    }
            CW_ADD_MANAGED_COMPONENT("Crowny.AudioListener", AudioListenerComponent)
            CW_ADD_MANAGED_COMPONENT("Crowny.AudioSource", AudioSourceComponent)
            CW_ADD_MANAGED_COMPONENT("Crowny.Camera", CameraComponent)
            CW_ADD_MANAGED_COMPONENT("Crowny.LightComponent", LightComponent)
            CW_ADD_MANAGED_COMPONENT("Crowny.Text", TextComponent)
            CW_ADD_MANAGED_COMPONENT("Crowny.SpriteRendererComponent", SpriteRendererComponent)
            CW_ADD_MANAGED_COMPONENT("Crowny.MeshRenderer", MeshRendererComponent)
            CW_ADD_MANAGED_COMPONENT("Crowny.Rigidbody2D", Rigidbody2DComponent)
            CW_ADD_MANAGED_COMPONENT("Crowny.BoxCollider2D", BoxCollider2DComponent)
            CW_ADD_MANAGED_COMPONENT("Crowny.CircleCollider2D", CircleCollider2DComponent)
            CW_ADD_MANAGED_COMPONENT("Crowny.Rigidbody3D", Rigidbody3DComponent)
            CW_ADD_MANAGED_COMPONENT("Crowny.BoxCollider3D", BoxCollider3DComponent)
            CW_ADD_MANAGED_COMPONENT("Crowny.SphereCollider3D", SphereCollider3DComponent)
            CW_ADD_MANAGED_COMPONENT("Crowny.CapsuleCollider3D", CapsuleCollider3DComponent)
#undef CW_ADD_MANAGED_COMPONENT
            return false;
        }

        bool RemoveComponent(Entity entity, StringView name)
        {
            if (name == "Crowny.Transform")
                return false;
#define CW_REMOVE_MANAGED_COMPONENT(managedName, nativeType)                                                                         \
    if (name == managedName)                                                                                                         \
    {                                                                                                                                \
        entity.RemoveComponentIfExists<nativeType>();                                                                                \
        return true;                                                                                                                 \
    }
            CW_REMOVE_MANAGED_COMPONENT("Crowny.AudioListener", AudioListenerComponent)
            CW_REMOVE_MANAGED_COMPONENT("Crowny.AudioSource", AudioSourceComponent)
            CW_REMOVE_MANAGED_COMPONENT("Crowny.Camera", CameraComponent)
            CW_REMOVE_MANAGED_COMPONENT("Crowny.LightComponent", LightComponent)
            CW_REMOVE_MANAGED_COMPONENT("Crowny.Text", TextComponent)
            CW_REMOVE_MANAGED_COMPONENT("Crowny.SpriteRendererComponent", SpriteRendererComponent)
            CW_REMOVE_MANAGED_COMPONENT("Crowny.MeshRenderer", MeshRendererComponent)
            CW_REMOVE_MANAGED_COMPONENT("Crowny.Rigidbody2D", Rigidbody2DComponent)
            CW_REMOVE_MANAGED_COMPONENT("Crowny.BoxCollider2D", BoxCollider2DComponent)
            CW_REMOVE_MANAGED_COMPONENT("Crowny.CircleCollider2D", CircleCollider2DComponent)
            CW_REMOVE_MANAGED_COMPONENT("Crowny.Rigidbody3D", Rigidbody3DComponent)
            CW_REMOVE_MANAGED_COMPONENT("Crowny.BoxCollider3D", BoxCollider3DComponent)
            CW_REMOVE_MANAGED_COMPONENT("Crowny.SphereCollider3D", SphereCollider3DComponent)
            CW_REMOVE_MANAGED_COMPONENT("Crowny.CapsuleCollider3D", CapsuleCollider3DComponent)
#undef CW_REMOVE_MANAGED_COMPONENT
            return false;
        }

        class CoreClrBackend final : public ManagedBackend
        {
        public:
            ManagedOperationResult Start(const ManagedScriptingConfig& config) override
            {
                if (config.ExecutionMode != ManagedExecutionMode::Jit && config.ExecutionMode != ManagedExecutionMode::ReadyToRun)
                    return ManagedOperationResult::Failure("managed.coreclr.execution_mode",
                                                           "CoreCLR supports JIT and ReadyToRun execution modes.", ManagedBackendId::CoreCLR);
                const Path runtimeRoot = AbsolutePath(config.RuntimeRoot);
                if (config.RuntimeRoot.empty() || runtimeRoot.empty() || !IsDirectory(runtimeRoot))
                    return ManagedOperationResult::Failure("managed.coreclr.runtime_root_missing",
                                                           "CoreCLR requires a complete private .NET runtime root.", ManagedBackendId::CoreCLR);
#ifdef CW_PLATFORM_WIN32
                const Path dotnetHost = runtimeRoot / "dotnet.exe";
#else
                const Path dotnetHost = runtimeRoot / "dotnet";
#endif
                if (!IsRegularFile(dotnetHost) || !IsDirectory(runtimeRoot / "host" / "fxr") ||
                    !IsDirectory(runtimeRoot / "shared" / "Microsoft.NETCore.App"))
                    return ManagedOperationResult::Failure("managed.coreclr.runtime_root_incomplete",
                                                           "The CoreCLR runtime root is missing dotnet, hostfxr, or Microsoft.NETCore.App.",
                                                           ManagedBackendId::CoreCLR);
                m_Config = config;
                m_Config.RuntimeRoot = runtimeRoot;
                m_Started = true;
                return ManagedOperationResult::Success();
            }

            void Shutdown() override
            {
                if (m_RuntimeReady && m_Api.shutdown != nullptr)
                    m_Api.shutdown();
                m_Instances.clear();
                m_Catalog = {};
                m_CurrentProgram = {};
                m_RuntimeReady = false;
                m_ProgramLoaded = false;
                m_Started = false;
                m_Api = {};
                m_NextHandle = 1;
                Lock lock(m_DiagnosticMutex);
                m_Diagnostics.clear();
            }

            ManagedCapabilities GetCapabilities() const override
            {
                ManagedCapabilities capabilities;
                capabilities.DynamicProgramLoading = true;
                capabilities.Reload = true;
                capabilities.RuntimeReflection = true;
                capabilities.ManagedDebugging = m_Config.EnableDebugging;
                capabilities.Profiling = m_Config.EnableProfiling;
                capabilities.Threads = true;
                capabilities.NativeDynamicLibraries = true;
                return capabilities;
            }

            ManagedOperationResult LoadProgram(const ManagedProgramDefinition& program) override
            {
                if (!m_Started)
                    return NotStarted();
                if (!m_Instances.empty())
                    return ManagedOperationResult::Failure("managed.coreclr.instances_active",
                                                           "Destroy live script instances before loading a new program.", ManagedBackendId::CoreCLR);
                if (m_ProgramLoaded)
                    return ManagedOperationResult::Failure("managed.coreclr.program_already_loaded",
                                                           "Unload or reload the current CoreCLR program before loading another one.",
                                                           ManagedBackendId::CoreCLR);
                if (!m_RuntimeReady)
                {
                    ManagedOperationResult initialized = InitializeRuntime(program);
                    if (!initialized.Succeeded)
                        return initialized;
                }
                ManagedOperationResult result = LoadManagedProgram(program, m_Catalog);
                if (result.Succeeded)
                {
                    m_CurrentProgram = program;
                    m_ProgramLoaded = true;
                }
                return result;
            }

            ManagedBackendReloadResult ReloadProgram(const ManagedProgramDefinition& program,
                                                     const Vector<ManagedBackendReloadInstance>& snapshots) override
            {
                if (!m_RuntimeReady || !m_ProgramLoaded)
                    return { ProgramNotLoaded(), {} };
                const ManagedProgramDefinition previousProgram = m_CurrentProgram;
                ScriptCatalog replacementCatalog;
                Map<uint64_t, Instance> replacementInstances;
                const cw_managed_status unloadStatus = m_Api.unload_program();
                m_ProgramLoaded = false;
                ManagedOperationResult replacement =
                  unloadStatus == CW_MANAGED_STATUS_OK
                    ? LoadManagedProgram(program, replacementCatalog)
                    : StatusFailure(unloadStatus, "managed.coreclr.reload_unload_failed", "The previous managed program could not unload.");
                if (replacement.Succeeded)
                {
                    m_ProgramLoaded = true;
                    replacement = RecreateInstances(snapshots, replacementCatalog, replacementInstances);
                }
                if (replacement.Succeeded)
                {
                    Vector<uint64_t> handles;
                    handles.reserve(snapshots.size());
                    for (const ManagedBackendReloadInstance& snapshot : snapshots)
                        handles.push_back(snapshot.PreviousHandle);
                    m_Instances = std::move(replacementInstances);
                    m_Catalog = std::move(replacementCatalog);
                    m_CurrentProgram = program;
                    return { replacement, std::move(handles) };
                }

                if (m_ProgramLoaded)
                {
                    m_Api.unload_program();
                    m_ProgramLoaded = false;
                }
                ScriptCatalog restoredCatalog;
                ManagedOperationResult restored = LoadManagedProgram(previousProgram, restoredCatalog);
                Map<uint64_t, Instance> restoredInstances;
                if (restored.Succeeded)
                {
                    m_ProgramLoaded = true;
                    restored = RecreateInstances(snapshots, restoredCatalog, restoredInstances);
                }
                if (restored.Succeeded)
                {
                    m_Instances = std::move(restoredInstances);
                    m_Catalog = std::move(restoredCatalog);
                    m_CurrentProgram = previousProgram;
                }
                else
                {
                    if (m_ProgramLoaded)
                        m_Api.unload_program();
                    m_ProgramLoaded = false;
                    m_Instances.clear();
                    m_Catalog = {};
                    m_CurrentProgram = {};
                    replacement.Diagnostics.insert(replacement.Diagnostics.end(), restored.Diagnostics.begin(), restored.Diagnostics.end());
                    replacement.Diagnostics.push_back({ ManagedDiagnosticSeverity::Error, "managed.coreclr.reload_rollback_failed",
                                                        "The last working CoreCLR program could not be restored.", {},
                                                        ManagedBackendId::CoreCLR, {}, {} });
                }
                return { replacement, {}, !restored.Succeeded };
            }

            const ScriptCatalog& GetScriptCatalog() const override { return m_Catalog; }

            ManagedBackendCreateResult CreateScript(const ScriptCreateRequest& request) override
            {
                if (!m_ProgramLoaded)
                    return { ProgramNotLoaded(), 0 };
                const ScriptTypeSchema* schema = m_Catalog.FindType(request.Identity);
                if (schema == nullptr)
                    return { ManagedOperationResult::Failure("managed.script.type_missing", "The CoreCLR script type is not in the catalog.",
                                                             ManagedBackendId::CoreCLR),
                             0 };
                ScriptStateResult migrated = MigrateScriptState(request.InitialState, *schema, ManagedBackendId::CoreCLR);
                if (!migrated.Result.Succeeded)
                    return { migrated.Result, 0 };
                uint64_t managedHandle = 0;
                ManagedOperationResult created = CreateManaged(*schema, request.Entity, migrated.State, managedHandle);
                if (!created.Succeeded)
                    return { created, 0 };
                if (m_NextHandle == 0)
                {
                    m_Api.destroy_script(managedHandle);
                    return { ManagedOperationResult::Failure("managed.coreclr.handle_exhausted", "CoreCLR script handles are exhausted.",
                                                             ManagedBackendId::CoreCLR),
                             0 };
                }
                const uint64_t logicalHandle = m_NextHandle++;
                m_Instances.emplace(
                  logicalHandle, Instance{ managedHandle, request.Entity, schema->Identity, std::move(migrated.State.OrphanedMembers) });
                return { created, logicalHandle };
            }

            ManagedOperationResult DestroyScript(uint64_t handle) override
            {
                const auto instance = m_Instances.find(handle);
                if (instance == m_Instances.end())
                    return StaleHandle();
                const cw_managed_status status = m_Api.destroy_script(instance->second.ManagedHandle);
                if (status != CW_MANAGED_STATUS_OK)
                    return StatusFailure(status, "managed.coreclr.destroy_failed", "The managed script could not be destroyed.");
                m_Instances.erase(instance);
                return ManagedOperationResult::Success();
            }

            ManagedOperationResult Dispatch(uint64_t handle, const ScriptEvent& event) override
            {
                const auto instance = m_Instances.find(handle);
                if (instance == m_Instances.end())
                    return StaleHandle();
                cw_managed_event nativeEvent{};
                nativeEvent.size = sizeof(nativeEvent);
                nativeEvent.kind = static_cast<uint32_t>(event.Kind);
                nativeEvent.delta_time = event.DeltaTime;
                nativeEvent.other_entity = ToAbiUuid(event.OtherEntity);
                nativeEvent.relative_velocity[0] = event.RelativeVelocity.x;
                nativeEvent.relative_velocity[1] = event.RelativeVelocity.y;
                nativeEvent.relative_velocity[2] = event.RelativeVelocity.z;
                Vector<cw_managed_contact_point> contacts(event.Contacts.size());
                for (size_t index = 0; index < event.Contacts.size(); ++index)
                {
                    const ScriptContactPoint& source = event.Contacts[index];
                    cw_managed_contact_point& target = contacts[index];
                    target.position[0] = source.Position.x;
                    target.position[1] = source.Position.y;
                    target.position[2] = source.Position.z;
                    target.normal[0] = source.Normal.x;
                    target.normal[1] = source.Normal.y;
                    target.normal[2] = source.Normal.z;
                    target.separation = source.Separation;
                    target.impulse = source.Impulse;
                }
                nativeEvent.payload.data = reinterpret_cast<const uint8_t*>(contacts.data());
                nativeEvent.payload.length = contacts.size() * sizeof(cw_managed_contact_point);
                const cw_managed_status status = m_Api.dispatch(instance->second.ManagedHandle, &nativeEvent);
                return status == CW_MANAGED_STATUS_OK
                         ? ManagedOperationResult::Success()
                         : StatusFailure(status, "managed.coreclr.dispatch_failed", "A managed script event failed.");
            }

            ManagedBackendStateResult CaptureState(uint64_t handle) override
            {
                const auto instance = m_Instances.find(handle);
                if (instance == m_Instances.end())
                    return { StaleHandle(), {} };
                Vector<uint8_t> bytes;
                cw_managed_blob_writer writer{ sizeof(writer), &bytes, &WriteBlob };
                const cw_managed_status status = m_Api.capture_state(instance->second.ManagedHandle, &writer);
                if (status != CW_MANAGED_STATUS_OK)
                    return { StatusFailure(status, "managed.coreclr.capture_failed", "Managed script state capture failed."), {} };
                if (bytes.empty())
                    return { ManagedOperationResult::Failure("managed.coreclr.capture_empty",
                                                              "The managed host returned empty script state.", ManagedBackendId::CoreCLR),
                             {} };
                const ScriptTypeSchema* schema = m_Catalog.FindType(instance->second.Identity);
                if (schema == nullptr)
                    return { ManagedOperationResult::Failure("managed.coreclr.type_missing",
                                                              "The live script type is no longer in the catalog.", ManagedBackendId::CoreCLR),
                             {} };
                ScriptState state;
                ManagedOperationResult parsed = ParseManagedStateJson(
                  StringView(reinterpret_cast<const char*>(bytes.data()), bytes.size()), state, ManagedBackendId::CoreCLR, schema);
                if (parsed.Succeeded)
                    state.OrphanedMembers = instance->second.OrphanedMembers;
                return { parsed, std::move(state) };
            }

            ManagedOperationResult ApplyState(uint64_t handle, const ScriptState& state) override
            {
                const auto instance = m_Instances.find(handle);
                if (instance == m_Instances.end())
                    return StaleHandle();
                const ScriptTypeSchema* schema = m_Catalog.FindType(instance->second.Identity);
                if (schema == nullptr)
                    return ManagedOperationResult::Failure("managed.coreclr.type_missing", "The live script type is no longer in the catalog.",
                                                           ManagedBackendId::CoreCLR);
                ScriptStateResult migrated = MigrateScriptState(state, *schema, ManagedBackendId::CoreCLR);
                if (!migrated.Result.Succeeded)
                    return migrated.Result;
                const String json = WriteManagedStateJson(migrated.State);
                const cw_managed_blob blob{ reinterpret_cast<const uint8_t*>(json.data()), json.size() };
                const cw_managed_status status = m_Api.apply_state(instance->second.ManagedHandle, blob);
                if (status != CW_MANAGED_STATUS_OK)
                    return StatusFailure(status, "managed.coreclr.apply_failed", "Managed script state application failed.");
                instance->second.OrphanedMembers = std::move(migrated.State.OrphanedMembers);
                return migrated.Result;
            }

            Vector<ManagedDiagnostic> Update() override
            {
                CollectManagedDiagnostics();
                Lock lock(m_DiagnosticMutex);
                Vector<ManagedDiagnostic> diagnostics = std::move(m_Diagnostics);
                m_Diagnostics.clear();
                return diagnostics;
            }

        private:
            struct Instance
            {
                uint64_t ManagedHandle = 0;
                UUID Entity;
                ScriptTypeIdentity Identity;
                Map<String, ScriptValue> OrphanedMembers;
            };

            ManagedOperationResult InitializeRuntime(const ManagedProgramDefinition& program)
            {
                const ManagedProgramArtifact* nethost = FindArtifact(program, ManagedProgramArtifactKind::NativeLibrary, "nethost");
                const ManagedProgramArtifact* runtimeConfig =
                  FindArtifact(program, ManagedProgramArtifactKind::RuntimeConfig, "managed-host");
                const ManagedProgramArtifact* hostAssembly =
                  FindArtifact(program, ManagedProgramArtifactKind::EngineAssembly, "managed-host");
                const ManagedProgramArtifact* hostDependencies =
                  FindArtifact(program, ManagedProgramArtifactKind::DependencyManifest, "managed-host");
                if (nethost == nullptr || runtimeConfig == nullptr || hostAssembly == nullptr || !IsRegularFile(nethost->Filepath) ||
                    !IsRegularFile(runtimeConfig->Filepath) || !IsRegularFile(hostAssembly->Filepath) || hostDependencies == nullptr ||
                    !IsRegularFile(hostDependencies->Filepath))
                    return ManagedOperationResult::Failure("managed.coreclr.host_artifacts_missing",
                                                           "CoreCLR requires nethost and the managed host DLL, deps, and runtimeconfig files.",
                                                           ManagedBackendId::CoreCLR);
                if (!m_NetHost.Load(nethost->Filepath))
                    return ManagedOperationResult::Failure("managed.coreclr.nethost_load_failed", "Could not load the packaged nethost library.",
                                                           ManagedBackendId::CoreCLR);
                auto getHostFxrPath = reinterpret_cast<GetHostFxrPathFn>(m_NetHost.Find("get_hostfxr_path"));
                if (getHostFxrPath == nullptr)
                    return ManagedOperationResult::Failure("managed.coreclr.nethost_symbol_missing", "nethost has no get_hostfxr_path export.",
                                                           ManagedBackendId::CoreCLR);

                const auto dotnetRoot = ToHostString(m_Config.RuntimeRoot);
                Vector<HostChar> hostFxrPath(1024);
                size_t hostFxrPathSize = hostFxrPath.size();
                const GetHostFxrParameters pathParameters{ sizeof(pathParameters), nullptr, dotnetRoot.c_str() };
                int32_t pathStatus = getHostFxrPath(hostFxrPath.data(), &hostFxrPathSize, &pathParameters);
                if (pathStatus == NETHOST_BUFFER_TOO_SMALL && hostFxrPathSize > hostFxrPath.size())
                {
                    hostFxrPath.resize(hostFxrPathSize);
                    pathStatus = getHostFxrPath(hostFxrPath.data(), &hostFxrPathSize, &pathParameters);
                }
                if (pathStatus != HOSTFXR_SUCCESS || hostFxrPathSize == 0 || !m_HostFxr.Load(Path(hostFxrPath.data())))
                    return ManagedOperationResult::Failure("managed.coreclr.hostfxr_path_failed", "nethost could not resolve the private hostfxr.",
                                                           ManagedBackendId::CoreCLR);

                auto initialize = reinterpret_cast<HostFxrInitializeForRuntimeConfigFn>(m_HostFxr.Find("hostfxr_initialize_for_runtime_config"));
                auto getDelegate = reinterpret_cast<HostFxrGetRuntimeDelegateFn>(m_HostFxr.Find("hostfxr_get_runtime_delegate"));
                auto close = reinterpret_cast<HostFxrCloseFn>(m_HostFxr.Find("hostfxr_close"));
                if (initialize == nullptr || getDelegate == nullptr || close == nullptr)
                    return ManagedOperationResult::Failure("managed.coreclr.hostfxr_symbol_missing", "hostfxr is missing required hosting exports.",
                                                           ManagedBackendId::CoreCLR);

                const auto configPath = ToHostString(runtimeConfig->Filepath);
                const HostFxrInitializeParameters initializeParameters{ sizeof(initializeParameters), nullptr, dotnetRoot.c_str() };
                HostFxrHandle context = nullptr;
                const int32_t initializeStatus = initialize(configPath.c_str(), &initializeParameters, &context);
                if (initializeStatus == HOSTFXR_SUCCESS_DIFFERENT_PROPERTIES)
                {
                    if (context != nullptr)
                        close(context);
                    return ManagedOperationResult::Failure(
                      "managed.coreclr.runtime_properties_mismatch",
                      "A process-wide CoreCLR is already running with different runtime properties.", ManagedBackendId::CoreCLR);
                }
                if (!IsUsableHostFxrInitialization(initializeStatus) || context == nullptr)
                {
                    if (context != nullptr)
                        close(context);
                    return ManagedOperationResult::Failure("managed.coreclr.initialize_failed", "hostfxr could not initialize CoreCLR.",
                                                           ManagedBackendId::CoreCLR);
                }
                void* loader = nullptr;
                const int32_t delegateStatus = getDelegate(context, HostFxrDelegateType::LoadAssemblyAndGetFunctionPointer, &loader);
                close(context);
                if (delegateStatus != HOSTFXR_SUCCESS || loader == nullptr)
                    return ManagedOperationResult::Failure("managed.coreclr.delegate_failed", "hostfxr did not provide the component loader.",
                                                           ManagedBackendId::CoreCLR);

                auto loadAssembly = reinterpret_cast<LoadAssemblyAndGetFunctionPointerFn>(loader);
                const auto assemblyPath = ToHostString(hostAssembly->Filepath);
                const auto typeName = ToHostString(StringView(CW_MANAGED_BOOTSTRAP_TYPE));
                const auto methodName = ToHostString(StringView(CW_MANAGED_BOOTSTRAP_METHOD));
                void* getApiPointer = nullptr;
                const HostChar* unmanagedCallersOnly = reinterpret_cast<const HostChar*>(static_cast<intptr_t>(-1));
                if (loadAssembly(assemblyPath.c_str(), typeName.c_str(), methodName.c_str(), unmanagedCallersOnly, nullptr, &getApiPointer) != 0 ||
                    getApiPointer == nullptr)
                    return ManagedOperationResult::Failure("managed.coreclr.bootstrap_failed", "CoreCLR could not load the Crowny managed bootstrap.",
                                                           ManagedBackendId::CoreCLR);

                const auto getApi = reinterpret_cast<cw_managed_get_api_fn>(getApiPointer);
                if (cw_managed_status status = getApi(&m_Api, sizeof(m_Api)); status != CW_MANAGED_STATUS_OK)
                    return StatusFailure(status, "managed.coreclr.api_failed", "The managed bootstrap rejected the ABI table request.");
                ManagedOperationResult validation = ValidateManagedProgramApi(m_Api, ManagedBackendId::CoreCLR);
                if (!validation.Succeeded)
                    return validation;
                cw_managed_host_api hostApi{ sizeof(hostApi), CW_MANAGED_ABI_VERSION, this,          &Log,
                                             &GetEntityName, &SetEntityName,          &FindEntityByName,
                                             &GetEntityParent, &SetEntityParent,      &DestroyEntity,
                                             &InvokeHostBinding };
                if (cw_managed_status status = m_Api.initialize(&hostApi); status != CW_MANAGED_STATUS_OK)
                    return StatusFailure(status, "managed.coreclr.bootstrap_initialize_failed", "The managed bootstrap failed to initialize.");
                m_RuntimeReady = true;
                return ManagedOperationResult::Success();
            }

            ManagedOperationResult LoadManagedProgram(const ManagedProgramDefinition& program, ScriptCatalog& catalog)
            {
                const ManagedProgramArtifact* gameAssembly = FindArtifact(program, ManagedProgramArtifactKind::GameAssembly, "game");
                const ManagedProgramArtifact* gameDependencies =
                  FindArtifact(program, ManagedProgramArtifactKind::DependencyManifest, "game");
                if (gameAssembly == nullptr || !IsRegularFile(gameAssembly->Filepath) || gameDependencies == nullptr ||
                    !IsRegularFile(gameDependencies->Filepath))
                    return ManagedOperationResult::Failure("managed.coreclr.game_assembly_missing",
                                                           "The CoreCLR game assembly or dependency manifest is missing.",
                                                           ManagedBackendId::CoreCLR);
                AbiString path(ToUtf8Path(gameAssembly->Filepath));
                if (cw_managed_status status = m_Api.load_program(path.View, program.Generation); status != CW_MANAGED_STATUS_OK)
                    return StatusFailure(status, "managed.coreclr.program_load_failed", "The CoreCLR game program failed to load.");
                Vector<uint8_t> bytes;
                cw_managed_blob_writer writer{ sizeof(writer), &bytes, &WriteBlob };
                if (cw_managed_status status = m_Api.get_catalog(&writer); status != CW_MANAGED_STATUS_OK)
                {
                    m_Api.unload_program();
                    return StatusFailure(status, "managed.coreclr.catalog_failed", "The CoreCLR script catalog could not be read.");
                }
                if (bytes.empty())
                {
                    m_Api.unload_program();
                    return ManagedOperationResult::Failure("managed.coreclr.catalog_empty",
                                                           "The CoreCLR managed host returned an empty script catalog.",
                                                           ManagedBackendId::CoreCLR);
                }
                ManagedOperationResult parsed = ParseManagedCatalogJson(
                  StringView(reinterpret_cast<const char*>(bytes.data()), bytes.size()), catalog, ManagedBackendId::CoreCLR);
                if (!parsed.Succeeded)
                    m_Api.unload_program();
                return parsed;
            }

            ManagedOperationResult RecreateInstances(const Vector<ManagedBackendReloadInstance>& snapshots, const ScriptCatalog& catalog,
                                                     Map<uint64_t, Instance>& instances)
            {
                for (const ManagedBackendReloadInstance& snapshot : snapshots)
                {
                    const ScriptTypeSchema* schema = catalog.FindType(snapshot.State.Identity);
                    if (schema == nullptr)
                        return ManagedOperationResult::Failure("managed.coreclr.reload_type_missing",
                                                               "A live script type is missing from the replacement program.", ManagedBackendId::CoreCLR);
                    ScriptStateResult migrated = MigrateScriptState(snapshot.State, *schema, ManagedBackendId::CoreCLR);
                    if (!migrated.Result.Succeeded)
                        return migrated.Result;
                    uint64_t managedHandle = 0;
                    ManagedOperationResult result = CreateManaged(*schema, snapshot.Entity, migrated.State, managedHandle);
                    if (!result.Succeeded)
                        return result;
                    instances.emplace(snapshot.PreviousHandle,
                                      Instance{ managedHandle, snapshot.Entity, schema->Identity,
                                                std::move(migrated.State.OrphanedMembers) });
                }
                return ManagedOperationResult::Success();
            }

            ManagedOperationResult CreateManaged(const ScriptTypeSchema& schema, const UUID& entity, const ScriptState& state,
                                                 uint64_t& managedHandle)
            {
                AbiString assembly(schema.Identity.Assembly);
                AbiString typeNamespace(schema.Identity.Namespace);
                AbiString typeName(schema.Identity.TypeName);
                const String json = WriteManagedStateJson(state);
                const cw_managed_blob blob{ reinterpret_cast<const uint8_t*>(json.data()), json.size() };
                const cw_managed_status status = m_Api.create_script(assembly.View, typeNamespace.View, typeName.View, ToAbiUuid(entity), blob,
                                                                     &managedHandle);
                return status == CW_MANAGED_STATUS_OK
                         ? ManagedOperationResult::Success()
                         : StatusFailure(status, "managed.coreclr.create_failed", "The CoreCLR script instance could not be created.");
            }

            ManagedOperationResult StatusFailure(cw_managed_status status, String code, String message)
            {
                CollectManagedDiagnostics();
                ManagedOperationResult result = ManagedOperationResult::Failure(std::move(code), message + " Status " + std::to_string(status) + ".",
                                                                                 ManagedBackendId::CoreCLR);
                Lock lock(m_DiagnosticMutex);
                result.Diagnostics.insert(result.Diagnostics.end(), m_Diagnostics.begin(), m_Diagnostics.end());
                m_Diagnostics.clear();
                return result;
            }

            void CollectManagedDiagnostics()
            {
                if (!m_RuntimeReady || m_Api.collect_diagnostics == nullptr)
                    return;
                Vector<uint8_t> bytes;
                cw_managed_blob_writer writer{ sizeof(writer), &bytes, &WriteBlob };
                if (m_Api.collect_diagnostics(&writer) != CW_MANAGED_STATUS_OK || bytes.empty())
                    return;
                Vector<ManagedDiagnostic> diagnostics = ParseManagedDiagnosticsJson(
                  StringView(reinterpret_cast<const char*>(bytes.data()), bytes.size()), ManagedBackendId::CoreCLR);
                Lock lock(m_DiagnosticMutex);
                m_Diagnostics.insert(m_Diagnostics.end(), std::make_move_iterator(diagnostics.begin()),
                                     std::make_move_iterator(diagnostics.end()));
            }

            static String Decode(cw_managed_string_view value)
            {
                return value.data == nullptr ? String() : String(reinterpret_cast<const char*>(value.data), value.length);
            }

            static Ref<Scene> ActiveScene()
            {
                SceneManager* manager = SceneManager::TryGet();
                return manager != nullptr ? manager->GetActiveScene() : nullptr;
            }

            static cw_managed_status CW_MANAGED_CALL GetEntityName(void* context, cw_managed_uuid entityId,
                                                                    cw_managed_string_view* name)
            {
                if (context == nullptr || name == nullptr)
                    return CW_MANAGED_STATUS_INVALID_ARGUMENT;
                try
                {
                    const Ref<Scene> scene = ActiveScene();
                    const Entity entity = scene != nullptr ? scene->TryGetEntityFromUuid(FromAbiUuid(entityId)) : Entity();
                    if (!entity)
                        return CW_MANAGED_STATUS_STALE_HANDLE;
                    thread_local String storage;
                    storage = entity.GetName();
                    if (storage.size() > std::numeric_limits<uint32_t>::max())
                        return CW_MANAGED_STATUS_BUFFER_WRITE_FAILED;
                    name->data = reinterpret_cast<const uint8_t*>(storage.data());
                    name->length = static_cast<uint32_t>(storage.size());
                    return CW_MANAGED_STATUS_OK;
                }
                catch (...)
                {
                    return CW_MANAGED_STATUS_MANAGED_EXCEPTION;
                }
            }

            static cw_managed_status CW_MANAGED_CALL SetEntityName(void* context, cw_managed_uuid entityId,
                                                                    cw_managed_string_view name)
            {
                if (context == nullptr || (name.data == nullptr && name.length != 0))
                    return CW_MANAGED_STATUS_INVALID_ARGUMENT;
                try
                {
                    const Ref<Scene> scene = ActiveScene();
                    Entity entity = scene != nullptr ? scene->TryGetEntityFromUuid(FromAbiUuid(entityId)) : Entity();
                    if (!entity)
                        return CW_MANAGED_STATUS_STALE_HANDLE;
                    entity.GetComponent<TagComponent>().Tag = Decode(name);
                    return CW_MANAGED_STATUS_OK;
                }
                catch (...)
                {
                    return CW_MANAGED_STATUS_MANAGED_EXCEPTION;
                }
            }

            static cw_managed_status CW_MANAGED_CALL FindEntityByName(void* context, cw_managed_string_view name,
                                                                       cw_managed_uuid* entityId)
            {
                if (context == nullptr || entityId == nullptr || (name.data == nullptr && name.length != 0))
                    return CW_MANAGED_STATUS_INVALID_ARGUMENT;
                try
                {
                    const Ref<Scene> scene = ActiveScene();
                    if (scene == nullptr)
                        return CW_MANAGED_STATUS_NOT_INITIALIZED;
                    const Entity entity = scene->FindEntityByName(Decode(name));
                    *entityId = entity ? ToAbiUuid(entity.GetUuid()) : cw_managed_uuid{};
                    return CW_MANAGED_STATUS_OK;
                }
                catch (...)
                {
                    return CW_MANAGED_STATUS_MANAGED_EXCEPTION;
                }
            }

            static cw_managed_status CW_MANAGED_CALL GetEntityParent(void* context, cw_managed_uuid entityId,
                                                                      cw_managed_uuid* parentId)
            {
                if (context == nullptr || parentId == nullptr)
                    return CW_MANAGED_STATUS_INVALID_ARGUMENT;
                try
                {
                    const Ref<Scene> scene = ActiveScene();
                    const Entity entity = scene != nullptr ? scene->TryGetEntityFromUuid(FromAbiUuid(entityId)) : Entity();
                    if (!entity)
                        return CW_MANAGED_STATUS_STALE_HANDLE;
                    const Entity parent = entity.GetParent();
                    *parentId = parent ? ToAbiUuid(parent.GetUuid()) : cw_managed_uuid{};
                    return CW_MANAGED_STATUS_OK;
                }
                catch (...)
                {
                    return CW_MANAGED_STATUS_MANAGED_EXCEPTION;
                }
            }

            static cw_managed_status CW_MANAGED_CALL SetEntityParent(void* context, cw_managed_uuid entityId,
                                                                      cw_managed_uuid parentId)
            {
                if (context == nullptr)
                    return CW_MANAGED_STATUS_INVALID_ARGUMENT;
                try
                {
                    const Ref<Scene> scene = ActiveScene();
                    Entity entity = scene != nullptr ? scene->TryGetEntityFromUuid(FromAbiUuid(entityId)) : Entity();
                    if (!entity)
                        return CW_MANAGED_STATUS_STALE_HANDLE;
                    const UUID parentUuid = FromAbiUuid(parentId);
                    if (parentUuid.Empty())
                        return CW_MANAGED_STATUS_OK;
                    const Entity parent = scene->TryGetEntityFromUuid(parentUuid);
                    if (!parent)
                        return CW_MANAGED_STATUS_STALE_HANDLE;
                    return entity.SetParent(parent) ? CW_MANAGED_STATUS_OK : CW_MANAGED_STATUS_INVALID_ARGUMENT;
                }
                catch (...)
                {
                    return CW_MANAGED_STATUS_MANAGED_EXCEPTION;
                }
            }

            static cw_managed_status CW_MANAGED_CALL DestroyEntity(void* context, cw_managed_uuid entityId)
            {
                if (context == nullptr)
                    return CW_MANAGED_STATUS_INVALID_ARGUMENT;
                try
                {
                    const Ref<Scene> scene = ActiveScene();
                    const Entity entity = scene != nullptr ? scene->TryGetEntityFromUuid(FromAbiUuid(entityId)) : Entity();
                    if (!entity)
                        return CW_MANAGED_STATUS_STALE_HANDLE;
                    scene->DestroyEntity(entity);
                    return CW_MANAGED_STATUS_OK;
                }
                catch (...)
                {
                    return CW_MANAGED_STATUS_MANAGED_EXCEPTION;
                }
            }

            static cw_managed_status CW_MANAGED_CALL InvokeHostBinding(void* context, uint32_t binding, cw_managed_uuid entityId,
                                                                         cw_managed_blob input, cw_managed_blob* output)
            {
                if (context == nullptr || output == nullptr || (input.data == nullptr && input.length != 0))
                    return CW_MANAGED_STATUS_INVALID_ARGUMENT;
                WriteBindingResult(output, nullptr, 0);
                try
                {
                    const auto resolveEntity = [&]() {
                        const Ref<Scene> scene = ActiveScene();
                        return scene != nullptr ? scene->TryGetEntityFromUuid(FromAbiUuid(entityId)) : Entity();
                    };
                    const auto readCode = [&]() {
                        uint32_t code = 0;
                        if (input.length == sizeof(code))
                            std::memcpy(&code, input.data, sizeof(code));
                        return code;
                    };
                    const auto writeVector2 = [&](const glm::vec2& value) {
                        const float fields[] = { value.x, value.y };
                        return WriteBindingResult(output, fields, sizeof(fields));
                    };
                    const auto writeVector3 = [&](const glm::vec3& value) {
                        const float fields[] = { value.x, value.y, value.z };
                        return WriteBindingResult(output, fields, sizeof(fields));
                    };
                    const auto writeQuaternion = [&](const glm::quat& value) {
                        const float fields[] = { value.x, value.y, value.z, value.w };
                        return WriteBindingResult(output, fields, sizeof(fields));
                    };

                    switch (binding)
                    {
                    case CW_MANAGED_BINDING_ENTITY_HAS_COMPONENT:
                    case CW_MANAGED_BINDING_ENTITY_ADD_COMPONENT:
                    case CW_MANAGED_BINDING_ENTITY_REMOVE_COMPONENT: {
                        if (input.length > std::numeric_limits<size_t>::max())
                            return CW_MANAGED_STATUS_INVALID_ARGUMENT;
                        Entity entity = resolveEntity();
                        if (!entity)
                            return CW_MANAGED_STATUS_STALE_HANDLE;
                        const StringView name(reinterpret_cast<const char*>(input.data), static_cast<size_t>(input.length));
                        if (binding == CW_MANAGED_BINDING_ENTITY_HAS_COMPONENT)
                        {
                            const uint8_t result = HasComponent(entity, name) ? 1 : 0;
                            return WriteBindingResult(output, result);
                        }
                        const bool succeeded = binding == CW_MANAGED_BINDING_ENTITY_ADD_COMPONENT ? AddComponent(entity, name)
                                                                                                  : RemoveComponent(entity, name);
                        return succeeded ? CW_MANAGED_STATUS_OK : CW_MANAGED_STATUS_INVALID_ARGUMENT;
                    }
                    case CW_MANAGED_BINDING_TRANSFORM_GET_POSITION:
                    case CW_MANAGED_BINDING_TRANSFORM_GET_LOCAL_POSITION:
                    case CW_MANAGED_BINDING_TRANSFORM_GET_SCALE:
                    case CW_MANAGED_BINDING_TRANSFORM_GET_LOCAL_SCALE:
                    case CW_MANAGED_BINDING_TRANSFORM_GET_ROTATION:
                    case CW_MANAGED_BINDING_TRANSFORM_GET_LOCAL_ROTATION:
                    case CW_MANAGED_BINDING_TRANSFORM_GET_LOCAL_TO_WORLD_MATRIX:
                    case CW_MANAGED_BINDING_TRANSFORM_GET_WORLD_TO_LOCAL_MATRIX:
                    case CW_MANAGED_BINDING_TRANSFORM_GET_EULER_ANGLES:
                    case CW_MANAGED_BINDING_TRANSFORM_GET_LOCAL_EULER_ANGLES: {
                        Entity entity = resolveEntity();
                        if (!entity)
                            return CW_MANAGED_STATUS_STALE_HANDLE;
                        switch (binding)
                        {
                        case CW_MANAGED_BINDING_TRANSFORM_GET_POSITION: return writeVector3(entity.GetWorldPosition());
                        case CW_MANAGED_BINDING_TRANSFORM_GET_LOCAL_POSITION: return writeVector3(entity.GetLocalPosition());
                        case CW_MANAGED_BINDING_TRANSFORM_GET_SCALE: return writeVector3(entity.GetWorldScale());
                        case CW_MANAGED_BINDING_TRANSFORM_GET_LOCAL_SCALE: return writeVector3(entity.GetLocalScale());
                        case CW_MANAGED_BINDING_TRANSFORM_GET_ROTATION: return writeQuaternion(entity.GetWorldRotation());
                        case CW_MANAGED_BINDING_TRANSFORM_GET_LOCAL_ROTATION: return writeQuaternion(entity.GetLocalRotation());
                        case CW_MANAGED_BINDING_TRANSFORM_GET_LOCAL_TO_WORLD_MATRIX:
                            return WriteBindingResult(output, glm::value_ptr(entity.GetWorldMatrix()), sizeof(glm::mat4));
                        case CW_MANAGED_BINDING_TRANSFORM_GET_WORLD_TO_LOCAL_MATRIX: {
                            const glm::mat4 inverse = glm::inverse(entity.GetWorldMatrix());
                            return WriteBindingResult(output, glm::value_ptr(inverse), sizeof(inverse));
                        }
                        case CW_MANAGED_BINDING_TRANSFORM_GET_EULER_ANGLES:
                            return writeVector3(glm::degrees(glm::eulerAngles(entity.GetWorldRotation())));
                        case CW_MANAGED_BINDING_TRANSFORM_GET_LOCAL_EULER_ANGLES:
                            return writeVector3(glm::degrees(glm::eulerAngles(entity.GetLocalRotation())));
                        default: return CW_MANAGED_STATUS_INVALID_ARGUMENT;
                        }
                    }
                    case CW_MANAGED_BINDING_TRANSFORM_SET_POSITION:
                    case CW_MANAGED_BINDING_TRANSFORM_SET_LOCAL_POSITION:
                    case CW_MANAGED_BINDING_TRANSFORM_SET_SCALE:
                    case CW_MANAGED_BINDING_TRANSFORM_SET_LOCAL_SCALE:
                    case CW_MANAGED_BINDING_TRANSFORM_SET_EULER_ANGLES:
                    case CW_MANAGED_BINDING_TRANSFORM_SET_LOCAL_EULER_ANGLES: {
                        float value[3]{};
                        if (!ReadBindingFloats(input, value, 3))
                            return CW_MANAGED_STATUS_INVALID_ARGUMENT;
                        Entity entity = resolveEntity();
                        if (!entity)
                            return CW_MANAGED_STATUS_STALE_HANDLE;
                        const glm::vec3 vector(value[0], value[1], value[2]);
                        if (binding == CW_MANAGED_BINDING_TRANSFORM_SET_POSITION)
                            entity.SetWorldPosition(vector);
                        else if (binding == CW_MANAGED_BINDING_TRANSFORM_SET_LOCAL_POSITION)
                            entity.SetPosition(vector);
                        else if (binding == CW_MANAGED_BINDING_TRANSFORM_SET_SCALE)
                            entity.SetWorldScale(vector);
                        else if (binding == CW_MANAGED_BINDING_TRANSFORM_SET_LOCAL_SCALE)
                            entity.SetScale(vector);
                        else if (binding == CW_MANAGED_BINDING_TRANSFORM_SET_EULER_ANGLES)
                            entity.SetWorldRotation(glm::quat(glm::radians(vector)));
                        else
                            entity.SetRotation(glm::quat(glm::radians(vector)));
                        return CW_MANAGED_STATUS_OK;
                    }
                    case CW_MANAGED_BINDING_TRANSFORM_SET_ROTATION:
                    case CW_MANAGED_BINDING_TRANSFORM_SET_LOCAL_ROTATION: {
                        float value[4]{};
                        if (!ReadBindingFloats(input, value, 4))
                            return CW_MANAGED_STATUS_INVALID_ARGUMENT;
                        Entity entity = resolveEntity();
                        if (!entity)
                            return CW_MANAGED_STATUS_STALE_HANDLE;
                        const glm::quat rotation(value[3], value[0], value[1], value[2]);
                        if (binding == CW_MANAGED_BINDING_TRANSFORM_SET_ROTATION)
                            entity.SetWorldRotation(rotation);
                        else
                            entity.SetRotation(rotation);
                        return CW_MANAGED_STATUS_OK;
                    }
                    case CW_MANAGED_BINDING_INPUT_GET_KEY:
                    case CW_MANAGED_BINDING_INPUT_GET_KEY_DOWN:
                    case CW_MANAGED_BINDING_INPUT_GET_KEY_UP: {
                        if (input.length != sizeof(uint32_t))
                            return CW_MANAGED_STATUS_INVALID_ARGUMENT;
                        const KeyCode code = static_cast<KeyCode>(readCode());
                        const bool pressed = binding == CW_MANAGED_BINDING_INPUT_GET_KEY        ? Input::IsKeyPressed(code)
                                             : binding == CW_MANAGED_BINDING_INPUT_GET_KEY_DOWN ? Input::IsKeyDown(code)
                                                                                               : Input::IsKeyUp(code);
                        const uint8_t result = pressed ? 1 : 0;
                        return WriteBindingResult(output, result);
                    }
                    case CW_MANAGED_BINDING_INPUT_GET_MOUSE_BUTTON:
                    case CW_MANAGED_BINDING_INPUT_GET_MOUSE_BUTTON_DOWN:
                    case CW_MANAGED_BINDING_INPUT_GET_MOUSE_BUTTON_UP: {
                        if (input.length != sizeof(uint32_t))
                            return CW_MANAGED_STATUS_INVALID_ARGUMENT;
                        const MouseCode code = static_cast<MouseCode>(readCode());
                        const bool pressed = binding == CW_MANAGED_BINDING_INPUT_GET_MOUSE_BUTTON        ? Input::IsMouseButtonPressed(code)
                                             : binding == CW_MANAGED_BINDING_INPUT_GET_MOUSE_BUTTON_DOWN ? Input::IsMouseButtonDown(code)
                                                                                                         : Input::IsMouseButtonUp(code);
                        const uint8_t result = pressed ? 1 : 0;
                        return WriteBindingResult(output, result);
                    }
                    case CW_MANAGED_BINDING_INPUT_GET_MOUSE_SCROLL_X: return WriteBindingResult(output, Input::GetMouseScrollX());
                    case CW_MANAGED_BINDING_INPUT_GET_MOUSE_SCROLL_Y: return WriteBindingResult(output, Input::GetMouseScrollY());
                    case CW_MANAGED_BINDING_INPUT_GET_MOUSE_POSITION: return writeVector2(Input::GetMousePosition());
                    case CW_MANAGED_BINDING_TIME_GET_TIME: return WriteBindingResult(output, Time::GetTime());
                    case CW_MANAGED_BINDING_TIME_GET_FIXED_DELTA_TIME: return WriteBindingResult(output, Time::GetFixedDeltaTime());
                    case CW_MANAGED_BINDING_TIME_GET_SMOOTH_DELTA_TIME: return WriteBindingResult(output, Time::GetSmoothDeltaTime());
                    case CW_MANAGED_BINDING_TIME_GET_REALTIME_SINCE_STARTUP:
                        return WriteBindingResult(output, Time::GetRealtimeSinceStartup());
                    case CW_MANAGED_BINDING_TIME_GET_FRAME_COUNT: return WriteBindingResult(output, Time::GetFrameCount());
                    default: return CW_MANAGED_STATUS_INVALID_ARGUMENT;
                    }
                }
                catch (...)
                {
                    return CW_MANAGED_STATUS_MANAGED_EXCEPTION;
                }
            }

            static void CW_MANAGED_CALL Log(void* context, uint32_t severity, cw_managed_string_view code, cw_managed_string_view message,
                                            cw_managed_string_view stack)
            {
                if (context == nullptr)
                    return;
                try
                {
                    auto* backend = static_cast<CoreClrBackend*>(context);
                    ManagedDiagnostic diagnostic;
                    diagnostic.Severity = severity == 0 ? ManagedDiagnosticSeverity::Info
                                          : severity == 1 ? ManagedDiagnosticSeverity::Warning
                                                          : ManagedDiagnosticSeverity::Error;
                    diagnostic.Code = Decode(code);
                    diagnostic.Message = Decode(message);
                    diagnostic.ManagedStack = Decode(stack);
                    diagnostic.Backend = ManagedBackendId::CoreCLR;
                    Lock lock(backend->m_DiagnosticMutex);
                    backend->m_Diagnostics.push_back(std::move(diagnostic));
                }
                catch (...)
                {
                    // Native callbacks must not let C++ exceptions cross the managed ABI.
                }
            }

            static ManagedOperationResult NotStarted()
            {
                return ManagedOperationResult::Failure("managed.coreclr.not_started", "The CoreCLR backend is not running.",
                                                       ManagedBackendId::CoreCLR);
            }

            static ManagedOperationResult ProgramNotLoaded()
            {
                return ManagedOperationResult::Failure("managed.coreclr.program_not_loaded", "No CoreCLR game program is loaded.",
                                                       ManagedBackendId::CoreCLR);
            }

            static ManagedOperationResult StaleHandle()
            {
                return ManagedOperationResult::Failure("managed.coreclr.stale_handle", "The CoreCLR script handle is stale.",
                                                       ManagedBackendId::CoreCLR);
            }

            bool m_Started = false;
            bool m_RuntimeReady = false;
            bool m_ProgramLoaded = false;
            uint64_t m_NextHandle = 1;
            ManagedScriptingConfig m_Config;
            DynamicLibrary m_NetHost;
            DynamicLibrary m_HostFxr;
            cw_managed_program_api m_Api{};
            ManagedProgramDefinition m_CurrentProgram;
            ScriptCatalog m_Catalog;
            Map<uint64_t, Instance> m_Instances;
            Mutex m_DiagnosticMutex;
            Vector<ManagedDiagnostic> m_Diagnostics;
        };
    } // namespace

    Scope<ManagedBackend> CreateCoreClrBackend() { return CreateScope<CoreClrBackend>(); }
#endif
} // namespace Crowny
