#pragma once

#include "Panels/ImGuiComponentEditor.h"
#include "Panels/ImGuiPanel.h"
#include "Crowny/ImGui/ImGuiMenu.h"

#include "Panels/ImGuiGLInfoPanel.h"
#include "Panels/ImGuiViewportPanel.h"
#include "Panels/ImGuiHierarchyPanel.h"
#include "Panels/ImGuiInspectorPanel.h"
#include "Panels/ImGuiMaterialPanel.h"
#include "Crowny/Scene/Scene.h"

#include "Crowny/Scripting/CWMonoClass.h"

#include <entt/entt.hpp>
#include <Crowny.h>

namespace Crowny
{
	class ImGuiTextureEditor;
	class ImGuiViewportWindow;

	class EditorLayer : public Layer
	{
	public:
		EditorLayer();
		virtual ~EditorLayer() = default;

		virtual void OnAttach() override;
		virtual void OnDetach() override;

		virtual void OnUpdate(Timestep ts) override;
		virtual void OnImGuiRender() override;
		virtual void OnEvent(Event& e) override;

		bool OnKeyPressed(KeyPressedEvent& e);

		void CreateNewScene();
		void OpenScene();
		void SaveActiveScene();
		void SaveActiveSceneAs();

	private:
		ImGuiMenuBar* m_MenuBar;
		Ref<Scene> m_Temp;

		ImGuiPanel* m_GLInfoPanel;
		ImGuiInspectorPanel* m_InspectorPanel;
		ImGuiHierarchyPanel* m_HierarchyPanel;
		ImGuiViewportPanel* m_ViewportPanel;
		ImGuiTextureEditor* m_TextureEditor;
		ImGuiMaterialPanel* m_MaterialEditor;

		std::vector<ImGuiPanel*> m_ImGuiWindows;
		
		Ref<Framebuffer> m_Framebuffer; // Maybe Viewport window should take care of this?
		glm::vec2 m_ViewportSize = { 1280.0f, 720.0f }; // and dis
	};
}