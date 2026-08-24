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

        // Resolve the nearest prefab root for a linked entity, including nested instances of the same asset.
        static Entity GetInstanceRoot(Entity entity);

        // Return true when every linked entity can be resolved in the prefab asset.
        static bool CanApplyInstanceToPrefab(Entity instanceRoot);

        // Replace mapped prefab component values from an instance and save. Structural hierarchy changes are not applied.
        static bool ApplyInstanceToPrefab(Entity instanceRoot);

        // Discard all overrides on the instance and re-sync from prefab.
        static bool RevertInstance(Entity instanceRoot);

        // Unlink one prefab instance while preserving nested prefab instances.
        static void UnlinkPrefab(Entity entity);

    private:
        static void AttachPrefabComponents(Entity entity, const UUID& prefabAssetUuid);
        static Entity InstantiateEntityRecursive(Scene& prefabScene, Entity prefabEntity, Scene& targetScene, Entity* targetParent,
                                                 const UUID& prefabAssetUuid);
    };

} // namespace Crowny
