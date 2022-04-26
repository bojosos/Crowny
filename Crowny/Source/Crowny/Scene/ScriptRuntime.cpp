#include "cwpch.h"

#include "Crowny/Ecs/Components.h"
#include "Crowny/Scene/SceneManager.h"
#include "Crowny/Scene/ScriptRuntime.h"

#include "Crowny/Scripting/Mono/MonoManager.h"
#include "Crowny/Scripting/ScriptObjectManager.h"
#include "Crowny/Scripting/ScriptSceneObjectManager.h"

namespace Crowny
{

    void ScriptRuntime::Init()
    {
        Ref<Scene> scene = SceneManager::GetActiveScene();

        scene->GetAllEntitiesWith<MonoScriptComponent>().each([&](entt::entity entity, MonoScriptComponent& sc) {
            sc.OnInitialize({ entity, scene.get() });
        });
    }

    void ScriptRuntime::OnStart()
    {
        Ref<Scene> activeScene = SceneManager::GetActiveScene();
        activeScene->GetAllEntitiesWith<MonoScriptComponent>().each(
          [&](entt::entity entity, MonoScriptComponent& sc) { sc.OnStart(); });

        activeScene->GetAllEntitiesWith<AudioSourceComponent>().each(
          [&](entt::entity entity, AudioSourceComponent& sc) { sc.OnInitialize(); });
    }

    void ScriptRuntime::OnUpdate()
    {
        Ref<Scene> activeScene = SceneManager::GetActiveScene();
        activeScene->GetAllEntitiesWith<MonoScriptComponent>().each(
          [&](entt::entity entity, MonoScriptComponent& sc) { sc.OnUpdate(); });
    }

    void ScriptRuntime::OnShutdown()
    {
        Ref<Scene> activeScene = SceneManager::GetActiveScene();
        activeScene->GetAllEntitiesWith<MonoScriptComponent>().each(
          [&](entt::entity entity, MonoScriptComponent& sc) { sc.OnDestroy(); });

        activeScene->GetAllEntitiesWith<AudioSourceComponent>().each(
          [&](entt::entity entity, AudioSourceComponent& sc) { sc.Stop(); });
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