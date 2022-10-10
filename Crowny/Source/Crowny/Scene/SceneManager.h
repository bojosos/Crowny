#pragma once

#include "Crowny/Scene/Scene.h"

namespace Crowny
{
    class SceneManager
    {
    public:
        static Ref<Scene> GetActiveScene();
        static void AddScene(const Ref<Scene>& scene);
        static void SetActiveScene(const Ref<Scene>& scene);
        static void Shutdown();
        static uint32_t GetSceneCount() { return (uint32_t)s_Scenes.size(); }
        /*
        static Scene& LoadScene(uint32_t buildIndex);
        static Scene& LoadScene(const String& name);
        */
    private:
        static uint32_t s_ActiveIndex;
        static Vector<Ref<Scene>> s_Scenes;
        friend class Scene;
    };
} // namespace Crowny