#include "cwpch.h"

#include "Crowny/Application/Application.h"
#include "Crowny/Application/EngineRuntime.h"
#include "Crowny/Ecs/Components.h"
#include "Crowny/Scene/SceneManager.h"
#include "Crowny/Scene/ScriptRuntime.h"
#include "Crowny/Scripting/Managed/ManagedProgramPackage.h"
#include "Crowny/Scripting/Managed/ManagedScripting.h"

namespace Crowny
{
    namespace
    {
        struct ScriptInvocation
        {
            UUID EntityId;
            uint64_t ScriptId = 0;
        };

        ManagedScripting* GetManagedScripting()
        {
            Application* application = Application::TryGet();
            return application != nullptr ? application->GetRuntime().GetManagedScripting() : nullptr;
        }

        void LogDiagnostics(const ManagedOperationResult& result, const ScriptTypeIdentity& identity, const UUID& entity)
        {
            for (const ManagedDiagnostic& diagnostic : result.Diagnostics)
            {
                const String& scriptName = identity.TypeName;
                if (diagnostic.Severity == ManagedDiagnosticSeverity::Error)
                    CW_ENGINE_ERROR("Managed script '{}:{}' on entity {} [{}]: {}", identity.Assembly, scriptName, entity.ToString(),
                                    diagnostic.Code, diagnostic.Message);
                else if (diagnostic.Severity == ManagedDiagnosticSeverity::Warning)
                    CW_ENGINE_WARN("Managed script '{}:{}' on entity {} [{}]: {}", identity.Assembly, scriptName, entity.ToString(),
                                   diagnostic.Code, diagnostic.Message);
                else
                    CW_ENGINE_INFO("Managed script '{}:{}' on entity {} [{}]: {}", identity.Assembly, scriptName, entity.ToString(),
                                   diagnostic.Code, diagnostic.Message);
            }
        }

        void LogDiagnostics(const Vector<ManagedDiagnostic>& diagnostics)
        {
            for (const ManagedDiagnostic& diagnostic : diagnostics)
            {
                if (diagnostic.Severity == ManagedDiagnosticSeverity::Error)
                    CW_ENGINE_ERROR("Managed scripting [{}]: {}", diagnostic.Code, diagnostic.Message);
                else if (diagnostic.Severity == ManagedDiagnosticSeverity::Warning)
                    CW_ENGINE_WARN("Managed scripting [{}]: {}", diagnostic.Code, diagnostic.Message);
                else
                    CW_ENGINE_INFO("Managed scripting [{}]: {}", diagnostic.Code, diagnostic.Message);
            }
        }

        Vector<ScriptInvocation>& CollectScriptInvocations(const Ref<Scene>& scene)
        {
            static Vector<ScriptInvocation> invocations;
            invocations.clear();
            auto view = scene->GetAllEntitiesWith<ManagedScriptComponent>();
            size_t scriptCount = 0;
            view.each([&scriptCount](const ManagedScriptComponent& component) { scriptCount += component.Scripts.size(); });
            invocations.reserve(scriptCount);
            view.each([&](entt::entity handle, const ManagedScriptComponent& component) {
                const Entity entity(handle, scene.get());
                for (const ManagedScript& script : component.Scripts)
                    invocations.push_back({ entity.GetUuid(), script.InstanceId });
            });
            return invocations;
        }

        ManagedScript* FindScript(const Ref<Scene>& scene, const ScriptInvocation& invocation, Entity& entity)
        {
            entity = scene->TryGetEntityFromUuid(invocation.EntityId);
            if (!entity || !entity.HasComponent<ManagedScriptComponent>())
                return nullptr;
            return entity.GetComponent<ManagedScriptComponent>().FindScript(invocation.ScriptId);
        }

        ManagedScript* FindScript(Entity entity, uint64_t runtimeInstanceId)
        {
            if (!entity || !entity.HasComponent<ManagedScriptComponent>())
                return nullptr;
            return entity.GetComponent<ManagedScriptComponent>().FindScript(runtimeInstanceId);
        }

    } // namespace

    void ScriptRuntime::Init() {}

    bool ScriptRuntime::CreateScript(Entity entity, ManagedScript& script, bool dispatchStart)
    {
        ManagedScripting* managed = GetManagedScripting();
        if (managed == nullptr || !managed->IsStarted() || script.GetRuntimeHandle().IsValid())
            return script.GetRuntimeHandle().IsValid();

        ScriptCreateRequest request;
        request.Identity = script.GetTypeIdentity();
        request.Entity = entity.GetUuid();
        request.RuntimeInstanceId = script.InstanceId;
        request.InitialState = CaptureState(script);
        ScriptCreateResult created = managed->CreateScript(request);
        if (!created.Result.Succeeded)
        {
            LogDiagnostics(created.Result, request.Identity, request.Entity);
            return false;
        }

        // Managed construction may append to the script vector. Re-resolve the
        // occurrence before storing its handle or dispatching lifecycle events.
        ManagedScript* current = FindScript(entity, request.RuntimeInstanceId);
        if (current == nullptr)
        {
            ManagedOperationResult destroyed = managed->DestroyScript(created.Handle);
            if (!destroyed.Succeeded)
                LogDiagnostics(destroyed, request.Identity, request.Entity);
            CW_ENGINE_ERROR("Managed script '{}:{}' on entity {} disappeared during construction.", request.Identity.Assembly,
                            request.Identity.TypeName, request.Entity.ToString());
            return false;
        }

        current->SetRuntimeHandle(created.Handle);
        if (dispatchStart)
            Dispatch(*current, ScriptEvent::Lifecycle(ScriptEventKind::Start));
        return true;
    }

    void ScriptRuntime::DestroyScript(Entity entity, ManagedScript& script, bool dispatchDestroy)
    {
        const ScriptInstanceHandle handle = script.GetRuntimeHandle();
        if (!handle.IsValid())
            return;
        ManagedScripting* managed = GetManagedScripting();
        if (managed != nullptr && managed->IsStarted())
        {
            if (dispatchDestroy)
            {
                ManagedOperationResult dispatched = managed->Dispatch(handle, ScriptEvent::Lifecycle(ScriptEventKind::Destroy));
                if (!dispatched.Succeeded)
                    LogDiagnostics(dispatched, script.GetTypeIdentity(), entity.GetUuid());
            }
            CaptureState(script);
            ManagedOperationResult destroyed = managed->DestroyScript(handle);
            if (!destroyed.Succeeded)
                LogDiagnostics(destroyed, script.GetTypeIdentity(), entity.GetUuid());
        }
        script.ClearRuntimeHandle();
    }

    void ScriptRuntime::Dispatch(ManagedScript& script, const ScriptEvent& event)
    {
        ManagedScripting* managed = GetManagedScripting();
        if (managed == nullptr || !managed->IsStarted() || !script.GetRuntimeHandle().IsValid())
            return;
        ManagedOperationResult result = managed->Dispatch(script.GetRuntimeHandle(), event);
        if (!result.Succeeded)
            LogDiagnostics(result, script.GetTypeIdentity(), event.OtherEntity);
    }

    ScriptState ScriptRuntime::CaptureState(ManagedScript& script)
    {
        ManagedScripting* managed = GetManagedScripting();
        if (managed == nullptr || !managed->IsStarted() || !script.GetRuntimeHandle().IsValid())
            return script.GetState();
        ScriptStateResult captured = managed->CaptureState(script.GetRuntimeHandle());
        if (!captured.Result.Succeeded)
        {
            LogDiagnostics(captured.Result, script.GetTypeIdentity(), UUID::EMPTY);
            return script.GetState();
        }
        script.SetState(captured.State);
        return std::move(captured.State);
    }

    bool ScriptRuntime::ApplyState(ManagedScript& script, const ScriptState& state)
    {
        ManagedScripting* managed = GetManagedScripting();
        if (managed != nullptr && managed->IsStarted() && script.GetRuntimeHandle().IsValid())
        {
            ManagedOperationResult applied = managed->ApplyState(script.GetRuntimeHandle(), state);
            if (!applied.Succeeded)
            {
                LogDiagnostics(applied, script.GetTypeIdentity(), UUID::EMPTY);
                return false;
            }
        }
        return script.SetState(state);
    }

    bool ScriptRuntime::RunsInEditor(const ScriptTypeIdentity& identity)
    {
        ManagedScripting* managed = GetManagedScripting();
        const ScriptTypeSchema* schema = managed != nullptr ? managed->GetScriptCatalog().FindType(identity) : nullptr;
        return schema != nullptr && (schema->Flags & ScriptTypeFlags::RunInEditor) != ScriptTypeFlags::None;
    }

    void ScriptRuntime::NotifyEntityDestroyed(const Entity& entity)
    {
        if (ManagedScripting* managed = GetManagedScripting())
            managed->NotifyEntityDestroyed(entity);
    }

    void ScriptRuntime::NotifyComponentDestroyed(uint64_t instanceId)
    {
        if (ManagedScripting* managed = GetManagedScripting())
            managed->NotifyComponentDestroyed(instanceId);
    }

    void ScriptRuntime::NotifySceneEventsAvailable()
    {
        if (ManagedScripting* managed = GetManagedScripting())
            managed->NotifySceneEventsAvailable();
    }

    void ScriptRuntime::OnStart() { OnStart(SceneManager::TryGet() != nullptr ? SceneManager::TryGet()->GetActiveScene() : nullptr); }

    void ScriptRuntime::OnStart(const Ref<Scene>& scene)
    {
        if (scene == nullptr)
            return;
        {
            SceneManager::CallbackScope callbackScope =
              SceneManager::TryGet() != nullptr ? SceneManager::TryGet()->DeferSceneChanges() : SceneManager::CallbackScope();
            const Vector<ScriptInvocation>& invocations = CollectScriptInvocations(scene);
            for (const ScriptInvocation& invocation : invocations)
            {
                Entity entity;
                ManagedScript* script = FindScript(scene, invocation, entity);
                if (script != nullptr)
                    CreateScript(entity, *script, true);
            }
        }
    }

    void ScriptRuntime::OnUpdate() { OnUpdate(SceneManager::TryGet() != nullptr ? SceneManager::TryGet()->GetActiveScene() : nullptr); }

    void ScriptRuntime::OnUpdate(const Ref<Scene>& scene)
    {
        if (scene == nullptr)
            return;
        {
            SceneManager::CallbackScope callbackScope =
              SceneManager::TryGet() != nullptr ? SceneManager::TryGet()->DeferSceneChanges() : SceneManager::CallbackScope();
            const Vector<ScriptInvocation>& invocations = CollectScriptInvocations(scene);
            for (const ScriptInvocation& invocation : invocations)
            {
                Entity entity;
                ManagedScript* script = FindScript(scene, invocation, entity);
                if (script != nullptr)
                    Dispatch(*script, ScriptEvent::Lifecycle(ScriptEventKind::Update));
            }
        }
        if (ManagedScripting* managed = GetManagedScripting())
            LogDiagnostics(managed->Update());
    }

    void ScriptRuntime::OnShutdown() { OnShutdown(SceneManager::TryGet() != nullptr ? SceneManager::TryGet()->GetActiveScene() : nullptr); }

    void ScriptRuntime::OnShutdown(const Ref<Scene>& scene)
    {
        if (scene == nullptr)
            return;
        SceneManager::CallbackScope callbackScope =
          SceneManager::TryGet() != nullptr ? SceneManager::TryGet()->DeferSceneChanges() : SceneManager::CallbackScope();
        const Vector<ScriptInvocation>& invocations = CollectScriptInvocations(scene);
        for (const ScriptInvocation& invocation : invocations)
        {
            Entity entity;
            ManagedScript* script = FindScript(scene, invocation, entity);
            if (script != nullptr)
                DestroyScript(entity, *script, true);
        }

        if (ManagedScripting* managed = GetManagedScripting())
            managed->NotifySceneDestroyed(scene.get());
    }

    void ScriptRuntime::Reload()
    {
        ManagedScripting* managed = GetManagedScripting();
        Application* application = Application::TryGet();
        if (managed == nullptr || application == nullptr || !managed->IsStarted() || !managed->GetCapabilities().Reload)
            return;

        static uint64_t generation = 1;
        const ApplicationDesc& description = application->GetApplicationDesc();
        ManagedProgramDefinition program;
        program.Generation = ++generation;
        if (description.Script.Backend == ManagedBackendPreset::CoreCLR)
        {
            Path manifest = description.Script.ProgramManifest;
            if (manifest.is_relative())
                manifest = description.WorkingDirectory / manifest;
            ManagedProgramPackageResult package = LoadManagedProgramPackage(manifest, generation);
            if (!package.Result.Succeeded)
            {
                LogDiagnostics(package.Result.Diagnostics);
                return;
            }
            program = std::move(package.Package.Program);
        }
        else
        {
            Path engineAssembly = description.EngineAssemblyPath;
            if (engineAssembly.is_relative())
                engineAssembly = description.WorkingDirectory / engineAssembly;
            Path gameAssembly = description.GameAssemblyPath;
            if (gameAssembly.is_relative())
                gameAssembly = description.WorkingDirectory / gameAssembly;
            program.Artifacts.push_back({ ManagedProgramArtifactKind::EngineAssembly, CROWNY_ASSEMBLY, std::move(engineAssembly) });
            if (!description.GameAssemblyPath.empty() && fs::is_regular_file(gameAssembly))
                program.Artifacts.push_back({ ManagedProgramArtifactKind::GameAssembly, GAME_ASSEMBLY, std::move(gameAssembly) });
        }

        ManagedOperationResult reloaded = managed->ReloadProgram(program);
        if (!reloaded.Succeeded)
            LogDiagnostics(reloaded.Diagnostics);
    }

    void ScriptRuntime::UnloadAssemblies()
    {
        if (ManagedScripting* managed = GetManagedScripting())
            managed->Shutdown();
    }
} // namespace Crowny
