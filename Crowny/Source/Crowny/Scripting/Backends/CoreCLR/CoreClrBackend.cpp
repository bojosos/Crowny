#include "cwpch.h"

#include "Crowny/Assets/AssetManager.h"
#include "Crowny/Common/Time.h"
#include "Crowny/Common/UTF8.h"
#include "Crowny/Ecs/Components.h"
#include "Crowny/Input/Input.h"
#include "Crowny/Physics/Physics2D.h"
#include "Crowny/Renderer/Font.h"
#include "Crowny/Scene/SceneManager.h"
#include "Crowny/Scripting/Managed/Internal/ManagedBackend.h"
#include "Crowny/Scripting/Managed/Interop/CrownyManagedAbi.h"
#include "Crowny/Scripting/Managed/Interop/ManagedAbiValidation.h"
#include "Crowny/Scripting/Managed/Interop/ManagedJson.h"

#include <cstring>
#include <limits>

#include <glm/gtc/matrix_inverse.hpp>
#include <glm/gtc/matrix_transform.hpp>
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
        using HostFxrInitializeForRuntimeConfigFn = int32_t(CW_HOSTFXR_CALL*)(const HostChar*, const HostFxrInitializeParameters*, HostFxrHandle*);
        using HostFxrGetRuntimeDelegateFn = int32_t(CW_HOSTFXR_CALL*)(HostFxrHandle, HostFxrDelegateType, void**);
        using HostFxrCloseFn = int32_t(CW_HOSTFXR_CALL*)(HostFxrHandle);
        using LoadAssemblyAndGetFunctionPointerFn = int32_t(CW_CORECLR_DELEGATE_CALL*)(const HostChar*, const HostChar*, const HostChar*,
                                                                                       const HostChar*, void*, void**);

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

        bool IsUsableHostFxrInitialization(int32_t status) { return status == HOSTFXR_SUCCESS || status == HOSTFXR_SUCCESS_ALREADY_INITIALIZED; }

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
                const uint8_t value = character >= '0' && character <= '9'   ? static_cast<uint8_t>(character - '0')
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

        cw_managed_status ResolveFontHandle(const cw_managed_uuid& fontId, AssetHandle<Font>& font)
        {
            AssetManager* assetManager = AssetManager::TryGet();
            if (assetManager == nullptr)
                return CW_MANAGED_STATUS_NOT_INITIALIZED;
            const UUID uuid = FromAbiUuid(fontId);
            if (uuid.Empty())
                return CW_MANAGED_STATUS_STALE_HANDLE;
            font = assetManager->LoadFromUUID<Font>(uuid);
            return font ? CW_MANAGED_STATUS_OK : CW_MANAGED_STATUS_STALE_HANDLE;
        }

        UUID GetSourceFontUuid(const AssetHandle<Font>& font, const Font* source)
        {
            if (source == nullptr)
                return {};
            if (font.Get() == source)
                return font.GetUUID();
            if (!font)
                return {};
            for (const AssetHandle<Font>& fallback : font->GetFallbackFonts())
            {
                if (fallback.Get() == source)
                    return fallback.GetUUID();
            }
            return {};
        }

        cw_managed_font_character_info ToAbiCharacterInfo(const AssetHandle<Font>& font, const CharacterInfo& source)
        {
            cw_managed_font_character_info result{};
            result.source_font = ToAbiUuid(GetSourceFontUuid(font, source.SourceFont));
            result.requested_code_point = static_cast<uint32_t>(source.RequestedCodePoint);
            result.resolved_code_point = static_cast<uint32_t>(source.ResolvedCodePoint);
            result.glyph_index = source.GlyphIndex;
            result.advance = source.Advance;
            result.plane_left = source.PlaneLeft;
            result.plane_bottom = source.PlaneBottom;
            result.plane_right = source.PlaneRight;
            result.plane_top = source.PlaneTop;
            result.atlas_left = source.AtlasLeft;
            result.atlas_bottom = source.AtlasBottom;
            result.atlas_right = source.AtlasRight;
            result.atlas_top = source.AtlasTop;
            result.whitespace = source.Whitespace ? 1 : 0;
            result.valid = source.Valid ? 1 : 0;
            return result;
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

        template <typename T> bool ReadBindingValue(cw_managed_blob input, T& output)
        {
            if (input.data == nullptr || input.length != sizeof(T))
                return false;
            std::memcpy(&output, input.data, sizeof(T));
            return true;
        }

        cw_managed_status WriteBindingResult(cw_managed_blob* output, const void* data, size_t size)
        {
            if (output == nullptr || (data == nullptr && size != 0))
                return CW_MANAGED_STATUS_INVALID_ARGUMENT;
            thread_local Vector<uint8_t> storage;
            if (size != 0)
            {
                storage.resize(size);
                std::memcpy(storage.data(), data, size);
            }
            output->data = size == 0 ? nullptr : storage.data();
            output->length = size;
            return CW_MANAGED_STATUS_OK;
        }

        template <typename T> cw_managed_status WriteBindingResult(cw_managed_blob* output, const T& value)
        {
            return WriteBindingResult(output, &value, sizeof(value));
        }

        static_assert(sizeof(glm::mat4) == 64, "The managed matrix binding requires a packed 4x4 float matrix.");

        bool HasComponent(Entity entity, StringView name)
        {
            if (name == "Crowny.Transform")
                return entity.HasComponent<TransformComponent>();
#define CW_HAS_MANAGED_COMPONENT(managedName, nativeType)                                                                                            \
    if (name == managedName)                                                                                                                         \
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
#define CW_ADD_MANAGED_COMPONENT(managedName, nativeType)                                                                                            \
    if (name == managedName)                                                                                                                         \
    {                                                                                                                                                \
        entity.AddOrGetComponent<nativeType>();                                                                                                      \
        return true;                                                                                                                                 \
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
#define CW_REMOVE_MANAGED_COMPONENT(managedName, nativeType)                                                                                         \
    if (name == managedName)                                                                                                                         \
    {                                                                                                                                                \
        entity.RemoveComponentIfExists<nativeType>();                                                                                                \
        return true;                                                                                                                                 \
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
                    return ManagedOperationResult::Failure("managed.coreclr.execution_mode", "CoreCLR supports JIT and ReadyToRun execution modes.",
                                                           ManagedBackendId::CoreCLR);
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
                    replacement.Diagnostics.push_back({ ManagedDiagnosticSeverity::Error,
                                                        "managed.coreclr.reload_rollback_failed",
                                                        "The last working CoreCLR program could not be restored.",
                                                        {},
                                                        ManagedBackendId::CoreCLR,
                                                        {},
                                                        {} });
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
                m_Instances.emplace(logicalHandle,
                                    Instance{ managedHandle, request.Entity, schema->Identity, std::move(migrated.State.OrphanedMembers) });
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
                return status == CW_MANAGED_STATUS_OK ? ManagedOperationResult::Success()
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
                    return { ManagedOperationResult::Failure("managed.coreclr.capture_empty", "The managed host returned empty script state.",
                                                             ManagedBackendId::CoreCLR),
                             {} };
                const ScriptTypeSchema* schema = m_Catalog.FindType(instance->second.Identity);
                if (schema == nullptr)
                    return { ManagedOperationResult::Failure("managed.coreclr.type_missing", "The live script type is no longer in the catalog.",
                                                             ManagedBackendId::CoreCLR),
                             {} };
                ScriptState state;
                ManagedOperationResult parsed = ParseManagedStateJson(StringView(reinterpret_cast<const char*>(bytes.data()), bytes.size()), state,
                                                                      ManagedBackendId::CoreCLR, schema);
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
                const ManagedProgramArtifact* runtimeConfig = FindArtifact(program, ManagedProgramArtifactKind::RuntimeConfig, "managed-host");
                const ManagedProgramArtifact* hostAssembly = FindArtifact(program, ManagedProgramArtifactKind::EngineAssembly, "managed-host");
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
                    return ManagedOperationResult::Failure("managed.coreclr.runtime_properties_mismatch",
                                                           "A process-wide CoreCLR is already running with different runtime properties.",
                                                           ManagedBackendId::CoreCLR);
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
                cw_managed_host_api hostApi{};
                hostApi.size = sizeof(hostApi);
                hostApi.abi_version = CW_MANAGED_ABI_VERSION;
                hostApi.context = this;
                hostApi.log = &Log;
                hostApi.get_entity_name = &GetEntityName;
                hostApi.set_entity_name = &SetEntityName;
                hostApi.find_entity_by_name = &FindEntityByName;
                hostApi.get_entity_parent = &GetEntityParent;
                hostApi.set_entity_parent = &SetEntityParent;
                hostApi.destroy_entity = &DestroyEntity;
                hostApi.invoke_host_binding = &InvokeHostBinding;
#define CW_ASSIGN_HOST_FUNCTION(functionName, fieldName) hostApi.fieldName = &functionName;
                CW_MANAGED_HOST_FUNCTION_LIST(CW_ASSIGN_HOST_FUNCTION)
#undef CW_ASSIGN_HOST_FUNCTION
                if (cw_managed_status status = m_Api.initialize(&hostApi); status != CW_MANAGED_STATUS_OK)
                    return StatusFailure(status, "managed.coreclr.bootstrap_initialize_failed", "The managed bootstrap failed to initialize.");
                m_RuntimeReady = true;
                return ManagedOperationResult::Success();
            }

            ManagedOperationResult LoadManagedProgram(const ManagedProgramDefinition& program, ScriptCatalog& catalog)
            {
                const ManagedProgramArtifact* gameAssembly = FindArtifact(program, ManagedProgramArtifactKind::GameAssembly, "game");
                const ManagedProgramArtifact* gameDependencies = FindArtifact(program, ManagedProgramArtifactKind::DependencyManifest, "game");
                if (gameAssembly == nullptr || !IsRegularFile(gameAssembly->Filepath) || gameDependencies == nullptr ||
                    !IsRegularFile(gameDependencies->Filepath))
                    return ManagedOperationResult::Failure("managed.coreclr.game_assembly_missing",
                                                           "The CoreCLR game assembly or dependency manifest is missing.", ManagedBackendId::CoreCLR);
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
                                                           "The CoreCLR managed host returned an empty script catalog.", ManagedBackendId::CoreCLR);
                }
                ManagedOperationResult parsed =
                  ParseManagedCatalogJson(StringView(reinterpret_cast<const char*>(bytes.data()), bytes.size()), catalog, ManagedBackendId::CoreCLR);
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
                                                               "A live script type is missing from the replacement program.",
                                                               ManagedBackendId::CoreCLR);
                    ScriptStateResult migrated = MigrateScriptState(snapshot.State, *schema, ManagedBackendId::CoreCLR);
                    if (!migrated.Result.Succeeded)
                        return migrated.Result;
                    uint64_t managedHandle = 0;
                    ManagedOperationResult result = CreateManaged(*schema, snapshot.Entity, migrated.State, managedHandle);
                    if (!result.Succeeded)
                        return result;
                    instances.emplace(snapshot.PreviousHandle,
                                      Instance{ managedHandle, snapshot.Entity, schema->Identity, std::move(migrated.State.OrphanedMembers) });
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
                const cw_managed_status status =
                  m_Api.create_script(assembly.View, typeNamespace.View, typeName.View, ToAbiUuid(entity), blob, &managedHandle);
                return status == CW_MANAGED_STATUS_OK
                         ? ManagedOperationResult::Success()
                         : StatusFailure(status, "managed.coreclr.create_failed", "The CoreCLR script instance could not be created.");
            }

            ManagedOperationResult StatusFailure(cw_managed_status status, String code, String message)
            {
                CollectManagedDiagnostics();
                ManagedOperationResult result =
                  ManagedOperationResult::Failure(std::move(code), message + " Status " + std::to_string(status) + ".", ManagedBackendId::CoreCLR);
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
                Vector<ManagedDiagnostic> diagnostics =
                  ParseManagedDiagnosticsJson(StringView(reinterpret_cast<const char*>(bytes.data()), bytes.size()), ManagedBackendId::CoreCLR);
                Lock lock(m_DiagnosticMutex);
                m_Diagnostics.insert(m_Diagnostics.end(), std::make_move_iterator(diagnostics.begin()), std::make_move_iterator(diagnostics.end()));
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

            static cw_managed_status CW_MANAGED_CALL GetEntityName(void* context, cw_managed_uuid entityId, cw_managed_string_view* name)
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

            static cw_managed_status CW_MANAGED_CALL SetEntityName(void* context, cw_managed_uuid entityId, cw_managed_string_view name)
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

            static cw_managed_status CW_MANAGED_CALL FindEntityByName(void* context, cw_managed_string_view name, cw_managed_uuid* entityId)
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

            static cw_managed_status CW_MANAGED_CALL GetEntityParent(void* context, cw_managed_uuid entityId, cw_managed_uuid* parentId)
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

            static cw_managed_status CW_MANAGED_CALL SetEntityParent(void* context, cw_managed_uuid entityId, cw_managed_uuid parentId)
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

            static cw_managed_status ForwardTypedBinding(void* context, uint32_t binding, cw_managed_uuid entityId, const void* inputData,
                                                         size_t inputSize, void* resultData, size_t resultSize)
            {
                if (resultSize != 0 && resultData == nullptr)
                    return CW_MANAGED_STATUS_INVALID_ARGUMENT;
                cw_managed_blob input{ static_cast<const uint8_t*>(inputData), inputSize };
                cw_managed_blob output{};
                const cw_managed_status status = InvokeHostBinding(context, binding, entityId, input, &output);
                if (status != CW_MANAGED_STATUS_OK)
                    return status;
                if (output.length != resultSize || (resultSize != 0 && (output.data == nullptr || resultData == nullptr)))
                    return CW_MANAGED_STATUS_BUFFER_WRITE_FAILED;
                if (resultSize != 0)
                    std::memcpy(resultData, output.data, resultSize);
                return CW_MANAGED_STATUS_OK;
            }

#define CW_TYPED_ENTITY_GET(functionName, bindingName, resultType)                                                                                   \
    static cw_managed_status CW_MANAGED_CALL functionName(void* context, cw_managed_uuid entity, resultType* result)                                 \
    {                                                                                                                                                \
        return ForwardTypedBinding(context, bindingName, entity, nullptr, 0, result, sizeof(resultType));                                            \
    }
#define CW_TYPED_ENTITY_SET_VALUE(functionName, bindingName, valueType)                                                                              \
    static cw_managed_status CW_MANAGED_CALL functionName(void* context, cw_managed_uuid entity, valueType value)                                    \
    {                                                                                                                                                \
        return ForwardTypedBinding(context, bindingName, entity, &value, sizeof(value), nullptr, 0);                                                 \
    }
#define CW_TYPED_ENTITY_SET_STRUCT(functionName, bindingName, valueType)                                                                             \
    static cw_managed_status CW_MANAGED_CALL functionName(void* context, cw_managed_uuid entity, const valueType* value)                             \
    {                                                                                                                                                \
        return ForwardTypedBinding(context, bindingName, entity, value, value != nullptr ? sizeof(valueType) : 0, nullptr, 0);                       \
    }
#define CW_TYPED_GLOBAL_GET(functionName, bindingName, resultType)                                                                                   \
    static cw_managed_status CW_MANAGED_CALL functionName(void* context, resultType* result)                                                         \
    {                                                                                                                                                \
        return ForwardTypedBinding(context, bindingName, {}, nullptr, 0, result, sizeof(resultType));                                                \
    }
#define CW_TYPED_INPUT_STRING_GET(functionName, bindingName, resultType)                                                                             \
    static cw_managed_status CW_MANAGED_CALL functionName(void* context, cw_managed_string_view name, resultType* result)                            \
    {                                                                                                                                                \
        return ForwardTypedBinding(context, bindingName, {}, name.data, name.length, result, sizeof(resultType));                                    \
    }

            static cw_managed_status CW_MANAGED_CALL EntityHasComponent(void* context, cw_managed_uuid entity, cw_managed_string_view typeName,
                                                                        uint8_t* result)
            {
                return ForwardTypedBinding(context, CW_MANAGED_BINDING_ENTITY_HAS_COMPONENT, entity, typeName.data, typeName.length, result,
                                           sizeof(*result));
            }

            static cw_managed_status CW_MANAGED_CALL EntityAddComponent(void* context, cw_managed_uuid entity, cw_managed_string_view typeName)
            {
                return ForwardTypedBinding(context, CW_MANAGED_BINDING_ENTITY_ADD_COMPONENT, entity, typeName.data, typeName.length, nullptr, 0);
            }

            static cw_managed_status CW_MANAGED_CALL EntityRemoveComponent(void* context, cw_managed_uuid entity, cw_managed_string_view typeName)
            {
                return ForwardTypedBinding(context, CW_MANAGED_BINDING_ENTITY_REMOVE_COMPONENT, entity, typeName.data, typeName.length, nullptr, 0);
            }

            CW_TYPED_ENTITY_GET(TransformGetPosition, CW_MANAGED_BINDING_TRANSFORM_GET_POSITION, cw_managed_vec3)
            CW_TYPED_ENTITY_SET_STRUCT(TransformSetPosition, CW_MANAGED_BINDING_TRANSFORM_SET_POSITION, cw_managed_vec3)
            CW_TYPED_ENTITY_GET(TransformGetLocalPosition, CW_MANAGED_BINDING_TRANSFORM_GET_LOCAL_POSITION, cw_managed_vec3)
            CW_TYPED_ENTITY_SET_STRUCT(TransformSetLocalPosition, CW_MANAGED_BINDING_TRANSFORM_SET_LOCAL_POSITION, cw_managed_vec3)
            CW_TYPED_ENTITY_GET(TransformGetScale, CW_MANAGED_BINDING_TRANSFORM_GET_SCALE, cw_managed_vec3)
            CW_TYPED_ENTITY_SET_STRUCT(TransformSetScale, CW_MANAGED_BINDING_TRANSFORM_SET_SCALE, cw_managed_vec3)
            CW_TYPED_ENTITY_GET(TransformGetLocalScale, CW_MANAGED_BINDING_TRANSFORM_GET_LOCAL_SCALE, cw_managed_vec3)
            CW_TYPED_ENTITY_SET_STRUCT(TransformSetLocalScale, CW_MANAGED_BINDING_TRANSFORM_SET_LOCAL_SCALE, cw_managed_vec3)
            CW_TYPED_ENTITY_GET(TransformGetRotation, CW_MANAGED_BINDING_TRANSFORM_GET_ROTATION, cw_managed_quat)
            CW_TYPED_ENTITY_SET_STRUCT(TransformSetRotation, CW_MANAGED_BINDING_TRANSFORM_SET_ROTATION, cw_managed_quat)
            CW_TYPED_ENTITY_GET(TransformGetLocalRotation, CW_MANAGED_BINDING_TRANSFORM_GET_LOCAL_ROTATION, cw_managed_quat)
            CW_TYPED_ENTITY_SET_STRUCT(TransformSetLocalRotation, CW_MANAGED_BINDING_TRANSFORM_SET_LOCAL_ROTATION, cw_managed_quat)
            CW_TYPED_ENTITY_GET(TransformGetLocalToWorldMatrix, CW_MANAGED_BINDING_TRANSFORM_GET_LOCAL_TO_WORLD_MATRIX, cw_managed_mat4)
            CW_TYPED_ENTITY_GET(TransformGetWorldToLocalMatrix, CW_MANAGED_BINDING_TRANSFORM_GET_WORLD_TO_LOCAL_MATRIX, cw_managed_mat4)
            CW_TYPED_ENTITY_GET(TransformGetEulerAngles, CW_MANAGED_BINDING_TRANSFORM_GET_EULER_ANGLES, cw_managed_vec3)
            CW_TYPED_ENTITY_SET_STRUCT(TransformSetEulerAngles, CW_MANAGED_BINDING_TRANSFORM_SET_EULER_ANGLES, cw_managed_vec3)
            CW_TYPED_ENTITY_GET(TransformGetLocalEulerAngles, CW_MANAGED_BINDING_TRANSFORM_GET_LOCAL_EULER_ANGLES, cw_managed_vec3)
            CW_TYPED_ENTITY_SET_STRUCT(TransformSetLocalEulerAngles, CW_MANAGED_BINDING_TRANSFORM_SET_LOCAL_EULER_ANGLES, cw_managed_vec3)

#define CW_TYPED_INPUT_BUTTON(functionName, bindingName)                                                                                             \
    static cw_managed_status CW_MANAGED_CALL functionName(void* context, uint32_t code, uint8_t* result)                                             \
    {                                                                                                                                                \
        return ForwardTypedBinding(context, bindingName, {}, &code, sizeof(code), result, sizeof(*result));                                          \
    }
#define CW_TYPED_GAMEPAD_VALUE(functionName, bindingName, resultType)                                                                                \
    static cw_managed_status CW_MANAGED_CALL functionName(void* context, uint32_t gamepad, uint32_t code, resultType* result)                        \
    {                                                                                                                                                \
        const uint32_t input[] = { gamepad, code };                                                                                                  \
        return ForwardTypedBinding(context, bindingName, {}, input, sizeof(input), result, sizeof(*result));                                         \
    }
            CW_TYPED_INPUT_BUTTON(InputGetKey, CW_MANAGED_BINDING_INPUT_GET_KEY)
            CW_TYPED_INPUT_BUTTON(InputGetKeyDown, CW_MANAGED_BINDING_INPUT_GET_KEY_DOWN)
            CW_TYPED_INPUT_BUTTON(InputGetKeyUp, CW_MANAGED_BINDING_INPUT_GET_KEY_UP)
            CW_TYPED_INPUT_BUTTON(InputGetMouseButton, CW_MANAGED_BINDING_INPUT_GET_MOUSE_BUTTON)
            CW_TYPED_INPUT_BUTTON(InputGetMouseButtonDown, CW_MANAGED_BINDING_INPUT_GET_MOUSE_BUTTON_DOWN)
            CW_TYPED_INPUT_BUTTON(InputGetMouseButtonUp, CW_MANAGED_BINDING_INPUT_GET_MOUSE_BUTTON_UP)
            CW_TYPED_GLOBAL_GET(InputGetMouseScrollX, CW_MANAGED_BINDING_INPUT_GET_MOUSE_SCROLL_X, float)
            CW_TYPED_GLOBAL_GET(InputGetMouseScrollY, CW_MANAGED_BINDING_INPUT_GET_MOUSE_SCROLL_Y, float)
            CW_TYPED_GLOBAL_GET(InputGetMousePosition, CW_MANAGED_BINDING_INPUT_GET_MOUSE_POSITION, cw_managed_vec2)
            CW_TYPED_GLOBAL_GET(InputGetMouseDelta, CW_MANAGED_BINDING_INPUT_GET_MOUSE_DELTA, cw_managed_vec2)
            CW_TYPED_INPUT_BUTTON(InputIsGamepadConnected, CW_MANAGED_BINDING_INPUT_IS_GAMEPAD_CONNECTED)
            CW_TYPED_GAMEPAD_VALUE(InputGetGamepadButton, CW_MANAGED_BINDING_INPUT_GET_GAMEPAD_BUTTON, uint8_t)
            CW_TYPED_GAMEPAD_VALUE(InputGetGamepadButtonDown, CW_MANAGED_BINDING_INPUT_GET_GAMEPAD_BUTTON_DOWN, uint8_t)
            CW_TYPED_GAMEPAD_VALUE(InputGetGamepadButtonUp, CW_MANAGED_BINDING_INPUT_GET_GAMEPAD_BUTTON_UP, uint8_t)
            CW_TYPED_GAMEPAD_VALUE(InputGetGamepadAxis, CW_MANAGED_BINDING_INPUT_GET_GAMEPAD_AXIS, float)
            CW_TYPED_INPUT_STRING_GET(InputGetAction, CW_MANAGED_BINDING_INPUT_GET_ACTION, uint8_t)
            CW_TYPED_INPUT_STRING_GET(InputGetActionDown, CW_MANAGED_BINDING_INPUT_GET_ACTION_DOWN, uint8_t)
            CW_TYPED_INPUT_STRING_GET(InputGetActionUp, CW_MANAGED_BINDING_INPUT_GET_ACTION_UP, uint8_t)
            CW_TYPED_INPUT_STRING_GET(InputGetAxis, CW_MANAGED_BINDING_INPUT_GET_AXIS, float)
            CW_TYPED_INPUT_STRING_GET(InputGetActionVector, CW_MANAGED_BINDING_INPUT_GET_ACTION_VECTOR, cw_managed_vec2)
            CW_TYPED_INPUT_STRING_GET(InputEnableActionMap, CW_MANAGED_BINDING_INPUT_ENABLE_ACTION_MAP, uint8_t)
            CW_TYPED_INPUT_STRING_GET(InputDisableActionMap, CW_MANAGED_BINDING_INPUT_DISABLE_ACTION_MAP, uint8_t)

            static cw_managed_status CW_MANAGED_CALL InputClearActionRebinds(void* context)
            {
                return ForwardTypedBinding(context, CW_MANAGED_BINDING_INPUT_CLEAR_ACTION_REBINDS, {}, nullptr, 0, nullptr, 0);
            }

            CW_TYPED_GLOBAL_GET(TimeGetTime, CW_MANAGED_BINDING_TIME_GET_TIME, float)
            CW_TYPED_GLOBAL_GET(TimeGetFixedDeltaTime, CW_MANAGED_BINDING_TIME_GET_FIXED_DELTA_TIME, float)
            CW_TYPED_GLOBAL_GET(TimeGetSmoothDeltaTime, CW_MANAGED_BINDING_TIME_GET_SMOOTH_DELTA_TIME, float)
            CW_TYPED_GLOBAL_GET(TimeGetRealtimeSinceStartup, CW_MANAGED_BINDING_TIME_GET_REALTIME_SINCE_STARTUP, float)
            CW_TYPED_GLOBAL_GET(TimeGetFrameCount, CW_MANAGED_BINDING_TIME_GET_FRAME_COUNT, uint32_t)

            CW_TYPED_ENTITY_GET(Rigidbody2DGetMass, CW_MANAGED_BINDING_RIGIDBODY_2_DGET_MASS, float)
            CW_TYPED_ENTITY_SET_VALUE(Rigidbody2DSetMass, CW_MANAGED_BINDING_RIGIDBODY_2_DSET_MASS, float)
            CW_TYPED_ENTITY_GET(Rigidbody2DGetBodyType, CW_MANAGED_BINDING_RIGIDBODY_2_DGET_BODY_TYPE, int32_t)
            CW_TYPED_ENTITY_SET_VALUE(Rigidbody2DSetBodyType, CW_MANAGED_BINDING_RIGIDBODY_2_DSET_BODY_TYPE, int32_t)
            CW_TYPED_ENTITY_GET(Rigidbody2DGetSleepMode, CW_MANAGED_BINDING_RIGIDBODY_2_DGET_SLEEP_MODE, int32_t)
            CW_TYPED_ENTITY_SET_VALUE(Rigidbody2DSetSleepMode, CW_MANAGED_BINDING_RIGIDBODY_2_DSET_SLEEP_MODE, int32_t)
            CW_TYPED_ENTITY_GET(Rigidbody2DGetCollisionDetectionMode, CW_MANAGED_BINDING_RIGIDBODY_2_DGET_COLLISION_DETECTION_MODE, int32_t)
            CW_TYPED_ENTITY_SET_VALUE(Rigidbody2DSetCollisionDetectionMode, CW_MANAGED_BINDING_RIGIDBODY_2_DSET_COLLISION_DETECTION_MODE, int32_t)
            CW_TYPED_ENTITY_GET(Rigidbody2DGetInterpolation, CW_MANAGED_BINDING_RIGIDBODY_2_DGET_INTERPOLATION, int32_t)
            CW_TYPED_ENTITY_SET_VALUE(Rigidbody2DSetInterpolation, CW_MANAGED_BINDING_RIGIDBODY_2_DSET_INTERPOLATION, int32_t)
            CW_TYPED_ENTITY_GET(Rigidbody2DGetAutoMass, CW_MANAGED_BINDING_RIGIDBODY_2_DGET_AUTO_MASS, uint8_t)
            CW_TYPED_ENTITY_SET_VALUE(Rigidbody2DSetAutoMass, CW_MANAGED_BINDING_RIGIDBODY_2_DSET_AUTO_MASS, uint8_t)
            CW_TYPED_ENTITY_GET(Rigidbody2DGetLayer, CW_MANAGED_BINDING_RIGIDBODY_2_DGET_LAYER, int32_t)
            CW_TYPED_ENTITY_SET_VALUE(Rigidbody2DSetLayer, CW_MANAGED_BINDING_RIGIDBODY_2_DSET_LAYER, int32_t)
            CW_TYPED_ENTITY_GET(Rigidbody2DGetLinearDrag, CW_MANAGED_BINDING_RIGIDBODY_2_DGET_LINEAR_DRAG, float)
            CW_TYPED_ENTITY_SET_VALUE(Rigidbody2DSetLinearDrag, CW_MANAGED_BINDING_RIGIDBODY_2_DSET_LINEAR_DRAG, float)
            CW_TYPED_ENTITY_GET(Rigidbody2DGetAngularDrag, CW_MANAGED_BINDING_RIGIDBODY_2_DGET_ANGULAR_DRAG, float)
            CW_TYPED_ENTITY_SET_VALUE(Rigidbody2DSetAngularDrag, CW_MANAGED_BINDING_RIGIDBODY_2_DSET_ANGULAR_DRAG, float)
            CW_TYPED_ENTITY_GET(Rigidbody2DGetGravityScale, CW_MANAGED_BINDING_RIGIDBODY_2_DGET_GRAVITY_SCALE, float)
            CW_TYPED_ENTITY_SET_VALUE(Rigidbody2DSetGravityScale, CW_MANAGED_BINDING_RIGIDBODY_2_DSET_GRAVITY_SCALE, float)
            CW_TYPED_ENTITY_GET(Rigidbody2DGetCenterOfMass, CW_MANAGED_BINDING_RIGIDBODY_2_DGET_CENTER_OF_MASS, cw_managed_vec2)
            CW_TYPED_ENTITY_SET_STRUCT(Rigidbody2DSetCenterOfMass, CW_MANAGED_BINDING_RIGIDBODY_2_DSET_CENTER_OF_MASS, cw_managed_vec2)
            CW_TYPED_ENTITY_GET(Rigidbody2DGetInertia, CW_MANAGED_BINDING_RIGIDBODY_2_DGET_INERTIA, float)
            CW_TYPED_ENTITY_SET_VALUE(Rigidbody2DSetInertia, CW_MANAGED_BINDING_RIGIDBODY_2_DSET_INERTIA, float)
            CW_TYPED_ENTITY_GET(Rigidbody2DGetConstraints, CW_MANAGED_BINDING_RIGIDBODY_2_DGET_CONSTRAINTS, uint32_t)
            CW_TYPED_ENTITY_SET_VALUE(Rigidbody2DSetConstraints, CW_MANAGED_BINDING_RIGIDBODY_2_DSET_CONSTRAINTS, uint32_t)
            CW_TYPED_ENTITY_GET(Rigidbody2DGetRotation, CW_MANAGED_BINDING_RIGIDBODY_2_DGET_ROTATION, float)
            CW_TYPED_ENTITY_GET(Rigidbody2DGetPosition, CW_MANAGED_BINDING_RIGIDBODY_2_DGET_POSITION, cw_managed_vec2)
            CW_TYPED_ENTITY_GET(Rigidbody2DGetLinearVelocity, CW_MANAGED_BINDING_RIGIDBODY_2_DGET_LINEAR_VELOCITY, cw_managed_vec2)
            CW_TYPED_ENTITY_SET_STRUCT(Rigidbody2DSetLinearVelocity, CW_MANAGED_BINDING_RIGIDBODY_2_DSET_LINEAR_VELOCITY, cw_managed_vec2)
            CW_TYPED_ENTITY_GET(Rigidbody2DGetAngularVelocity, CW_MANAGED_BINDING_RIGIDBODY_2_DGET_ANGULAR_VELOCITY, float)
            CW_TYPED_ENTITY_SET_VALUE(Rigidbody2DSetAngularVelocity, CW_MANAGED_BINDING_RIGIDBODY_2_DSET_ANGULAR_VELOCITY, float)
            CW_TYPED_ENTITY_GET(Rigidbody2DGetAwake, CW_MANAGED_BINDING_RIGIDBODY_2_DGET_AWAKE, uint8_t)
            CW_TYPED_ENTITY_SET_VALUE(Rigidbody2DSetAwake, CW_MANAGED_BINDING_RIGIDBODY_2_DSET_AWAKE, uint8_t)

            static cw_managed_status CW_MANAGED_CALL Rigidbody2DAddForce(void* context, cw_managed_uuid entity, const cw_managed_vec2* force,
                                                                         int32_t mode)
            {
                struct Payload
                {
                    cw_managed_vec2 Force;
                    int32_t Mode;
                } payload{ force != nullptr ? *force : cw_managed_vec2{}, mode };
                return force != nullptr
                         ? ForwardTypedBinding(context, CW_MANAGED_BINDING_RIGIDBODY_2_DADD_FORCE, entity, &payload, sizeof(payload), nullptr, 0)
                         : CW_MANAGED_STATUS_INVALID_ARGUMENT;
            }

            static cw_managed_status CW_MANAGED_CALL Rigidbody2DAddForceAtPosition(void* context, cw_managed_uuid entity,
                                                                                   const cw_managed_vec2* force, const cw_managed_vec2* position,
                                                                                   int32_t mode)
            {
                struct Payload
                {
                    cw_managed_vec2 Force;
                    cw_managed_vec2 Position;
                    int32_t Mode;
                } payload{ force != nullptr ? *force : cw_managed_vec2{}, position != nullptr ? *position : cw_managed_vec2{}, mode };
                return force != nullptr && position != nullptr ? ForwardTypedBinding(context, CW_MANAGED_BINDING_RIGIDBODY_2_DADD_FORCE_AT_POSITION,
                                                                                     entity, &payload, sizeof(payload), nullptr, 0)
                                                               : CW_MANAGED_STATUS_INVALID_ARGUMENT;
            }

            static cw_managed_status CW_MANAGED_CALL Rigidbody2DAddTorque(void* context, cw_managed_uuid entity, float torque, int32_t mode)
            {
                struct Payload
                {
                    float Torque;
                    int32_t Mode;
                } payload{ torque, mode };
                return ForwardTypedBinding(context, CW_MANAGED_BINDING_RIGIDBODY_2_DADD_TORQUE, entity, &payload, sizeof(payload), nullptr, 0);
            }

            CW_TYPED_ENTITY_GET(AudioSourceGetVolume, CW_MANAGED_BINDING_AUDIO_SOURCE_GET_VOLUME, float)
            CW_TYPED_ENTITY_SET_VALUE(AudioSourceSetVolume, CW_MANAGED_BINDING_AUDIO_SOURCE_SET_VOLUME, float)
            CW_TYPED_ENTITY_GET(AudioSourceGetPitch, CW_MANAGED_BINDING_AUDIO_SOURCE_GET_PITCH, float)
            CW_TYPED_ENTITY_SET_VALUE(AudioSourceSetPitch, CW_MANAGED_BINDING_AUDIO_SOURCE_SET_PITCH, float)
            CW_TYPED_ENTITY_GET(AudioSourceGetMinDistance, CW_MANAGED_BINDING_AUDIO_SOURCE_GET_MIN_DISTANCE, float)
            CW_TYPED_ENTITY_SET_VALUE(AudioSourceSetMinDistance, CW_MANAGED_BINDING_AUDIO_SOURCE_SET_MIN_DISTANCE, float)
            CW_TYPED_ENTITY_GET(AudioSourceGetMaxDistance, CW_MANAGED_BINDING_AUDIO_SOURCE_GET_MAX_DISTANCE, float)
            CW_TYPED_ENTITY_SET_VALUE(AudioSourceSetMaxDistance, CW_MANAGED_BINDING_AUDIO_SOURCE_SET_MAX_DISTANCE, float)
            CW_TYPED_ENTITY_GET(AudioSourceGetLoop, CW_MANAGED_BINDING_AUDIO_SOURCE_GET_LOOP, uint8_t)
            CW_TYPED_ENTITY_SET_VALUE(AudioSourceSetLoop, CW_MANAGED_BINDING_AUDIO_SOURCE_SET_LOOP, uint8_t)
            CW_TYPED_ENTITY_GET(AudioSourceGetMuted, CW_MANAGED_BINDING_AUDIO_SOURCE_GET_MUTED, uint8_t)
            CW_TYPED_ENTITY_SET_VALUE(AudioSourceSetMuted, CW_MANAGED_BINDING_AUDIO_SOURCE_SET_MUTED, uint8_t)
            CW_TYPED_ENTITY_GET(AudioSourceGetPlayOnAwake, CW_MANAGED_BINDING_AUDIO_SOURCE_GET_PLAY_ON_AWAKE, uint8_t)
            CW_TYPED_ENTITY_SET_VALUE(AudioSourceSetPlayOnAwake, CW_MANAGED_BINDING_AUDIO_SOURCE_SET_PLAY_ON_AWAKE, uint8_t)
            CW_TYPED_ENTITY_GET(AudioSourceGetTime, CW_MANAGED_BINDING_AUDIO_SOURCE_GET_TIME, float)
            CW_TYPED_ENTITY_SET_VALUE(AudioSourceSetTime, CW_MANAGED_BINDING_AUDIO_SOURCE_SET_TIME, float)
            CW_TYPED_ENTITY_GET(AudioSourceGetClip, CW_MANAGED_BINDING_AUDIO_SOURCE_GET_CLIP, cw_managed_uuid)
            CW_TYPED_ENTITY_SET_VALUE(AudioSourceSetClip, CW_MANAGED_BINDING_AUDIO_SOURCE_SET_CLIP, cw_managed_uuid)
            CW_TYPED_ENTITY_GET(AudioSourceGetState, CW_MANAGED_BINDING_AUDIO_SOURCE_GET_STATE, int32_t)

#define CW_TYPED_ENTITY_ACTION(functionName, bindingName)                                                                                            \
    static cw_managed_status CW_MANAGED_CALL functionName(void* context, cw_managed_uuid entity)                                                     \
    {                                                                                                                                                \
        return ForwardTypedBinding(context, bindingName, entity, nullptr, 0, nullptr, 0);                                                            \
    }
            CW_TYPED_ENTITY_ACTION(AudioSourcePlay, CW_MANAGED_BINDING_AUDIO_SOURCE_PLAY)
            CW_TYPED_ENTITY_ACTION(AudioSourcePause, CW_MANAGED_BINDING_AUDIO_SOURCE_PAUSE)
            CW_TYPED_ENTITY_ACTION(AudioSourceStop, CW_MANAGED_BINDING_AUDIO_SOURCE_STOP)

            static cw_managed_status CW_MANAGED_CALL TextGetText(void* context, cw_managed_uuid entity, cw_managed_string_view* result)
            {
                if (result == nullptr)
                    return CW_MANAGED_STATUS_INVALID_ARGUMENT;
                cw_managed_blob output{};
                const cw_managed_status status = InvokeHostBinding(context, CW_MANAGED_BINDING_TEXT_GET_TEXT, entity, {}, &output);
                if (status != CW_MANAGED_STATUS_OK || output.length > std::numeric_limits<uint32_t>::max())
                    return status != CW_MANAGED_STATUS_OK ? status : CW_MANAGED_STATUS_BUFFER_WRITE_FAILED;
                result->data = output.data;
                result->length = static_cast<uint32_t>(output.length);
                return CW_MANAGED_STATUS_OK;
            }

            static cw_managed_status CW_MANAGED_CALL TextSetText(void* context, cw_managed_uuid entity, cw_managed_string_view value)
            {
                return value.data != nullptr || value.length == 0
                         ? ForwardTypedBinding(context, CW_MANAGED_BINDING_TEXT_SET_TEXT, entity, value.data, value.length, nullptr, 0)
                         : CW_MANAGED_STATUS_INVALID_ARGUMENT;
            }

            CW_TYPED_ENTITY_GET(TextGetFont, CW_MANAGED_BINDING_TEXT_GET_FONT, cw_managed_uuid)
            CW_TYPED_ENTITY_SET_VALUE(TextSetFont, CW_MANAGED_BINDING_TEXT_SET_FONT, cw_managed_uuid)
            CW_TYPED_ENTITY_GET(TextGetColor, CW_MANAGED_BINDING_TEXT_GET_COLOR, cw_managed_vec4)
            CW_TYPED_ENTITY_SET_STRUCT(TextSetColor, CW_MANAGED_BINDING_TEXT_SET_COLOR, cw_managed_vec4)
            CW_TYPED_ENTITY_GET(TextGetSize, CW_MANAGED_BINDING_TEXT_GET_SIZE, float)
            CW_TYPED_ENTITY_SET_VALUE(TextSetSize, CW_MANAGED_BINDING_TEXT_SET_SIZE, float)
            CW_TYPED_ENTITY_GET(TextGetAutoSize, CW_MANAGED_BINDING_TEXT_GET_AUTO_SIZE, uint8_t)
            CW_TYPED_ENTITY_SET_VALUE(TextSetAutoSize, CW_MANAGED_BINDING_TEXT_SET_AUTO_SIZE, uint8_t)
            CW_TYPED_ENTITY_GET(TextGetAutoSizeMin, CW_MANAGED_BINDING_TEXT_GET_AUTO_SIZE_MIN, float)
            CW_TYPED_ENTITY_SET_VALUE(TextSetAutoSizeMin, CW_MANAGED_BINDING_TEXT_SET_AUTO_SIZE_MIN, float)
            CW_TYPED_ENTITY_GET(TextGetAutoSizeMax, CW_MANAGED_BINDING_TEXT_GET_AUTO_SIZE_MAX, float)
            CW_TYPED_ENTITY_SET_VALUE(TextSetAutoSizeMax, CW_MANAGED_BINDING_TEXT_SET_AUTO_SIZE_MAX, float)
            CW_TYPED_ENTITY_GET(TextGetLayoutSize, CW_MANAGED_BINDING_TEXT_GET_LAYOUT_SIZE, cw_managed_vec2)
            CW_TYPED_ENTITY_SET_STRUCT(TextSetLayoutSize, CW_MANAGED_BINDING_TEXT_SET_LAYOUT_SIZE, cw_managed_vec2)
            CW_TYPED_ENTITY_GET(TextGetWrapping, CW_MANAGED_BINDING_TEXT_GET_WRAPPING, uint8_t)
            CW_TYPED_ENTITY_SET_VALUE(TextSetWrapping, CW_MANAGED_BINDING_TEXT_SET_WRAPPING, uint8_t)
            CW_TYPED_ENTITY_GET(TextGetWrapMode, CW_MANAGED_BINDING_TEXT_GET_WRAP_MODE, int32_t)
            CW_TYPED_ENTITY_SET_VALUE(TextSetWrapMode, CW_MANAGED_BINDING_TEXT_SET_WRAP_MODE, int32_t)
            CW_TYPED_ENTITY_GET(TextGetOverflow, CW_MANAGED_BINDING_TEXT_GET_OVERFLOW, int32_t)
            CW_TYPED_ENTITY_SET_VALUE(TextSetOverflow, CW_MANAGED_BINDING_TEXT_SET_OVERFLOW, int32_t)
            CW_TYPED_ENTITY_GET(TextGetClipToBounds, CW_MANAGED_BINDING_TEXT_GET_CLIP_TO_BOUNDS, uint8_t)
            CW_TYPED_ENTITY_SET_VALUE(TextSetClipToBounds, CW_MANAGED_BINDING_TEXT_SET_CLIP_TO_BOUNDS, uint8_t)
            CW_TYPED_ENTITY_GET(TextGetMaxLines, CW_MANAGED_BINDING_TEXT_GET_MAX_LINES, uint32_t)
            CW_TYPED_ENTITY_SET_VALUE(TextSetMaxLines, CW_MANAGED_BINDING_TEXT_SET_MAX_LINES, uint32_t)
            CW_TYPED_ENTITY_GET(TextGetHorizontalAlignment, CW_MANAGED_BINDING_TEXT_GET_HORIZONTAL_ALIGNMENT, int32_t)
            CW_TYPED_ENTITY_SET_VALUE(TextSetHorizontalAlignment, CW_MANAGED_BINDING_TEXT_SET_HORIZONTAL_ALIGNMENT, int32_t)
            CW_TYPED_ENTITY_GET(TextGetVerticalAlignment, CW_MANAGED_BINDING_TEXT_GET_VERTICAL_ALIGNMENT, int32_t)
            CW_TYPED_ENTITY_SET_VALUE(TextSetVerticalAlignment, CW_MANAGED_BINDING_TEXT_SET_VERTICAL_ALIGNMENT, int32_t)
            CW_TYPED_ENTITY_GET(TextGetFontStyle, CW_MANAGED_BINDING_TEXT_GET_FONT_STYLE, uint32_t)
            CW_TYPED_ENTITY_SET_VALUE(TextSetFontStyle, CW_MANAGED_BINDING_TEXT_SET_FONT_STYLE, uint32_t)
            CW_TYPED_ENTITY_GET(TextGetOutlineColor, CW_MANAGED_BINDING_TEXT_GET_OUTLINE_COLOR, cw_managed_vec4)
            CW_TYPED_ENTITY_SET_STRUCT(TextSetOutlineColor, CW_MANAGED_BINDING_TEXT_SET_OUTLINE_COLOR, cw_managed_vec4)
            CW_TYPED_ENTITY_GET(TextGetOutlineWidth, CW_MANAGED_BINDING_TEXT_GET_OUTLINE_WIDTH, float)
            CW_TYPED_ENTITY_SET_VALUE(TextSetOutlineWidth, CW_MANAGED_BINDING_TEXT_SET_OUTLINE_WIDTH, float)
            CW_TYPED_ENTITY_GET(TextGetShadowColor, CW_MANAGED_BINDING_TEXT_GET_SHADOW_COLOR, cw_managed_vec4)
            CW_TYPED_ENTITY_SET_STRUCT(TextSetShadowColor, CW_MANAGED_BINDING_TEXT_SET_SHADOW_COLOR, cw_managed_vec4)
            CW_TYPED_ENTITY_GET(TextGetShadowOffset, CW_MANAGED_BINDING_TEXT_GET_SHADOW_OFFSET, cw_managed_vec2)
            CW_TYPED_ENTITY_SET_STRUCT(TextSetShadowOffset, CW_MANAGED_BINDING_TEXT_SET_SHADOW_OFFSET, cw_managed_vec2)
            CW_TYPED_ENTITY_GET(TextGetShadowSoftness, CW_MANAGED_BINDING_TEXT_GET_SHADOW_SOFTNESS, float)
            CW_TYPED_ENTITY_SET_VALUE(TextSetShadowSoftness, CW_MANAGED_BINDING_TEXT_SET_SHADOW_SOFTNESS, float)
            CW_TYPED_ENTITY_GET(TextGetCharacterSpacing, CW_MANAGED_BINDING_TEXT_GET_CHARACTER_SPACING, float)
            CW_TYPED_ENTITY_SET_VALUE(TextSetCharacterSpacing, CW_MANAGED_BINDING_TEXT_SET_CHARACTER_SPACING, float)
            CW_TYPED_ENTITY_GET(TextGetWordSpacing, CW_MANAGED_BINDING_TEXT_GET_WORD_SPACING, float)
            CW_TYPED_ENTITY_SET_VALUE(TextSetWordSpacing, CW_MANAGED_BINDING_TEXT_SET_WORD_SPACING, float)
            CW_TYPED_ENTITY_GET(TextGetLineSpacing, CW_MANAGED_BINDING_TEXT_GET_LINE_SPACING, float)
            CW_TYPED_ENTITY_SET_VALUE(TextSetLineSpacing, CW_MANAGED_BINDING_TEXT_SET_LINE_SPACING, float)
            CW_TYPED_ENTITY_GET(TextGetParagraphSpacing, CW_MANAGED_BINDING_TEXT_GET_PARAGRAPH_SPACING, float)
            CW_TYPED_ENTITY_SET_VALUE(TextSetParagraphSpacing, CW_MANAGED_BINDING_TEXT_SET_PARAGRAPH_SPACING, float)
            CW_TYPED_ENTITY_GET(TextGetTabWidth, CW_MANAGED_BINDING_TEXT_GET_TAB_WIDTH, uint32_t)
            CW_TYPED_ENTITY_SET_VALUE(TextSetTabWidth, CW_MANAGED_BINDING_TEXT_SET_TAB_WIDTH, uint32_t)
            CW_TYPED_ENTITY_GET(TextGetUseCustomDecorationColor, CW_MANAGED_BINDING_TEXT_GET_USE_CUSTOM_DECORATION_COLOR, uint8_t)
            CW_TYPED_ENTITY_SET_VALUE(TextSetUseCustomDecorationColor, CW_MANAGED_BINDING_TEXT_SET_USE_CUSTOM_DECORATION_COLOR, uint8_t)
            CW_TYPED_ENTITY_GET(TextGetDecorationColor, CW_MANAGED_BINDING_TEXT_GET_DECORATION_COLOR, cw_managed_vec4)
            CW_TYPED_ENTITY_SET_STRUCT(TextSetDecorationColor, CW_MANAGED_BINDING_TEXT_SET_DECORATION_COLOR, cw_managed_vec4)
            CW_TYPED_ENTITY_GET(TextGetDecorationThickness, CW_MANAGED_BINDING_TEXT_GET_DECORATION_THICKNESS, float)
            CW_TYPED_ENTITY_SET_VALUE(TextSetDecorationThickness, CW_MANAGED_BINDING_TEXT_SET_DECORATION_THICKNESS, float)
            CW_TYPED_ENTITY_GET(TextGetUnderlineOffset, CW_MANAGED_BINDING_TEXT_GET_UNDERLINE_OFFSET, float)
            CW_TYPED_ENTITY_SET_VALUE(TextSetUnderlineOffset, CW_MANAGED_BINDING_TEXT_SET_UNDERLINE_OFFSET, float)
            CW_TYPED_ENTITY_GET(TextGetStrikethroughOffset, CW_MANAGED_BINDING_TEXT_GET_STRIKETHROUGH_OFFSET, float)
            CW_TYPED_ENTITY_SET_VALUE(TextSetStrikethroughOffset, CW_MANAGED_BINDING_TEXT_SET_STRIKETHROUGH_OFFSET, float)
            CW_TYPED_ENTITY_GET(TextGetUseKerning, CW_MANAGED_BINDING_TEXT_GET_USE_KERNING, uint8_t)
            CW_TYPED_ENTITY_SET_VALUE(TextSetUseKerning, CW_MANAGED_BINDING_TEXT_SET_USE_KERNING, uint8_t)
            CW_TYPED_ENTITY_GET(TextGetSortingLayer, CW_MANAGED_BINDING_TEXT_GET_SORTING_LAYER, int32_t)
            CW_TYPED_ENTITY_SET_VALUE(TextSetSortingLayer, CW_MANAGED_BINDING_TEXT_SET_SORTING_LAYER, int32_t)
            CW_TYPED_ENTITY_GET(TextGetOrderInLayer, CW_MANAGED_BINDING_TEXT_GET_ORDER_IN_LAYER, int32_t)
            CW_TYPED_ENTITY_SET_VALUE(TextSetOrderInLayer, CW_MANAGED_BINDING_TEXT_SET_ORDER_IN_LAYER, int32_t)

            CW_TYPED_ENTITY_GET(FontGetIsValid, CW_MANAGED_BINDING_FONT_GET_IS_VALID, uint8_t)
            CW_TYPED_ENTITY_GET(FontGetGlyphCount, CW_MANAGED_BINDING_FONT_GET_GLYPH_COUNT, uint32_t)
            CW_TYPED_ENTITY_GET(FontGetTabWidth, CW_MANAGED_BINDING_FONT_GET_TAB_WIDTH, uint32_t)
            CW_TYPED_ENTITY_GET(FontGetAtlasWidth, CW_MANAGED_BINDING_FONT_GET_ATLAS_WIDTH, uint32_t)
            CW_TYPED_ENTITY_GET(FontGetAtlasHeight, CW_MANAGED_BINDING_FONT_GET_ATLAS_HEIGHT, uint32_t)
            CW_TYPED_ENTITY_GET(FontGetAtlasPixelRange, CW_MANAGED_BINDING_FONT_GET_ATLAS_PIXEL_RANGE, float)
            CW_TYPED_ENTITY_GET(FontGetFallbackCount, CW_MANAGED_BINDING_FONT_GET_FALLBACK_COUNT, uint32_t)

            static cw_managed_status CW_MANAGED_CALL FontHasGlyph(void* context, cw_managed_uuid font, uint32_t codePoint, uint8_t* result)
            {
                return ForwardTypedBinding(context, CW_MANAGED_BINDING_FONT_HAS_GLYPH, font, &codePoint, sizeof(codePoint), result, sizeof(*result));
            }

            static cw_managed_status CW_MANAGED_CALL FontGetCharacterInfo(void* context, cw_managed_uuid font, uint32_t codePoint,
                                                                          uint8_t useFallbacks,
                                                                          cw_managed_font_character_info* result)
            {
                Array<uint8_t, sizeof(codePoint) + sizeof(useFallbacks)> payload{};
                std::memcpy(payload.data(), &codePoint, sizeof(codePoint));
                payload[sizeof(codePoint)] = useFallbacks;
                return ForwardTypedBinding(context, CW_MANAGED_BINDING_FONT_GET_CHARACTER_INFO, font, payload.data(), payload.size(), result,
                                           sizeof(*result));
            }

            static cw_managed_status CW_MANAGED_CALL FontGetFallback(void* context, cw_managed_uuid font, uint32_t index,
                                                                      cw_managed_uuid* result)
            {
                return ForwardTypedBinding(context, CW_MANAGED_BINDING_FONT_GET_FALLBACK, font, &index, sizeof(index), result, sizeof(*result));
            }

            static cw_managed_status CW_MANAGED_CALL FontAddFallback(void* context, cw_managed_uuid font, cw_managed_uuid value,
                                                                      uint8_t* result)
            {
                return ForwardTypedBinding(context, CW_MANAGED_BINDING_FONT_ADD_FALLBACK, font, &value, sizeof(value), result, sizeof(*result));
            }

            CW_TYPED_ENTITY_ACTION(FontClearFallbacks, CW_MANAGED_BINDING_FONT_CLEAR_FALLBACKS)

            static cw_managed_status CW_MANAGED_CALL MathMatrixDeterminant(void* context, const cw_managed_mat4* matrix, float* result)
            {
                return ForwardTypedBinding(context, CW_MANAGED_BINDING_MATH_MATRIX_DETERMINANT, {}, matrix, matrix != nullptr ? sizeof(*matrix) : 0,
                                           result, sizeof(*result));
            }

#define CW_TYPED_MATRIX_OPERATION(functionName, bindingName)                                                                                         \
    static cw_managed_status CW_MANAGED_CALL functionName(void* context, const cw_managed_mat4* matrix, cw_managed_mat4* result)                     \
    {                                                                                                                                                \
        return ForwardTypedBinding(context, bindingName, {}, matrix, matrix != nullptr ? sizeof(*matrix) : 0, result, sizeof(*result));              \
    }
            CW_TYPED_MATRIX_OPERATION(MathMatrixInverse, CW_MANAGED_BINDING_MATH_MATRIX_INVERSE)
            CW_TYPED_MATRIX_OPERATION(MathMatrixAffineInverse, CW_MANAGED_BINDING_MATH_MATRIX_AFFINE_INVERSE)

            static cw_managed_status CW_MANAGED_CALL MathLookAt(void* context, const cw_managed_vec3* from, const cw_managed_vec3* to,
                                                                const cw_managed_vec3* up, cw_managed_mat4* result)
            {
                struct Payload
                {
                    cw_managed_vec3 From;
                    cw_managed_vec3 To;
                    cw_managed_vec3 Up;
                } payload{ from != nullptr ? *from : cw_managed_vec3{}, to != nullptr ? *to : cw_managed_vec3{},
                           up != nullptr ? *up : cw_managed_vec3{} };
                return from != nullptr && to != nullptr && up != nullptr
                         ? ForwardTypedBinding(context, CW_MANAGED_BINDING_MATH_LOOK_AT, {}, &payload, sizeof(payload), result, sizeof(*result))
                         : CW_MANAGED_STATUS_INVALID_ARGUMENT;
            }

#undef CW_TYPED_MATRIX_OPERATION
#undef CW_TYPED_ENTITY_ACTION
#undef CW_TYPED_GAMEPAD_VALUE
#undef CW_TYPED_INPUT_BUTTON
#undef CW_TYPED_INPUT_STRING_GET
#undef CW_TYPED_GLOBAL_GET
#undef CW_TYPED_ENTITY_SET_STRUCT
#undef CW_TYPED_ENTITY_SET_VALUE
#undef CW_TYPED_ENTITY_GET

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
                    const auto writeVector4 = [&](const glm::vec4& value) {
                        const float fields[] = { value.x, value.y, value.z, value.w };
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
                        const bool succeeded =
                          binding == CW_MANAGED_BINDING_ENTITY_ADD_COMPONENT ? AddComponent(entity, name) : RemoveComponent(entity, name);
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
                        case CW_MANAGED_BINDING_TRANSFORM_GET_POSITION:
                            return writeVector3(entity.GetWorldPosition());
                        case CW_MANAGED_BINDING_TRANSFORM_GET_LOCAL_POSITION:
                            return writeVector3(entity.GetLocalPosition());
                        case CW_MANAGED_BINDING_TRANSFORM_GET_SCALE:
                            return writeVector3(entity.GetWorldScale());
                        case CW_MANAGED_BINDING_TRANSFORM_GET_LOCAL_SCALE:
                            return writeVector3(entity.GetLocalScale());
                        case CW_MANAGED_BINDING_TRANSFORM_GET_ROTATION:
                            return writeQuaternion(entity.GetWorldRotation());
                        case CW_MANAGED_BINDING_TRANSFORM_GET_LOCAL_ROTATION:
                            return writeQuaternion(entity.GetLocalRotation());
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
                        default:
                            return CW_MANAGED_STATUS_INVALID_ARGUMENT;
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
                    case CW_MANAGED_BINDING_INPUT_GET_MOUSE_SCROLL_X:
                        return WriteBindingResult(output, Input::GetMouseScrollX());
                    case CW_MANAGED_BINDING_INPUT_GET_MOUSE_SCROLL_Y:
                        return WriteBindingResult(output, Input::GetMouseScrollY());
                    case CW_MANAGED_BINDING_INPUT_GET_MOUSE_POSITION:
                        return writeVector2(Input::GetMousePosition());
                    case CW_MANAGED_BINDING_INPUT_GET_MOUSE_DELTA:
                        return writeVector2(Input::GetMouseDelta());
                    case CW_MANAGED_BINDING_INPUT_IS_GAMEPAD_CONNECTED: {
                        if (input.length != sizeof(uint32_t))
                            return CW_MANAGED_STATUS_INVALID_ARGUMENT;
                        const uint8_t result = Input::IsGamepadConnected(readCode()) ? 1 : 0;
                        return WriteBindingResult(output, result);
                    }
                    case CW_MANAGED_BINDING_INPUT_GET_GAMEPAD_BUTTON:
                    case CW_MANAGED_BINDING_INPUT_GET_GAMEPAD_BUTTON_DOWN:
                    case CW_MANAGED_BINDING_INPUT_GET_GAMEPAD_BUTTON_UP:
                    case CW_MANAGED_BINDING_INPUT_GET_GAMEPAD_AXIS: {
                        if (input.data == nullptr || input.length != 2 * sizeof(uint32_t))
                            return CW_MANAGED_STATUS_INVALID_ARGUMENT;
                        uint32_t values[2]{};
                        std::memcpy(values, input.data, sizeof(values));
                        if (binding == CW_MANAGED_BINDING_INPUT_GET_GAMEPAD_AXIS)
                            return WriteBindingResult(output, Input::GetGamepadAxis(values[0], static_cast<GamepadAxisCode>(values[1])));
                        const GamepadButtonCode code = static_cast<GamepadButtonCode>(values[1]);
                        const bool pressed = binding == CW_MANAGED_BINDING_INPUT_GET_GAMEPAD_BUTTON ? Input::IsGamepadButtonPressed(values[0], code)
                                             : binding == CW_MANAGED_BINDING_INPUT_GET_GAMEPAD_BUTTON_DOWN
                                               ? Input::IsGamepadButtonDown(values[0], code)
                                               : Input::IsGamepadButtonUp(values[0], code);
                        const uint8_t result = pressed ? 1 : 0;
                        return WriteBindingResult(output, result);
                    }
                    case CW_MANAGED_BINDING_INPUT_GET_ACTION:
                    case CW_MANAGED_BINDING_INPUT_GET_ACTION_DOWN:
                    case CW_MANAGED_BINDING_INPUT_GET_ACTION_UP:
                    case CW_MANAGED_BINDING_INPUT_GET_AXIS:
                    case CW_MANAGED_BINDING_INPUT_GET_ACTION_VECTOR:
                    case CW_MANAGED_BINDING_INPUT_ENABLE_ACTION_MAP:
                    case CW_MANAGED_BINDING_INPUT_DISABLE_ACTION_MAP: {
                        const StringView name(input.data != nullptr ? reinterpret_cast<const char*>(input.data) : "", input.length);
                        if (binding == CW_MANAGED_BINDING_INPUT_GET_AXIS)
                            return WriteBindingResult(output, Input::GetAxis(name));
                        if (binding == CW_MANAGED_BINDING_INPUT_GET_ACTION_VECTOR)
                            return writeVector2(Input::GetActionVector(name));
                        if (binding == CW_MANAGED_BINDING_INPUT_ENABLE_ACTION_MAP || binding == CW_MANAGED_BINDING_INPUT_DISABLE_ACTION_MAP)
                        {
                            const bool enabled = binding == CW_MANAGED_BINDING_INPUT_ENABLE_ACTION_MAP;
                            const uint8_t result = Input::SetActionMapEnabled(name, enabled) ? 1 : 0;
                            return WriteBindingResult(output, result);
                        }
                        const bool active = binding == CW_MANAGED_BINDING_INPUT_GET_ACTION        ? Input::GetAction(name)
                                            : binding == CW_MANAGED_BINDING_INPUT_GET_ACTION_DOWN ? Input::GetActionDown(name)
                                                                                                  : Input::GetActionUp(name);
                        const uint8_t result = active ? 1 : 0;
                        return WriteBindingResult(output, result);
                    }
                    case CW_MANAGED_BINDING_INPUT_CLEAR_ACTION_REBINDS:
                        if (input.length != 0)
                            return CW_MANAGED_STATUS_INVALID_ARGUMENT;
                        Input::ClearActionRebinds();
                        return CW_MANAGED_STATUS_OK;
                    case CW_MANAGED_BINDING_TIME_GET_TIME:
                        return WriteBindingResult(output, Time::GetTime());
                    case CW_MANAGED_BINDING_TIME_GET_FIXED_DELTA_TIME:
                        return WriteBindingResult(output, Time::GetFixedDeltaTime());
                    case CW_MANAGED_BINDING_TIME_GET_SMOOTH_DELTA_TIME:
                        return WriteBindingResult(output, Time::GetSmoothDeltaTime());
                    case CW_MANAGED_BINDING_TIME_GET_REALTIME_SINCE_STARTUP:
                        return WriteBindingResult(output, Time::GetRealtimeSinceStartup());
                    case CW_MANAGED_BINDING_TIME_GET_FRAME_COUNT:
                        return WriteBindingResult(output, Time::GetFrameCount());
                    case CW_MANAGED_BINDING_RIGIDBODY_2_DGET_MASS:
                    case CW_MANAGED_BINDING_RIGIDBODY_2_DGET_BODY_TYPE:
                    case CW_MANAGED_BINDING_RIGIDBODY_2_DGET_SLEEP_MODE:
                    case CW_MANAGED_BINDING_RIGIDBODY_2_DGET_COLLISION_DETECTION_MODE:
                    case CW_MANAGED_BINDING_RIGIDBODY_2_DGET_INTERPOLATION:
                    case CW_MANAGED_BINDING_RIGIDBODY_2_DGET_AUTO_MASS:
                    case CW_MANAGED_BINDING_RIGIDBODY_2_DGET_LAYER:
                    case CW_MANAGED_BINDING_RIGIDBODY_2_DGET_LINEAR_DRAG:
                    case CW_MANAGED_BINDING_RIGIDBODY_2_DGET_ANGULAR_DRAG:
                    case CW_MANAGED_BINDING_RIGIDBODY_2_DGET_GRAVITY_SCALE:
                    case CW_MANAGED_BINDING_RIGIDBODY_2_DGET_CENTER_OF_MASS:
                    case CW_MANAGED_BINDING_RIGIDBODY_2_DGET_INERTIA:
                    case CW_MANAGED_BINDING_RIGIDBODY_2_DGET_CONSTRAINTS:
                    case CW_MANAGED_BINDING_RIGIDBODY_2_DGET_ROTATION:
                    case CW_MANAGED_BINDING_RIGIDBODY_2_DGET_POSITION:
                    case CW_MANAGED_BINDING_RIGIDBODY_2_DGET_LINEAR_VELOCITY:
                    case CW_MANAGED_BINDING_RIGIDBODY_2_DGET_ANGULAR_VELOCITY:
                    case CW_MANAGED_BINDING_RIGIDBODY_2_DGET_AWAKE: {
                        Entity entity = resolveEntity();
                        if (!entity || !entity.HasComponent<Rigidbody2DComponent>())
                            return CW_MANAGED_STATUS_STALE_HANDLE;
                        auto& rigidbody = entity.GetComponent<Rigidbody2DComponent>();
                        Physics2D* physics = Physics2D::IsStartedUp() ? Physics2D::TryGet() : nullptr;
                        switch (binding)
                        {
                        case CW_MANAGED_BINDING_RIGIDBODY_2_DGET_MASS:
                            return WriteBindingResult(output, physics != nullptr ? physics->GetMass(entity) : rigidbody.GetMass());
                        case CW_MANAGED_BINDING_RIGIDBODY_2_DGET_BODY_TYPE:
                            return WriteBindingResult(output, static_cast<int32_t>(rigidbody.GetBodyType()));
                        case CW_MANAGED_BINDING_RIGIDBODY_2_DGET_SLEEP_MODE:
                            return WriteBindingResult(output, static_cast<int32_t>(rigidbody.GetSleepMode()));
                        case CW_MANAGED_BINDING_RIGIDBODY_2_DGET_COLLISION_DETECTION_MODE:
                            return WriteBindingResult(output, static_cast<int32_t>(rigidbody.GetCollisionDetectionMode()));
                        case CW_MANAGED_BINDING_RIGIDBODY_2_DGET_INTERPOLATION:
                            return WriteBindingResult(output, static_cast<int32_t>(rigidbody.GetInterpolationMode()));
                        case CW_MANAGED_BINDING_RIGIDBODY_2_DGET_AUTO_MASS: {
                            const uint8_t value = rigidbody.GetAutoMass() ? 1 : 0;
                            return WriteBindingResult(output, value);
                        }
                        case CW_MANAGED_BINDING_RIGIDBODY_2_DGET_LAYER:
                            return WriteBindingResult(output, static_cast<int32_t>(rigidbody.GetLayerMask()));
                        case CW_MANAGED_BINDING_RIGIDBODY_2_DGET_LINEAR_DRAG:
                            return WriteBindingResult(output, rigidbody.GetLinearDrag());
                        case CW_MANAGED_BINDING_RIGIDBODY_2_DGET_ANGULAR_DRAG:
                            return WriteBindingResult(output, rigidbody.GetAngularDrag());
                        case CW_MANAGED_BINDING_RIGIDBODY_2_DGET_GRAVITY_SCALE:
                            return WriteBindingResult(output, rigidbody.GetGravityScale());
                        case CW_MANAGED_BINDING_RIGIDBODY_2_DGET_CENTER_OF_MASS:
                            return writeVector2(physics != nullptr ? physics->GetCenterOfMass(entity) : rigidbody.GetCenterOfMass());
                        case CW_MANAGED_BINDING_RIGIDBODY_2_DGET_INERTIA:
                            return WriteBindingResult(output, physics != nullptr ? physics->GetInertia(entity) : rigidbody.GetInertia());
                        case CW_MANAGED_BINDING_RIGIDBODY_2_DGET_CONSTRAINTS:
                            return WriteBindingResult(output, static_cast<uint32_t>(rigidbody.GetConstraints()));
                        case CW_MANAGED_BINDING_RIGIDBODY_2_DGET_ROTATION:
                            return WriteBindingResult(output, physics != nullptr ? physics->GetRotation(entity) : 0.0f);
                        case CW_MANAGED_BINDING_RIGIDBODY_2_DGET_POSITION:
                            return writeVector2(physics != nullptr ? physics->GetPosition(entity) : glm::vec2(entity.GetWorldPosition()));
                        case CW_MANAGED_BINDING_RIGIDBODY_2_DGET_LINEAR_VELOCITY:
                            return writeVector2(physics != nullptr ? physics->GetLinearVelocity(entity) : glm::vec2(0.0f));
                        case CW_MANAGED_BINDING_RIGIDBODY_2_DGET_ANGULAR_VELOCITY:
                            return WriteBindingResult(output, physics != nullptr ? physics->GetAngularVelocity(entity) : 0.0f);
                        case CW_MANAGED_BINDING_RIGIDBODY_2_DGET_AWAKE: {
                            const uint8_t value = physics != nullptr && physics->IsBodyAwake(entity) ? 1 : 0;
                            return WriteBindingResult(output, value);
                        }
                        default:
                            return CW_MANAGED_STATUS_INVALID_ARGUMENT;
                        }
                    }
                    case CW_MANAGED_BINDING_RIGIDBODY_2_DSET_MASS:
                    case CW_MANAGED_BINDING_RIGIDBODY_2_DSET_BODY_TYPE:
                    case CW_MANAGED_BINDING_RIGIDBODY_2_DSET_SLEEP_MODE:
                    case CW_MANAGED_BINDING_RIGIDBODY_2_DSET_COLLISION_DETECTION_MODE:
                    case CW_MANAGED_BINDING_RIGIDBODY_2_DSET_INTERPOLATION:
                    case CW_MANAGED_BINDING_RIGIDBODY_2_DSET_AUTO_MASS:
                    case CW_MANAGED_BINDING_RIGIDBODY_2_DSET_LAYER:
                    case CW_MANAGED_BINDING_RIGIDBODY_2_DSET_LINEAR_DRAG:
                    case CW_MANAGED_BINDING_RIGIDBODY_2_DSET_ANGULAR_DRAG:
                    case CW_MANAGED_BINDING_RIGIDBODY_2_DSET_GRAVITY_SCALE:
                    case CW_MANAGED_BINDING_RIGIDBODY_2_DSET_CENTER_OF_MASS:
                    case CW_MANAGED_BINDING_RIGIDBODY_2_DSET_INERTIA:
                    case CW_MANAGED_BINDING_RIGIDBODY_2_DSET_CONSTRAINTS:
                    case CW_MANAGED_BINDING_RIGIDBODY_2_DSET_LINEAR_VELOCITY:
                    case CW_MANAGED_BINDING_RIGIDBODY_2_DSET_ANGULAR_VELOCITY:
                    case CW_MANAGED_BINDING_RIGIDBODY_2_DSET_AWAKE: {
                        Entity entity = resolveEntity();
                        if (!entity || !entity.HasComponent<Rigidbody2DComponent>())
                            return CW_MANAGED_STATUS_STALE_HANDLE;
                        auto& rigidbody = entity.GetComponent<Rigidbody2DComponent>();
                        if (binding == CW_MANAGED_BINDING_RIGIDBODY_2_DSET_AUTO_MASS || binding == CW_MANAGED_BINDING_RIGIDBODY_2_DSET_AWAKE)
                        {
                            uint8_t value = 0;
                            if (!ReadBindingValue(input, value))
                                return CW_MANAGED_STATUS_INVALID_ARGUMENT;
                            if (binding == CW_MANAGED_BINDING_RIGIDBODY_2_DSET_AUTO_MASS)
                                rigidbody.SetAutoMass(value != 0, entity);
                            else if (Physics2D::IsStartedUp())
                                Physics2D::TryGet()->SetBodyAwake(entity, value != 0);
                            return CW_MANAGED_STATUS_OK;
                        }
                        if (binding == CW_MANAGED_BINDING_RIGIDBODY_2_DSET_BODY_TYPE || binding == CW_MANAGED_BINDING_RIGIDBODY_2_DSET_SLEEP_MODE ||
                            binding == CW_MANAGED_BINDING_RIGIDBODY_2_DSET_COLLISION_DETECTION_MODE ||
                            binding == CW_MANAGED_BINDING_RIGIDBODY_2_DSET_INTERPOLATION || binding == CW_MANAGED_BINDING_RIGIDBODY_2_DSET_LAYER)
                        {
                            int32_t value = 0;
                            if (!ReadBindingValue(input, value))
                                return CW_MANAGED_STATUS_INVALID_ARGUMENT;
                            if (binding == CW_MANAGED_BINDING_RIGIDBODY_2_DSET_BODY_TYPE)
                                rigidbody.SetBodyType(static_cast<RigidbodyBodyType>(value));
                            else if (binding == CW_MANAGED_BINDING_RIGIDBODY_2_DSET_SLEEP_MODE)
                                rigidbody.SetSleepMode(static_cast<RigidbodySleepMode>(value));
                            else if (binding == CW_MANAGED_BINDING_RIGIDBODY_2_DSET_COLLISION_DETECTION_MODE)
                                rigidbody.SetCollisionDetectionMode(static_cast<CollisionDetectionMode2D>(value));
                            else if (binding == CW_MANAGED_BINDING_RIGIDBODY_2_DSET_INTERPOLATION)
                                rigidbody.SetInterpolationMode(static_cast<RigidbodyInterpolation>(value));
                            else if (value >= 0 && value < static_cast<int32_t>(Physics2DLayerCount))
                                rigidbody.SetLayerMask(static_cast<uint32_t>(value), entity);
                            else
                                return CW_MANAGED_STATUS_INVALID_ARGUMENT;
                            return CW_MANAGED_STATUS_OK;
                        }
                        if (binding == CW_MANAGED_BINDING_RIGIDBODY_2_DSET_CONSTRAINTS)
                        {
                            uint32_t value = 0;
                            if (!ReadBindingValue(input, value))
                                return CW_MANAGED_STATUS_INVALID_ARGUMENT;
                            rigidbody.SetConstraints(Rigidbody2DConstraints(value));
                            return CW_MANAGED_STATUS_OK;
                        }
                        if (binding == CW_MANAGED_BINDING_RIGIDBODY_2_DSET_CENTER_OF_MASS ||
                            binding == CW_MANAGED_BINDING_RIGIDBODY_2_DSET_LINEAR_VELOCITY)
                        {
                            float value[2]{};
                            if (!ReadBindingFloats(input, value, 2))
                                return CW_MANAGED_STATUS_INVALID_ARGUMENT;
                            const glm::vec2 vector(value[0], value[1]);
                            if (binding == CW_MANAGED_BINDING_RIGIDBODY_2_DSET_CENTER_OF_MASS)
                                rigidbody.SetCenterOfMass(vector);
                            else if (Physics2D::IsStartedUp())
                                Physics2D::TryGet()->SetLinearVelocity(entity, vector);
                            return CW_MANAGED_STATUS_OK;
                        }
                        float value = 0.0f;
                        if (!ReadBindingValue(input, value))
                            return CW_MANAGED_STATUS_INVALID_ARGUMENT;
                        switch (binding)
                        {
                        case CW_MANAGED_BINDING_RIGIDBODY_2_DSET_MASS:
                            if (rigidbody.GetAutoMass())
                                return CW_MANAGED_STATUS_INVALID_ARGUMENT;
                            rigidbody.SetMass(value);
                            break;
                        case CW_MANAGED_BINDING_RIGIDBODY_2_DSET_LINEAR_DRAG:
                            rigidbody.SetLinearDrag(value);
                            break;
                        case CW_MANAGED_BINDING_RIGIDBODY_2_DSET_ANGULAR_DRAG:
                            rigidbody.SetAngularDrag(value);
                            break;
                        case CW_MANAGED_BINDING_RIGIDBODY_2_DSET_GRAVITY_SCALE:
                            rigidbody.SetGravityScale(value);
                            break;
                        case CW_MANAGED_BINDING_RIGIDBODY_2_DSET_INERTIA:
                            rigidbody.SetInertia(value);
                            break;
                        case CW_MANAGED_BINDING_RIGIDBODY_2_DSET_ANGULAR_VELOCITY:
                            if (Physics2D::IsStartedUp())
                                Physics2D::TryGet()->SetAngularVelocity(entity, value);
                            break;
                        default:
                            return CW_MANAGED_STATUS_INVALID_ARGUMENT;
                        }
                        return CW_MANAGED_STATUS_OK;
                    }
                    case CW_MANAGED_BINDING_RIGIDBODY_2_DADD_FORCE:
                    case CW_MANAGED_BINDING_RIGIDBODY_2_DADD_FORCE_AT_POSITION:
                    case CW_MANAGED_BINDING_RIGIDBODY_2_DADD_TORQUE: {
                        Entity entity = resolveEntity();
                        if (!entity || !entity.HasComponent<Rigidbody2DComponent>())
                            return CW_MANAGED_STATUS_STALE_HANDLE;
                        Physics2D* physics = Physics2D::IsStartedUp() ? Physics2D::TryGet() : nullptr;
                        if (physics == nullptr)
                            return CW_MANAGED_STATUS_NOT_INITIALIZED;
                        if (binding == CW_MANAGED_BINDING_RIGIDBODY_2_DADD_FORCE)
                        {
                            struct Payload
                            {
                                float Force[2];
                                int32_t Mode;
                            } payload{};
                            static_assert(sizeof(Payload) == 12);
                            if (!ReadBindingValue(input, payload))
                                return CW_MANAGED_STATUS_INVALID_ARGUMENT;
                            physics->AddForce(entity, glm::vec2(payload.Force[0], payload.Force[1]), static_cast<ForceMode>(payload.Mode));
                        }
                        else if (binding == CW_MANAGED_BINDING_RIGIDBODY_2_DADD_FORCE_AT_POSITION)
                        {
                            struct Payload
                            {
                                float Force[2];
                                float Position[2];
                                int32_t Mode;
                            } payload{};
                            static_assert(sizeof(Payload) == 20);
                            if (!ReadBindingValue(input, payload))
                                return CW_MANAGED_STATUS_INVALID_ARGUMENT;
                            physics->AddForceAt(entity, glm::vec2(payload.Force[0], payload.Force[1]),
                                                glm::vec2(payload.Position[0], payload.Position[1]), static_cast<ForceMode>(payload.Mode));
                        }
                        else
                        {
                            struct Payload
                            {
                                float Torque;
                                int32_t Mode;
                            } payload{};
                            static_assert(sizeof(Payload) == 8);
                            if (!ReadBindingValue(input, payload))
                                return CW_MANAGED_STATUS_INVALID_ARGUMENT;
                            physics->AddTorque(entity, payload.Torque, static_cast<ForceMode>(payload.Mode));
                        }
                        return CW_MANAGED_STATUS_OK;
                    }
                    case CW_MANAGED_BINDING_AUDIO_SOURCE_GET_VOLUME:
                    case CW_MANAGED_BINDING_AUDIO_SOURCE_GET_PITCH:
                    case CW_MANAGED_BINDING_AUDIO_SOURCE_GET_MIN_DISTANCE:
                    case CW_MANAGED_BINDING_AUDIO_SOURCE_GET_MAX_DISTANCE:
                    case CW_MANAGED_BINDING_AUDIO_SOURCE_GET_LOOP:
                    case CW_MANAGED_BINDING_AUDIO_SOURCE_GET_MUTED:
                    case CW_MANAGED_BINDING_AUDIO_SOURCE_GET_PLAY_ON_AWAKE:
                    case CW_MANAGED_BINDING_AUDIO_SOURCE_GET_TIME:
                    case CW_MANAGED_BINDING_AUDIO_SOURCE_GET_CLIP:
                    case CW_MANAGED_BINDING_AUDIO_SOURCE_GET_STATE: {
                        Entity entity = resolveEntity();
                        if (!entity || !entity.HasComponent<AudioSourceComponent>())
                            return CW_MANAGED_STATUS_STALE_HANDLE;
                        auto& source = entity.GetComponent<AudioSourceComponent>();
                        switch (binding)
                        {
                        case CW_MANAGED_BINDING_AUDIO_SOURCE_GET_VOLUME:
                            return WriteBindingResult(output, source.GetVolume());
                        case CW_MANAGED_BINDING_AUDIO_SOURCE_GET_PITCH:
                            return WriteBindingResult(output, source.GetPitch());
                        case CW_MANAGED_BINDING_AUDIO_SOURCE_GET_MIN_DISTANCE:
                            return WriteBindingResult(output, source.GetMinDistance());
                        case CW_MANAGED_BINDING_AUDIO_SOURCE_GET_MAX_DISTANCE:
                            return WriteBindingResult(output, source.GetMaxDistance());
                        case CW_MANAGED_BINDING_AUDIO_SOURCE_GET_LOOP: {
                            const uint8_t value = source.GetLooping() ? 1 : 0;
                            return WriteBindingResult(output, value);
                        }
                        case CW_MANAGED_BINDING_AUDIO_SOURCE_GET_MUTED: {
                            const uint8_t value = source.GetIsMuted() ? 1 : 0;
                            return WriteBindingResult(output, value);
                        }
                        case CW_MANAGED_BINDING_AUDIO_SOURCE_GET_PLAY_ON_AWAKE: {
                            const uint8_t value = source.GetPlayOnAwake() ? 1 : 0;
                            return WriteBindingResult(output, value);
                        }
                        case CW_MANAGED_BINDING_AUDIO_SOURCE_GET_TIME:
                            return WriteBindingResult(output, source.GetTime());
                        case CW_MANAGED_BINDING_AUDIO_SOURCE_GET_CLIP:
                            return WriteBindingResult(output, ToAbiUuid(source.GetClip().GetUUID()));
                        case CW_MANAGED_BINDING_AUDIO_SOURCE_GET_STATE:
                            return WriteBindingResult(output, static_cast<int32_t>(source.GetState()));
                        default:
                            return CW_MANAGED_STATUS_INVALID_ARGUMENT;
                        }
                    }
                    case CW_MANAGED_BINDING_AUDIO_SOURCE_SET_VOLUME:
                    case CW_MANAGED_BINDING_AUDIO_SOURCE_SET_PITCH:
                    case CW_MANAGED_BINDING_AUDIO_SOURCE_SET_MIN_DISTANCE:
                    case CW_MANAGED_BINDING_AUDIO_SOURCE_SET_MAX_DISTANCE:
                    case CW_MANAGED_BINDING_AUDIO_SOURCE_SET_LOOP:
                    case CW_MANAGED_BINDING_AUDIO_SOURCE_SET_MUTED:
                    case CW_MANAGED_BINDING_AUDIO_SOURCE_SET_PLAY_ON_AWAKE:
                    case CW_MANAGED_BINDING_AUDIO_SOURCE_SET_TIME:
                    case CW_MANAGED_BINDING_AUDIO_SOURCE_SET_CLIP:
                    case CW_MANAGED_BINDING_AUDIO_SOURCE_PLAY:
                    case CW_MANAGED_BINDING_AUDIO_SOURCE_PAUSE:
                    case CW_MANAGED_BINDING_AUDIO_SOURCE_STOP: {
                        Entity entity = resolveEntity();
                        if (!entity || !entity.HasComponent<AudioSourceComponent>())
                            return CW_MANAGED_STATUS_STALE_HANDLE;
                        auto& source = entity.GetComponent<AudioSourceComponent>();
                        if (binding == CW_MANAGED_BINDING_AUDIO_SOURCE_PLAY || binding == CW_MANAGED_BINDING_AUDIO_SOURCE_PAUSE ||
                            binding == CW_MANAGED_BINDING_AUDIO_SOURCE_STOP)
                        {
                            if (input.length != 0)
                                return CW_MANAGED_STATUS_INVALID_ARGUMENT;
                            if (binding == CW_MANAGED_BINDING_AUDIO_SOURCE_PLAY)
                                source.Play();
                            else if (binding == CW_MANAGED_BINDING_AUDIO_SOURCE_PAUSE)
                                source.Pause();
                            else
                                source.Stop();
                            return CW_MANAGED_STATUS_OK;
                        }
                        if (binding == CW_MANAGED_BINDING_AUDIO_SOURCE_SET_LOOP || binding == CW_MANAGED_BINDING_AUDIO_SOURCE_SET_MUTED ||
                            binding == CW_MANAGED_BINDING_AUDIO_SOURCE_SET_PLAY_ON_AWAKE)
                        {
                            uint8_t value = 0;
                            if (!ReadBindingValue(input, value))
                                return CW_MANAGED_STATUS_INVALID_ARGUMENT;
                            if (binding == CW_MANAGED_BINDING_AUDIO_SOURCE_SET_LOOP)
                                source.SetLooping(value != 0);
                            else if (binding == CW_MANAGED_BINDING_AUDIO_SOURCE_SET_MUTED)
                                source.SetIsMuted(value != 0);
                            else
                                source.SetPlayOnAwake(value != 0);
                            return CW_MANAGED_STATUS_OK;
                        }
                        if (binding == CW_MANAGED_BINDING_AUDIO_SOURCE_SET_CLIP)
                        {
                            cw_managed_uuid value{};
                            if (!ReadBindingValue(input, value))
                                return CW_MANAGED_STATUS_INVALID_ARGUMENT;
                            const UUID uuid = FromAbiUuid(value);
                            if (uuid.Empty())
                                source.SetClip({});
                            else if (AssetManager::IsStartedUp())
                                source.SetClip(AssetManager::TryGet()->LoadFromUUID<AudioClip>(uuid));
                            else
                                return CW_MANAGED_STATUS_NOT_INITIALIZED;
                            return CW_MANAGED_STATUS_OK;
                        }
                        float value = 0.0f;
                        if (!ReadBindingValue(input, value))
                            return CW_MANAGED_STATUS_INVALID_ARGUMENT;
                        if (binding == CW_MANAGED_BINDING_AUDIO_SOURCE_SET_VOLUME)
                            source.SetVolume(value);
                        else if (binding == CW_MANAGED_BINDING_AUDIO_SOURCE_SET_PITCH)
                            source.SetPitch(value);
                        else if (binding == CW_MANAGED_BINDING_AUDIO_SOURCE_SET_MIN_DISTANCE)
                            source.SetMinDistance(value);
                        else if (binding == CW_MANAGED_BINDING_AUDIO_SOURCE_SET_MAX_DISTANCE)
                            source.SetMaxDistance(value);
                        else
                            source.SetTime(value);
                        return CW_MANAGED_STATUS_OK;
                    }
                    case CW_MANAGED_BINDING_TEXT_GET_TEXT:
                    case CW_MANAGED_BINDING_TEXT_GET_FONT:
                    case CW_MANAGED_BINDING_TEXT_GET_COLOR:
                    case CW_MANAGED_BINDING_TEXT_GET_SIZE:
                    case CW_MANAGED_BINDING_TEXT_GET_AUTO_SIZE:
                    case CW_MANAGED_BINDING_TEXT_GET_AUTO_SIZE_MIN:
                    case CW_MANAGED_BINDING_TEXT_GET_AUTO_SIZE_MAX:
                    case CW_MANAGED_BINDING_TEXT_GET_LAYOUT_SIZE:
                    case CW_MANAGED_BINDING_TEXT_GET_WRAPPING:
                    case CW_MANAGED_BINDING_TEXT_GET_WRAP_MODE:
                    case CW_MANAGED_BINDING_TEXT_GET_OVERFLOW:
                    case CW_MANAGED_BINDING_TEXT_GET_CLIP_TO_BOUNDS:
                    case CW_MANAGED_BINDING_TEXT_GET_MAX_LINES:
                    case CW_MANAGED_BINDING_TEXT_GET_HORIZONTAL_ALIGNMENT:
                    case CW_MANAGED_BINDING_TEXT_GET_VERTICAL_ALIGNMENT:
                    case CW_MANAGED_BINDING_TEXT_GET_FONT_STYLE:
                    case CW_MANAGED_BINDING_TEXT_GET_OUTLINE_COLOR:
                    case CW_MANAGED_BINDING_TEXT_GET_OUTLINE_WIDTH:
                    case CW_MANAGED_BINDING_TEXT_GET_SHADOW_COLOR:
                    case CW_MANAGED_BINDING_TEXT_GET_SHADOW_OFFSET:
                    case CW_MANAGED_BINDING_TEXT_GET_SHADOW_SOFTNESS:
                    case CW_MANAGED_BINDING_TEXT_GET_CHARACTER_SPACING:
                    case CW_MANAGED_BINDING_TEXT_GET_WORD_SPACING:
                    case CW_MANAGED_BINDING_TEXT_GET_LINE_SPACING:
                    case CW_MANAGED_BINDING_TEXT_GET_PARAGRAPH_SPACING:
                    case CW_MANAGED_BINDING_TEXT_GET_TAB_WIDTH:
                    case CW_MANAGED_BINDING_TEXT_GET_USE_CUSTOM_DECORATION_COLOR:
                    case CW_MANAGED_BINDING_TEXT_GET_DECORATION_COLOR:
                    case CW_MANAGED_BINDING_TEXT_GET_DECORATION_THICKNESS:
                    case CW_MANAGED_BINDING_TEXT_GET_UNDERLINE_OFFSET:
                    case CW_MANAGED_BINDING_TEXT_GET_STRIKETHROUGH_OFFSET:
                    case CW_MANAGED_BINDING_TEXT_GET_USE_KERNING:
                    case CW_MANAGED_BINDING_TEXT_GET_SORTING_LAYER:
                    case CW_MANAGED_BINDING_TEXT_GET_ORDER_IN_LAYER: {
                        Entity entity = resolveEntity();
                        if (!entity || !entity.HasComponent<TextComponent>())
                            return CW_MANAGED_STATUS_STALE_HANDLE;
                        const TextComponent& text = entity.GetComponent<TextComponent>();
                        switch (binding)
                        {
                        case CW_MANAGED_BINDING_TEXT_GET_TEXT:
                            return WriteBindingResult(output, text.Text.data(), text.Text.size());
                        case CW_MANAGED_BINDING_TEXT_GET_FONT:
                            return WriteBindingResult(output, ToAbiUuid(text.Font.GetUUID()));
                        case CW_MANAGED_BINDING_TEXT_GET_COLOR:
                            return writeVector4(text.Color);
                        case CW_MANAGED_BINDING_TEXT_GET_SIZE:
                            return WriteBindingResult(output, text.Size);
                        case CW_MANAGED_BINDING_TEXT_GET_AUTO_SIZE: {
                            const uint8_t value = text.AutoSize ? 1 : 0;
                            return WriteBindingResult(output, value);
                        }
                        case CW_MANAGED_BINDING_TEXT_GET_AUTO_SIZE_MIN:
                            return WriteBindingResult(output, text.AutoSizeMin);
                        case CW_MANAGED_BINDING_TEXT_GET_AUTO_SIZE_MAX:
                            return WriteBindingResult(output, text.AutoSizeMax);
                        case CW_MANAGED_BINDING_TEXT_GET_LAYOUT_SIZE:
                            return writeVector2(text.LayoutSize);
                        case CW_MANAGED_BINDING_TEXT_GET_WRAPPING: {
                            const uint8_t value = text.Wrapping ? 1 : 0;
                            return WriteBindingResult(output, value);
                        }
                        case CW_MANAGED_BINDING_TEXT_GET_WRAP_MODE:
                            return WriteBindingResult(output, static_cast<int32_t>(text.WrapMode));
                        case CW_MANAGED_BINDING_TEXT_GET_OVERFLOW:
                            return WriteBindingResult(output, static_cast<int32_t>(text.Overflow));
                        case CW_MANAGED_BINDING_TEXT_GET_CLIP_TO_BOUNDS: {
                            const uint8_t value = text.ClipToBounds ? 1 : 0;
                            return WriteBindingResult(output, value);
                        }
                        case CW_MANAGED_BINDING_TEXT_GET_MAX_LINES:
                            return WriteBindingResult(output, text.MaxLines);
                        case CW_MANAGED_BINDING_TEXT_GET_HORIZONTAL_ALIGNMENT:
                            return WriteBindingResult(output, static_cast<int32_t>(text.HorizontalAlignment));
                        case CW_MANAGED_BINDING_TEXT_GET_VERTICAL_ALIGNMENT:
                            return WriteBindingResult(output, static_cast<int32_t>(text.VerticalAlignment));
                        case CW_MANAGED_BINDING_TEXT_GET_FONT_STYLE:
                            return WriteBindingResult(output, static_cast<uint32_t>(text.FontStyle));
                        case CW_MANAGED_BINDING_TEXT_GET_OUTLINE_COLOR:
                            return writeVector4(text.OutlineColor);
                        case CW_MANAGED_BINDING_TEXT_GET_OUTLINE_WIDTH:
                            return WriteBindingResult(output, text.Thickness);
                        case CW_MANAGED_BINDING_TEXT_GET_SHADOW_COLOR:
                            return writeVector4(text.ShadowColor);
                        case CW_MANAGED_BINDING_TEXT_GET_SHADOW_OFFSET:
                            return writeVector2(text.ShadowOffset);
                        case CW_MANAGED_BINDING_TEXT_GET_SHADOW_SOFTNESS:
                            return WriteBindingResult(output, text.ShadowSoftness);
                        case CW_MANAGED_BINDING_TEXT_GET_CHARACTER_SPACING:
                            return WriteBindingResult(output, text.CharacterSpacing);
                        case CW_MANAGED_BINDING_TEXT_GET_WORD_SPACING:
                            return WriteBindingResult(output, text.WordSpacing);
                        case CW_MANAGED_BINDING_TEXT_GET_LINE_SPACING:
                            return WriteBindingResult(output, text.LineSpacing);
                        case CW_MANAGED_BINDING_TEXT_GET_PARAGRAPH_SPACING:
                            return WriteBindingResult(output, text.ParagraphSpacing);
                        case CW_MANAGED_BINDING_TEXT_GET_TAB_WIDTH:
                            return WriteBindingResult(output, text.TabWidth);
                        case CW_MANAGED_BINDING_TEXT_GET_USE_CUSTOM_DECORATION_COLOR: {
                            const uint8_t value = text.UseCustomDecorationColor ? 1 : 0;
                            return WriteBindingResult(output, value);
                        }
                        case CW_MANAGED_BINDING_TEXT_GET_DECORATION_COLOR:
                            return writeVector4(text.DecorationColor);
                        case CW_MANAGED_BINDING_TEXT_GET_DECORATION_THICKNESS:
                            return WriteBindingResult(output, text.DecorationThickness);
                        case CW_MANAGED_BINDING_TEXT_GET_UNDERLINE_OFFSET:
                            return WriteBindingResult(output, text.UnderlineOffset);
                        case CW_MANAGED_BINDING_TEXT_GET_STRIKETHROUGH_OFFSET:
                            return WriteBindingResult(output, text.StrikethroughOffset);
                        case CW_MANAGED_BINDING_TEXT_GET_USE_KERNING: {
                            const uint8_t value = text.UseKerning ? 1 : 0;
                            return WriteBindingResult(output, value);
                        }
                        case CW_MANAGED_BINDING_TEXT_GET_SORTING_LAYER:
                            return WriteBindingResult(output, text.SortingLayer);
                        case CW_MANAGED_BINDING_TEXT_GET_ORDER_IN_LAYER:
                            return WriteBindingResult(output, text.OrderInLayer);
                        default:
                            return CW_MANAGED_STATUS_INVALID_ARGUMENT;
                        }
                    }
                    case CW_MANAGED_BINDING_TEXT_SET_TEXT:
                    case CW_MANAGED_BINDING_TEXT_SET_FONT:
                    case CW_MANAGED_BINDING_TEXT_SET_COLOR:
                    case CW_MANAGED_BINDING_TEXT_SET_SIZE:
                    case CW_MANAGED_BINDING_TEXT_SET_AUTO_SIZE:
                    case CW_MANAGED_BINDING_TEXT_SET_AUTO_SIZE_MIN:
                    case CW_MANAGED_BINDING_TEXT_SET_AUTO_SIZE_MAX:
                    case CW_MANAGED_BINDING_TEXT_SET_LAYOUT_SIZE:
                    case CW_MANAGED_BINDING_TEXT_SET_WRAPPING:
                    case CW_MANAGED_BINDING_TEXT_SET_WRAP_MODE:
                    case CW_MANAGED_BINDING_TEXT_SET_OVERFLOW:
                    case CW_MANAGED_BINDING_TEXT_SET_CLIP_TO_BOUNDS:
                    case CW_MANAGED_BINDING_TEXT_SET_MAX_LINES:
                    case CW_MANAGED_BINDING_TEXT_SET_HORIZONTAL_ALIGNMENT:
                    case CW_MANAGED_BINDING_TEXT_SET_VERTICAL_ALIGNMENT:
                    case CW_MANAGED_BINDING_TEXT_SET_FONT_STYLE:
                    case CW_MANAGED_BINDING_TEXT_SET_OUTLINE_COLOR:
                    case CW_MANAGED_BINDING_TEXT_SET_OUTLINE_WIDTH:
                    case CW_MANAGED_BINDING_TEXT_SET_SHADOW_COLOR:
                    case CW_MANAGED_BINDING_TEXT_SET_SHADOW_OFFSET:
                    case CW_MANAGED_BINDING_TEXT_SET_SHADOW_SOFTNESS:
                    case CW_MANAGED_BINDING_TEXT_SET_CHARACTER_SPACING:
                    case CW_MANAGED_BINDING_TEXT_SET_WORD_SPACING:
                    case CW_MANAGED_BINDING_TEXT_SET_LINE_SPACING:
                    case CW_MANAGED_BINDING_TEXT_SET_PARAGRAPH_SPACING:
                    case CW_MANAGED_BINDING_TEXT_SET_TAB_WIDTH:
                    case CW_MANAGED_BINDING_TEXT_SET_USE_CUSTOM_DECORATION_COLOR:
                    case CW_MANAGED_BINDING_TEXT_SET_DECORATION_COLOR:
                    case CW_MANAGED_BINDING_TEXT_SET_DECORATION_THICKNESS:
                    case CW_MANAGED_BINDING_TEXT_SET_UNDERLINE_OFFSET:
                    case CW_MANAGED_BINDING_TEXT_SET_STRIKETHROUGH_OFFSET:
                    case CW_MANAGED_BINDING_TEXT_SET_USE_KERNING:
                    case CW_MANAGED_BINDING_TEXT_SET_SORTING_LAYER:
                    case CW_MANAGED_BINDING_TEXT_SET_ORDER_IN_LAYER: {
                        Entity entity = resolveEntity();
                        if (!entity || !entity.HasComponent<TextComponent>())
                            return CW_MANAGED_STATUS_STALE_HANDLE;
                        TextComponent& text = entity.GetComponent<TextComponent>();

                        if (binding == CW_MANAGED_BINDING_TEXT_SET_TEXT)
                        {
                            if (input.length == 0)
                                text.Text.clear();
                            else
                                text.Text.assign(reinterpret_cast<const char*>(input.data), static_cast<size_t>(input.length));
                            return CW_MANAGED_STATUS_OK;
                        }
                        if (binding == CW_MANAGED_BINDING_TEXT_SET_FONT)
                        {
                            cw_managed_uuid value{};
                            if (!ReadBindingValue(input, value))
                                return CW_MANAGED_STATUS_INVALID_ARGUMENT;
                            const UUID uuid = FromAbiUuid(value);
                            if (uuid.Empty())
                                text.Font = {};
                            else if (AssetManager::IsStartedUp())
                                text.Font = AssetManager::TryGet()->LoadFromUUID<Font>(uuid);
                            else
                                return CW_MANAGED_STATUS_NOT_INITIALIZED;
                            return CW_MANAGED_STATUS_OK;
                        }
                        if (binding == CW_MANAGED_BINDING_TEXT_SET_COLOR || binding == CW_MANAGED_BINDING_TEXT_SET_OUTLINE_COLOR ||
                            binding == CW_MANAGED_BINDING_TEXT_SET_SHADOW_COLOR || binding == CW_MANAGED_BINDING_TEXT_SET_DECORATION_COLOR)
                        {
                            float value[4]{};
                            if (!ReadBindingFloats(input, value, 4))
                                return CW_MANAGED_STATUS_INVALID_ARGUMENT;
                            const glm::vec4 color(value[0], value[1], value[2], value[3]);
                            if (binding == CW_MANAGED_BINDING_TEXT_SET_COLOR)
                                text.Color = color;
                            else if (binding == CW_MANAGED_BINDING_TEXT_SET_OUTLINE_COLOR)
                                text.OutlineColor = color;
                            else if (binding == CW_MANAGED_BINDING_TEXT_SET_SHADOW_COLOR)
                                text.ShadowColor = color;
                            else
                                text.DecorationColor = color;
                            return CW_MANAGED_STATUS_OK;
                        }
                        if (binding == CW_MANAGED_BINDING_TEXT_SET_LAYOUT_SIZE || binding == CW_MANAGED_BINDING_TEXT_SET_SHADOW_OFFSET)
                        {
                            float value[2]{};
                            if (!ReadBindingFloats(input, value, 2))
                                return CW_MANAGED_STATUS_INVALID_ARGUMENT;
                            const glm::vec2 vector(value[0], value[1]);
                            if (binding == CW_MANAGED_BINDING_TEXT_SET_LAYOUT_SIZE)
                                text.LayoutSize = glm::max(vector, glm::vec2(0.0f));
                            else
                                text.ShadowOffset = vector;
                            return CW_MANAGED_STATUS_OK;
                        }
                        if (binding == CW_MANAGED_BINDING_TEXT_SET_AUTO_SIZE || binding == CW_MANAGED_BINDING_TEXT_SET_WRAPPING ||
                            binding == CW_MANAGED_BINDING_TEXT_SET_CLIP_TO_BOUNDS ||
                            binding == CW_MANAGED_BINDING_TEXT_SET_USE_CUSTOM_DECORATION_COLOR || binding == CW_MANAGED_BINDING_TEXT_SET_USE_KERNING)
                        {
                            uint8_t value = 0;
                            if (!ReadBindingValue(input, value))
                                return CW_MANAGED_STATUS_INVALID_ARGUMENT;
                            const bool enabled = value != 0;
                            if (binding == CW_MANAGED_BINDING_TEXT_SET_AUTO_SIZE)
                                text.AutoSize = enabled;
                            else if (binding == CW_MANAGED_BINDING_TEXT_SET_WRAPPING)
                                text.Wrapping = enabled;
                            else if (binding == CW_MANAGED_BINDING_TEXT_SET_CLIP_TO_BOUNDS)
                                text.ClipToBounds = enabled;
                            else if (binding == CW_MANAGED_BINDING_TEXT_SET_USE_CUSTOM_DECORATION_COLOR)
                                text.UseCustomDecorationColor = enabled;
                            else
                                text.UseKerning = enabled;
                            return CW_MANAGED_STATUS_OK;
                        }
                        if (binding == CW_MANAGED_BINDING_TEXT_SET_MAX_LINES || binding == CW_MANAGED_BINDING_TEXT_SET_FONT_STYLE ||
                            binding == CW_MANAGED_BINDING_TEXT_SET_TAB_WIDTH)
                        {
                            uint32_t value = 0;
                            if (!ReadBindingValue(input, value))
                                return CW_MANAGED_STATUS_INVALID_ARGUMENT;
                            if (binding == CW_MANAGED_BINDING_TEXT_SET_MAX_LINES)
                                text.MaxLines = value;
                            else if (binding == CW_MANAGED_BINDING_TEXT_SET_FONT_STYLE)
                                text.FontStyle = static_cast<TextFontStyleBits>(value);
                            else
                                text.TabWidth = std::max(1u, value);
                            return CW_MANAGED_STATUS_OK;
                        }
                        if (binding == CW_MANAGED_BINDING_TEXT_SET_WRAP_MODE || binding == CW_MANAGED_BINDING_TEXT_SET_OVERFLOW ||
                            binding == CW_MANAGED_BINDING_TEXT_SET_HORIZONTAL_ALIGNMENT ||
                            binding == CW_MANAGED_BINDING_TEXT_SET_VERTICAL_ALIGNMENT || binding == CW_MANAGED_BINDING_TEXT_SET_SORTING_LAYER ||
                            binding == CW_MANAGED_BINDING_TEXT_SET_ORDER_IN_LAYER)
                        {
                            int32_t value = 0;
                            if (!ReadBindingValue(input, value))
                                return CW_MANAGED_STATUS_INVALID_ARGUMENT;
                            if (binding == CW_MANAGED_BINDING_TEXT_SET_WRAP_MODE)
                                text.WrapMode = static_cast<TextWrapMode>(value);
                            else if (binding == CW_MANAGED_BINDING_TEXT_SET_OVERFLOW)
                                text.Overflow = static_cast<TextOverflow>(value);
                            else if (binding == CW_MANAGED_BINDING_TEXT_SET_HORIZONTAL_ALIGNMENT)
                                text.HorizontalAlignment = static_cast<TextHorizontalAlignment>(value);
                            else if (binding == CW_MANAGED_BINDING_TEXT_SET_VERTICAL_ALIGNMENT)
                                text.VerticalAlignment = static_cast<TextVerticalAlignment>(value);
                            else if (binding == CW_MANAGED_BINDING_TEXT_SET_SORTING_LAYER)
                                text.SortingLayer = value;
                            else
                                text.OrderInLayer = value;
                            return CW_MANAGED_STATUS_OK;
                        }

                        float value = 0.0f;
                        if (!ReadBindingValue(input, value))
                            return CW_MANAGED_STATUS_INVALID_ARGUMENT;
                        if (binding == CW_MANAGED_BINDING_TEXT_SET_SIZE)
                            text.Size = value;
                        else if (binding == CW_MANAGED_BINDING_TEXT_SET_AUTO_SIZE_MIN)
                        {
                            text.AutoSizeMin = std::max(0.0f, value);
                            text.AutoSizeMax = std::max(text.AutoSizeMin, text.AutoSizeMax);
                        }
                        else if (binding == CW_MANAGED_BINDING_TEXT_SET_AUTO_SIZE_MAX)
                        {
                            text.AutoSizeMax = std::max(0.0f, value);
                            text.AutoSizeMin = std::min(text.AutoSizeMin, text.AutoSizeMax);
                        }
                        else if (binding == CW_MANAGED_BINDING_TEXT_SET_OUTLINE_WIDTH)
                            text.Thickness = value;
                        else if (binding == CW_MANAGED_BINDING_TEXT_SET_SHADOW_SOFTNESS)
                            text.ShadowSoftness = std::max(0.0f, value);
                        else if (binding == CW_MANAGED_BINDING_TEXT_SET_CHARACTER_SPACING)
                            text.CharacterSpacing = value;
                        else if (binding == CW_MANAGED_BINDING_TEXT_SET_WORD_SPACING)
                            text.WordSpacing = value;
                        else if (binding == CW_MANAGED_BINDING_TEXT_SET_LINE_SPACING)
                            text.LineSpacing = value;
                        else if (binding == CW_MANAGED_BINDING_TEXT_SET_PARAGRAPH_SPACING)
                            text.ParagraphSpacing = value;
                        else if (binding == CW_MANAGED_BINDING_TEXT_SET_DECORATION_THICKNESS)
                            text.DecorationThickness = std::max(0.0f, value);
                        else if (binding == CW_MANAGED_BINDING_TEXT_SET_UNDERLINE_OFFSET)
                            text.UnderlineOffset = value;
                        else if (binding == CW_MANAGED_BINDING_TEXT_SET_STRIKETHROUGH_OFFSET)
                            text.StrikethroughOffset = value;
                        else
                            return CW_MANAGED_STATUS_INVALID_ARGUMENT;
                        return CW_MANAGED_STATUS_OK;
                    }
                    case CW_MANAGED_BINDING_FONT_GET_IS_VALID:
                    case CW_MANAGED_BINDING_FONT_GET_GLYPH_COUNT:
                    case CW_MANAGED_BINDING_FONT_GET_TAB_WIDTH:
                    case CW_MANAGED_BINDING_FONT_GET_ATLAS_WIDTH:
                    case CW_MANAGED_BINDING_FONT_GET_ATLAS_HEIGHT:
                    case CW_MANAGED_BINDING_FONT_GET_ATLAS_PIXEL_RANGE:
                    case CW_MANAGED_BINDING_FONT_HAS_GLYPH:
                    case CW_MANAGED_BINDING_FONT_GET_CHARACTER_INFO:
                    case CW_MANAGED_BINDING_FONT_GET_FALLBACK_COUNT:
                    case CW_MANAGED_BINDING_FONT_GET_FALLBACK:
                    case CW_MANAGED_BINDING_FONT_ADD_FALLBACK:
                    case CW_MANAGED_BINDING_FONT_CLEAR_FALLBACKS: {
                        AssetHandle<Font> font;
                        const cw_managed_status fontStatus = ResolveFontHandle(entityId, font);
                        if (fontStatus != CW_MANAGED_STATUS_OK)
                            return fontStatus;

                        switch (binding)
                        {
                        case CW_MANAGED_BINDING_FONT_GET_IS_VALID: {
                            if (input.length != 0)
                                return CW_MANAGED_STATUS_INVALID_ARGUMENT;
                            const uint8_t result = font->IsValid() ? 1 : 0;
                            return WriteBindingResult(output, result);
                        }
                        case CW_MANAGED_BINDING_FONT_GET_GLYPH_COUNT: {
                            if (input.length != 0)
                                return CW_MANAGED_STATUS_INVALID_ARGUMENT;
                            const uint32_t result = static_cast<uint32_t>(
                              std::min(font->GetGlyphCount(), static_cast<size_t>(std::numeric_limits<uint32_t>::max())));
                            return WriteBindingResult(output, result);
                        }
                        case CW_MANAGED_BINDING_FONT_GET_TAB_WIDTH:
                            return input.length == 0 ? WriteBindingResult(output, font->GetTabWidth())
                                                     : CW_MANAGED_STATUS_INVALID_ARGUMENT;
                        case CW_MANAGED_BINDING_FONT_GET_ATLAS_WIDTH:
                            return input.length == 0 ? WriteBindingResult(output, font->GetAtlasWidth())
                                                     : CW_MANAGED_STATUS_INVALID_ARGUMENT;
                        case CW_MANAGED_BINDING_FONT_GET_ATLAS_HEIGHT:
                            return input.length == 0 ? WriteBindingResult(output, font->GetAtlasHeight())
                                                     : CW_MANAGED_STATUS_INVALID_ARGUMENT;
                        case CW_MANAGED_BINDING_FONT_GET_ATLAS_PIXEL_RANGE:
                            return input.length == 0 ? WriteBindingResult(output, font->GetAtlasPixelRange())
                                                     : CW_MANAGED_STATUS_INVALID_ARGUMENT;
                        case CW_MANAGED_BINDING_FONT_HAS_GLYPH: {
                            uint32_t codePoint = 0;
                            if (!ReadBindingValue(input, codePoint))
                                return CW_MANAGED_STATUS_INVALID_ARGUMENT;
                            const uint8_t result = font->HasGlyph(static_cast<char32_t>(codePoint)) ? 1 : 0;
                            return WriteBindingResult(output, result);
                        }
                        case CW_MANAGED_BINDING_FONT_GET_CHARACTER_INFO: {
                            if (input.data == nullptr || input.length != sizeof(uint32_t) + sizeof(uint8_t))
                                return CW_MANAGED_STATUS_INVALID_ARGUMENT;
                            uint32_t codePoint = 0;
                            std::memcpy(&codePoint, input.data, sizeof(codePoint));
                            const bool useFallbacks = input.data[sizeof(codePoint)] != 0;
                            const CharacterInfo characterInfo = font->GetCharacterInfo(static_cast<char32_t>(codePoint), useFallbacks);
                            return WriteBindingResult(output, ToAbiCharacterInfo(font, characterInfo));
                        }
                        case CW_MANAGED_BINDING_FONT_GET_FALLBACK_COUNT: {
                            if (input.length != 0)
                                return CW_MANAGED_STATUS_INVALID_ARGUMENT;
                            const uint32_t result = static_cast<uint32_t>(font->GetFallbackFonts().size());
                            return WriteBindingResult(output, result);
                        }
                        case CW_MANAGED_BINDING_FONT_GET_FALLBACK: {
                            uint32_t index = 0;
                            if (!ReadBindingValue(input, index))
                                return CW_MANAGED_STATUS_INVALID_ARGUMENT;
                            const UUID fallback = index < font->GetFallbackFonts().size() ? font->GetFallbackFonts()[index].GetUUID() : UUID{};
                            return WriteBindingResult(output, ToAbiUuid(fallback));
                        }
                        case CW_MANAGED_BINDING_FONT_ADD_FALLBACK: {
                            cw_managed_uuid fallbackId{};
                            if (!ReadBindingValue(input, fallbackId))
                                return CW_MANAGED_STATUS_INVALID_ARGUMENT;
                            AssetHandle<Font> fallback;
                            const cw_managed_status fallbackStatus = ResolveFontHandle(fallbackId, fallback);
                            if (fallbackStatus != CW_MANAGED_STATUS_OK)
                                return fallbackStatus;
                            const uint8_t result = font->AddFallbackFont(fallback) ? 1 : 0;
                            return WriteBindingResult(output, result);
                        }
                        case CW_MANAGED_BINDING_FONT_CLEAR_FALLBACKS:
                            if (input.length != 0)
                                return CW_MANAGED_STATUS_INVALID_ARGUMENT;
                            font->ClearFallbackFonts();
                            return CW_MANAGED_STATUS_OK;
                        default:
                            return CW_MANAGED_STATUS_INVALID_ARGUMENT;
                        }
                    }
                    case CW_MANAGED_BINDING_MATH_MATRIX_DETERMINANT:
                    case CW_MANAGED_BINDING_MATH_MATRIX_INVERSE:
                    case CW_MANAGED_BINDING_MATH_MATRIX_AFFINE_INVERSE: {
                        glm::mat4 matrix(1.0f);
                        if (!ReadBindingFloats(input, glm::value_ptr(matrix), 16))
                            return CW_MANAGED_STATUS_INVALID_ARGUMENT;
                        if (binding == CW_MANAGED_BINDING_MATH_MATRIX_DETERMINANT)
                            return WriteBindingResult(output, glm::determinant(matrix));
                        const glm::mat4 result =
                          binding == CW_MANAGED_BINDING_MATH_MATRIX_INVERSE ? glm::inverse(matrix) : glm::affineInverse(matrix);
                        return WriteBindingResult(output, glm::value_ptr(result), sizeof(result));
                    }
                    case CW_MANAGED_BINDING_MATH_LOOK_AT: {
                        float value[9]{};
                        if (!ReadBindingFloats(input, value, 9))
                            return CW_MANAGED_STATUS_INVALID_ARGUMENT;
                        const glm::mat4 result = glm::lookAt(glm::vec3(value[0], value[1], value[2]), glm::vec3(value[3], value[4], value[5]),
                                                             glm::vec3(value[6], value[7], value[8]));
                        return WriteBindingResult(output, glm::value_ptr(result), sizeof(result));
                    }
                    default:
                        return CW_MANAGED_STATUS_INVALID_ARGUMENT;
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
                    diagnostic.Severity = severity == 0   ? ManagedDiagnosticSeverity::Info
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
