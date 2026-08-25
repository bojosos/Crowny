#pragma once

#include "Panels/EditorPanelRegistration.h"
#include "Panels/ImGuiPanel.h"

#include <type_traits>

namespace Crowny
{
    class ImGuiMenu;

    class EditorPanelRegistry
    {
    public:
        template <typename T, typename... Args> T& Add(const EditorPanelRegistration<T>& registration, Args&&... args)
        {
            static_assert(std::is_base_of_v<ImGuiPanel, T>, "Editor panels must derive from ImGuiPanel");

            const EditorPanelMetadata& metadata = registration.Metadata;
            CW_ENGINE_ASSERT(!metadata.Name.empty(), "Editor panels must have a name");

            Scope<T> panel = CreateScope<T>(String(metadata.Name), std::forward<Args>(args)...);
            panel->SetShown(metadata.OpenByDefault);
            T& result = *panel;
            AddPanel(metadata, std::move(panel));
            return result;
        }

        Scope<ImGuiMenu> CreateMenu(StringView rootPath) const;
        void Render();
        void Clear();

        size_t GetPanelCount() const { return m_Panels.size(); }

    private:
        struct Metadata
        {
            String Name;
            String MenuPath;
            String Shortcut;
            bool OpenByDefault = true;
        };

        struct Entry
        {
            Metadata Info;
            Scope<ImGuiPanel> Panel;
        };

        void AddPanel(const EditorPanelMetadata& metadata, Scope<ImGuiPanel> panel);

        Vector<Entry> m_Panels;
    };
} // namespace Crowny
