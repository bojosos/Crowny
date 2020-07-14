#pragma once
/*
namespace entt
{
	struct registry;
	struct entity;
}*/

namespace Crowny
{
	class EntityManager
	{
	public:
		//static entt::entity CreateEntity();
		//static void DeleteEntity(const entt::entity& entity);
	private:
		static EntityManager *s_Instance;
	public:
		//entt::registry m_Registry;
	};
}