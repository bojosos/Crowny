#pragma once

#include "Crowny/ImGui/ImGuiWindow.h"
#include "Crowny/Renderer/Framebuffer.h"

namespace Crowny
{
	class ImGuiViewportWindow : public ImGuiWindow
	{
	public:
		ImGuiViewportWindow(const std::string& name, const Ref<Framebuffer>& framebuffer, glm::vec2& viewportsize);
		~ImGuiViewportWindow() = default;

		virtual void Render() override;
	private:
		Ref<Framebuffer> m_Framebuffer;
		glm::vec2& m_ViewportSize;

		bool m_ViewportFocused = false;
		bool m_ViewportHovered = false;
	};
}