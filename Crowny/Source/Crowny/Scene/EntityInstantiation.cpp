#include "cwpch.h"

#include "Crowny/Scene/EntityInstantiation.h"

#include "Crowny/Ecs/Components.h"
#include "Crowny/Scene/ComponentCopy.h"
#include "Crowny/Scene/Prefab.h"
#include "Crowny/Scene/ScriptRuntime.h"

namespace Crowny
{
    namespace
    {
        Entity InstantiateRecursive(Scene& targetScene, Entity source, Entity parent, const UUID* prefabAssetUuid)
        {
            Entity newEntity = targetScene.CreateEntity(source.GetName());
            CopyAllExistingComponents(newEntity, source);

            if (prefabAssetUuid != nullptr)
            {
                newEntity.RemoveComponentIfExists<PrefabComponent>();
                newEntity.AddComponent<PrefabComponent>(*prefabAssetUuid, source.GetUuid());
            }

            if (parent)
                newEntity.SetParent(parent);

            for (const Entity& child : source.GetChildren())
                InstantiateRecursive(targetScene, child, newEntity, prefabAssetUuid);

            return newEntity;
        }

        Entity FinalizeInstantiation(Scene& targetScene, Entity root, const EntityInstantiateOptions& options)
        {
            if (!root)
                return Entity::Invalid;

            if (options.ApplyWorldPosition)
                root.SetWorldPosition(options.WorldPosition);
            if (options.ApplyWorldRotation)
                root.SetWorldRotation(options.WorldRotation);

            // Construct managed instances for the script components copied with the subtree so Update,
            // FixedUpdate and collision callbacks dispatch to them like any scene-start script.
            if (targetScene.IsRuntimeActive())
                ScriptRuntime::OnEntityTreeCreated(root);

            return root;
        }
    } // namespace

    void EntityInstantiator::RemapCopiedReferences(Entity source, Entity destination)
    {
        UnorderedMap<UUID, UUID> remap;
        Vector<Entity> copies;
        std::function<void(Entity, Entity)> collect = [&](Entity original, Entity copy) {
            remap[original.GetUuid()] = copy.GetUuid();
            copies.push_back(copy);
            const auto& originals = original.GetChildren();
            const auto& children = copy.GetChildren();
            for (size_t i = 0; i < std::min(originals.size(), children.size()); ++i)
                collect(originals[i], children[i]);
        };
        collect(source, destination);
        for (Entity copy : copies)
        {
            if (!copy.HasComponent<DecalComponent>())
                continue;
            auto& decal = copy.GetComponent<DecalComponent>();
            const auto target = remap.find(decal.Target);
            if (target != remap.end())
                decal.Target = target->second;
            decal.Age = 0.0f;
            decal.LifetimeRunning = true;
        }
    }

    Entity EntityInstantiator::InstantiatePrefab(Scene& targetScene, const AssetHandle<Prefab>& prefab, const EntityInstantiateOptions& options)
    {
        if (!prefab.IsLoaded())
            return Entity::Invalid;

        const Prefab* prefabAsset = prefab.Get();
        if (prefabAsset == nullptr || !prefabAsset->GetInternalScene())
            return Entity::Invalid;

        Entity prefabRoot = prefabAsset->GetRootEntity();
        if (!prefabRoot)
            return Entity::Invalid;

        const UUID prefabAssetUuid = prefab.GetUUID();
        Entity root = InstantiateRecursive(targetScene, prefabRoot, options.Parent, &prefabAssetUuid);
        RemapCopiedReferences(prefabRoot, root);
        return FinalizeInstantiation(targetScene, root, options);
    }

    Entity EntityInstantiator::InstantiateEntity(Scene& targetScene, Entity source, const EntityInstantiateOptions& options)
    {
        if (!source || source.GetScene() == nullptr)
            return Entity::Invalid;

        const Entity targetRoot = targetScene.GetRootEntity();
        if (targetRoot && source == targetRoot)
            return Entity::Invalid;

        Entity root = InstantiateRecursive(targetScene, source, options.Parent, nullptr);
        RemapCopiedReferences(source, root);
        return FinalizeInstantiation(targetScene, root, options);
    }
} // namespace Crowny
