#pragma once

#include "Crowny/Common/Module.h"
#include "Crowny/Scene/Scene.h"

namespace Crowny
{
    class SceneManager : public Module<SceneManager>
    {
    public:
        SceneManager() = default;
        ~SceneManager() = default;

        Ref<Scene> GetActiveScene() const;
        void SetActiveScene(const Ref<Scene>& scene);

    protected:
        void OnStartUp() override;
        void OnShutdown() override;

    private:
        Ref<Scene> m_ActiveScene;
    };

    extern SceneManager* gSceneManager;
} // namespace Crowny
