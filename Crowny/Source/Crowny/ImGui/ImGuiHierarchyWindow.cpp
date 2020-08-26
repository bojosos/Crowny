#include "cwpch.h"

#include "Crowny/ImGui/ImGuiHierarchyWindow.h"

#include "Crowny/SceneManagement/SceneManager.h"
#include "Crowny/Ecs/Entity.h"
#include "Crowny/Ecs/Components.h"
#include "Crowny/Input/Input.h"

#include <imgui.h>

namespace Crowny
{
	Entity ImGuiHierarchyWindow::SelectedEntity;

	ImGuiHierarchyWindow::ImGuiHierarchyWindow(const std::string& name) : ImGuiWindow(name)
	{

	}

	void ImGuiHierarchyWindow::DisplayTree(Entity& e, uint32_t i)
	{
		if (!SceneManager::GetActiveScene()->m_Registry.valid(e.m_EntityHandle))
			return;

		auto& rc = e.GetComponent<RelationshipComponent>();
		auto& tc = e.GetComponent<TagComponent>();

		std::string name = tc.Tag.empty() ? "Entity" : tc.Tag.c_str();
		ImGuiTreeNodeFlags selected = (ImGuiHierarchyWindow::SelectedEntity == e) ? ImGuiTreeNodeFlags_Selected : 0;
		ImGui::AlignTextToFramePadding();

		ImGui::PushID(i);

		if (!rc.Children.empty())
		{
			bool open = ImGui::TreeNodeEx(name.c_str(), ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_OpenOnDoubleClick | selected);

			//if (ImGui::BeginDragDropSource()) {
			//	ImGui::SetDragDropPayload("_TREENODE", nullptr, 0);
			//		ImGui::Text("wat");
			//		ImGui::EndDragDropSource();
			//}
			
			if (ImGui::BeginPopupContextItem())
			{
				if (ImGui::Selectable("New Entity Tree"))
				{
					e.AddChild("New Entity");
					ImGuiHierarchyWindow::SelectedEntity = rc.Children.back();
				}
				if (ImGui::Selectable("Delete") || Input::IsKeyPressed(KeyCode::Delete))
				{
					for (int i = 0; i < rc.Children.size(); i++)
					{
						if (rc.Children[i] == ImGuiHierarchyWindow::SelectedEntity)
						{
							rc.Children.erase(rc.Children.begin() + i);
							break;
						}
					}

					ImGuiHierarchyWindow::SelectedEntity = *SceneManager::GetActiveScene()->m_SceneEntity;
				}

				ImGui::EndPopup();
			}

			if (open)
			{
				if (ImGui::IsItemClicked())
					ImGuiHierarchyWindow::SelectedEntity = e;

				for (auto& c : rc.Children)
					DisplayTree(c, ++i);

				ImGui::TreePop();
			}
		}
		else
		{
			ImGui::TreeNodeEx(name.c_str(), ImGuiTreeNodeFlags_NoTreePushOnOpen | selected | ImGuiTreeNodeFlags_Leaf);

			if(ImGui::IsItemClicked())
			{
				ImGuiHierarchyWindow::SelectedEntity = e;
			}
			
			if (ImGui::BeginPopupContextItem())
			{
				if (ImGui::Selectable("New Entity One"))
				{
					e.AddChild("New Entity");
				}
				ImGui::EndPopup();
			}
		}

		ImGui::PopID();
	}

	void ImGuiHierarchyWindow::Render()
	{
		ImGui::Begin("Hierarchy", &m_Shown);
		Scene* activeScene = SceneManager::GetActiveScene();
		if (ImGui::BeginPopupContextWindow())
		{
			if (ImGui::Selectable("New Entity"))
			{
				activeScene->CreateEntity("New Entity");
			}
			ImGui::EndPopup();
		}

		ImGui::SetNextItemOpen(true, ImGuiCond_Once);

		DisplayTree(*activeScene->m_SceneEntity);

		ImGui::End();
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
