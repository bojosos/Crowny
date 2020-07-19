#pragma once

#include "Crowny.h"
#include "Crowny/ImGui/ImGuiComponentEditor.h"
#include "Crowny/ImGui/ImGuiWindow.h"

#include <entt/entt.hpp>

namespace Crowny
{
	class ImGuiTextureEditor;

	class EditorLayer : public Layer
	{
	public:
		EditorLayer();
		virtual ~EditorLayer() = default;

		virtual void OnAttach() override;
		virtual void OnDetach() override;

		void OnUpdate(Timestep ts) override;
		virtual void OnImGuiRender() override;
		void OnEvent(Event& e) override;

	private:
		entt::entity m_Entity;
		entt::registry m_Registry;
		ComponentEditor<entt::entity> m_ComponentEditor;
		Camera m_Camera;

		std::vector<ImGuiWindow*> m_ImGuiWindows;
		
		Ref<Framebuffer> m_Framebuffer;
		ImGuiTextureEditor* m_TextureEditor;
		glm::vec2 m_ViewportSize = { 0.0f, 0.0f };
		glm::vec4 m_SquareColor = { 0.2f, 0.3f, 0.8f, 1.0f };
	};
}