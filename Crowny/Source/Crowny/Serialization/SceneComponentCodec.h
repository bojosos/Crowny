#pragma once

#include "Crowny/Common/Types.h"
#include "Crowny/Ecs/Entity.h"

#include <span>

class BinaryDataStreamInputArchive;
class BinaryDataStreamOutputArchive;

namespace YAML
{
    class Emitter;
    class Node;
} // namespace YAML

namespace Crowny
{
    enum class SceneComponentId : uint32_t
    {
        Tag = 0,
        Transform = 1,
        Camera = 2,
        SpriteRenderer = 3,
        MeshRenderer = 4,
        Text = 5,
        AudioListener = 6,
        AudioSource = 7,
        ManagedScript = 8,
        Rigidbody2D = 9,
        BoxCollider2D = 10,
        CircleCollider2D = 11,
        Relationship = 12,
        Prefab = 13,
        ProceduralMesh = 14,
        Rigidbody3D = 15,
        BoxCollider3D = 16,
        SphereCollider3D = 17,
        CapsuleCollider3D = 18,
        Animation = 19,
        Light = 20
    };

    enum class SceneComponentYamlType : uint8_t
    {
        Map,
        Null
    };

    struct SceneComponentReadContext
    {
        Scene* TargetScene = nullptr;
        UnorderedMap<Entity, Vector<UUID>>* Relationships = nullptr;
    };

    struct SceneComponentCodec
    {
        using HasComponentFunction = bool (*)(Entity);
        using WriteYamlFunction = void (*)(YAML::Emitter&, Entity);
        using ReadYamlFunction = void (*)(const YAML::Node&, Entity, SceneComponentReadContext&);
        using WriteBinaryFunction = void (*)(BinaryDataStreamOutputArchive&, Entity);
        using ReadBinaryFunction = void (*)(BinaryDataStreamInputArchive&, Entity, SceneComponentReadContext&);
        SceneComponentId Id;
        const char* YamlName;
        SceneComponentYamlType YamlType;
        HasComponentFunction HasComponent;
        HasComponentFunction ShouldSerialize;
        WriteYamlFunction WriteYaml;
        ReadYamlFunction ReadYaml;
        WriteBinaryFunction WriteBinary;
        ReadBinaryFunction ReadBinary;

        // These names are optional. Prefab tools use the property-path prefix,
        // while editor tools can use the display name without owning another table.
        const char* PrefabPath;
        const char* EditorName;
    };

    std::span<const SceneComponentCodec> GetSceneComponentCodecs();
    const SceneComponentCodec* FindSceneComponentCodec(SceneComponentId id);
    const SceneComponentCodec* FindSceneComponentCodec(uint32_t stableId);
} // namespace Crowny
