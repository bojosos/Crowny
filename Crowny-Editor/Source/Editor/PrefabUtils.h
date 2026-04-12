#pragma once

#include "Crowny/Assets/AssetHandle.h"
#include "Crowny/Ecs/Entity.h"

namespace Crowny
{
    class Prefab;

    class PrefabUtils
    {
    public:
        // Create a .cwprefab file from an entity hierarchy. The entity becomes linked to the new prefab.
        static void CreatePrefabFromEntity(Entity entity);

        // Instantiate a prefab into the active scene as linked entities, parented to the given parent.
        static Entity InstantiatePrefab(const AssetHandle<Prefab>& prefab, Entity parent);

        // Sync a single prefab instance root from its prefab asset (non-overridden properties updated).
        static void SyncInstance(Entity instanceRoot, const AssetHandle<Prefab>& prefab);

        // Sync all instances of the given prefab asset in the active scene.
        static void SyncAllInstances(const UUID& prefabAssetUuid);

        // Push instance values back into the prefab asset and save. Then syncs all other instances.
        static void ApplyInstanceToPrefab(Entity instanceRoot);

        // Discard all overrides on the instance and re-sync from prefab.
        static void RevertInstance(Entity instanceRoot);

        // Remove PrefabComponent from entity and all children, unlinking from prefab.
        static void UnlinkPrefab(Entity entity);

    private:
        static void AttachPrefabComponents(Entity entity, const UUID& prefabAssetUuid);
        static Entity InstantiateEntityRecursive(Scene& prefabScene, Entity prefabEntity, Scene& targetScene, Entity* targetParent,
                                                 const UUID& prefabAssetUuid);
    };

} // namespace Crowny
