#include "cwpch.h"

#include "Crowny/Application/Application.h"
#include "Crowny/Scene/SceneManager.h"
#include "Crowny/Scene/ScriptRuntime.h"
#include "Crowny/Window/Window.h"

namespace Crowny
{
    SceneManager* gSceneManager = nullptr;

    void SceneManager::OnStartUp() { gSceneManager = this; }

    void SceneManager::OnShutdown() { gSceneManager = nullptr; }

    Ref<Scene> SceneManager::GetActiveScene() const
    {
        return m_ActiveScene;
    }

    void SceneManager::SetActiveScene(const Ref<Scene>& scene)
    {
        m_ActiveScene = scene;
        String title = gApplication->GetApplicationDesc().Name;
        if (scene)
            title += " - " + scene->GetName();
        gApplication->GetWindow().SetTitle(title);
    }
} // namespace Crowny
