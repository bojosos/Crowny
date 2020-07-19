#include "cwpch.h"

#include "Crowny/ImGui/ImGuiHierarchyWindow.h"

#include <imgui.h>

namespace Crowny
{

	ImGuiHierarchyWindow::ImGuiHierarchyWindow(const std::string& name) : ImGuiWindow(name)
	{

	}

	void ImGuiHierarchyWindow::Render()
	{
		if (m_Shown) {
			ImGui::Begin("Hierarchy");
			ImGui::SetNextItemOpen(true, ImGuiCond_Once);
			if (ImGui::TreeNode("Scene"))
			{
				if (ImGui::TreeNode("Entity"))
				{
					for (int i = 0; i < 5; i++)
					{
						if (ImGui::TreeNode((void*)(intptr_t)i, "Child %d", i))
						{
							ImGui::Text("");
							ImGui::SameLine();
							ImGui::Selectable("test", false);
							if (ImGui::IsItemClicked()) CW_ENGINE_INFO("Clicked");
							ImGui::TreePop();
						}
					}
					ImGui::TreePop();
				}
				ImGui::TreePop();
			}
			ImGui::End();
		}
	}

	void ImGuiHierarchyWindow::Show()
	{
		m_Shown = true;
	}

	void ImGuiHierarchyWindow::Hide()
	{
		m_Shown = false;
	}

}