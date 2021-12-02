#pragma once

#include "Panels/ImGuiPanel.h"

#include "Crowny/Events/Event.h"
#include "Crowny/RenderAPI/RenderTarget.h"

#include <ImGuizmo.h>

namespace Crowny
{

    class ViewportPanel : public ImGuiPanel
    {
    public:
        ViewportPanel(const String& name);
        ~ViewportPanel() = default;

        virtual void Render() override;
        const glm::vec2& GetViewportSize() const { return m_ViewportSize; }
        const glm::vec4& GetViewportBounds() const { return m_ViewportBounds; }
        void SetEventCallback(const EventCallbackFn& onclicked);
        void SetEditorRenderTarget(const Ref<RenderTarget>& rt);

        void DisalbeGizmo() { m_GizmoMode = -1; }
        void EnableGizmo() { m_GizmoMode = ImGuizmo::TRANSLATE; }

    private:
        Ref<RenderTarget> m_RenderTarget;
        EventCallbackFn OnEvent;
        int32_t m_GizmoMode = ImGuizmo::TRANSLATE;
        glm::vec2 m_ViewportSize = { 1.0f, 1.0f };
        glm::vec4 m_ViewportBounds;
    };

} // namespace Crowny