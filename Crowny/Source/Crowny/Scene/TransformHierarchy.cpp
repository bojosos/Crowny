#include "cwpch.h"

#include "Crowny/Scene/Scene.h"

#include "Crowny/Ecs/Components.h"
#include "Crowny/Ecs/Entity.h"
#include "Crowny/Physics/Physics2D.h"

namespace Crowny
{
    Scene::TransformMutationScope::TransformMutationScope(Scene* scene) : m_Scene(scene)
    {
        if (m_Scene != nullptr)
            m_Scene->BeginTransformMutation();
    }

    Scene::TransformMutationScope::TransformMutationScope(TransformMutationScope&& other) noexcept
      : m_Scene(std::exchange(other.m_Scene, nullptr))
    {
    }

    Scene::TransformMutationScope& Scene::TransformMutationScope::operator=(TransformMutationScope&& other) noexcept
    {
        if (this == &other)
            return *this;
        if (m_Scene != nullptr)
            m_Scene->EndTransformMutation();
        m_Scene = std::exchange(other.m_Scene, nullptr);
        return *this;
    }

    Scene::TransformMutationScope::~TransformMutationScope()
    {
        if (m_Scene != nullptr)
            m_Scene->EndTransformMutation();
    }

    Scene::TransformMutationScope Scene::DeferTransformChanges() { return TransformMutationScope(this); }

    void Scene::BeginTransformMutation() { m_TransformMutationDepth++; }

    void Scene::EndTransformMutation()
    {
        CW_ENGINE_ASSERT(m_TransformMutationDepth > 0u);
        if (m_TransformMutationDepth == 0u)
            return;
        if (m_TransformMutationDepth == 1u)
        {
            FlushTransformChanges();
            m_TransformMutationDepth = 0u;
        }
        else
        {
            m_TransformMutationDepth--;
        }
    }

    void Scene::MarkHierarchyTopologyDirty()
    {
        m_HierarchyTopologyDirty = true;
        m_HierarchyTopologyGeneration++;
        if (m_HierarchyTopologyGeneration == 0u)
            m_HierarchyTopologyGeneration = 1u;
    }

    void Scene::ResetTransformHierarchyCache()
    {
        m_TransformHierarchyOrder.clear();
        m_TransformHierarchyIndexSlots.clear();
        m_TransformHierarchyBuildStack.clear();
        m_ImmediateTransformTraversalStack.clear();
        m_WorldTransformResolveStack.clear();
        m_PendingTransformChanges.clear();
        m_PendingTransformSlots.clear();
        m_ProcessingTransformChanges.clear();
        m_TransformRangeEvents.clear();
        m_LastTransformPropagationStats = {};
        m_PendingTransformQueueRequestCount = 0u;
        m_TransformQueueGeneration++;
        if (m_TransformQueueGeneration == 0u)
            m_TransformQueueGeneration = 1u;
        m_TransformHierarchyCacheGeneration++;
        if (m_TransformHierarchyCacheGeneration == 0u)
            m_TransformHierarchyCacheGeneration = 1u;
        m_FlushingTransformChanges = false;
        MarkHierarchyTopologyDirty();
    }

    void Scene::RebuildTransformHierarchyCache()
    {
        m_TransformHierarchyOrder.clear();
        m_TransformHierarchyBuildStack.clear();

        m_TransformHierarchyCacheGeneration++;
        if (m_TransformHierarchyCacheGeneration == 0u)
            m_TransformHierarchyCacheGeneration = 1u;

        const auto relationships = m_Registry.view<RelationshipComponent>();
        const size_t relationshipCount = m_Registry.storage<RelationshipComponent>().size();
        m_TransformHierarchyOrder.reserve(relationshipCount);
        m_TransformHierarchyBuildStack.reserve(relationshipCount);
        size_t requiredIndexSlotCount = 0u;
        for (const entt::entity handle : relationships)
            requiredIndexSlotCount =
              std::max(requiredIndexSlotCount, static_cast<size_t>(entt::entt_traits<entt::entity>::to_entity(handle)) + 1u);
        if (m_TransformHierarchyIndexSlots.size() < requiredIndexSlotCount)
            m_TransformHierarchyIndexSlots.resize(requiredIndexSlotCount);

        const auto appendSubtree = [&](entt::entity rootHandle) {
            m_TransformHierarchyBuildStack.push_back({ rootHandle, std::numeric_limits<uint32_t>::max(),
                                                       std::numeric_limits<uint32_t>::max(), false });
            while (!m_TransformHierarchyBuildStack.empty())
            {
                const TransformHierarchyBuildEntry buildEntry = m_TransformHierarchyBuildStack.back();
                m_TransformHierarchyBuildStack.pop_back();
                if (buildEntry.Expanded)
                {
                    if (buildEntry.OrderIndex < m_TransformHierarchyOrder.size())
                        m_TransformHierarchyOrder[buildEntry.OrderIndex].SubtreeEnd =
                          static_cast<uint32_t>(m_TransformHierarchyOrder.size());
                    continue;
                }
                if (!m_Registry.valid(buildEntry.Handle))
                    continue;
                const RelationshipComponent* relationship = m_Registry.try_get<RelationshipComponent>(buildEntry.Handle);
                const size_t slotIndex = entt::entt_traits<entt::entity>::to_entity(buildEntry.Handle);
                if (slotIndex >= m_TransformHierarchyIndexSlots.size())
                    m_TransformHierarchyIndexSlots.resize(slotIndex + 1u);
                TransformHierarchyIndexSlot& slot = m_TransformHierarchyIndexSlots[slotIndex];
                if (relationship == nullptr ||
                    (slot.Generation == m_TransformHierarchyCacheGeneration && slot.Handle == buildEntry.Handle))
                    continue;

                const uint32_t orderIndex = static_cast<uint32_t>(m_TransformHierarchyOrder.size());
                slot.Handle = buildEntry.Handle;
                slot.Generation = m_TransformHierarchyCacheGeneration;
                slot.OrderIndex = orderIndex;
                m_TransformHierarchyOrder.push_back({ buildEntry.Handle, buildEntry.ParentIndex, orderIndex + 1u });
                m_TransformHierarchyBuildStack.push_back({ buildEntry.Handle, buildEntry.ParentIndex, orderIndex, true });

                for (auto child = relationship->Children.rbegin(); child != relationship->Children.rend(); ++child)
                {
                    if (!*child || child->GetScene() != this || child->GetParent().GetHandle() != buildEntry.Handle)
                        continue;
                    m_TransformHierarchyBuildStack.push_back(
                      { child->GetHandle(), orderIndex, std::numeric_limits<uint32_t>::max(), false });
                }
            }
        };

        if (m_RootEntity != nullptr && *m_RootEntity)
            appendSubtree(m_RootEntity->GetHandle());
        for (const entt::entity handle : relationships)
        {
            const RelationshipComponent& relationship = relationships.get<RelationshipComponent>(handle);
            if (!relationship.Parent)
                appendSubtree(handle);
        }
        // Corrupt or partially reconstructed relationships must not make an entity disappear from the derived cache.
        for (const entt::entity handle : relationships)
            appendSubtree(handle);

        m_HierarchyTopologyDirty = false;
    }

    void Scene::QueueTransformChange(Entity entity, bool updatePhysics)
    {
        if (!entity || entity.GetScene() != this ||
            !m_Registry.all_of<TransformComponent, RelationshipComponent>(entity.GetHandle()))
            return;

        m_PendingTransformQueueRequestCount++;
        const size_t slotIndex = entt::entt_traits<entt::entity>::to_entity(entity.GetHandle());
        if (slotIndex >= m_PendingTransformSlots.size())
            m_PendingTransformSlots.resize(slotIndex + 1u);
        PendingTransformSlot& slot = m_PendingTransformSlots[slotIndex];
        if (slot.Generation != m_TransformQueueGeneration || slot.Handle != entity.GetHandle())
        {
            const uint32_t index = static_cast<uint32_t>(m_PendingTransformChanges.size());
            slot.Handle = entity.GetHandle();
            slot.Generation = m_TransformQueueGeneration;
            slot.ChangeIndex = index;
            m_PendingTransformChanges.push_back({ entity.GetHandle(), updatePhysics });
        }
        else
        {
            CW_ENGINE_ASSERT(slot.ChangeIndex < m_PendingTransformChanges.size());
            m_PendingTransformChanges[slot.ChangeIndex].UpdatePhysics |= updatePhysics;
        }

        if (m_TransformMutationDepth == 0u && !m_FlushingTransformChanges)
            FlushTransformChanges();
    }

    void Scene::ResolveWorldTransform(Entity entity)
    {
        FlushTransformChanges();
        if (!entity || entity.GetScene() != this)
            return;

        m_WorldTransformResolveStack.clear();
        Entity current = entity;
        while (current)
        {
            TransformComponent& transform = current.GetTransform();
            if (transform.IsCachedWorldTransformValid())
                break;
            m_WorldTransformResolveStack.push_back(current.GetHandle());
            current = current.GetParent();
        }

        for (auto iter = m_WorldTransformResolveStack.rbegin(); iter != m_WorldTransformResolveStack.rend(); ++iter)
        {
            if (!m_Registry.valid(*iter))
                continue;
            TransformComponent* transform = m_Registry.try_get<TransformComponent>(*iter);
            RelationshipComponent* relationship = m_Registry.try_get<RelationshipComponent>(*iter);
            if (transform != nullptr && relationship != nullptr)
            {
                const glm::mat4* parentWorld = nullptr;
                if (relationship->Parent && relationship->Parent.GetScene() == this)
                {
                    const TransformComponent* parentTransform =
                      m_Registry.try_get<TransformComponent>(relationship->Parent.GetHandle());
                    if (parentTransform != nullptr && parentTransform->IsCachedWorldTransformValid())
                        parentWorld = &parentTransform->WorldTransformCache;
                }
                transform->UpdateWorldTransform(parentWorld);
            }
        }
    }

    void Scene::ProcessTransformEntity(entt::entity handle, bool updatePhysics)
    {
        if (!m_Registry.valid(handle))
            return;
        TransformComponent* transform = m_Registry.try_get<TransformComponent>(handle);
        RelationshipComponent* relationship = m_Registry.try_get<RelationshipComponent>(handle);
        if (transform == nullptr || relationship == nullptr)
            return;

        m_LastTransformPropagationStats.VisitedEntityCount++;
        if (updatePhysics)
            m_LastTransformPropagationStats.PhysicsEnabledEntityVisitCount++;
        transform->InvalidateWorld();
        AudioListenerComponent* listener = m_Registry.try_get<AudioListenerComponent>(handle);
        AudioSourceComponent* source = m_Registry.try_get<AudioSourceComponent>(handle);
        if (listener != nullptr || source != nullptr)
        {
            ResolveWorldTransform(Entity{ handle, this });
            const Transform& worldTransform = transform->GetWorldTransform(relationship->Parent);
            if (listener != nullptr)
                listener->OnTransformChanged(worldTransform);
            if (source != nullptr)
                source->OnTransformChanged(worldTransform);
        }

        Entity entity{ handle, this };
        if (updatePhysics && m_Registry.all_of<Rigidbody2DComponent>(handle) && Physics2D::IsStartedUp() &&
            Physics2D::TryGet()->IsSimulating())
            Physics2D::TryGet()->UpdateTransform(entity);
        if (updatePhysics)
            UpdatePhysics3DTransform(entity);
    }

    void Scene::ProcessTransformRange(uint32_t begin, uint32_t end, bool updatePhysics)
    {
        end = std::min<uint32_t>(end, static_cast<uint32_t>(m_TransformHierarchyOrder.size()));
        for (uint32_t index = begin; index < end; index++)
            ProcessTransformEntity(m_TransformHierarchyOrder[index].Handle, updatePhysics);
    }

    void Scene::ProcessTransformSubtree(entt::entity root, bool updatePhysics)
    {
        m_ImmediateTransformTraversalStack.clear();
        m_ImmediateTransformTraversalStack.push_back(root);
        while (!m_ImmediateTransformTraversalStack.empty())
        {
            const entt::entity handle = m_ImmediateTransformTraversalStack.back();
            m_ImmediateTransformTraversalStack.pop_back();
            ProcessTransformEntity(handle, updatePhysics);
            if (!m_Registry.valid(handle))
                continue;
            const RelationshipComponent* relationship = m_Registry.try_get<RelationshipComponent>(handle);
            if (relationship == nullptr)
                continue;
            for (auto child = relationship->Children.rbegin(); child != relationship->Children.rend(); ++child)
            {
                if (*child && child->GetScene() == this && child->GetParent().GetHandle() == handle)
                    m_ImmediateTransformTraversalStack.push_back(child->GetHandle());
            }
        }
    }

    void Scene::FlushTransformChanges()
    {
        if (m_FlushingTransformChanges || m_PendingTransformChanges.empty())
            return;

        m_FlushingTransformChanges = true;
        m_LastTransformPropagationStats = {};

        if (m_HierarchyTopologyDirty && m_TransformMutationDepth == 0u)
        {
            while (!m_PendingTransformChanges.empty())
            {
                m_ProcessingTransformChanges.clear();
                m_ProcessingTransformChanges.swap(m_PendingTransformChanges);
                m_TransformQueueGeneration++;
                if (m_TransformQueueGeneration == 0u)
                    m_TransformQueueGeneration = 1u;
                m_LastTransformPropagationStats.QueueRequestCount += std::exchange(m_PendingTransformQueueRequestCount, 0u);
                m_LastTransformPropagationStats.FlushPassCount++;
                m_LastTransformPropagationStats.UniqueRootCount += static_cast<uint32_t>(m_ProcessingTransformChanges.size());
                m_LastTransformPropagationStats.MergedRangeCount += static_cast<uint32_t>(m_ProcessingTransformChanges.size());
                for (const PendingTransformChange& change : m_ProcessingTransformChanges)
                    ProcessTransformSubtree(change.Root, change.UpdatePhysics);
                m_ProcessingTransformChanges.clear();
            }
            m_FlushingTransformChanges = false;
            return;
        }

        while (!m_PendingTransformChanges.empty())
        {
            if (m_HierarchyTopologyDirty)
                RebuildTransformHierarchyCache();

            m_ProcessingTransformChanges.clear();
            m_ProcessingTransformChanges.swap(m_PendingTransformChanges);
            m_TransformQueueGeneration++;
            if (m_TransformQueueGeneration == 0u)
                m_TransformQueueGeneration = 1u;
            m_LastTransformPropagationStats.QueueRequestCount += std::exchange(m_PendingTransformQueueRequestCount, 0u);
            m_LastTransformPropagationStats.FlushPassCount++;

            m_TransformRangeEvents.clear();
            m_TransformRangeEvents.reserve(m_ProcessingTransformChanges.size() * 2u);
            for (const PendingTransformChange& change : m_ProcessingTransformChanges)
            {
                const size_t slotIndex = entt::entt_traits<entt::entity>::to_entity(change.Root);
                if (slotIndex >= m_TransformHierarchyIndexSlots.size())
                    continue;
                const TransformHierarchyIndexSlot& slot = m_TransformHierarchyIndexSlots[slotIndex];
                if (slot.Generation != m_TransformHierarchyCacheGeneration || slot.Handle != change.Root ||
                    slot.OrderIndex >= m_TransformHierarchyOrder.size())
                    continue;
                const TransformHierarchyEntry& hierarchyEntry = m_TransformHierarchyOrder[slot.OrderIndex];
                if (hierarchyEntry.SubtreeEnd <= slot.OrderIndex)
                    continue;
                m_LastTransformPropagationStats.UniqueRootCount++;
                m_TransformRangeEvents.push_back({ slot.OrderIndex, 1, change.UpdatePhysics ? 1 : 0 });
                m_TransformRangeEvents.push_back({ hierarchyEntry.SubtreeEnd, -1, change.UpdatePhysics ? -1 : 0 });
            }
            std::sort(m_TransformRangeEvents.begin(), m_TransformRangeEvents.end(),
                      [](const TransformRangeEvent& lhs, const TransformRangeEvent& rhs) { return lhs.OrderIndex < rhs.OrderIndex; });

            const uint64_t topologyGeneration = m_HierarchyTopologyGeneration;
            int32_t activeRangeCount = 0;
            int32_t activePhysicsRangeCount = 0;
            uint32_t previousIndex = m_TransformRangeEvents.empty() ? 0u : m_TransformRangeEvents.front().OrderIndex;
            size_t eventIndex = 0u;
            bool restartRequested = false;
            while (eventIndex < m_TransformRangeEvents.size())
            {
                const uint32_t orderIndex = m_TransformRangeEvents[eventIndex].OrderIndex;
                if (previousIndex < orderIndex && activeRangeCount > 0)
                {
                    ProcessTransformRange(previousIndex, orderIndex, activePhysicsRangeCount > 0);
                    if (m_HierarchyTopologyGeneration != topologyGeneration)
                    {
                        restartRequested = true;
                        break;
                    }
                }

                const int32_t previousActiveRangeCount = activeRangeCount;
                while (eventIndex < m_TransformRangeEvents.size() && m_TransformRangeEvents[eventIndex].OrderIndex == orderIndex)
                {
                    activeRangeCount += m_TransformRangeEvents[eventIndex].ActiveDelta;
                    activePhysicsRangeCount += m_TransformRangeEvents[eventIndex].PhysicsDelta;
                    eventIndex++;
                }
                if (previousActiveRangeCount == 0 && activeRangeCount > 0)
                    m_LastTransformPropagationStats.MergedRangeCount++;
                previousIndex = orderIndex;
            }

            if (restartRequested)
            {
                for (const PendingTransformChange& change : m_ProcessingTransformChanges)
                    QueueTransformChange(Entity{ change.Root, this }, change.UpdatePhysics);
            }
            m_ProcessingTransformChanges.clear();
        }

        m_FlushingTransformChanges = false;
    }
} // namespace Crowny
