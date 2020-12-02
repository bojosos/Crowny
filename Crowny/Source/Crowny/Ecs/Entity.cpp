#include "cwpch.h"

#include "Crowny/Scene/Scene.h"
#include "Crowny/Ecs/Entity.h"
#include "Crowny/Ecs/Components.h"

namespace Crowny
{
	Entity::Entity(entt::entity entity, Scene* scene) : m_EntityHandle(entity), m_Scene(scene)
	{
		
	}

	bool Entity::IsValid() const { return m_Scene && m_Scene->m_Registry.valid(m_EntityHandle); }

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

	Entity Entity::GetChild(uint32_t index) const
	{
		return GetComponent<RelationshipComponent>().Children[index];
	}

	const std::vector<Entity>& Entity::GetChildren() const
	{
		return GetComponent<RelationshipComponent>().Children;
	}

	uint32_t Entity::GetChildCount() const
	{
		return (uint32_t)GetComponent<RelationshipComponent>().Children.size();
	}

	Entity Entity::GetParent() const
	{
		return GetComponent<RelationshipComponent>().Parent;
	}

	void Entity::Destroy()
	{
		m_Scene->m_Registry.destroy(m_EntityHandle);
		m_EntityHandle = entt::null;
	}

}
