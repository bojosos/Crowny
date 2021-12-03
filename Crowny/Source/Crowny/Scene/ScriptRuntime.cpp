#include "cwpch.h"

#include "Crowny/Ecs/Components.h"
#include "Crowny/Scene/SceneManager.h"
#include "Crowny/Scene/ScriptRuntime.h"

namespace Crowny
{

    void ScriptRuntime::Init()
    {
        Ref<Scene> scene = SceneManager::GetActiveScene();

        scene->m_Registry.view<MonoScriptComponent>().each([&](entt::entity entity, MonoScriptComponent& sc) {
            sc.OnInitialize({ entity, scene.get() });
        });
    }

    void ScriptRuntime::OnStart()
    {
        Ref<Scene> activeScene = SceneManager::GetActiveScene();
        activeScene->m_Registry.view<MonoScriptComponent>().each(
          [&](entt::entity entity, MonoScriptComponent& sc) { sc.OnStart(); });

        activeScene->m_Registry.view<AudioSourceComponent>().each(
          [&](entt::entity entity, AudioSourceComponent& sc) { sc.OnInitialize(); });
    }

    void ScriptRuntime::OnUpdate()
    {
        Ref<Scene> activeScene = SceneManager::GetActiveScene();
        activeScene->m_Registry.view<MonoScriptComponent>().each(
          [&](entt::entity entity, MonoScriptComponent& sc) { sc.OnUpdate(); });
    }

    void ScriptRuntime::OnShutdown()
    {
        Ref<Scene> activeScene = SceneManager::GetActiveScene();
        activeScene->m_Registry.view<MonoScriptComponent>().each(
          [&](entt::entity entity, MonoScriptComponent& sc) { sc.OnDestroy(); });

        activeScene->m_Registry.view<AudioSourceComponent>().each(
          [&](entt::entity entity, AudioSourceComponent& sc) { sc.Stop(); });
    }

} // namespace Crowny