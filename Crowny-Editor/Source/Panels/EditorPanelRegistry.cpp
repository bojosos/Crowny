#include "cwepch.h"

#include "Panels/EditorPanelRegistry.h"

#include "Crowny/ImGui/ImGuiMenu.h"

namespace Crowny
{
    void EditorPanelRegistry::AddPanel(const EditorPanelMetadata& metadata, Scope<ImGuiPanel> panel)
    {
        CW_ENGINE_ASSERT(panel, "Cannot register a null editor panel");
        for (const Entry& entry : m_Panels)
        {
            CW_ENGINE_ASSERT(entry.Info.Name != metadata.Name, "An editor panel with this name is already registered");
            CW_ENGINE_ASSERT(metadata.MenuPath.empty() || entry.Info.MenuPath != metadata.MenuPath,
                             "An editor panel with this menu path is already registered");
        }

        Metadata storedMetadata{ String(metadata.Name), String(metadata.MenuPath), String(metadata.Shortcut), metadata.OpenByDefault };
        m_Panels.push_back({ std::move(storedMetadata), std::move(panel) });
    }

    Scope<ImGuiMenu> EditorPanelRegistry::CreateMenu(StringView rootPath) const
    {
        struct MenuNode
        {
            String Name;
            const Entry* PanelEntry = nullptr;
            Vector<MenuNode> Children;
        };

        const auto splitPath = [](StringView path) {
            Vector<StringView> segments;
            size_t start = 0;
            while (start < path.size())
            {
                const size_t end = path.find('/', start);
                const size_t length = end == StringView::npos ? path.size() - start : end - start;
                if (length > 0)
                    segments.push_back(path.substr(start, length));
                if (end == StringView::npos)
                    break;
                start = end + 1;
            }
            return segments;
        };

        CW_ENGINE_ASSERT(!rootPath.empty(), "Editor panel menu root cannot be empty");
        Scope<ImGuiMenu> menu = CreateScope<ImGuiMenu>(String(rootPath));
        MenuNode root;

        for (const Entry& entry : m_Panels)
        {
            if (entry.Info.MenuPath.empty())
                continue;

            const Vector<StringView> segments = splitPath(entry.Info.MenuPath);
            if (segments.size() < 2 || segments.front() != rootPath)
                continue;

            MenuNode* current = &root;
            bool validPath = true;
            for (size_t i = 1; i < segments.size(); i++)
            {
                if (current->PanelEntry != nullptr)
                {
                    validPath = false;
                    break;
                }

                auto child = std::find_if(current->Children.begin(), current->Children.end(),
                                          [segment = segments[i]](const MenuNode& node) { return node.Name == segment; });
                if (child == current->Children.end())
                {
                    current->Children.push_back({ String(segments[i]) });
                    current = &current->Children.back();
                }
                else
                    current = &*child;
            }

            if (!validPath || current->PanelEntry != nullptr || !current->Children.empty())
            {
                CW_ENGINE_ASSERT(false, "Editor panel menu paths cannot overlap");
                continue;
            }
            current->PanelEntry = &entry;
        }

        const auto sortChildren = [&](const auto& self, MenuNode& node) -> void {
            std::sort(node.Children.begin(), node.Children.end(), [](const MenuNode& lhs, const MenuNode& rhs) { return lhs.Name < rhs.Name; });
            for (MenuNode& child : node.Children)
                self(self, child);
        };
        sortChildren(sortChildren, root);

        const auto addChildren = [&](const auto& self, ImGuiMenu& parent, const MenuNode& node) -> void {
            for (const MenuNode& child : node.Children)
            {
                if (child.PanelEntry != nullptr)
                {
                    ImGuiPanel* panel = child.PanelEntry->Panel.get();
                    parent.AddItem(
                      child.Name, child.PanelEntry->Info.Shortcut, [panel]() { panel->Toggle(); }, [panel]() { return panel->IsShown(); });
                    continue;
                }

                ImGuiMenu& submenu = parent.AddMenu(child.Name);
                self(self, submenu, child);
            }
        };
        addChildren(addChildren, *menu, root);
        return menu;
    }

    void EditorPanelRegistry::Render()
    {
        for (const Entry& entry : m_Panels)
            entry.Panel->Render();
    }

    void EditorPanelRegistry::Clear() { m_Panels.clear(); }
} // namespace Crowny
