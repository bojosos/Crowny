#include "cwpch.h"

#include "Crowny/ImGui/ImGuiMenu.h"

#include <imgui.h>

namespace Crowny
{

    ImGuiMenuBar::~ImGuiMenuBar()
    {
        for (auto* menu : m_Menus)
            delete menu;
    }

    void ImGuiMenuBar::Render()
    {
        ImGui::BeginMenuBar();
        for (auto* menu : m_Menus)
        {
            menu->Render();
        }
        ImGui::EndMenuBar();
    }

    void ImGuiMenuBar::AddMenu(ImGuiMenu* menu) { m_Menus.push_back(menu); }

    ImGuiMenu::ImGuiMenu(const String& title) : m_Title(title) {}

    ImGuiMenu::~ImGuiMenu()
    {
        for (auto* menu : m_Menus)
            delete menu;
        for (auto* item : m_Items)
            delete item;
    }

    void ImGuiMenu::Render()
    {
        uint32_t tmp = 0;
        for (auto* item : m_Items)
        {
            tmp = std::max(tmp, item->GetTotalWidth());
        }
        if (ImGui::BeginMenu(m_Title.c_str()))
        {
            uint32_t menuIndex = 0, itemIndex = 0;
            for (auto i : m_Order)
            {
                if (i)
                    m_Menus[menuIndex++]->Render();
                else
                {
                    m_Items[itemIndex++]->Render(tmp);
                }
            }
            ImGui::EndMenu();
        }
    }

    void ImGuiMenu::AddItem(ImGuiMenuItem* item)
    {
        m_Order.push_back(false);
        m_Items.push_back(item);
    }

    void ImGuiMenu::AddMenu(ImGuiMenu* menu)
    {
        m_Order.push_back(true);
        m_Menus.push_back(menu);
    }

    ImGuiMenuItem::ImGuiMenuItem(const String& title, const String& combination, const EventCallbackFn& onclicked,
                                 bool* shown)
      : m_Title(title), m_Combination(combination), OnClicked(onclicked), m_Shown(shown)
    {
    }

    uint32_t ImGuiMenuItem::GetTotalWidth()
    {
        return (uint32_t)(ImGui::CalcTextSize(m_Combination.c_str()).x + 5.0f + ImGui::CalcTextSize(m_Title.c_str()).x);
    }

    void ImGuiMenuItem::Render(uint32_t maxWidth)
    {
        bool clicked = ImGui::MenuItem(m_Title.c_str(), nullptr, m_Shown);

        ImGui::SameLine();
        ImGui::SetCursorPosX(ImGui::GetCursorPosX() + maxWidth - ImGui::CalcTextSize(m_Combination.c_str()).x -
                             2 * ImGui::GetStyle().ItemSpacing.x);
        ImGuiStyle& style = ImGui::GetStyle();
        ImGui::PushStyleColor(ImGuiCol_Text, style.Colors[ImGuiCol_TextDisabled]);
        ImGui::Text("%s", m_Combination.c_str());
        ImGui::PopStyleColor();

        if (clicked)
        {
            auto e = ImGuiMenuItemClickedEvent(m_Title);
            OnClicked(e);
        }
    }

} // namespace Crowny