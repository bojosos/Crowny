#include "cwepch.h"

#include "Panels/ImGuiPanel.h"

#include <imgui.h>

namespace Crowny
{
    ImGuiPanel::ImGuiPanel(const String& name) : m_Name(name), m_Shown(true) {}

    void ImGuiPanel::BeginPanel(ImGuiWindowFlags flags)
    {
        if (m_Shown)
        {
            ImGui::Begin(m_Name.c_str(), &m_Shown, flags);
            UpdateState();
        }
    }

    void ImGuiPanel::EndPanel()
    {
        if (m_Shown)
            ImGui::End();
    }

    void ImGuiPanel::UpdateState()
    {
        m_Hovered = ImGui::IsWindowHovered();
        m_Focused = ImGui::IsWindowFocused();
    }

    void ImGuiPanel::RegisterInMenu(ImGuiMenu* menu)
    {
        menu->AddItem(new ImGuiMenuItem(m_Name, "", [&](auto& event) { /*m_Shown = !m_Shown;*/ }, &m_Shown));
    }
} // namespace Crowny