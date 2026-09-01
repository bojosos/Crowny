#include "cwpch.h"

#include "Crowny/Assets/AssetManager.h"
#include "Crowny/Build/ManagedBuild.h"
#include "Crowny/Common/StringUtils.h"
#include "Crowny/Ecs/Components.h"
#include "Crowny/Scene/SceneManager.h"
#include "Crowny/Scripting/Backends/Mono/MonoBackend.h"
#include "Crowny/Scripting/Backends/Mono/MonoBindingRegistry.h"
#include "Crowny/Scripting/Backends/Mono/MonoScriptRuntime.h"
#include "Crowny/Scripting/Bindings/Scene/ScriptEntityBehaviour.h"
#include "Crowny/Scripting/Bindings/Scene/ScriptSceneManager.h"
#include "Crowny/Scripting/Bindings/ScriptBindings.h"
#include "Crowny/Scripting/Managed/Internal/ManagedBackend.h"
#include "Crowny/Scripting/Managed/Interop/ManagedHostBindings.h"
#include "Crowny/Scripting/Managed/Interop/ManagedJson.h"
#include "Crowny/Scripting/ManagedReload.h"
#include "Crowny/Scripting/Mono/MonoAssembly.h"
#include "Crowny/Scripting/Mono/MonoClass.h"
#include "Crowny/Scripting/Mono/MonoManager.h"
#include "Crowny/Scripting/Mono/MonoMethod.h"
#include "Crowny/Scripting/Mono/MonoUtils.h"
#include "Crowny/Scripting/ScriptAssetManager.h"
#include "Crowny/Scripting/ScriptObjectManager.h"
#include "Crowny/Scripting/ScriptSceneObjectManager.h"

namespace Crowny
{
    namespace
    {
        constexpr uint32_t MONO_DEBUG_PORT = 17615;
        cw_managed_host_api* g_MonoHostApi = nullptr;

        void* CW_MANAGED_CALL GetMonoHostApi() { return g_MonoHostApi; }

        const ManagedProgramArtifact* FindArtifact(const ManagedProgramDefinition& program, ManagedProgramArtifactKind kind)
        {
            const auto artifact = std::find_if(program.Artifacts.begin(), program.Artifacts.end(),
                                               [kind](const ManagedProgramArtifact& value) { return value.Kind == kind; });
            return artifact == program.Artifacts.end() ? nullptr : &*artifact;
        }

        Vector<const ManagedProgramArtifact*> FindArtifacts(const ManagedProgramDefinition& program, ManagedProgramArtifactKind kind)
        {
            Vector<const ManagedProgramArtifact*> artifacts;
            for (const ManagedProgramArtifact& artifact : program.Artifacts)
                if (artifact.Kind == kind)
                    artifacts.push_back(&artifact);
            return artifacts;
        }

        Vector<ManagedProgramArtifact> DiscoverStagedDependencies(const Path& gameAssembly)
        {
            Vector<ManagedProgramArtifact> dependencies;
            const Path directory = gameAssembly.parent_path() / "Dependencies";
            std::error_code error;
            if (!fs::is_directory(directory, error))
                return dependencies;

            Map<String, ManagedProgramArtifact> byIdentity;
            Map<String, Vector<String>> references;
            for (const fs::directory_entry& entry : fs::directory_iterator(directory, error))
            {
                if (error)
                    break;
                String extension = entry.path().extension().string();
                StringUtils::ToLower(extension);
                if (!entry.is_regular_file(error) || extension != ".dll")
                    continue;

                const ManagedAssemblyInspection inspection = InspectManagedAssembly(entry.path());
                if (!inspection.IsPureManaged() || inspection.Identity.Name == CROWNY_ASSEMBLY || inspection.Identity.Name == GAME_ASSEMBLY)
                    continue;

                const String identity = inspection.Identity.ToString();
                if (byIdentity.contains(identity))
                {
                    CW_ENGINE_WARN("Ignoring duplicate staged managed dependency {}.", entry.path().string());
                    continue;
                }
                byIdentity.emplace(identity,
                                   ManagedProgramArtifact{ ManagedProgramArtifactKind::DependencyAssembly, inspection.Identity.Name, entry.path() });
                for (const ManagedAssemblyIdentity& reference : inspection.References)
                    references[identity].push_back(reference.ToString());
            }

            Set<String> emitted;
            Set<String> visiting;
            std::function<void(const String&)> visit = [&](const String& identity) {
                if (emitted.contains(identity) || !byIdentity.contains(identity) || !visiting.insert(identity).second)
                    return;
                for (const String& reference : references[identity])
                    visit(reference);
                visiting.erase(identity);
                emitted.insert(identity);
                dependencies.push_back(byIdentity.at(identity));
            };
            Vector<String> identities;
            identities.reserve(byIdentity.size());
            for (const auto& [identity, dependency] : byIdentity)
                identities.push_back(identity);
            std::sort(identities.begin(), identities.end());
            for (const String& identity : identities)
                visit(identity);
            return dependencies;
        }

        Entity ResolveEntity(const UUID& uuid)
        {
            if (uuid == UUID::EMPTY || SceneManager::TryGet() == nullptr)
                return {};
            const Ref<Scene>& scene = SceneManager::TryGet()->GetActiveScene();
            return scene != nullptr ? scene->TryGetEntityFromUuid(uuid) : Entity{};
        }

        MonoObject* ResolveManagedScriptComponent(UUID* entityId, MonoReflectionType* reflectionType)
        {
            if (entityId == nullptr || reflectionType == nullptr)
                return nullptr;
            const Entity entity = ResolveEntity(*entityId);
            if (!entity || !entity.HasComponent<ManagedScriptComponent>())
                return nullptr;
            ::MonoClass* requestedClass = MonoUtils::GetClass(reflectionType);
            if (requestedClass == nullptr || !ScriptSceneObjectManager::IsStartedUp())
                return nullptr;
            for (ManagedScript& script : entity.GetComponent<ManagedScriptComponent>().Scripts)
            {
                ScriptEntityBehaviour* behaviour = ScriptSceneObjectManager::Get().GetManagedScriptComponent(script.InstanceId);
                MonoObject* candidate = behaviour != nullptr ? behaviour->GetManagedInstance() : nullptr;
                if (candidate != nullptr && MonoUtils::IsSubClassOf(MonoUtils::GetClass(candidate), requestedClass))
                    return candidate;
            }
            return nullptr;
        }

        ManagedScript* FindScript(const UUID& entityId, uint64_t runtimeInstanceId, const ScriptTypeIdentity* identity = nullptr)
        {
            Entity entity = ResolveEntity(entityId);
            if (!entity || !entity.HasComponent<ManagedScriptComponent>())
                return nullptr;
            ManagedScriptComponent& component = entity.GetComponent<ManagedScriptComponent>();
            if (runtimeInstanceId != 0)
                return component.FindScript(runtimeInstanceId);
            const auto script = std::find_if(component.Scripts.begin(), component.Scripts.end(), [&](ManagedScript& candidate) {
                return identity != nullptr && candidate.GetTypeIdentity() == *identity;
            });
            return script == component.Scripts.end() ? nullptr : &*script;
        }

        ManagedOperationResult Failure(String code, String message)
        {
            return ManagedOperationResult::Failure(std::move(code), std::move(message), ManagedBackendId::Mono);
        }

        class MonoBackend final : public ManagedBackend
        {
        public:
            ManagedOperationResult Start(const ManagedScriptingConfig& config) override
            {
                if (config.ExecutionMode != ManagedExecutionMode::Interpreter && config.ExecutionMode != ManagedExecutionMode::Jit)
                    return Failure("managed.mono.execution_mode", "Mono supports interpreter and JIT execution modes.");
                if (g_MonoHostApi != nullptr)
                    return Failure("managed.mono.already_started", "Another Mono managed backend is already running.");
                if (!MonoManager::IsStartedUp())
                {
                    const MonoRuntimePaths paths =
                      config.RuntimeRoot.empty() ? ResolveMonoRuntimePaths(Path(".")) : ResolveMonoRuntimePaths(Vector<Path>{ config.RuntimeRoot });
                    if (!paths.HasRuntime())
                        return Failure("managed.mono.runtime_missing", "Mono requires a complete runtime root.");
                    MonoManager::StartUp(paths.LibraryDirectory, paths.EtcDirectory, config.EnableDebugging ? MONO_DEBUG_PORT : 0);
                    m_OwnsMono = true;
                }
                m_Config = config;
                m_HostApi = {};
                m_CaptureManagedCatalog = nullptr;
                m_CaptureManagedState = nullptr;
                m_PrepareManagedScript = nullptr;
                m_TryApplyManagedState = nullptr;
                m_InvokeManagedButton = nullptr;
                m_HostApi.size = sizeof(m_HostApi);
                m_HostApi.abi_version = CW_MANAGED_ABI_VERSION;
                m_HostApi.context = this;
                PopulateManagedHostBindings(m_HostApi);
                g_MonoHostApi = &m_HostApi;
                m_Started = true;
                return ManagedOperationResult::Success();
            }

            void Shutdown() override
            {
                for (auto& [handle, instance] : m_Instances)
                    DestroyRuntimeInstance(instance);
                m_Instances.clear();
                m_Catalog = {};
                m_CurrentProgram = {};
                m_ProgramLoaded = false;
                ReleaseManagedHostBindings(this);
                if (g_MonoHostApi == &m_HostApi)
                    g_MonoHostApi = nullptr;
                m_HostApi = {};
                m_CaptureManagedCatalog = nullptr;
                m_CaptureManagedState = nullptr;
                m_PrepareManagedScript = nullptr;
                m_TryApplyManagedState = nullptr;
                m_InvokeManagedButton = nullptr;
                if (m_OwnsScriptAssets)
                    ScriptAssetManager::Shutdown();
                if (m_OwnsScriptObjects)
                    ScriptObjectManager::Shutdown();
                if (m_OwnsSceneObjects && ScriptSceneObjectManager::IsStartedUp())
                {
                    ScriptSceneObjectManager::Get().Del();
                    ScriptSceneObjectManager::Shutdown();
                }
                if (m_OwnsMonoBindings)
                    MonoBindingRegistry::Shutdown();
                if (m_OwnsMono)
                    MonoManager::Shutdown();
                m_OwnsScriptAssets = false;
                m_OwnsScriptObjects = false;
                m_OwnsSceneObjects = false;
                m_OwnsMonoBindings = false;
                m_OwnsMono = false;
                m_Started = false;
                m_NextHandle = 1;
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
                    return Failure("managed.mono.not_started", "The Mono adapter is not running.");
                if (m_ProgramLoaded)
                    return Failure("managed.mono.program_already_loaded", "Unload or reload the current Mono program first.");
                EnsureServices();
                ManagedOperationResult loaded = LoadAssemblies(program);
                if (!loaded.Succeeded)
                    return loaded;
                ManagedOperationResult captured = CaptureCatalog(m_Catalog);
                if (!captured.Succeeded)
                    return captured;
                if (ManagedOperationResult valid = ValidateScriptCatalog(m_Catalog, ManagedBackendId::Mono); !valid.Succeeded)
                    return valid;
                m_CurrentProgram = program;
                m_ProgramLoaded = true;
                return ManagedOperationResult::Success();
            }

            ManagedBackendReloadResult ReloadProgram(const ManagedProgramDefinition& program,
                                                     const Vector<ManagedBackendReloadInstance>& snapshots) override
            {
                if (!m_ProgramLoaded)
                    return { Failure("managed.mono.program_not_loaded", "No Mono program is loaded."), {} };
                const ManagedProgramDefinition previous = m_CurrentProgram;
                const AssemblyRefreshResult replacement = RefreshAssemblies(program);
                if (!replacement.Succeeded())
                {
                    ManagedBackendReloadResult failure = MonoBackendDetail::BuildAssemblyRefreshFailure(replacement);
                    if (replacement.Status == AssemblyRefreshStatus::PreviousDomainRestored)
                    {
                        ScriptCatalog restoredCatalog;
                        ManagedOperationResult restored = BindCurrentEngineAssembly();
                        if (restored.Succeeded)
                            restored = CaptureCatalog(restoredCatalog);
                        if (restored.Succeeded)
                            restored = ValidateScriptCatalog(restoredCatalog, ManagedBackendId::Mono);
                        if (restored.Succeeded)
                        {
                            m_Catalog = std::move(restoredCatalog);
                            restored = RestoreSnapshots(snapshots, m_Catalog);
                        }
                        if (!restored.Succeeded)
                            failure = MonoBackendDetail::AddReloadRollbackDiagnostics(std::move(failure.Result), true, restored);
                    }
                    if (failure.ProgramInvalidated)
                        InvalidateProgram();
                    return failure;
                }
                ScriptCatalog replacementCatalog;
                ManagedOperationResult valid = BindCurrentEngineAssembly();
                if (valid.Succeeded)
                    valid = CaptureCatalog(replacementCatalog);
                if (valid.Succeeded)
                    valid = ValidateScriptCatalog(replacementCatalog, ManagedBackendId::Mono);
                if (valid.Succeeded)
                    valid = RestoreSnapshots(snapshots, replacementCatalog);
                if (!valid.Succeeded)
                {
                    const bool assembliesRestored = RefreshAssemblies(previous).Succeeded();
                    if (!assembliesRestored)
                    {
                        ManagedBackendReloadResult failure =
                          MonoBackendDetail::AddReloadRollbackDiagnostics(std::move(valid), false, ManagedOperationResult::Success());
                        if (failure.ProgramInvalidated)
                            InvalidateProgram();
                        return failure;
                    }
                    ScriptCatalog restoredCatalog;
                    ManagedOperationResult stateRestoration = BindCurrentEngineAssembly();
                    if (stateRestoration.Succeeded)
                        stateRestoration = CaptureCatalog(restoredCatalog);
                    if (stateRestoration.Succeeded)
                        stateRestoration = ValidateScriptCatalog(restoredCatalog, ManagedBackendId::Mono);
                    if (stateRestoration.Succeeded)
                    {
                        m_Catalog = std::move(restoredCatalog);
                        stateRestoration = RestoreSnapshots(snapshots, m_Catalog);
                    }
                    if (!stateRestoration.Succeeded)
                    {
                        ManagedBackendReloadResult failure =
                          MonoBackendDetail::AddReloadRollbackDiagnostics(std::move(valid), true, stateRestoration);
                        if (failure.ProgramInvalidated)
                            InvalidateProgram();
                        return failure;
                    }
                    return { std::move(valid), {} };
                }
                m_Catalog = std::move(replacementCatalog);
                m_CurrentProgram = program;
                Vector<uint64_t> handles;
                handles.reserve(snapshots.size());
                for (const ManagedBackendReloadInstance& snapshot : snapshots)
                    handles.push_back(snapshot.PreviousHandle);
                return { ManagedOperationResult::Success(), std::move(handles) };
            }

            const ScriptCatalog& GetScriptCatalog() const override { return m_Catalog; }

            ManagedBackendCreateResult CreateScript(const ScriptCreateRequest& request) override
            {
                if (!m_ProgramLoaded)
                    return { Failure("managed.mono.program_not_loaded", "No Mono program is loaded."), 0 };
                const ScriptTypeSchema* schema = m_Catalog.FindType(request.Identity);
                if (schema == nullptr)
                    return { Failure("managed.script.type_missing", "The Mono script type is not in the catalog."), 0 };
                Entity entity = ResolveEntity(request.Entity);
                if (!entity)
                    return { Failure("managed.mono.entity_missing", "The script entity is no longer in the active scene."), 0 };
                bool componentAdded = false;
                bool occurrenceAdded = false;
                bool createAttempted = false;
                ManagedScript* script = FindScript(request.Entity, request.RuntimeInstanceId, &request.Identity);
                if (script == nullptr && request.RuntimeInstanceId == 0)
                {
                    componentAdded = !entity.HasComponent<ManagedScriptComponent>();
                    ManagedScriptComponent& component =
                      componentAdded ? entity.AddComponent<ManagedScriptComponent>() : entity.GetComponent<ManagedScriptComponent>();
                    component.Scripts.emplace_back(request.Identity);
                    script = &component.Scripts.back();
                    occurrenceAdded = true;
                }
                if (script == nullptr)
                    return { Failure("managed.mono.script_occurrence_missing", "The persisted Mono script occurrence was not found."), 0 };
                const uint64_t runtimeInstanceId = script->InstanceId;
                const auto findCurrentScript = [&]() { return FindScript(request.Entity, runtimeInstanceId, &request.Identity); };
                const auto rollbackCreate = [&]() {
                    if (!occurrenceAdded && createAttempted && ScriptSceneObjectManager::IsStartedUp())
                    {
                        if (ManagedScript* current = findCurrentScript())
                            ScriptSceneObjectManager::Get().DestroyManagedScriptComponent(entity, current);
                    }
                    MonoBackendDetail::RollbackAddedScriptOccurrence(entity, runtimeInstanceId, occurrenceAdded, componentAdded);
                };
                if (m_NextHandle == 0)
                {
                    rollbackCreate();
                    return { Failure("managed.mono.handle_exhausted", "Mono script handles are exhausted."), 0 };
                }
                if (std::any_of(m_Instances.begin(), m_Instances.end(), [&](const auto& entry) {
                        return entry.second.Entity == request.Entity && entry.second.RuntimeInstanceId == runtimeInstanceId;
                    }))
                {
                    rollbackCreate();
                    return { Failure("managed.mono.script_occurrence_active", "The persisted Mono script occurrence is already active."), 0 };
                }
                const ScriptTypeIdentity identity = script->GetTypeIdentity();
                MonoClass* scriptClass = MonoManager::Get().FindClass(identity.Assembly, identity.Namespace, identity.TypeName);
                if (scriptClass == nullptr)
                {
                    rollbackCreate();
                    return { Failure("managed.mono.script_class_missing", "Mono could not resolve the managed script class."), 0 };
                }
                createAttempted = true;
                MonoObject* managedInstance = scriptClass->CreateInstance(true);
                script = findCurrentScript();
                if (script == nullptr)
                {
                    rollbackCreate();
                    return { Failure("managed.mono.create_reentered", "The Mono script was removed while its constructor was running."), 0 };
                }
                ScriptEntityBehaviour* behaviour = ScriptSceneObjectManager::Get().CreateManagedScriptComponent(managedInstance, entity, *script);
                if (behaviour == nullptr)
                {
                    rollbackCreate();
                    return { Failure("managed.mono.create_failed", "Mono could not create the managed script bridge."), 0 };
                }
                if (m_PrepareManagedScript == nullptr)
                {
                    rollbackCreate();
                    return { Failure("managed.mono.lifecycle_bridge_missing", "CrownySharp does not expose the shared script lifecycle."), 0 };
                }
                Instance instance;
                instance.Entity = request.Entity;
                instance.RuntimeInstanceId = runtimeInstanceId;
                instance.Identity = identity;
                if (!instance.Runtime.Bind(behaviour->GetManagedInstance(), scriptClass))
                {
                    rollbackCreate();
                    return { Failure("managed.mono.runtime_bind_failed", "Mono could not bind the managed script callbacks."), 0 };
                }
                void* preparationParameters[1] = { instance.Runtime.GetInstance() };
                MonoString* preparationError = reinterpret_cast<MonoString*>(m_PrepareManagedScript->Invoke(nullptr, preparationParameters));
                if (preparationError != nullptr)
                {
                    const String message = MonoUtils::FromMonoString(preparationError);
                    rollbackCreate();
                    return { Failure("managed.mono.require_component_failed", message), 0 };
                }
                if (request.InitialState.Identity.IsValid())
                {
                    ScriptStateResult normalized = NormalizeScriptState(request.InitialState, *schema, ManagedBackendId::Mono);
                    if (!normalized.Result.Succeeded)
                    {
                        rollbackCreate();
                        return { std::move(normalized.Result), 0 };
                    }
                    ManagedOperationResult applied = ApplyState(instance, normalized.State);
                    if (!applied.Succeeded)
                    {
                        rollbackCreate();
                        return { std::move(applied), 0 };
                    }
                }
                const uint64_t handle = m_NextHandle++;
                m_Instances.emplace(handle, std::move(instance));
                return { ManagedOperationResult::Success(), handle };
            }

            ManagedOperationResult DestroyScript(uint64_t handle) override
            {
                const auto instance = m_Instances.find(handle);
                if (instance == m_Instances.end())
                    return StaleHandle();
                DestroyRuntimeInstance(instance->second);
                m_Instances.erase(instance);
                return ManagedOperationResult::Success();
            }

            ManagedOperationResult Dispatch(uint64_t handle, const ScriptEvent& event) override
            {
                const auto instance = m_Instances.find(handle);
                if (instance == m_Instances.end())
                    return StaleHandle();
                if (FindScript(instance->second.Entity, instance->second.RuntimeInstanceId) == nullptr ||
                    instance->second.Runtime.GetInstance() == nullptr)
                    return StaleHandle();
                Entity self = ResolveEntity(instance->second.Entity);
                Entity other = ResolveEntity(event.OtherEntity);
                instance->second.Runtime.Dispatch(self, other, event);
                return ManagedOperationResult::Success();
            }

            ManagedBackendStateResult CaptureState(uint64_t handle) override
            {
                const auto instance = m_Instances.find(handle);
                if (instance == m_Instances.end())
                    return { StaleHandle(), {} };
                ManagedScript* script = FindScript(instance->second.Entity, instance->second.RuntimeInstanceId);
                MonoObject* managedInstance = instance->second.Runtime.GetInstance();
                if (script == nullptr || managedInstance == nullptr)
                    return { StaleHandle(), {} };
                if (m_CaptureManagedState == nullptr)
                    return { Failure("managed.mono.state_bridge_missing", "CrownySharp does not expose the shared managed state codec."), {} };
                void* parameters[1] = { managedInstance };
                MonoString* encoded = reinterpret_cast<MonoString*>(m_CaptureManagedState->Invoke(nullptr, parameters));
                if (encoded == nullptr)
                    return { Failure("managed.mono.state_capture_failed", "The shared managed state codec could not capture the Mono script."), {} };
                ScriptState state;
                const ScriptTypeSchema* schema = m_Catalog.FindType(script->GetTypeIdentity());
                ManagedOperationResult parsed = ParseManagedStateJson(MonoUtils::FromMonoString(encoded), state, ManagedBackendId::Mono, schema);
                if (!parsed.Succeeded)
                    return { std::move(parsed), {} };
                return { ManagedOperationResult::Success(), std::move(state) };
            }

            ManagedOperationResult ApplyState(uint64_t handle, const ScriptState& state) override
            {
                const auto instance = m_Instances.find(handle);
                if (instance == m_Instances.end())
                    return StaleHandle();
                if (FindScript(instance->second.Entity, instance->second.RuntimeInstanceId) == nullptr)
                    return StaleHandle();
                const ScriptTypeSchema* schema = m_Catalog.FindType(instance->second.Identity);
                if (schema == nullptr)
                    return Failure("managed.mono.type_missing", "The live script type is no longer in the catalog.");
                ScriptStateResult normalized = NormalizeScriptState(state, *schema, ManagedBackendId::Mono);
                if (!normalized.Result.Succeeded)
                    return normalized.Result;
                return ApplyState(instance->second, normalized.State);
            }

            ScriptInvocationResult InvokeButton(uint64_t handle, uint64_t methodId,
                                                const Vector<ScriptValue>& arguments) override
            {
                const auto instance = m_Instances.find(handle);
                if (instance == m_Instances.end() || instance->second.Runtime.GetInstance() == nullptr)
                    return { StaleHandle(), false, {} };
                if (m_InvokeManagedButton == nullptr)
                    return { Failure("managed.mono.button_bridge_missing",
                                     "CrownySharp does not expose the shared inspector button invoker."), false, {} };
                MonoString* encodedArguments = MonoUtils::ToMonoString(WriteManagedArgumentsJson(arguments));
                void* parameters[3] = { instance->second.Runtime.GetInstance(), &methodId, encodedArguments };
                MonoString* encodedResult = reinterpret_cast<MonoString*>(m_InvokeManagedButton->Invoke(nullptr, parameters));
                if (encodedResult == nullptr)
                    return { Failure("managed.mono.button_failed", "The shared inspector button invocation failed."), false, {} };
                return ParseManagedInvocationResultJson(MonoUtils::FromMonoString(encodedResult), ManagedBackendId::Mono);
            }

            Vector<ManagedDiagnostic> Update() override
            {
                if (ScriptObjectManager::IsStartedUp())
                    ScriptObjectManager::Get().Update();
                if (ScriptSceneObjectManager::IsStartedUp())
                    ScriptSceneManager::DispatchPendingEvents();
                return {};
            }

            void NotifyEntityDestroyed(const Entity& entity) override
            {
                if (ScriptSceneObjectManager::IsStartedUp())
                    ScriptSceneObjectManager::Get().NotifyEntityDestroyed(entity);
            }

            void NotifyComponentDestroyed(uint64_t instanceId) override
            {
                if (ScriptSceneObjectManager::IsStartedUp())
                    ScriptSceneObjectManager::Get().NotifyComponentDestroyed(instanceId);
            }

            void NotifySceneDestroyed(const Scene* scene) override
            {
                if (ScriptSceneObjectManager::IsStartedUp())
                    ScriptSceneObjectManager::Get().DestroySceneObjects(scene);
            }

            void NotifySceneEventsAvailable() override { ScriptSceneManager::DispatchPendingEvents(); }

        private:
            struct Instance
            {
                UUID Entity;
                uint64_t RuntimeInstanceId = 0;
                ScriptTypeIdentity Identity;
                MonoScriptRuntime Runtime;
            };

            template <typename T> static void StartModule(bool& owned)
            {
                if (!T::IsStartedUp())
                {
                    T::StartUp();
                    owned = true;
                }
            }

            void EnsureServices()
            {
                ScriptBindings::Register();
                StartModule<MonoBindingRegistry>(m_OwnsMonoBindings);
                StartModule<ScriptSceneObjectManager>(m_OwnsSceneObjects);
                StartModule<ScriptObjectManager>(m_OwnsScriptObjects);
                StartModule<ScriptAssetManager>(m_OwnsScriptAssets);
            }

            ManagedOperationResult LoadAssemblies(const ManagedProgramDefinition& program)
            {
                const ManagedProgramArtifact* engine = FindArtifact(program, ManagedProgramArtifactKind::EngineAssembly);
                const ManagedProgramArtifact* game = FindArtifact(program, ManagedProgramArtifactKind::GameAssembly);
                if (engine == nullptr || !fs::is_regular_file(engine->Filepath))
                    return Failure("managed.mono.engine_assembly_missing", "Mono requires the CrownySharp engine assembly.");
                try
                {
                    MonoAssembly* loadedEngine = MonoManager::Get().GetAssembly(CROWNY_ASSEMBLY);
                    if (loadedEngine == nullptr || !loadedEngine->IsLoaded())
                        loadedEngine = &MonoManager::Get().LoadAssembly(engine->Filepath, CROWNY_ASSEMBLY);
                    ManagedOperationResult bound = BindEngineAssembly(*loadedEngine);
                    if (!bound.Succeeded)
                        return bound;
                    MonoBindingRegistry::Get().LoadAssembly(CROWNY_ASSEMBLY);
                    Vector<const ManagedProgramArtifact*> dependencyArtifacts =
                      FindArtifacts(program, ManagedProgramArtifactKind::DependencyAssembly);
                    Vector<ManagedProgramArtifact> discoveredDependencies;
                    if (dependencyArtifacts.empty() && game != nullptr && fs::is_regular_file(game->Filepath))
                    {
                        discoveredDependencies = DiscoverStagedDependencies(game->Filepath);
                        for (const ManagedProgramArtifact& dependency : discoveredDependencies)
                            dependencyArtifacts.push_back(&dependency);
                    }
                    for (const ManagedProgramArtifact* dependency : dependencyArtifacts)
                    {
                        if (dependency == nullptr || dependency->LogicalName.empty() || !fs::is_regular_file(dependency->Filepath))
                            return Failure("managed.mono.dependency_assembly_missing",
                                           "Mono requires every staged managed dependency to be present before the game assembly is loaded.");
                        MonoAssembly* loadedDependency = MonoManager::Get().GetAssembly(dependency->LogicalName);
                        if (loadedDependency == nullptr || !loadedDependency->IsLoaded())
                            loadedDependency = &MonoManager::Get().LoadAssembly(dependency->Filepath, dependency->LogicalName);
                        if (loadedDependency == nullptr || !loadedDependency->IsLoaded())
                            return Failure("managed.mono.dependency_assembly_load_failed",
                                           "Mono could not load a staged managed dependency before the game assembly.");
                    }
                    if (game != nullptr && fs::is_regular_file(game->Filepath))
                    {
                        MonoAssembly* loadedGame = MonoManager::Get().GetAssembly(GAME_ASSEMBLY);
                        if (loadedGame == nullptr || !loadedGame->IsLoaded())
                            MonoManager::Get().LoadAssembly(game->Filepath, GAME_ASSEMBLY);
                        MonoBindingRegistry::Get().LoadAssembly(GAME_ASSEMBLY);
                    }
                }
                catch (const std::exception& error)
                {
                    return Failure("managed.mono.assembly_load_failed", String("Mono could not load the managed program: ") + error.what());
                }
                return ManagedOperationResult::Success();
            }

            Vector<AssemblyRefreshInfo> GetRefreshAssemblies(const ManagedProgramDefinition& program) const
            {
                Vector<AssemblyRefreshInfo> assemblies;
                if (const ManagedProgramArtifact* engine = FindArtifact(program, ManagedProgramArtifactKind::EngineAssembly))
                    assemblies.emplace_back(CROWNY_ASSEMBLY, engine->Filepath);
                Vector<const ManagedProgramArtifact*> dependencyArtifacts = FindArtifacts(program, ManagedProgramArtifactKind::DependencyAssembly);
                Vector<ManagedProgramArtifact> discoveredDependencies;
                if (dependencyArtifacts.empty())
                {
                    if (const ManagedProgramArtifact* game = FindArtifact(program, ManagedProgramArtifactKind::GameAssembly))
                    {
                        discoveredDependencies = DiscoverStagedDependencies(game->Filepath);
                        for (const ManagedProgramArtifact& dependency : discoveredDependencies)
                            dependencyArtifacts.push_back(&dependency);
                    }
                }
                for (const ManagedProgramArtifact* dependency : dependencyArtifacts)
                    if (dependency != nullptr && !dependency->LogicalName.empty())
                        assemblies.emplace_back(dependency->LogicalName, dependency->Filepath);
                if (const ManagedProgramArtifact* game = FindArtifact(program, ManagedProgramArtifactKind::GameAssembly))
                    assemblies.emplace_back(GAME_ASSEMBLY, game->Filepath);
                return assemblies;
            }

            AssemblyRefreshResult RefreshAssemblies(const ManagedProgramDefinition& program)
            {
                const Vector<AssemblyRefreshInfo> assemblies = GetRefreshAssemblies(program);
                if (assemblies.empty())
                    return { AssemblyRefreshStatus::CurrentDomainKept };
                return ScriptObjectManager::Get().RefreshAssemblies(assemblies);
            }

            ManagedOperationResult BindCurrentEngineAssembly()
            {
                MonoAssembly* engine = MonoManager::Get().GetAssembly(CROWNY_ASSEMBLY);
                if (engine == nullptr || !engine->IsLoaded())
                    return Failure("managed.mono.engine_assembly_missing", "Mono did not load the CrownySharp engine assembly.");
                return BindEngineAssembly(*engine);
            }

            ManagedOperationResult RestoreSnapshots(const Vector<ManagedBackendReloadInstance>& snapshots, const ScriptCatalog& catalog)
            {
                for (const ManagedBackendReloadInstance& snapshot : snapshots)
                {
                    const auto instance = m_Instances.find(snapshot.PreviousHandle);
                    if (instance == m_Instances.end())
                        return StaleHandle();
                    const ScriptTypeSchema* schema = catalog.FindType(snapshot.State.Identity);
                    if (schema == nullptr)
                        return Failure("managed.mono.reload_type_missing", "A live Mono script type is missing from the replacement program.");
                    ScriptStateResult normalized = NormalizeScriptState(snapshot.State, *schema, ManagedBackendId::Mono);
                    if (!normalized.Result.Succeeded)
                        return normalized.Result;
                    ManagedScript* script = FindScript(instance->second.Entity, instance->second.RuntimeInstanceId);
                    if (script == nullptr)
                        return StaleHandle();
                    const ScriptTypeIdentity& identity = script->GetTypeIdentity();
                    MonoClass* scriptClass = MonoManager::Get().FindClass(identity.Assembly, identity.Namespace, identity.TypeName);
                    ScriptEntityBehaviour* behaviour = ScriptSceneObjectManager::Get().GetManagedScriptComponent(instance->second.RuntimeInstanceId);
                    if (scriptClass == nullptr || behaviour == nullptr ||
                        !instance->second.Runtime.Bind(behaviour->GetManagedInstance(), scriptClass))
                        return Failure("managed.mono.reload_bind_failed", "Mono could not rebind a managed script after assembly reload.");
                    instance->second.Identity = identity;
                    ManagedOperationResult applied = ApplyState(instance->second, normalized.State);
                    if (!applied.Succeeded)
                        return applied;
                }
                return ManagedOperationResult::Success();
            }

            ManagedOperationResult ApplyState(const Instance& instance, const ScriptState& state)
            {
                if (state.Identity != instance.Identity)
                    return Failure("managed.mono.state_identity_mismatch", "The script state identity does not match the Mono script type.");
                if (instance.Runtime.GetInstance() == nullptr)
                    return Failure("managed.mono.state_instance_missing", "The Mono script instance is no longer live.");
                if (m_TryApplyManagedState == nullptr)
                    return Failure("managed.mono.state_bridge_missing", "CrownySharp does not expose the shared managed state codec.");
                MonoString* encoded = MonoUtils::ToMonoString(WriteManagedStateJson(state));
                void* parameters[2] = { instance.Runtime.GetInstance(), encoded };
                MonoString* error = reinterpret_cast<MonoString*>(m_TryApplyManagedState->Invoke(nullptr, parameters));
                if (error != nullptr)
                    return Failure("managed.mono.state_apply_failed", MonoUtils::FromMonoString(error));
                return ManagedOperationResult::Success();
            }

            ManagedOperationResult CaptureCatalog(ScriptCatalog& catalog)
            {
                MonoAssembly* game = MonoManager::Get().GetAssembly(GAME_ASSEMBLY);
                if (game == nullptr || !game->IsLoaded())
                {
                    catalog = {};
                    catalog.ManifestVersion = MANAGED_CATALOG_VERSION;
                    return ManagedOperationResult::Success();
                }
                if (m_CaptureManagedCatalog == nullptr)
                    return Failure("managed.mono.catalog_bridge_missing", "CrownySharp does not expose the shared script catalog codec.");
                MonoString* assemblyName = MonoUtils::ToMonoString(GAME_ASSEMBLY);
                void* parameters[1] = { assemblyName };
                MonoString* encoded = reinterpret_cast<MonoString*>(m_CaptureManagedCatalog->Invoke(nullptr, parameters));
                if (encoded == nullptr)
                    return Failure("managed.mono.catalog_capture_failed", "The shared script catalog codec could not inspect the Mono program.");
                return ParseManagedCatalogJson(MonoUtils::FromMonoString(encoded), catalog, ManagedBackendId::Mono);
            }

            ManagedOperationResult BindEngineAssembly(MonoAssembly& engine)
            {
                MonoClass* runtimeContext = engine.GetClass(CROWNY_NS, "ManagedRuntimeContext");
                if (runtimeContext == nullptr)
                    return Failure("managed.mono.host_api_missing", "CrownySharp does not contain the managed runtime context.");
                runtimeContext->AddInternalCall("Internal_GetNativeHostApi", reinterpret_cast<const void*>(&GetMonoHostApi));
                MonoClass* runtimeAdapter = engine.GetClass(CROWNY_NS, "ManagedRuntimeAdapter");
                if (runtimeAdapter == nullptr)
                    return Failure("managed.mono.runtime_adapter_missing", "CrownySharp does not contain the Mono runtime adapter.");
                runtimeAdapter->AddInternalCall("Internal_ResolveScriptComponent", reinterpret_cast<const void*>(&ResolveManagedScriptComponent));

                MonoClass* stateCodec = engine.GetClass(CROWNY_NS, "ManagedStateCodec");
                if (stateCodec == nullptr)
                    return Failure("managed.mono.state_bridge_missing", "CrownySharp does not contain the shared managed state codec.");
                m_CaptureManagedState = stateCodec->GetMethod("Capture", 1);
                m_TryApplyManagedState = stateCodec->GetMethod("TryApply", 2);
                if (m_CaptureManagedState == nullptr || m_TryApplyManagedState == nullptr)
                    return Failure("managed.mono.state_bridge_invalid", "CrownySharp's shared managed state codec has an incompatible surface.");

                MonoClass* catalogCodec = engine.GetClass(CROWNY_NS, "ManagedScriptCatalog");
                if (catalogCodec == nullptr)
                    return Failure("managed.mono.catalog_bridge_missing", "CrownySharp does not contain the shared script catalog codec.");
                m_CaptureManagedCatalog = catalogCodec->GetMethod("CaptureLoadedAssembly", 1);
                if (m_CaptureManagedCatalog == nullptr)
                    return Failure("managed.mono.catalog_bridge_invalid", "CrownySharp's shared script catalog codec has an incompatible surface.");

                MonoClass* lifecycle = engine.GetClass(CROWNY_NS, "ManagedScriptLifecycle");
                if (lifecycle == nullptr)
                    return Failure("managed.mono.lifecycle_bridge_missing", "CrownySharp does not contain the shared script lifecycle.");
                m_PrepareManagedScript = lifecycle->GetMethod("TryPrepare", 1);
                if (m_PrepareManagedScript == nullptr)
                    return Failure("managed.mono.lifecycle_bridge_invalid", "CrownySharp's shared script lifecycle has an incompatible surface.");

                MonoClass* buttonInvoker = engine.GetClass(CROWNY_NS, "ManagedButtonInvoker");
                if (buttonInvoker == nullptr)
                    return Failure("managed.mono.button_bridge_missing", "CrownySharp does not contain the shared inspector button invoker.");
                m_InvokeManagedButton = buttonInvoker->GetMethod("Invoke", 3);
                if (m_InvokeManagedButton == nullptr)
                    return Failure("managed.mono.button_bridge_invalid", "CrownySharp's inspector button invoker has an incompatible surface.");
                return ManagedOperationResult::Success();
            }

            void DestroyRuntimeInstance(Instance& instance)
            {
                Entity entity = ResolveEntity(instance.Entity);
                ManagedScript* script = FindScript(instance.Entity, instance.RuntimeInstanceId);
                if (entity && script != nullptr)
                {
                    if (ScriptSceneObjectManager::IsStartedUp())
                        ScriptSceneObjectManager::Get().DestroyManagedScriptComponent(entity, script);
                    script->ClearRuntimeHandle();
                }
                instance.Runtime.Clear();
            }

            void InvalidateProgram()
            {
                for (auto& [handle, instance] : m_Instances)
                    DestroyRuntimeInstance(instance);
                m_Instances.clear();
                m_Catalog = {};
                m_CurrentProgram = {};
                m_ProgramLoaded = false;
            }

            ManagedOperationResult StaleHandle() const { return Failure("managed.mono.stale_handle", "The Mono script handle is stale."); }

            ManagedScriptingConfig m_Config;
            ManagedProgramDefinition m_CurrentProgram;
            ScriptCatalog m_Catalog;
            cw_managed_host_api m_HostApi{};
            Map<uint64_t, Instance> m_Instances;
            MonoMethod* m_CaptureManagedCatalog = nullptr;
            MonoMethod* m_CaptureManagedState = nullptr;
            MonoMethod* m_PrepareManagedScript = nullptr;
            MonoMethod* m_TryApplyManagedState = nullptr;
            MonoMethod* m_InvokeManagedButton = nullptr;
            uint64_t m_NextHandle = 1;
            bool m_Started = false;
            bool m_ProgramLoaded = false;
            bool m_OwnsMono = false;
            bool m_OwnsMonoBindings = false;
            bool m_OwnsSceneObjects = false;
            bool m_OwnsScriptObjects = false;
            bool m_OwnsScriptAssets = false;
        };
    } // namespace

    void MonoBackendDetail::RollbackAddedScriptOccurrence(Entity entity, uint64_t runtimeInstanceId, bool occurrenceAdded, bool componentAdded)
    {
        if (!occurrenceAdded || !entity || !entity.HasComponent<ManagedScriptComponent>())
            return;

        ManagedScriptComponent& component = entity.GetComponent<ManagedScriptComponent>();
        const auto script = std::find_if(component.Scripts.begin(), component.Scripts.end(),
                                         [runtimeInstanceId](const ManagedScript& candidate) { return candidate.InstanceId == runtimeInstanceId; });
        if (script != component.Scripts.end())
        {
            if (ScriptSceneObjectManager::IsStartedUp())
                ScriptSceneObjectManager::Get().DestroyManagedScriptComponent(entity, &*script);
            component.Scripts.erase(script);
        }
        if (componentAdded && component.Scripts.empty())
            entity.RemoveComponent<ManagedScriptComponent>();
    }

    ManagedBackendReloadResult MonoBackendDetail::BuildAssemblyRefreshFailure(const AssemblyRefreshResult& refresh)
    {
        switch (refresh.Status)
        {
        case AssemblyRefreshStatus::CurrentDomainKept:
            return { ManagedOperationResult::Failure("managed.mono.reload_failed",
                                                     "Mono rejected the replacement assemblies before unloading the current domain.",
                                                     ManagedBackendId::Mono),
                     {},
                     false };
        case AssemblyRefreshStatus::PreviousDomainRestored:
            return { ManagedOperationResult::Failure("managed.mono.reload_failed",
                                                     "Mono could not load the replacement assemblies; the previous domain was restored.",
                                                     ManagedBackendId::Mono),
                     {},
                     false };
        case AssemblyRefreshStatus::PreviousDomainRestoreFailed:
            return AddReloadRollbackDiagnostics(
              ManagedOperationResult::Failure("managed.mono.reload_failed",
                                              "Mono could not load the replacement assemblies or restore the previous domain.",
                                              ManagedBackendId::Mono),
              false, ManagedOperationResult::Success());
        case AssemblyRefreshStatus::ReplacementLoaded:
            break;
        }
        return { ManagedOperationResult::Failure("managed.mono.reload_failed", "Mono reload failed unexpectedly.", ManagedBackendId::Mono),
                 {},
                 false };
    }

    ManagedBackendReloadResult MonoBackendDetail::AddReloadRollbackDiagnostics(ManagedOperationResult failure, bool assembliesRestored,
                                                                               const ManagedOperationResult& stateRestoration)
    {
        failure.Succeeded = false;
        const bool programInvalidated = !assembliesRestored || !stateRestoration.Succeeded;
        if (!assembliesRestored)
        {
            failure.Diagnostics.push_back({ ManagedDiagnosticSeverity::Error,
                                            "managed.mono.reload_rollback_failed",
                                            "Mono could not restore the last working assemblies; the managed program was invalidated.",
                                            {},
                                            ManagedBackendId::Mono,
                                            {},
                                            {} });
            return { std::move(failure), {}, programInvalidated };
        }
        if (!stateRestoration.Succeeded)
        {
            failure.Diagnostics.insert(failure.Diagnostics.end(), stateRestoration.Diagnostics.begin(), stateRestoration.Diagnostics.end());
            failure.Diagnostics.push_back(
              { ManagedDiagnosticSeverity::Error,
                "managed.mono.reload_state_rollback_failed",
                "Mono restored the last working assemblies but not all live script state; the managed program was invalidated.",
                {},
                ManagedBackendId::Mono,
                {},
                {} });
        }
        return { std::move(failure), {}, programInvalidated };
    }

    Scope<ManagedBackend> CreateMonoBackend() { return CreateScope<MonoBackend>(); }
} // namespace Crowny
