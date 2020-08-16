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

	void ImGuiMenuBar::AddMenu(ImGuiMenu* menu)
	{
		m_Menus.push_back(menu);
	}

	ImGuiMenu::ImGuiMenu(const std::string& title) : m_Title(title)
	{

	}

	ImGuiMenu::~ImGuiMenu()
	{
		for (auto* menu : m_Menus)
			delete menu;
		for (auto* item : m_Items)
			delete item;
	}

	void ImGuiMenu::Render()
	{
		if (ImGui::BeginMenu(m_Title.c_str()))
		{
			uint32_t menuIndex = 0, itemIndex = 0;
			for (auto i : m_Order)
			{
				if (i)
					m_Menus[menuIndex++]->Render();
				else
					m_Items[itemIndex++]->Render();
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
	
	ImGuiMenuItem::ImGuiMenuItem(const std::string& title, const EventCallbackFn& onclicked)
		: m_Title(title), OnClicked(onclicked)
	{

	}

	void ImGuiMenuItem::Render()
	{
		if (ImGui::MenuItem(m_Title.c_str()))
		{
			OnClicked(ImGuiMenuItemClickedEvent(m_Title));
		}
	}

}