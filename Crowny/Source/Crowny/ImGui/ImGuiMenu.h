#pragma once

#include "Crowny/Common/Common.h"

namespace Crowny
{
    class ImGuiMenuItem
    {
    public:
        using Action = std::function<void()>;
        using CheckedState = std::function<bool()>;

        ImGuiMenuItem(String title, String shortcut, Action action, CheckedState checkedState = {});
        ~ImGuiMenuItem() = default;

        void Render();

    private:
        Action m_Action;
        CheckedState m_CheckedState;
        String m_Shortcut;
        String m_Title;
    };

    class ImGuiMenu
    {
    public:
        ImGuiMenu(const String& title);
        virtual ~ImGuiMenu() = default;

        virtual void Render();
        ImGuiMenuItem& AddItem(String title, String shortcut, ImGuiMenuItem::Action action,
                               ImGuiMenuItem::CheckedState checkedState = {});
        ImGuiMenu& AddMenu(String title);
        ImGuiMenu& AddMenu(Scope<ImGuiMenu> menu);

    protected:
        struct Entry
        {
            Scope<ImGuiMenuItem> Item;
            Scope<ImGuiMenu> Menu;
        };

        Vector<Entry> m_Entries;

        String m_Title;
    };

    class ImGuiMenuBar
    {
    public:
        ImGuiMenuBar() = default;
        ~ImGuiMenuBar() = default;

        ImGuiMenu& AddMenu(String title);
        ImGuiMenu& AddMenu(Scope<ImGuiMenu> menu);
        void Render();

    private:
        Vector<Scope<ImGuiMenu>> m_Menus;
    };
} // namespace Crowny
