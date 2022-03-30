#pragma once

#include "Panels/ImGuiPanel.h"

#include "Crowny/Events/Event.h"
#include "Crowny/RenderAPI/RenderTarget.h"

#include <imgui.h>
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
		
		void SetGizmoMode(int32_t gizmoMode) { m_GizmoMode = gizmoMode; }
		void SetGizmoLocalMode(bool local) { m_LocalMode = local; }
		
		bool GetGizmoLocalMode() const  { return m_LocalMode; }
		int32_t GetGizmoMode() const { return m_GizmoMode; }

        void DisalbeGizmo() { m_GizmoMode = -1; }
        void EnableGizmo() { m_GizmoMode = ImGuizmo::TRANSLATE; }
		
		bool IsMouseOverGizmo() const { return ImGuizmo::IsOver(); }

    private:
        bool m_LocalMode = true;
        Ref<RenderTarget> m_RenderTarget;
        EventCallbackFn OnEvent;
        int32_t m_GizmoMode = ImGuizmo::TRANSLATE;
        glm::vec2 m_ViewportSize = { 1.0f, 1.0f };
        glm::vec4 m_ViewportBounds;
    };

} // namespace Crowny