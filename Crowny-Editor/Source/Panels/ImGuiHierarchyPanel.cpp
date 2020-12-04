#include "cwepch.h"

#include "ImGuiHierarchyPanel.h"

#include "Crowny/Scene/SceneManager.h"
#include "Crowny/Ecs/Entity.h"
#include "Crowny/Ecs/Components.h"
#include "Crowny/Input/Input.h"

#include <imgui.h>
#include <misc/cpp/imgui_stdlib.h>

namespace Crowny
{
	Entity ImGuiHierarchyPanel::s_SelectedEntity;

	ImGuiHierarchyPanel::ImGuiHierarchyPanel(const std::string& name) : ImGuiPanel(name)
	{

	}

	void ImGuiHierarchyPanel::DisplayPopup(Entity e)
	{
		if (ImGui::MenuItem("New Entity"))
		{
			m_NewEntityParent = e;
		}
		
		if (ImGui::MenuItem("Rename"))
		{
			m_Renaming = e;
		}

		if (ImGui::MenuItem("Delete"))
		{
			auto& rr = e.GetParent().GetComponent<RelationshipComponent>().Children;
			for (int i = 0; i < rr.size(); i++)
			{
				if (rr[i] == ImGuiHierarchyPanel::s_SelectedEntity)
				{
					rr[i].Destroy();
					rr.erase(rr.begin() + i);
					break;
				}
			}
			
			ImGuiHierarchyPanel::s_SelectedEntity = SceneManager::GetActiveScene()->GetRootEntity();
		}
	}

	void ImGuiHierarchyPanel::Select(Entity e)
	{
		if (!m_SelectedItems.empty() && Input::IsKeyPressed(Key::LeftControl))
		{
			if (m_SelectedItems.find(e) == m_SelectedItems.end())
				m_SelectedItems.insert(e);
			else
			{
				m_SelectedItems.erase(e);
				if (m_SelectedItems.empty())
					s_SelectedEntity = {};
			}
		}
		else
		{
			m_SelectedItems.clear();
			m_SelectedItems.insert(e);
			ImGuiHierarchyPanel::s_SelectedEntity = e;
		}
	}

	void ImGuiHierarchyPanel::Rename(Entity e)
	{
		auto& tc = e.GetComponent<TagComponent>();
		auto& rc = e.GetComponent<RelationshipComponent>();

		ImGui::SetCursorPosX(ImGui::GetCursorPos().x + ImGui::GetStyle().FramePadding.x);
		if (ImGui::InputText("##renaming", &tc.Tag, ImGuiInputTextFlags_AutoSelectAll | ImGuiInputTextFlags_EnterReturnsTrue))
		{
			m_Renaming = {};
		}

		if (ImGui::IsMouseClicked(0) && !ImGui::IsItemClicked())
		{
			m_Renaming = {};
		}
		
	}

	void ImGuiHierarchyPanel::DisplayTreeNode(Entity e)
	{
		auto& tc = e.GetComponent<TagComponent>();
		auto& rc = e.GetComponent<RelationshipComponent>();
		std::string name = tc.Tag.empty() ? "Entity" : tc.Tag.c_str();
		
		ImGuiTreeNodeFlags selected = (m_SelectedItems.find(e) != m_SelectedItems.end()) ? ImGuiTreeNodeFlags_Selected : 0;
		ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_SpanAvailWidth | ImGuiTreeNodeFlags_FramePadding | ImGuiTreeNodeFlags_AllowItemOverlap | ImGuiTreeNodeFlags_OpenOnDoubleClick;
		
		bool open = true;
		if (e == m_Renaming)
		{
			Rename(e);
			ImGui::SetCursorPosX(ImGui::GetCursorPos().x + 2 * ImGui::GetStyle().FramePadding.x);
			for (auto& c : rc.Children) 
			{
				DisplayTree(c);
			}
		}
		else
		{
			open = ImGui::TreeNodeEx(name.c_str(), selected | flags);
			
			if (ImGui::BeginDragDropSource())
			{
				uint32_t tmp = (uint32_t)e.GetHandle();
				ImGui::SetDragDropPayload("ID", &tmp, sizeof(uint32_t));
				ImGui::Text("%s", name.c_str());
				ImGui::EndDragDropSource();
			}

			if (ImGui::BeginDragDropTarget())
			{
				if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("ID"))
				{
					CW_ENGINE_ASSERT(payload->DataSize == sizeof(uint32_t));
					uint32_t id = *(const uint32_t*)payload->Data;
					Entity((entt::entity)id, SceneManager::GetActiveScene().get()).SetParent(e);
				}
				ImGui::EndDragDropTarget();
			}

			if (ImGui::BeginPopupContextItem())
			{
				DisplayPopup(e);
				ImGui::EndPopup();
			}

			if (ImGui::IsItemClicked())
			{
				Select(e);
			}
			
			if (open)
			{
				for (auto& c : rc.Children) 
				{
					DisplayTree(c);
				}

				ImGui::TreePop();
			}
		}
	}

	void ImGuiHierarchyPanel::DisplayLeafNode(Entity e)
	{
		auto& tc = e.GetComponent<TagComponent>();

		std::string name = tc.Tag.empty() ? "Entity" : tc.Tag.c_str();
		
		ImGuiTreeNodeFlags selected = (m_SelectedItems.find(e) != m_SelectedItems.end()) ? ImGuiTreeNodeFlags_Selected : 0;
		ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_NoTreePushOnOpen | ImGuiTreeNodeFlags_SpanAvailWidth | ImGuiTreeNodeFlags_FramePadding | ImGuiTreeNodeFlags_AllowItemOverlap | ImGuiTreeNodeFlags_Leaf;

		if (e == m_Renaming)
		{
			Rename(e);
		} 
		else
		{
			ImGui::TreeNodeEx(name.c_str(), flags | selected);
			
			if (ImGui::BeginDragDropSource())
			{
				uint32_t tmp = (uint32_t)e.GetHandle();
				ImGui::SetDragDropPayload("ID", &tmp, sizeof(uint32_t));
				ImGui::Text("%s", name.c_str());
				ImGui::EndDragDropSource();
			}

			if (ImGui::BeginDragDropTarget())
			{
				if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("ID"))
				{
					CW_ENGINE_ASSERT(payload->DataSize == sizeof(uint32_t));
					uint32_t id = *(const uint32_t*)payload->Data;
					Entity((entt::entity)id, SceneManager::GetActiveScene().get()).SetParent(e);
				}
				ImGui::EndDragDropTarget();
			}

			if(ImGui::IsItemClicked())
			{
				Select(e);
			}

			if (ImGui::BeginPopupContextItem())
			{
				DisplayPopup(e);
				ImGui::EndPopup();
			}
		}
	}

	static int var = 100;

	void ImGuiHierarchyPanel::DisplayTree(Entity e)
	{
		if (!e.IsValid())
			return;

		auto& rc = e.GetComponent<RelationshipComponent>();
		
		ImGui::AlignTextToFramePadding();

		ImGui::PushID((int32_t)e.GetHandle());

		if (!rc.Children.empty())
		{
			DisplayTreeNode(e);
		}
		else
		{
			DisplayLeafNode(e);
		}

		if (Input::IsKeyUp(Key::Delete) && !m_Deleted && ImGui::IsWindowFocused())
		{
			m_Deleted = true;
			auto& rr = s_SelectedEntity.GetParent().GetComponent<RelationshipComponent>().Children;
			for (int i = 0; i < rr.size(); i++)
			{
				if (rr[i] == ImGuiHierarchyPanel::s_SelectedEntity)
				{
					rr[i].Destroy();
					rr.erase(rr.begin() + i);
					break;
				}
			}

			ImGuiHierarchyPanel::s_SelectedEntity = SceneManager::GetActiveScene()->GetRootEntity();
		}

		ImGui::PopID();
	}

	void ImGuiHierarchyPanel::Update()
	{
		if (m_NewEntityParent)
		{
			Entity newEntity = SceneManager::GetActiveScene()->CreateEntity("New Entity");
			m_NewEntityParent.AddChild(newEntity);
			ImGuiHierarchyPanel::s_SelectedEntity = newEntity;
			m_SelectedItems.clear();
			m_SelectedItems.insert(ImGuiHierarchyPanel::s_SelectedEntity);
			m_NewEntityParent = {};
		}

		if (Input::IsKeyDown(Key::F2) && ImGui::IsWindowFocused())
		{
			m_Renaming = s_SelectedEntity;
		}

	}

	void ImGuiHierarchyPanel::Render()
	{
		m_Deleted = false;
		ImGui::Begin("Hierarchy", &m_Shown);
		Ref<Scene> activeScene = SceneManager::GetActiveScene();
		if (ImGui::BeginPopupContextWindow(nullptr, ImGuiPopupFlags_NoOpenOverExistingPopup | ImGuiPopupFlags_MouseButtonRight))
		{
			if (ImGui::MenuItem("New Entity"))
			{
				m_NewEntityParent = activeScene->GetRootEntity();
			}

			if (ImGui::BeginMenu("Create"))
			{
				if (ImGui::MenuItem("Camera"))
				{
					//Currently is impossible to do
					//m_NewEntityParent = activeScene->GetRootEntity();
				}

				if (ImGui::MenuItem("Camera"))
				{
					//Currently is impossible to do
					//m_NewEntityParent = activeScene->GetRootEntity();
				}

				if (ImGui::MenuItem("Light"))
				{
					//Currently is impossible to do
					//m_NewEntityParent = activeScene->GetRootEntity();
				}

				if (ImGui::MenuItem("Sphere"))
				{
					//Currently is impossible to do
					//m_NewEntityParent = activeScene->GetRootEntity();
				}

				ImGui::EndMenu();
			}
			ImGui::EndPopup();
		}

		ImGui::SetNextItemOpen(true, ImGuiCond_Once);
		DisplayTree(activeScene->GetRootEntity());
		ImGui::End();
	}

	void ImGuiHierarchyPanel::Show()
	{
		m_Shown = true;
	}

	void ImGuiHierarchyPanel::Hide()
	{
		m_Shown = false;
	}

}
