#pragma once

#include "ImGuiPanel.h"
#include "Crowny/Renderer/Framebuffer.h"

namespace Crowny
{
	class ImGuiViewportPanel : public ImGuiPanel
	{
	public:
		ImGuiViewportPanel(const std::string& name, const Ref<Framebuffer>& framebuffer, glm::vec2& viewportsize);
		~ImGuiViewportPanel() = default;

		virtual void Render() override;
	private:
		int32_t m_GizmoMode = 0;
		Ref<Framebuffer> m_Framebuffer;
		glm::vec2& m_ViewportSize;
	};
}