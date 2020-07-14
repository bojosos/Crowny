#include "cwpch.h"

#include "Crowny/Ecs/EntityManager.h"
#include <entt/entt.hpp>

namespace Crowny
{

	EntityManager *EntityManager::s_Instance;
	/*
	entt::entity EntityManager::CreateEntity()
	{
		return s_Instance->m_Registry.create();
	}

	void EntityManager::DeleteEntity(const entt::entity& entity)
	{
		s_Instance->m_Registry.remove(entity);
	}*/

}