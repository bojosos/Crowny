#include "cwepch.h"

#include "ImGuiViewportPanel.h"
#include "Crowny/Application/Application.h"
#include "Crowny/Input/Input.h"

#include <imgui.h>

namespace Crowny
{

	static Ref<EnvironmentMap> envmap;
	ImGuiViewportPanel::ImGuiViewportPanel(const std::string& name, const Ref<Framebuffer>& framebuffer, glm::vec2& viewportSize) : ImGuiPanel(name), m_Framebuffer(framebuffer), m_ViewportSize(viewportSize)
	{
		
	}

	void ImGuiViewportPanel::Render()
	{
		bool m_ViewportFocused = ImGui::IsWindowFocused();
		bool m_ViewportHovered = ImGui::IsWindowHovered();
		Application::Get().GetImGuiLayer()->BlockEvents(!m_ViewportFocused && !m_ViewportHovered);

		ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2{ 0, 0 });
		ImGui::Begin("Viewport", &m_Shown);
			
		ImVec2 viewportPanelSize = ImGui::GetContentRegionAvail();
		m_ViewportSize = { viewportPanelSize.x, viewportPanelSize.y };
		uint64_t textureID = m_Framebuffer->GetColorAttachmentRendererID();
		ImGui::Image(reinterpret_cast<void*>(textureID), ImVec2(m_Framebuffer->GetProperties().Width, m_Framebuffer->GetProperties().Height), ImVec2{ 0, 1 }, ImVec2{ 1, 0 });
		ImGui::End();
		ImGui::PopStyleVar();
	}

}