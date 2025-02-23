#include "cwepch.h"

#include "Panels/ImGuiPanel.h"

#include <imgui.h>

namespace Crowny
{
    ImGuiPanel::ImGuiPanel(const String& name) : m_Name(name), m_Shown(true) {}

    void ImGuiPanel::BeginPanel(ImGuiWindowFlags flags)
    {
        m_BeginCalled = false;
        if (m_Shown)
        {
            ImGui::Begin(m_Name.c_str(), &m_Shown, flags);
            m_BeginCalled = true;
            UpdateState();
        }
    }

    void ImGuiPanel::EndPanel()
    {
        if (m_BeginCalled)
            ImGui::End();
    }

    void ImGuiPanel::UpdateState()
    {
        m_Hovered = ImGui::IsWindowHovered();
        m_Focused = ImGui::IsWindowFocused();
    }

    void ImGuiPanel::RegisterInMenu(ImGuiMenu* menu) { menu->AddItem(new ImGuiMenuItem(m_Name, "", nullptr, &m_Shown)); }
} // namespace Crowny