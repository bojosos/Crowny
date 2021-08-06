#pragma once

#include "ImGuiPanel.h"

#include "Crowny/Events/Event.h"

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
        void SetEventCallback(const EventCallbackFn& onclicked);

    private:
        EventCallbackFn OnEvent;
        int32_t m_GizmoMode = 0;
        glm::vec2 m_ViewportSize;
        glm::vec4 m_ViewportBounds;
    };

} // namespace Crowny