#include "cwpch.h"

#include "Crowny/ImGui/ImGuiInspectorWindow.h"
#include "Crowny/ImGui/ImGuiHierarchyWindow.h"
#include "Crowny/ImGui/ImGuiComponentEditor.h"
#include "Crowny/SceneManagement/SceneManager.h"
#include "Crowny/Ecs/Components.h"

#include <imgui.h>

namespace Crowny
{
	ImGuiInspectorWindow::ImGuiInspectorWindow(const std::string& name) : ImGuiWindow(name)
	{
		m_ComponentEditor.RegisterComponent<TransformComponent>("Transform");
		m_ComponentEditor.RegisterComponent<CameraComponent>("Camera");
		m_ComponentEditor.RegisterComponent<MeshRendererComponent>("Mesh Filter");
		m_ComponentEditor.RegisterComponent<TextComponent>("Text");
		m_ComponentEditor.RegisterComponent<SpriteRendererComponent>("Sprite Renderer");
	}

	void ImGuiInspectorWindow::Render()
	{
		ImGui::Begin("Inspector", &m_Shown);
		m_ComponentEditor.Render(SceneManager::GetActiveScene()->m_Registry, ImGuiHierarchyWindow::SelectedEntity);
		ImGui::End();
	}

	void ImGuiInspectorWindow::Show()
	{
		m_Shown = true;
	}

	void ImGuiInspectorWindow::Hide()
	{
		m_Shown = false;
	}

}