#include "cwpch.h"

#include "Crowny/Ecs/Components.h"
#include <imgui.h>

namespace Crowny
{
	template <>
	void ComponentEditorWidget<Components::Transform>(entt::registry& reg, entt::registry::entity_type e)
	{
		auto& t = reg.get<Components::Transform>(e);
		ImGui::DragFloat("x##Transform", &t.x, 0.1f);
		ImGui::DragFloat("y##Transform", &t.y, 0.1f);
	}

	template <>
	void ComponentEditorWidget<Components::Camera>(entt::registry& reg, entt::registry::entity_type e)
	{
		auto& v = reg.get<Components::Camera>(e);
		ImGui::DragFloat("x##Fov", &v.fov, 0.1f);
	}
}