#include "cwepch.h"

#include "Panels/SettingsPanel.h"

namespace Crowny
{
    SettingsPanel::SettingsPanel(const String& name) : ImGuiPanel(name) { }

    void SettingsPanel::Render()
    {
        BeginPanel();
        // ImGui::Checkbox("Show colliders", &m_ShowColliders);
        // ImGui::Checkbox("Show demo window", &m_ShowDemoWindow);
        EndPanel();
    }
}