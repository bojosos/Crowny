#include "cwpch.h"

#include "Crowny/Ecs/Entity.h"

namespace Crowny
{
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