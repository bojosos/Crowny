#include "cwpch.h"

#include "Crowny/Ecs/Components.h"
#include "Crowny/Ecs/Entity.h"
#include "Crowny/Physics/Physics2D.h"
#include "Crowny/Scene/Scene.h"

namespace Crowny
{
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
        {
            auto& children = oldParent.GetChildren();
            auto iterFind = std::find(children.begin(), children.end(), *this);
            if (iterFind != children.end())
            {
                oldSiblingIndex = static_cast<uint32_t>(std::distance(children.begin(), iterFind));
                children.erase(iterFind);
            }
        }

        GetComponent<RelationshipComponent>().Parent = entity;
        if (entity)
        {
            auto& children = entity.GetChildren();
            if (std::find(children.begin(), children.end(), *this) == children.end())
                children.push_back(*this);
        }

        if (!GetTransform().SetWorldMatrix(oldWorldTransform, entity))
        {
            if (entity)
            {
                auto& children = entity.GetChildren();
                children.erase(std::remove(children.begin(), children.end(), *this), children.end());
            }
            GetComponent<RelationshipComponent>().Parent = oldParent;
            if (oldParent)
            {
                auto& children = oldParent.GetChildren();
                const auto insertAt = children.begin() + std::min<size_t>(oldSiblingIndex, children.size());
                children.insert(insertAt, *this);
            }
            return false;
        }

        NotifyTransformChanged();
        return true;
    }

    uint32_t Entity::GetSiblingIndex() const
    {
        const Entity parent = GetParent();
        if (!parent)
            return 0;
        const auto& siblings = parent.GetChildren();
        const auto it = std::find(siblings.begin(), siblings.end(), *this);
        return it == siblings.end() ? static_cast<uint32_t>(siblings.size()) : static_cast<uint32_t>(std::distance(siblings.begin(), it));
    }

    bool Entity::SetSiblingIndex(uint32_t index)
    {
        Entity parent = GetParent();
        if (!parent)
            return false;
        auto& siblings = parent.GetChildren();
        const auto current = std::find(siblings.begin(), siblings.end(), *this);
        if (current == siblings.end())
            return false;
        Entity value = *current;
        siblings.erase(current);
        index = std::min<uint32_t>(index, static_cast<uint32_t>(siblings.size()));
        siblings.insert(siblings.begin() + index, value);
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

    Vector<Entity>& Entity::GetChildren() { return GetComponent<RelationshipComponent>().Children; }

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
        if (m_Scene->m_RootEntity && *m_Scene->m_RootEntity == *this)
            return;

        if (destroyChildren)
        {
            const Vector<Entity> children = GetChildren();
            for (Entity child : children)
                child.Destroy(true);
        }
        else
        {
            const Vector<Entity> children = GetChildren();
            Entity parent = GetParent();
            uint32_t insertionIndex = GetSiblingIndex();
            for (Entity child : children)
            {
                if (child.SetParent(parent) && parent)
                    child.SetSiblingIndex(insertionIndex++);
            }
        }

        m_Scene->m_EntityMap.erase(GetUuid());

        Entity parent = GetParent();
        if (parent)
        {
            Vector<Entity>& parentChildren = parent.GetChildren();
            auto iterFind = std::find(parentChildren.begin(), parentChildren.end(), *this);
            if (iterFind != parentChildren.end())
                parentChildren.erase(iterFind);
        }

        m_Scene->m_Registry.destroy(m_EntityHandle);
        m_EntityHandle = entt::null;
        m_Scene = nullptr;
    }

    void Entity::NotifyTransformChanged(bool updatePhysics)
    {
        TransformComponent& tc = GetTransform();
        tc.InvalidateWorld();
        Entity parent = GetParent();
        AudioListenerComponent* listener = m_Scene->m_Registry.try_get<AudioListenerComponent>(m_EntityHandle);
        AudioSourceComponent* source = m_Scene->m_Registry.try_get<AudioSourceComponent>(m_EntityHandle);
        if (listener || source)
        {
            const Transform& worldTransform = tc.GetWorldTransform(parent);
            if (listener)
                listener->OnTransformChanged(worldTransform);
            if (source)
                source->OnTransformChanged(worldTransform);
        }
        if (updatePhysics && HasComponent<Rigidbody2DComponent>() && Physics2D::IsStartedUp() && Physics2D::TryGet()->IsSimulating())
            Physics2D::TryGet()->UpdateTransform(*this);
        if (updatePhysics)
            m_Scene->UpdatePhysics3DTransform(*this);
        for (Entity child : GetChildren())
            child.NotifyTransformChanged();
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

    void Entity::SetWorldPosition(const glm::vec3& position)
    {
        TransformComponent& transform = GetTransform();
        transform.SetWorldPosition(position, GetParent());
        NotifyTransformChanged();
    }

    void Entity::SetWorldRotation(const glm::quat& rotation)
    {
        TransformComponent& transform = GetTransform();
        transform.SetWorldRotation(rotation, GetParent());
        NotifyTransformChanged();
    }

    void Entity::SetWorldScale(const glm::vec3& scale)
    {
        TransformComponent& transform = GetTransform();
        transform.SetWorldScale(scale, GetParent());
        NotifyTransformChanged();
    }

    bool Entity::SetWorldTransform(const glm::mat4& worldTransform, bool updatePhysics)
    {
        if (!GetTransform().SetWorldMatrix(worldTransform, GetParent()))
            return false;
        NotifyTransformChanged(updatePhysics);
        return true;
    }

    glm::vec3 Entity::GetWorldPosition() const { return GetTransform().GetWorldTransform(GetParent()).GetPosition(); }
    glm::quat Entity::GetWorldRotation() const { return GetTransform().GetWorldTransform(GetParent()).GetRotation(); }
    glm::vec3 Entity::GetWorldScale() const { return GetTransform().GetWorldTransform(GetParent()).GetScale(); }
    glm::vec3 Entity::GetLocalPosition() const { return GetTransform().GetLocalTransform().GetPosition(); }
    glm::quat Entity::GetLocalRotation() const { return GetTransform().GetLocalTransform().GetRotation(); }
    glm::vec3 Entity::GetLocalScale() const { return GetTransform().GetLocalTransform().GetScale(); }

    const Transform& Entity::GetWorldTransform() const
    {
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
        const TransformComponent& transform = GetTransform();
        return transform.GetWorldMatrix(GetParent());
    }

} // namespace Crowny
