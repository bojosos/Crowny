#pragma once

#include "Crowny.h"
#include "Crowny/ImGui/ImGuiComponentEditor.h"
#include "Crowny/ImGui/ImGuiWindow.h"
#include "Crowny/ImGui/ImGuiMenu.h"

#include "Crowny/ImGui/OpenGLInformationWindow.h"
#include "Crowny/ImGui/ImGuiViewportWindow.h"
#include "Crowny/ImGui/ImGuiHierarchyWindow.h"

#include <entt/entt.hpp>

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

	private:
		ImGuiMenuBar* m_MenuBar;
		ImGuiWindow* m_GLInfoWindow;
		ImGuiHierarchyWindow* m_HierarchyWindow;
		ImGuiViewportWindow* m_ViewportWindow;
		ImGuiTextureEditor* m_TextureEditor;

		ImGuiViewportWindow* m_Viewport;
		std::vector<ImGuiWindow*> m_ImGuiWindows;
		
		Ref<Framebuffer> m_Framebuffer; // Maybe Viewport window should take care of this?
		glm::vec2 m_ViewportSize = { 0.0f, 0.0f }; // and dis
	};
}