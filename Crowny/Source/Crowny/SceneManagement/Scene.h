#pragma once

#include "Crowny/Ecs/Entity.h"
#include "Crowny/Common/Timestep.h"
#include "entt/entt.hpp"

namespace Crowny
{
	// will change everything
	class Scene
	{
	public:
		Scene(const std::string& name);
		~Scene() = default;

		void OnUpdate(Timestep ts);

	private:
		std::vector<Ref<Entity>> m_Entities;
		uint32_t m_BuildIndex;
		std::string m_Name;
		std::string m_Filepath;
		bool m_IsLoaded;
		entt::registry m_Registry;

	public:
		Entity& CreateEntity();
	};
}
