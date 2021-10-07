#include "cwpch.h"

#include "Crowny/Ecs/Components.h"
#include "Crowny/Ecs/Entity.h"
#include "Crowny/Scene/Scene.h"

namespace Crowny
{
    Entity::Entity(entt::entity entity, Scene* scene) : m_EntityHandle(entity), m_Scene(scene) {}

    bool Entity::IsValid() const
    {
        return m_Scene && m_EntityHandle != entt::null && m_Scene->m_Registry.valid(m_EntityHandle);
    }

    void Entity::AddChild(Entity entity)
    {
        auto& rc = GetComponent<RelationshipComponent>();
        rc.Children.push_back(entity);
        entity.GetComponent<RelationshipComponent>().Parent = *this;
    }

    void Entity::SetParent(Entity entity)
    {
        auto& children = GetParent().GetComponent<RelationshipComponent>().Children;
        for (int i = 0; i < children.size(); i++)
        {
            if (children[i] == *this)
            {
                children.erase(children.begin() + i);
                break;
            }
        }

        GetComponent<RelationshipComponent>().Parent = entity;
        entity.AddChild(*this);
    }

    Entity Entity::GetChild(uint32_t index) const { return GetComponent<RelationshipComponent>().Children[index]; }

    const Vector<Entity>& Entity::GetChildren() const { return GetComponent<RelationshipComponent>().Children; }

    uint32_t Entity::GetChildCount() const { return (uint32_t)GetComponent<RelationshipComponent>().Children.size(); }

    Entity Entity::GetParent() const { return GetComponent<RelationshipComponent>().Parent; }

    const UUID& Entity::GetUuid() const { return GetComponent<IDComponent>().Uuid; }
    const TransformComponent& Entity::GetTransform() const { return GetComponent<TransformComponent>(); }
    const String& Entity::GetName() const { return GetComponent<TagComponent>().Tag; }

    void Entity::Destroy()
    {
        m_Scene->m_Registry.destroy(m_EntityHandle);
        m_EntityHandle = entt::null;
    }

} // namespace Crowny
