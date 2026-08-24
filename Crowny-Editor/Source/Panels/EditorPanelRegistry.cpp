#include "cwepch.h"

#include "Panels/EditorPanelRegistry.h"

#include "Crowny/ImGui/ImGuiMenu.h"

namespace Crowny
{
    void EditorPanelRegistry::AddPanel(EditorPanelDesc desc, Scope<ImGuiPanel> panel)
    {
        CW_ENGINE_ASSERT(panel, "Cannot register a null editor panel");
        for (const Entry& entry : m_Panels)
            CW_ENGINE_ASSERT(entry.Desc.Name != desc.Name, "An editor panel with this name is already registered");

        m_Panels.push_back({ std::move(desc), std::move(panel) });
    }

    void EditorPanelRegistry::AddViewMenuItems(ImGuiMenu& menu)
    {
        Vector<const Entry*> menuEntries;
        menuEntries.reserve(m_Panels.size());
        for (const Entry& entry : m_Panels)
        {
            if (entry.Desc.ShowInViewMenu)
                menuEntries.push_back(&entry);
        }
        std::sort(menuEntries.begin(), menuEntries.end(), [](const Entry* lhs, const Entry* rhs) { return lhs->Desc.Name < rhs->Desc.Name; });

        for (const Entry* entry : menuEntries)
        {
            ImGuiPanel* panel = entry->Panel.get();
            menu.AddItem(entry->Desc.Name, {}, [panel]() { panel->Toggle(); }, [panel]() { return panel->IsShown(); });
        }
    }

    void EditorPanelRegistry::Render()
    {
        for (const Entry& entry : m_Panels)
            entry.Panel->Render();
    }

    void EditorPanelRegistry::Clear() { m_Panels.clear(); }
} // namespace Crowny
