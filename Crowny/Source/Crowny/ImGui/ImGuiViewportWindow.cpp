#include "cwpch.h"

#include "Crowny/ImGui/ImGuiViewportWindow.h"
#include <imgui.h>

namespace Crowny
{

	ImGuiViewportWindow::ImGuiViewportWindow(const std::string& name) : ImGuiWindow(name)
	{

	}

	void ImGuiViewportWindow::Show()
	{

	}

	void ImGuiViewportWindow::Hide()
	{

	}

	void ImGuiViewportWindow::Render()
	{
		ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2{ 0, 0 });

		/*
		ImGui::Begin("Viewport");
		ImVec2 viewportPanelSize = ImGui::GetContentRegionAvail();
		m_ViewportSize = { viewportPanelSize.x, viewportPanelSize.y };
		uint32_t textureID = m_Framebuffer->GetColorAttachmentRendererID();
		ImGui::Image((void*)textureID, ImVec2{ m_ViewportSize.x, m_ViewportSize.y }, ImVec2{ 0, 1 }, ImVec2{ 1, 0 });
		ImGui::End();
		*/
		ImGui::PopStyleVar();
	}

}