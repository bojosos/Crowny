#pragma once

#include "Crowny/Assets/AssetHandle.h"
#include "Crowny/Ecs/Entity.h"

namespace Crowny
{
    class Prefab;
    class Scene;

    struct EntityInstantiateOptions
    {
        // Parent for the instantiated root. When valid, the world transform of the source is preserved.
        Entity Parent = Entity::Invalid;

        // World-space pose overrides applied to the instance root after the hierarchy is built.
        bool ApplyWorldPosition = false;
        glm::vec3 WorldPosition{0.0f};
        bool ApplyWorldRotation = false;
        glm::quat WorldRotation{1.0f, 0.0f, 0.0f, 0.0f};
    };

    /** Runtime entity instantiation, usable in play mode from scripts and the editor alike. */
    class EntityInstantiator
    {
    public:
        /** Instantiates a prefab asset into the scene as a linked instance hierarchy and returns the instance root. */
        static Entity InstantiatePrefab(Scene& targetScene, const AssetHandle<Prefab>& prefab, const EntityInstantiateOptions& options = {});

        /** Instantiates (deep-copies) an entity subtree with fresh UUIDs and returns the new root. */
        static Entity InstantiateEntity(Scene& targetScene, Entity source, const EntityInstantiateOptions& options = {});
    };
} // namespace Crowny
