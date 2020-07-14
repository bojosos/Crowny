#pragma once

#include "Crowny/Ecs/Components.h"
#include <entt/entt.hpp>

namespace Crowny
{
	class Entity
	{
	public:
		template <typename Component>
		void AddComponent();

		template <typename Component>
		void RemoveComponent();

	private:
		entt::entity m_Entity;

	};
}