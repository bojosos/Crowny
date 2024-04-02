#include "cwpch.h"

#include "Crowny/Ecs/Components.h"
#include "Crowny/Ecs/Entity.h"
#include "Crowny/Scene/Scene.h"

namespace Crowny
{
    EnttEntity::EnttEntity(entt::entity entity, Scene* scene) : m_EntityHandle(entity), m_Scene(scene) {}

    Entity::Entity(entt::entity entity, Scene* scene) : EnttEntity(entity, scene) {}

    void Entity::AddChild(Entity entity)
    {
        if (entity.IsChildOf(*this))
            return;
        entity.SetParent(*this);
    }

    void Entity::SetParent(Entity entity)
    {
        if (IsChildOf(entity))
            return;
        // Remove the child from the old parent.
        Entity oldParent = GetParent();
        if (oldParent)
        {
            auto& children = oldParent.GetComponent<RelationshipComponent>().Children;
            auto iterFind = std::find(children.begin(), children.end(), *this);
            if (iterFind != children.end())
                children.erase(iterFind);
        }

        GetComponent<RelationshipComponent>().Parent = entity;
        entity.GetComponent<RelationshipComponent>().Children.push_back(*this);
    }

    Entity Entity::GetChild(uint32_t index) const { return GetComponent<RelationshipComponent>().Children[index]; }

    bool Entity::IsChildOf(Entity parent, bool directOnly)
    {
        const auto& rc = parent.GetComponent<RelationshipComponent>();
        if (directOnly)
            return std::find(rc.Children.begin(), rc.Children.end(), *this) != rc.Children.end();
        else
        {
            parent = rc.Parent;
            while (parent)
            {
                if (parent == *this)
                    return true;
                parent = parent.GetParent();
            }
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
        Entity parent = GetParent();
        if (parent && !destroyChildren)
        {
            // Remove it from the parent.
            Vector<Entity>& parentChildren = parent.GetChildren();
            auto iterFind = std::find(parentChildren.begin(), parentChildren.end(), *this);
            CW_ENGINE_ASSERT(iterFind != parentChildren.end());
            CW_ENGINE_INFO("Erasing: {0}", GetName());
            if (iterFind != parentChildren.end())
                parentChildren.erase(iterFind);
        }
        if (destroyChildren)
        {
            Vector<Entity>& children = GetChildren();
            for (Entity child : children)
            {
                child.GetComponent<RelationshipComponent>().Parent.Clear();
                child.Destroy(true);
            }
            children.clear();
        }
        CW_ENGINE_INFO("Clearing: {0}", GetName());
        m_Scene->m_Registry.destroy(m_EntityHandle);
        m_EntityHandle = entt::null;
    }

    void Entity::NotifyTransformChanged(const Transform& transform)
    {
        NotifyTransformChangedComponentWrapper(TransformChangedNotifyComponents{}, transform);
    }

    void Entity::SetPosition(const glm::vec3& position)
    {
        TransformComponent& transform = GetTransform();
        transform.SetPosition(position);
        NotifyTransformChanged(transform.GetWorldTransform(GetParent()));
    }

    void Entity::SetRotation(const glm::quat& rotation)
    {
        TransformComponent& transform = GetTransform();
        transform.SetRotation(rotation);
        NotifyTransformChanged(transform.GetWorldTransform(GetParent()));
    }

    void Entity::SetScale(const glm::vec3& scale)
    {
        TransformComponent& transform = GetTransform();
        transform.SetScale(scale);
        NotifyTransformChanged(transform.GetWorldTransform(GetParent()));
    }

    void Entity::SetWorldPosition(const glm::vec3& position)
    {
        TransformComponent& transform = GetTransform();
        transform.SetWorldPosition(position, GetParent());
        NotifyTransformChanged(transform.GetWorldTransform(GetParent()));
    }

    void Entity::SetWorldRotation(const glm::quat& rotation)
    {
        TransformComponent& transform = GetTransform();
        transform.SetRotation(rotation);
        NotifyTransformChanged(transform.GetWorldTransform(GetParent()));
    }

    void Entity::SetWorldScale(const glm::vec3& scale)
    {
        TransformComponent& transform = GetTransform();
        transform.SetWorldScale(scale, GetParent());
        NotifyTransformChanged(transform.GetWorldTransform(GetParent()));
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