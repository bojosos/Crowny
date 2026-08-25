#pragma once

namespace Crowny
{
    class Scene;

    Scene* GetScriptSerializationScene();

    // Binary scene versions before assembly-qualified nested managed type metadata
    // used a shorter object/enum layout. The scope selects that legacy layout
    // while reading old scene data without changing standalone snapshots.
    class ScriptTypeMetadataSerializationScope
    {
    public:
        explicit ScriptTypeMetadataSerializationScope(bool assemblyQualified);
        ~ScriptTypeMetadataSerializationScope();

        ScriptTypeMetadataSerializationScope(const ScriptTypeMetadataSerializationScope&) = delete;
        ScriptTypeMetadataSerializationScope& operator=(const ScriptTypeMetadataSerializationScope&) = delete;

    private:
        bool m_PreviousAssemblyQualified = true;
    };

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
