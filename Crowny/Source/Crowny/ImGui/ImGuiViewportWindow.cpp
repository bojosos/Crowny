#include "cwpch.h"

#include "Crowny/ImGui/ImGuiViewportWindow.h"
#include "Crowny/Application/Application.h"
#include <imgui.h>

namespace Crowny
{

	ImGuiViewportWindow::ImGuiViewportWindow(const std::string& name, const Ref<Framebuffer>& framebuffer, glm::vec2& viewportSize) : ImGuiWindow(name), m_Framebuffer(framebuffer), m_ViewportSize(viewportSize)
	{

	}

	void ImGuiViewportWindow::Show()
	{
		m_Shown = true;
	}

	void ImGuiViewportWindow::Hide()
	{

	}

	void ImGuiViewportWindow::Render()
	{
		m_ViewportFocused = ImGui::IsWindowFocused();
		m_ViewportHovered = ImGui::IsWindowHovered();
		Application::Get().GetImGuiLayer()->BlockEvents(!m_ViewportFocused || !m_ViewportHovered);

		if (m_Shown)
		{
			ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2{ 0, 0 });
			ImGui::Begin("Viewport");
			
			ImVec2 viewportPanelSize = ImGui::GetContentRegionAvail();
			m_ViewportSize = { viewportPanelSize.x, viewportPanelSize.y };

			uint32_t textureID = m_Framebuffer->GetColorAttachmentRendererID();
			ImGui::Image((ImTextureID)textureID, ImVec2(m_Framebuffer->GetProperties().Width, m_Framebuffer->GetProperties().Height), ImVec2{ 0, 1 }, ImVec2{ 1, 0 });
			ImGui::End();
			ImGui::PopStyleVar();
		}
	}

}