#include "cwpch.h"

#include "Crowny/Ecs/Components.h"
#include "Crowny/Scene/SceneManager.h"
#include "Crowny/Scene/ScriptRuntime.h"

#include "Crowny/Scripting/Mono/MonoManager.h"
#include "Crowny/Scripting/Bindings/Scene/ScriptSceneManager.h"
#include "Crowny/Scripting/ScriptObjectManager.h"
#include "Crowny/Scripting/ScriptSceneObjectManager.h"

namespace Crowny
{
    namespace
    {
        struct ScriptInvocation
        {
            UUID EntityId;
            uint64_t ScriptId = 0;
        };

        Vector<ScriptInvocation>& CollectScriptInvocations(const Ref<Scene>& scene)
        {
            // Runtime callbacks execute on the main thread. Reusing this scratch
            // avoids allocating a traversal snapshot on every Update.
            static Vector<ScriptInvocation> invocations;
            invocations.clear();
            auto view = scene->GetAllEntitiesWith<MonoScriptComponent>();
            size_t scriptCount = 0;
            view.each([&scriptCount](const MonoScriptComponent& component) { scriptCount += component.Scripts.size(); });
            invocations.reserve(scriptCount);
            view.each([&](entt::entity handle, const MonoScriptComponent& component) {
                const Entity entity(handle, scene.get());
                const UUID entityId = entity.GetUuid();
                for (const MonoScript& script : component.Scripts)
                    invocations.push_back({ entityId, script.InstanceId });
            });
            return invocations;
        }

        MonoScript* FindScript(const Ref<Scene>& scene, const ScriptInvocation& invocation, Entity& entity)
        {
            entity = scene->TryGetEntityFromUuid(invocation.EntityId);
            if (!entity || !entity.HasComponent<MonoScriptComponent>())
                return nullptr;
            Vector<MonoScript>& scripts = entity.GetComponent<MonoScriptComponent>().Scripts;
            const auto script = std::find_if(scripts.begin(), scripts.end(), [&](const MonoScript& candidate) {
                return candidate.InstanceId == invocation.ScriptId;
            });
            return script != scripts.end() ? &*script : nullptr;
        }
    } // namespace

    void ScriptRuntime::Init() {}

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
                MonoScript* script = FindScript(scene, invocation, entity);
                if (script == nullptr)
                    continue;
                if (script->GetManagedInstance() == nullptr && MonoManager::IsStartedUp() && ScriptSceneObjectManager::IsStartedUp())
                {
                    script->Create(entity);
                    script = FindScript(scene, invocation, entity);
                    if (script == nullptr)
                        continue;
                }
                const MonoScript::RuntimeCallback callback = script->GetStartCallback();
                callback.Invoke();
            }
        }
        ScriptSceneManager::DispatchPendingEvents();
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
                MonoScript* script = FindScript(scene, invocation, entity);
                if (script != nullptr)
                {
                    const MonoScript::RuntimeCallback callback = script->GetUpdateCallback();
                    callback.Invoke();
                }
            }
        }
        ScriptSceneManager::DispatchPendingEvents();
    }

    void ScriptRuntime::OnShutdown() { OnShutdown(SceneManager::TryGet() != nullptr ? SceneManager::TryGet()->GetActiveScene() : nullptr); }

    void ScriptRuntime::OnShutdown(const Ref<Scene>& scene)
    {
        if (scene == nullptr)
            return;
        SceneManager::CallbackScope callbackScope = SceneManager::TryGet() != nullptr ? SceneManager::TryGet()->DeferSceneChanges() : SceneManager::CallbackScope();
        const Vector<ScriptInvocation>& invocations = CollectScriptInvocations(scene);
        for (const ScriptInvocation& invocation : invocations)
        {
            Entity entity;
            MonoScript* script = FindScript(scene, invocation, entity);
            if (script != nullptr)
            {
                const MonoScript::RuntimeCallback callback = script->GetDestroyCallback();
                callback.Invoke();
            }
        }

        if (ScriptSceneObjectManager::IsStartedUp())
            ScriptSceneObjectManager::Get().DestroySceneObjects(scene.get());
        scene->GetAllEntitiesWith<MonoScriptComponent>().each([](MonoScriptComponent& sc) {
            for (MonoScript& script : sc.Scripts)
                script.ClearRuntimeInstance();
        });
    }

    void ScriptRuntime::Reload()
    {
        Vector<AssemblyRefreshInfo> assemblies;

        Path engineAssemblyPath = String("Resources/Assemblies/") + CROWNY_ASSEMBLY + ".dll";
        assemblies.push_back(AssemblyRefreshInfo(CROWNY_ASSEMBLY, &engineAssemblyPath));

        Path gameAssmeblyPath = String("Resources/Assemblies/") + GAME_ASSEMBLY + ".dll";
        if (fs::exists(gameAssmeblyPath))
            assemblies.push_back(AssemblyRefreshInfo(GAME_ASSEMBLY, &gameAssmeblyPath));

        ScriptObjectManager::Get().RefreshAssemblies(assemblies);
    }

    void ScriptRuntime::UnloadAssemblies()
    {
        MonoManager::Get().UnloadScriptDomain();
        ScriptObjectManager::Get().ProcessFinalizedObjects();
        MonoManager::Shutdown();
        ScriptSceneObjectManager::Shutdown();
        ScriptObjectManager::Shutdown();
    }

} // namespace Crowny
