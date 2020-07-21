#include "cwpch.h"

#include "Crowny/Ecs/Entity.h"

namespace Crowny
{
	Entity::Entity(entt::entity entity) : m_Entity(entity)
	{

	}

	template <typename Component>
	void Entity::AddComponent()
	{
		m_Registry.emplace<Component>(m_Entity);
	}

	template <typename Component>
	void Entity::RemoveComponent()
	{
		m_Registry.remove<Component>(m_Entity);
	}
}