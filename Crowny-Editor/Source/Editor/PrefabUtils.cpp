#include "cwepch.h"

#include "Editor/PrefabUtils.h"

#include "Crowny/Assets/AssetManager.h"
#include "Crowny/Common/FileSystem.h"
#include "Crowny/Ecs/Components.h"
#include "Crowny/Ecs/Entity.h"
#include "Crowny/Scene/Prefab.h"
#include "Crowny/Scene/PrefabSync.h"
#include "Crowny/Scene/Scene.h"
#include "Crowny/Scene/SceneManager.h"
#include "Crowny/Serialization/PrefabSerializer.h"

#include "Editor/EditorUtils.h"
#include "Editor/ProjectLibrary.h"

namespace Crowny
{

    // Copy all existing components from src to dst (same pattern as Scene.cpp)
    template <typename... Component> static void CopyCompIfExists(Entity dst, Entity src)
    {
        (
          [&]() {
              if (src.HasComponent<Component>())
                  dst.AddOrReplaceComponent<Component>(src.GetComponent<Component>());
          }(),
          ...);
    }

    template <typename... Component> static void CopyCompIfExists(ComponentGroup<Component...>, Entity dst, Entity src)
    {
        CopyCompIfExists<Component...>(dst, src);
    }

    static void CopyAllExisting(Entity dst, Entity src) { CopyCompIfExists(AllComponents{}, dst, src); }

    static AssetHandle<Prefab> LoadPrefab(const UUID& prefabAssetUuid)
    {
        AssetHandle<Asset> assetHandle = AssetManager::TryGet()->LoadFromUUID(prefabAssetUuid);
        if (!assetHandle.IsLoaded() || assetHandle->GetAssetType() != AssetType::Prefab)
            return {};
        return static_asset_cast<Prefab>(assetHandle);
    }

    Entity PrefabUtils::GetInstanceRoot(Entity entity)
    {
        if (!entity || !entity.HasComponent<PrefabComponent>())
            return {};

        const UUID prefabAssetUuid = entity.GetComponent<PrefabComponent>().PrefabAssetUuid;
        const AssetHandle<Prefab> prefabHandle = LoadPrefab(prefabAssetUuid);
        const Prefab* prefab = prefabHandle.Get();
        if (!prefab)
            return entity;

        const UUID prefabRootUuid = prefab->GetRootEntityUuid();
        for (Entity current = entity; current && current.HasComponent<PrefabComponent>(); current = current.GetParent())
        {
            const auto& prefabComponent = current.GetComponent<PrefabComponent>();
            if (prefabComponent.PrefabAssetUuid != prefabAssetUuid)
                break;
            if (prefabComponent.PrefabEntityUuid == prefabRootUuid)
                return current;
        }

        return entity;
    }

    static bool HasValidPrefabMapping(Entity instanceEntity, const Scene& prefabScene, const UUID& prefabAssetUuid, const UUID& prefabRootEntityUuid,
                                      bool isInstanceRoot)
    {
        if (!instanceEntity.HasComponent<PrefabComponent>())
            return true;

        const auto& prefabComponent = instanceEntity.GetComponent<PrefabComponent>();
        if (prefabComponent.PrefabAssetUuid != prefabAssetUuid)
            return true;
        if (!isInstanceRoot && prefabComponent.PrefabEntityUuid == prefabRootEntityUuid)
            return true;
        if (!prefabScene.TryGetEntityFromUuid(prefabComponent.PrefabEntityUuid))
            return false;

        for (const Entity child : instanceEntity.GetChildren())
        {
            if (!HasValidPrefabMapping(child, prefabScene, prefabAssetUuid, prefabRootEntityUuid, false))
                return false;
        }
        return true;
    }

    template <typename Component> static void ApplyComponent(Entity prefabEntity, Entity instanceEntity)
    {
        if constexpr (std::is_same_v<Component, IDComponent> || std::is_same_v<Component, RelationshipComponent> ||
                      std::is_same_v<Component, PrefabComponent>)
        {
            return;
        }
        else if (instanceEntity.HasComponent<Component>())
        {
            prefabEntity.AddOrReplaceComponent<Component>(instanceEntity.GetComponent<Component>());
        }
        else
        {
            prefabEntity.RemoveComponentIfExists<Component>();
        }
    }

    template <typename... Component> static void ApplyComponents(ComponentGroup<Component...>, Entity prefabEntity, Entity instanceEntity)
    {
        (ApplyComponent<Component>(prefabEntity, instanceEntity), ...);
    }

    static void ApplyInstanceValues(Entity instanceEntity, Scene& prefabScene, const UUID& prefabAssetUuid, const UUID& prefabRootEntityUuid,
                                    bool isInstanceRoot)
    {
        if (!instanceEntity.HasComponent<PrefabComponent>())
            return;

        const auto& prefabComponent = instanceEntity.GetComponent<PrefabComponent>();
        if (prefabComponent.PrefabAssetUuid != prefabAssetUuid)
            return;
        if (!isInstanceRoot && prefabComponent.PrefabEntityUuid == prefabRootEntityUuid)
            return;

        Entity prefabEntity = prefabScene.GetEntityFromUuid(prefabComponent.PrefabEntityUuid);
        ApplyComponents(AllComponents{}, prefabEntity, instanceEntity);

        for (const Entity child : instanceEntity.GetChildren())
            ApplyInstanceValues(child, prefabScene, prefabAssetUuid, prefabRootEntityUuid, false);
    }

    static void ClearInstanceOverrides(Entity entity, const UUID& prefabAssetUuid, const UUID& prefabRootEntityUuid, bool isInstanceRoot)
    {
        if (!entity.HasComponent<PrefabComponent>())
            return;

        auto& prefabComponent = entity.GetComponent<PrefabComponent>();
        if (prefabComponent.PrefabAssetUuid != prefabAssetUuid)
            return;
        if (!isInstanceRoot && prefabComponent.PrefabEntityUuid == prefabRootEntityUuid)
            return;

        prefabComponent.Overrides.clear();
        for (const Entity child : entity.GetChildren())
            ClearInstanceOverrides(child, prefabAssetUuid, prefabRootEntityUuid, false);
    }

    static void UnlinkPrefabRecursive(Entity entity, const UUID& prefabAssetUuid, const UUID& prefabRootEntityUuid, bool isInstanceRoot)
    {
        if (entity.HasComponent<PrefabComponent>())
        {
            const auto& prefabComponent = entity.GetComponent<PrefabComponent>();
            if (prefabComponent.PrefabAssetUuid != prefabAssetUuid || (!isInstanceRoot && prefabComponent.PrefabEntityUuid == prefabRootEntityUuid))
                return;
            entity.RemoveComponent<PrefabComponent>();
        }

        for (const Entity child : entity.GetChildren())
            UnlinkPrefabRecursive(child, prefabAssetUuid, prefabRootEntityUuid, false);
    }

    void PrefabUtils::CreatePrefabFromEntity(Entity entity)
    {
        Ref<Prefab> prefab = CreateRef<Prefab>();
        Ref<Scene> activeScene = SceneManager::TryGet()->GetActiveScene();
        prefab->CaptureFromEntity(*activeScene, entity);

        // Determine save path
        Path savePath = EditorUtils::GetUniquePath(ProjectLibrary::Get().GetAssetFolder() / (entity.GetName() + ".cwprefab"));

        // Serialize the prefab file
        PrefabSerializer serializer(prefab);
        serializer.Serialize(savePath);

        // Let ProjectLibrary discover and import the new file
        ProjectLibrary::Get().Refresh(savePath);

        // Get the UUID assigned to the prefab asset
        Ref<LibraryEntry> entry = ProjectLibrary::Get().FindEntry(savePath);
        if (!entry || entry->Type != LibraryEntryType::File)
            return;
        FileEntry* fileEntry = static_cast<FileEntry*>(entry.get());
        if (!fileEntry->Metadata)
            return;
        const UUID& prefabAssetUuid = fileEntry->Metadata->Uuid;

        // Attach PrefabComponent to the source entity and all its children
        AttachPrefabComponents(entity, prefabAssetUuid);
    }

    void PrefabUtils::AttachPrefabComponents(Entity entity, const UUID& prefabAssetUuid)
    {
        if (entity.HasComponent<PrefabComponent>())
        {
            auto& pc = entity.GetComponent<PrefabComponent>();
            pc.PrefabAssetUuid = prefabAssetUuid;
            pc.PrefabEntityUuid = entity.GetUuid();
            pc.Overrides.clear();
        }
        else
        {
            entity.AddComponent<PrefabComponent>(prefabAssetUuid, entity.GetUuid());
        }

        for (const auto& child : entity.GetChildren())
            AttachPrefabComponents(child, prefabAssetUuid);
    }

    Entity PrefabUtils::InstantiatePrefab(const AssetHandle<Prefab>& prefabHandle, Entity parent)
    {
        if (!prefabHandle.IsLoaded())
            return {};

        Prefab* prefab = prefabHandle.Get();
        if (!prefab || !prefab->GetInternalScene())
            return {};

        Entity prefabRoot = prefab->GetRootEntity();
        if (!prefabRoot)
            return {};

        Ref<Scene> activeScene = SceneManager::TryGet()->GetActiveScene();
        const UUID prefabAssetUuid = prefabHandle.GetUUID();

        Entity instanceRoot = InstantiateEntityRecursive(*prefab->GetInternalScene(), prefabRoot, *activeScene, &parent, prefabAssetUuid);
        return instanceRoot;
    }

    Entity PrefabUtils::InstantiateEntityRecursive(Scene& prefabScene, Entity prefabEntity, Scene& targetScene, Entity* targetParent,
                                                   const UUID& prefabAssetUuid)
    {
        // Create new entity with a NEW UUID in the target scene
        Entity newEntity = targetScene.CreateEntity(prefabEntity.GetName());

        // Copy all components from the prefab entity
        CopyAllExisting(newEntity, prefabEntity);

        // Clear children (we'll rebuild hierarchy)
        auto& rc = newEntity.GetComponent<RelationshipComponent>();
        rc.Children.clear();

        // Add PrefabComponent linking back to the prefab
        if (newEntity.HasComponent<PrefabComponent>())
            newEntity.RemoveComponent<PrefabComponent>();
        newEntity.AddComponent<PrefabComponent>(prefabAssetUuid, prefabEntity.GetUuid());

        // Set parent
        if (targetParent && targetParent->IsValid())
            newEntity.SetParent(*targetParent);

        // Recursively instantiate children
        for (const auto& child : prefabEntity.GetChildren())
            InstantiateEntityRecursive(prefabScene, child, targetScene, &newEntity, prefabAssetUuid);

        return newEntity;
    }

    static void SyncEntityRecursive(Entity instanceEntity, Scene& prefabScene, const UUID& prefabAssetUuid, const UUID& prefabRootEntityUuid,
                                    bool isInstanceRoot)
    {
        if (!instanceEntity.HasComponent<PrefabComponent>())
            return;

        const auto& pc = instanceEntity.GetComponent<PrefabComponent>();
        if (pc.PrefabAssetUuid != prefabAssetUuid)
            return;
        if (!isInstanceRoot && pc.PrefabEntityUuid == prefabRootEntityUuid)
            return;

        Entity prefabEntity = prefabScene.GetEntityFromUuid(pc.PrefabEntityUuid);
        if (prefabEntity)
            PrefabSync::SyncEntity(instanceEntity, prefabEntity, pc);

        for (const auto& child : instanceEntity.GetChildren())
            SyncEntityRecursive(child, prefabScene, prefabAssetUuid, prefabRootEntityUuid, false);
    }

    void PrefabUtils::SyncInstance(Entity instanceRoot, const AssetHandle<Prefab>& prefab)
    {
        if (!prefab.IsLoaded())
            return;

        Prefab* p = prefab.Get();
        if (!p || !p->GetInternalScene())
            return;

        instanceRoot = GetInstanceRoot(instanceRoot);
        if (!instanceRoot)
            return;

        SyncEntityRecursive(instanceRoot, *p->GetInternalScene(), prefab.GetUUID(), p->GetRootEntityUuid(), true);
    }

    void PrefabUtils::SyncAllInstances(const UUID& prefabAssetUuid)
    {
        Ref<Scene> activeScene = SceneManager::TryGet()->GetActiveScene();
        if (!activeScene)
            return;

        AssetHandle<Asset> assetHandle = AssetManager::TryGet()->LoadFromUUID(prefabAssetUuid);
        if (!assetHandle.IsLoaded())
            return;
        AssetHandle<Prefab> prefabHandle = static_asset_cast<Prefab>(assetHandle);

        auto view = activeScene->GetAllEntitiesWith<PrefabComponent>();
        for (auto e : view)
        {
            Entity entity = { e, activeScene.get() };
            const auto& pc = entity.GetComponent<PrefabComponent>();
            if (pc.PrefabAssetUuid == prefabAssetUuid && pc.PrefabEntityUuid == prefabHandle.Get()->GetRootEntityUuid())
            {
                SyncInstance(entity, prefabHandle);
            }
        }
    }

    bool PrefabUtils::CanApplyInstanceToPrefab(Entity instanceRoot)
    {
        instanceRoot = GetInstanceRoot(instanceRoot);
        if (!instanceRoot)
            return false;

        const auto& pc = instanceRoot.GetComponent<PrefabComponent>();
        const UUID prefabAssetUuid = pc.PrefabAssetUuid;
        const Path prefabPath = ProjectLibrary::Get().UuidToPath(prefabAssetUuid);
        if (prefabPath.empty())
            return false;

        const AssetHandle<Prefab> prefabHandle = LoadPrefab(prefabAssetUuid);
        Prefab* prefab = prefabHandle.Get();
        return prefab && prefab->GetInternalScene() && pc.PrefabEntityUuid == prefab->GetRootEntityUuid() &&
               HasValidPrefabMapping(instanceRoot, *prefab->GetInternalScene(), prefabAssetUuid, prefab->GetRootEntityUuid(), true);
    }

    bool PrefabUtils::ApplyInstanceToPrefab(Entity instanceRoot)
    {
        instanceRoot = GetInstanceRoot(instanceRoot);
        if (!instanceRoot || !CanApplyInstanceToPrefab(instanceRoot))
            return false;

        const UUID prefabAssetUuid = instanceRoot.GetComponent<PrefabComponent>().PrefabAssetUuid;
        const AssetHandle<Prefab> prefabHandle = LoadPrefab(prefabAssetUuid);
        Prefab* prefab = prefabHandle.Get();

        // Update the existing prefab entities so their UUIDs remain stable for every linked instance.
        ApplyInstanceValues(instanceRoot, *prefab->GetInternalScene(), prefabAssetUuid, prefab->GetRootEntityUuid(), true);

        const Path prefabPath = ProjectLibrary::Get().UuidToPath(prefabAssetUuid);
        PrefabSerializer serializer(prefabHandle.GetInternalPtr());
        serializer.Serialize(prefabPath);

        ClearInstanceOverrides(instanceRoot, prefabAssetUuid, prefab->GetRootEntityUuid(), true);
        SyncAllInstances(prefabAssetUuid);
        return true;
    }

    bool PrefabUtils::RevertInstance(Entity instanceRoot)
    {
        instanceRoot = GetInstanceRoot(instanceRoot);
        if (!instanceRoot || !CanApplyInstanceToPrefab(instanceRoot))
            return false;

        const UUID prefabAssetUuid = instanceRoot.GetComponent<PrefabComponent>().PrefabAssetUuid;
        const AssetHandle<Prefab> prefabHandle = LoadPrefab(prefabAssetUuid);
        ClearInstanceOverrides(instanceRoot, prefabAssetUuid, prefabHandle->GetRootEntityUuid(), true);
        SyncInstance(instanceRoot, prefabHandle);

        return true;
    }

    void PrefabUtils::UnlinkPrefab(Entity entity)
    {
        Entity instanceRoot = GetInstanceRoot(entity);
        if (!instanceRoot)
            return;

        const auto& prefabComponent = instanceRoot.GetComponent<PrefabComponent>();
        const UUID prefabAssetUuid = prefabComponent.PrefabAssetUuid;
        const UUID prefabRootEntityUuid = prefabComponent.PrefabEntityUuid;
        UnlinkPrefabRecursive(instanceRoot, prefabAssetUuid, prefabRootEntityUuid, true);
    }

} // namespace Crowny
