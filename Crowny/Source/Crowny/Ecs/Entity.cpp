#include "cwpch.h"

#include "Crowny/Ecs/Components.h"
#include "Crowny/Ecs/Entity.h"
#include "Crowny/Scene/Scene.h"

namespace Crowny
{
    const Entity Entity::Invalid{};

    namespace
    {
        void RefreshSiblingIndices(Vector<Entity>& siblings, uint32_t firstIndex = 0)
        {
            for (uint32_t index = firstIndex; index < siblings.size(); index++)
            {
                if (siblings[index])
                    siblings[index].GetComponent<RelationshipComponent>().SiblingIndex = index;
            }
        }

        uint32_t ResolveSiblingIndex(const Vector<Entity>& siblings, Entity child, uint32_t cachedIndex)
        {
            if (cachedIndex < siblings.size() && siblings[cachedIndex] == child)
                return cachedIndex;

            const auto iter = std::find(siblings.begin(), siblings.end(), child);
            return iter == siblings.end() ? static_cast<uint32_t>(siblings.size()) : static_cast<uint32_t>(std::distance(siblings.begin(), iter));
        }

        bool RemoveChildReference(Entity parent, Entity child, uint32_t& removedIndex)
        {
            auto& siblings = parent.GetComponent<RelationshipComponent>().Children;
            auto& childRelationship = child.GetComponent<RelationshipComponent>();
            removedIndex = ResolveSiblingIndex(siblings, child, childRelationship.SiblingIndex);
            if (removedIndex >= siblings.size())
                return false;

            siblings.erase(siblings.begin() + removedIndex);
            childRelationship.SiblingIndex = 0;
            RefreshSiblingIndices(siblings, removedIndex);
            return true;
        }

        void InsertChildReference(Entity parent, Entity child, uint32_t index)
        {
            auto& siblings = parent.GetComponent<RelationshipComponent>().Children;
            auto& childRelationship = child.GetComponent<RelationshipComponent>();

            const uint32_t existingIndex = ResolveSiblingIndex(siblings, child, childRelationship.SiblingIndex);
            if (existingIndex < siblings.size())
            {
                siblings.erase(siblings.begin() + existingIndex);
                RefreshSiblingIndices(siblings, existingIndex);
            }

            index = std::min<uint32_t>(index, static_cast<uint32_t>(siblings.size()));
            siblings.insert(siblings.begin() + index, child);
            childRelationship.Parent = parent;
            RefreshSiblingIndices(siblings, index);
        }
    } // namespace

    EnttEntity::EnttEntity(entt::entity entity, Scene* scene) : m_EntityHandle(entity), m_Scene(scene) {}

    Entity::Entity(entt::entity entity, Scene* scene) : EnttEntity(entity, scene) {}

    void Entity::AddChild(Entity entity) { entity.SetParent(*this); }

    bool Entity::SetParent(Entity entity)
    {
        if (!IsValid() || (entity && (!entity.IsValid() || entity.m_Scene != m_Scene)))
            return false;
        if (m_Scene->m_RootEntity && *m_Scene->m_RootEntity == *this)
            return false;
        if (entity == *this || (entity && entity.IsChildOf(*this)))
            return false;

        Entity oldParent = GetParent();
        if (oldParent == entity)
            return true;

        const glm::mat4 oldWorldTransform = GetWorldMatrix();
        uint32_t oldSiblingIndex = 0;

        if (oldParent)
            RemoveChildReference(oldParent, *this, oldSiblingIndex);

        auto& relationship = GetComponent<RelationshipComponent>();
        relationship.Parent = entity;
        relationship.SiblingIndex = 0;
        if (entity)
            InsertChildReference(entity, *this, entity.GetChildCount());

        if (!GetTransform().SetWorldMatrix(oldWorldTransform, entity))
        {
            if (entity)
            {
                uint32_t ignoredIndex = 0;
                RemoveChildReference(entity, *this, ignoredIndex);
            }
            relationship.Parent = oldParent;
            relationship.SiblingIndex = 0;
            if (oldParent)
                InsertChildReference(oldParent, *this, oldSiblingIndex);
            return false;
        }

        m_Scene->MarkHierarchyTopologyDirty();
        NotifyTransformChanged();
        return true;
    }

    uint32_t Entity::GetSiblingIndex() const
    {
        const Entity parent = GetParent();
        if (!parent)
            return 0;
        const auto& siblings = parent.GetChildren();
        return ResolveSiblingIndex(siblings, *this, GetComponent<RelationshipComponent>().SiblingIndex);
    }

    bool Entity::SetSiblingIndex(uint32_t index)
    {
        Entity parent = GetParent();
        if (!parent)
            return false;
        auto& siblings = parent.GetComponent<RelationshipComponent>().Children;
        const uint32_t currentIndex = ResolveSiblingIndex(siblings, *this, GetComponent<RelationshipComponent>().SiblingIndex);
        if (currentIndex >= siblings.size())
            return false;
        index = std::min<uint32_t>(index, static_cast<uint32_t>(siblings.size() - 1));
        if (index == currentIndex)
            return true;

        Entity value = siblings[currentIndex];
        siblings.erase(siblings.begin() + currentIndex);
        siblings.insert(siblings.begin() + index, value);
        RefreshSiblingIndices(siblings, std::min(index, currentIndex));
        m_Scene->MarkHierarchyTopologyDirty();
        return true;
    }

    Entity Entity::GetChild(uint32_t index) const
    {
        const auto& children = GetComponent<RelationshipComponent>().Children;
        return index < children.size() ? children[index] : Entity{};
    }

    bool Entity::IsChildOf(Entity parent, bool directOnly) const
    {
        if (!parent || parent.GetScene() != m_Scene)
            return false;
        const auto& rc = parent.GetComponent<RelationshipComponent>();
        if (directOnly)
            return std::find(rc.Children.begin(), rc.Children.end(), *this) != rc.Children.end();

        Entity slow = GetParent();
        Entity fast = slow ? slow.GetParent() : Entity{};
        while (slow)
        {
            if (slow == parent)
                return true;
            if (fast && slow == fast)
                return false;

            slow = slow.GetParent();
            if (fast)
                fast = fast.GetParent();
            if (fast)
                fast = fast.GetParent();
        }
        return false;
    }

    const Vector<Entity>& Entity::GetChildren() const { return GetComponent<RelationshipComponent>().Children; }

    uint32_t Entity::GetChildCount() const { return (uint32_t)GetComponent<RelationshipComponent>().Children.size(); }

    Entity Entity::GetParent() const { return GetComponent<RelationshipComponent>().Parent; }

    const UUID& Entity::GetUuid() const { return GetComponent<IDComponent>().Uuid; }

    const TransformComponent& Entity::GetTransform() const { return GetComponent<TransformComponent>(); }

    TransformComponent& Entity::GetTransform() { return GetComponent<TransformComponent>(); }

    const String& Entity::GetName() const { return GetComponent<TagComponent>().Tag; }

    void Entity::Destroy(bool destroyChildren)
    {
        if (!IsValid())
            return;
        Scene* scene = m_Scene;
        const entt::entity handle = m_EntityHandle;
        const Array<Entity, 1> entities{ *this };
        const HierarchyDestroyMode mode = destroyChildren ? HierarchyDestroyMode::DestroySubtree : HierarchyDestroyMode::PreserveChildren;
        scene->DestroyEntities(entities, mode);
        if (!scene->m_Registry.valid(handle))
            Clear();
    }

    void Entity::NotifyTransformChanged(bool updatePhysics)
    {
        m_Scene->QueueTransformChange(*this, updatePhysics);
    }

    void Entity::SetPosition(const glm::vec3& position)
    {
        TransformComponent& transform = GetTransform();
        transform.SetPosition(position);
        NotifyTransformChanged();
    }

    void Entity::SetRotation(const glm::quat& rotation)
    {
        TransformComponent& transform = GetTransform();
        transform.SetRotation(rotation);
        NotifyTransformChanged();
    }

    void Entity::SetScale(const glm::vec3& scale)
    {
        TransformComponent& transform = GetTransform();
        transform.SetScale(scale);
        NotifyTransformChanged();
    }

    void Entity::SetLocalTransform(const Transform& transform, bool updatePhysics)
    {
        GetTransform().SetLocalTransform(transform);
        NotifyTransformChanged(updatePhysics);
    }

    void Entity::SetWorldPosition(const glm::vec3& position)
    {
        m_Scene->FlushTransformChanges();
        TransformComponent& transform = GetTransform();
        transform.SetWorldPosition(position, GetParent());
        NotifyTransformChanged();
    }

    void Entity::SetWorldRotation(const glm::quat& rotation)
    {
        m_Scene->FlushTransformChanges();
        TransformComponent& transform = GetTransform();
        transform.SetWorldRotation(rotation, GetParent());
        NotifyTransformChanged();
    }

    void Entity::SetWorldScale(const glm::vec3& scale)
    {
        m_Scene->FlushTransformChanges();
        TransformComponent& transform = GetTransform();
        transform.SetWorldScale(scale, GetParent());
        NotifyTransformChanged();
    }

    bool Entity::SetWorldTransform(const glm::mat4& worldTransform, bool updatePhysics)
    {
        m_Scene->FlushTransformChanges();
        if (!GetTransform().SetWorldMatrix(worldTransform, GetParent()))
            return false;
        NotifyTransformChanged(updatePhysics);
        return true;
    }

    glm::vec3 Entity::GetWorldPosition() const
    {
        m_Scene->ResolveWorldTransform(*this);
        return GetTransform().GetWorldTransform(GetParent()).GetPosition();
    }

    glm::quat Entity::GetWorldRotation() const
    {
        m_Scene->ResolveWorldTransform(*this);
        return GetTransform().GetWorldTransform(GetParent()).GetRotation();
    }

    glm::vec3 Entity::GetWorldScale() const
    {
        m_Scene->ResolveWorldTransform(*this);
        return GetTransform().GetWorldTransform(GetParent()).GetScale();
    }
    glm::vec3 Entity::GetLocalPosition() const { return GetTransform().GetLocalTransform().GetPosition(); }
    glm::quat Entity::GetLocalRotation() const { return GetTransform().GetLocalTransform().GetRotation(); }
    glm::vec3 Entity::GetLocalScale() const { return GetTransform().GetLocalTransform().GetScale(); }

    const Transform& Entity::GetWorldTransform() const
    {
        m_Scene->ResolveWorldTransform(*this);
        const TransformComponent& transform = GetTransform();
        return transform.GetWorldTransform(GetParent());
    }

    const Transform& Entity::GetLocalTransform() const
    {
        const TransformComponent& transform = GetTransform();
        return transform.GetLocalTransform();
    }

    const glm::mat4& Entity::GetWorldMatrix() const
    {
        m_Scene->ResolveWorldTransform(*this);
        const TransformComponent& transform = GetTransform();
        return transform.GetWorldMatrix(GetParent());
    }

} // namespace Crowny
