#pragma once

#include "Crowny/Events/ImGuiEvent.h"

namespace Crowny
{

    class ImGuiMenuItem
    {
    public:
        ImGuiMenuItem(const String& title, const String& combination, const EventCallbackFn& onclicked, bool* shown = nullptr);
        ~ImGuiMenuItem() = default;

        void Render(uint32_t maxWidth);
        uint32_t GetTotalWidth();

    private:
        bool* m_Shown = nullptr;
        EventCallbackFn OnClicked;
        String m_Combination;
        String m_Title;
    };

    class ImGuiMenu
    {
    public:
        ImGuiMenu(const String& title);
        ~ImGuiMenu();

        void Render();
        void AddMenu(ImGuiMenu* menu);
        void AddItem(ImGuiMenuItem* item);

    private:
        Vector<ImGuiMenuItem*> m_Items;
        Vector<ImGuiMenu*> m_Menus;
        Vector<bool> m_Order;

        String m_Title;
    };

    class ImGuiMenuBar
    {
    public:
        ImGuiMenuBar() = default;
        ~ImGuiMenuBar();
        void AddMenu(ImGuiMenu* menu);
        void Render();

    private:
        Vector<ImGuiMenu*> m_Menus;
    };
} // namespace Crowny