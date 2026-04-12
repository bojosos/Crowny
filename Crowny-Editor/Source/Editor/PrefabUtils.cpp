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

    void PrefabUtils::CreatePrefabFromEntity(Entity entity)
    {
        Ref<Prefab> prefab = CreateRef<Prefab>();
        Ref<Scene> activeScene = gSceneManager->GetActiveScene();
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

        for (auto& child : entity.GetChildren())
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

        Ref<Scene> activeScene = gSceneManager->GetActiveScene();
        UUID prefabAssetUuid = prefabHandle.GetUUID();

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
        for (auto& child : prefabEntity.GetChildren())
            InstantiateEntityRecursive(prefabScene, child, targetScene, &newEntity, prefabAssetUuid);

        return newEntity;
    }

    static void SyncEntityRecursive(Entity instanceEntity, Scene& prefabScene, const UUID& prefabAssetUuid)
    {
        if (!instanceEntity.HasComponent<PrefabComponent>())
            return;

        const auto& pc = instanceEntity.GetComponent<PrefabComponent>();
        if (pc.PrefabAssetUuid != prefabAssetUuid)
            return;

        Entity prefabEntity = prefabScene.GetEntityFromUuid(pc.PrefabEntityUuid);
        if (prefabEntity)
            PrefabSync::SyncEntity(instanceEntity, prefabEntity, pc);

        for (auto& child : instanceEntity.GetChildren())
            SyncEntityRecursive(child, prefabScene, prefabAssetUuid);
    }

    void PrefabUtils::SyncInstance(Entity instanceRoot, const AssetHandle<Prefab>& prefab)
    {
        if (!prefab.IsLoaded())
            return;

        Prefab* p = prefab.Get();
        if (!p || !p->GetInternalScene())
            return;

        SyncEntityRecursive(instanceRoot, *p->GetInternalScene(), prefab.GetUUID());
    }

    void PrefabUtils::SyncAllInstances(const UUID& prefabAssetUuid)
    {
        Ref<Scene> activeScene = gSceneManager->GetActiveScene();
        if (!activeScene)
            return;

        AssetHandle<Asset> assetHandle = gAssetManager->LoadFromUUID(prefabAssetUuid);
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

    void PrefabUtils::ApplyInstanceToPrefab(Entity instanceRoot)
    {
        if (!instanceRoot.HasComponent<PrefabComponent>())
            return;

        const auto& pc = instanceRoot.GetComponent<PrefabComponent>();
        const UUID& prefabAssetUuid = pc.PrefabAssetUuid;

        // Load the prefab asset
        AssetHandle<Asset> assetHandle = gAssetManager->LoadFromUUID(prefabAssetUuid);
        if (!assetHandle.IsLoaded())
            return;

        AssetHandle<Prefab> prefabHandle = static_asset_cast<Prefab>(assetHandle);
        Prefab* prefab = prefabHandle.Get();

        // Re-capture the entity hierarchy into the prefab
        Ref<Scene> activeScene = gSceneManager->GetActiveScene();
        prefab->CaptureFromEntity(*activeScene, instanceRoot);

        // Re-serialize the prefab file
        Path prefabPath = ProjectLibrary::Get().UuidToPath(prefabAssetUuid);
        if (!prefabPath.empty())
        {
            PrefabSerializer serializer(prefabHandle.GetInternalPtr());
            serializer.Serialize(prefabPath);
        }

        // Sync all other instances
        SyncAllInstances(prefabAssetUuid);
    }

    void PrefabUtils::RevertInstance(Entity instanceRoot)
    {
        if (!instanceRoot.HasComponent<PrefabComponent>())
            return;

        auto& pc = instanceRoot.GetComponent<PrefabComponent>();
        pc.Overrides.clear();

        // Load and sync from prefab
        AssetHandle<Asset> assetHandle = gAssetManager->LoadFromUUID(pc.PrefabAssetUuid);
        if (assetHandle.IsLoaded())
            SyncInstance(instanceRoot, static_asset_cast<Prefab>(assetHandle));
    }

    void PrefabUtils::UnlinkPrefab(Entity entity)
    {
        entity.RemoveComponentIfExists<PrefabComponent>();
        for (auto& child : entity.GetChildren())
            UnlinkPrefab(child);
    }

} // namespace Crowny
