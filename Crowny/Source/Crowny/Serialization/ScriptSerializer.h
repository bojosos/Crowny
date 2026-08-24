#pragma once

namespace Crowny
{
    class Scene;

    Scene* GetScriptSerializationScene();

    // Selects the scene used to resolve entity UUIDs while managed field data
    // is being materialized. Scopes nest, which keeps editor/play-scene reloads
    // independent from SceneManager's globally active scene.
    class ScriptSerializationSceneScope
    {
    public:
        explicit ScriptSerializationSceneScope(Scene* scene);
        ~ScriptSerializationSceneScope();

        ScriptSerializationSceneScope(const ScriptSerializationSceneScope&) = delete;
        ScriptSerializationSceneScope& operator=(const ScriptSerializationSceneScope&) = delete;

    private:
        Scene* m_PreviousScene = nullptr;
    };
} // namespace Crowny
