#include "cwpch.h"

#include "Crowny/Scene/Prefab.h"

#include "Crowny/Ecs/Components.h"
#include "Crowny/Ecs/Entity.h"
#include "Crowny/Scene/Scene.h"

namespace Crowny
{

    // Forward-declare the static helper from Scene.cpp that copies all existing components between entities.
    // We replicate the logic here since CopyAllExistingComponents is file-local in Scene.cpp.
    template <typename... Component> static void CopyComponentIfExistsPrefab(Entity dst, Entity src)
    {
        (
          [&]() {
              if constexpr (std::is_same_v<Component, RelationshipComponent>)
                  return;

              if (src.HasComponent<Component>())
                  dst.AddOrReplaceComponent<Component>(src.GetComponent<Component>());
          }(),
          ...);
    }

    template <typename... Component> static void CopyComponentIfExistsPrefab(ComponentGroup<Component...>, Entity dst, Entity src)
    {
        CopyComponentIfExistsPrefab<Component...>(dst, src);
    }

    static void CopyAllExistingComponentsPrefab(Entity dst, Entity src) { CopyComponentIfExistsPrefab(AllComponents{}, dst, src); }

    Prefab::Prefab() { m_PrefabScene = CreateRef<Scene>(false); }

    Prefab::~Prefab() = default;

    void Prefab::CaptureFromEntity(const Scene& sourceScene, const Entity& rootEntity)
    {
        m_PrefabScene = CreateRef<Scene>(false);
        m_PrefabScene->CreateRootEntity();

        Entity prefabRoot = m_PrefabScene->CreateEntityWithUuid(rootEntity.GetUuid(), rootEntity.GetName());
        CopyAllExistingComponentsPrefab(prefabRoot, const_cast<Entity&>(rootEntity));

        // Strip PrefabComponent from the prefab definition
        prefabRoot.RemoveComponentIfExists<PrefabComponent>();

        m_RootEntityUuid = rootEntity.GetUuid();

        // Recursively capture children
        for (const auto& child : rootEntity.GetChildren())
            CaptureEntityRecursive(const_cast<Scene&>(sourceScene), child, *m_PrefabScene, &prefabRoot);
    }

    void Prefab::CaptureEntityRecursive(Scene& source, Entity sourceEntity, Scene& dest, Entity* destParent)
    {
        Entity newNode = dest.CreateEntityWithUuid(sourceEntity.GetUuid(), sourceEntity.GetName());
        CopyAllExistingComponentsPrefab(newNode, sourceEntity);

        // Strip PrefabComponent
        newNode.RemoveComponentIfExists<PrefabComponent>();

        if (destParent)
            newNode.SetParent(*destParent);

        for (const auto& child : sourceEntity.GetChildren())
            CaptureEntityRecursive(source, child, dest, &newNode);
    }

    Entity Prefab::GetRootEntity() const
    {
        if (!m_PrefabScene || m_RootEntityUuid.Empty())
            return Entity::Invalid;
        return m_PrefabScene->GetEntityFromUuid(m_RootEntityUuid);
    }

} // namespace Crowny
