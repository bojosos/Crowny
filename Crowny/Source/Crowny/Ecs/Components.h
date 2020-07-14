#pragma once

#include <entt/entt.hpp>
#include "Crowny/ImGui/ImGuiComponentEditor.h"

namespace Components
{
	struct Transform
	{
		float x, y;
	};

	struct Camera
	{
		float fov;
	};
}

namespace Crowny
{
	template <>
	void ComponentEditorWidget<Components::Transform>(entt::registry& reg, entt::registry::entity_type e);

	template <>
	void ComponentEditorWidget<Components::Camera>(entt::registry& reg, entt::registry::entity_type e);
}