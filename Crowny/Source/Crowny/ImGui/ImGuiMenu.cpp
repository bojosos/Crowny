#include "cwpch.h"

#include "Crowny/ImGui/ImGuiMenu.h"

#include <imgui.h>

namespace Crowny
{
    void ImGuiMenuBar::Render()
    {
        if (ImGui::BeginMenuBar())
        {
            for (const Scope<ImGuiMenu>& menu : m_Menus)
                menu->Render();
            ImGui::EndMenuBar();
        }
    }

    ImGuiMenu& ImGuiMenuBar::AddMenu(String title) { return AddMenu(CreateScope<ImGuiMenu>(std::move(title))); }

    ImGuiMenu& ImGuiMenuBar::AddMenu(Scope<ImGuiMenu> menu)
    {
        CW_ENGINE_ASSERT(menu, "Cannot add a null menu");
        ImGuiMenu& result = *menu;
        m_Menus.push_back(std::move(menu));
        return result;
    }

    ImGuiMenu::ImGuiMenu(const String& title) : m_Title(title) {}

    void ImGuiMenu::Render()
    {
        if (ImGui::BeginMenu(m_Title.c_str()))
        {
            for (const Entry& entry : m_Entries)
            {
                if (entry.Menu)
                    entry.Menu->Render();
                else
                    entry.Item->Render();
            }
            ImGui::EndMenu();
        }
    }

    ImGuiMenuItem& ImGuiMenu::AddItem(String title, String shortcut, ImGuiMenuItem::Action action,
                                      ImGuiMenuItem::CheckedState checkedState)
    {
        Entry entry;
        entry.Item = CreateScope<ImGuiMenuItem>(std::move(title), std::move(shortcut), std::move(action), std::move(checkedState));
        ImGuiMenuItem& result = *entry.Item;
        m_Entries.push_back(std::move(entry));
        return result;
    }

    ImGuiMenu& ImGuiMenu::AddMenu(String title) { return AddMenu(CreateScope<ImGuiMenu>(std::move(title))); }

    ImGuiMenu& ImGuiMenu::AddMenu(Scope<ImGuiMenu> menu)
    {
        CW_ENGINE_ASSERT(menu, "Cannot add a null menu");
        Entry entry;
        entry.Menu = std::move(menu);
        ImGuiMenu& result = *entry.Menu;
        m_Entries.push_back(std::move(entry));
        return result;
    }

    ImGuiMenuItem::ImGuiMenuItem(String title, String shortcut, Action action, CheckedState checkedState)
      : m_Action(std::move(action)), m_CheckedState(std::move(checkedState)), m_Shortcut(std::move(shortcut)), m_Title(std::move(title))
    {
    }

    void ImGuiMenuItem::Render()
    {
        const char* shortcut = m_Shortcut.empty() ? nullptr : m_Shortcut.c_str();
        const bool clicked = m_CheckedState ? ImGui::MenuItem(m_Title.c_str(), shortcut, m_CheckedState())
                                            : ImGui::MenuItem(m_Title.c_str(), shortcut);
        if (clicked && m_Action)
            m_Action();
    }
} // namespace Crowny
