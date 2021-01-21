#include "cwepch.h"

#include "ImGuiPanel.h"

#include <imgui.h>

namespace Crowny
{
	ImGuiPanel::ImGuiPanel(const std::string& name) : m_Name(name), m_Shown(true)
	{

	}
	
	void ImGuiPanel::UpdateState()
	{
		m_Hovered = ImGui::IsWindowHovered();
		m_Focused = ImGui::IsWindowFocused();
	}
}