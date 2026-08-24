#include "cwepch.h"

#include "Panels/ImGuiPanel.h"

#include <imgui.h>

namespace Crowny
{
    ImGuiPanel::ImGuiPanel(const String& name) : m_Name(name), m_Shown(true) {}

    bool ImGuiPanel::BeginPanel(ImGuiWindowFlags flags)
    {
        m_BeginCalled = false;
        m_Focused = false;
        m_Hovered = false;
        if (!m_Shown)
            return false;

        const bool visible = ImGui::Begin(m_Name.c_str(), &m_Shown, flags);
        m_BeginCalled = true;
        UpdateState();
        return visible;
    }

    void ImGuiPanel::EndPanel()
    {
        if (m_BeginCalled)
        {
            ImGui::End();
            m_BeginCalled = false;
        }
    }

    void ImGuiPanel::UpdateState()
    {
        m_Hovered = ImGui::IsWindowHovered(ImGuiHoveredFlags_RootAndChildWindows);
        m_Focused = ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows);
    }

} // namespace Crowny
