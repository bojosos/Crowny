#pragma once

#include "Panels/ImGuiPanel.h"

#include <type_traits>

namespace Crowny
{
    class ImGuiMenu;

    struct EditorPanelDesc
    {
        String Name;
        bool OpenByDefault = true;
        bool ShowInViewMenu = true;
    };

    class EditorPanelRegistry
    {
    public:
        template <typename T, typename... Args> T& Add(EditorPanelDesc desc, Args&&... args)
        {
            static_assert(std::is_base_of_v<ImGuiPanel, T>, "Editor panels must derive from ImGuiPanel");

            Scope<T> panel = CreateScope<T>(desc.Name, std::forward<Args>(args)...);
            panel->SetShown(desc.OpenByDefault);
            T& result = *panel;
            AddPanel(std::move(desc), std::move(panel));
            return result;
        }

        void AddViewMenuItems(ImGuiMenu& menu);
        void Render();
        void Clear();

        size_t GetPanelCount() const { return m_Panels.size(); }

    private:
        struct Entry
        {
            EditorPanelDesc Desc;
            Scope<ImGuiPanel> Panel;
        };

        void AddPanel(EditorPanelDesc desc, Scope<ImGuiPanel> panel);

        Vector<Entry> m_Panels;
    };
} // namespace Crowny
