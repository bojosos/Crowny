#pragma once

#include "ImGuiPanel.h"

namespace Crowny
{
	class ImGuiViewportPanel : public ImGuiPanel
	{
	public:
		ImGuiViewportPanel(const std::string& name);
		~ImGuiViewportPanel() = default;

		virtual void Render() override;
		const glm::vec2& GetViewportSize() const { return m_ViewportSize; }
		const glm::vec4& GetViewportBounds() const { return m_ViewportBounds; }
	private:
		int32_t m_GizmoMode = 0;
		glm::vec2 m_ViewportSize;
		glm::vec4 m_ViewportBounds;
	};
}