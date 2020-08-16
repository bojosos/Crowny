#pragma once

#include "Crowny/ImGui/ImGuiWindow.h"
#include "Crowny/Events/ImGuiEvent.h"

namespace Crowny
{

	class ImGuiMenuItem
	{
	public:
		ImGuiMenuItem(const std::string& title, const EventCallbackFn& onclicked);
		~ImGuiMenuItem() = default;

		EventCallbackFn OnClicked;
		void Render();

	private:
		std::string m_Title;
	};

	class ImGuiMenu
	{
	public:
		ImGuiMenu(const std::string& title);
		~ImGuiMenu();

		void Render();
		void AddMenu(ImGuiMenu* menu);
		void AddItem(ImGuiMenuItem* item);

	private:
		std::vector<ImGuiMenuItem*> m_Items;
		std::vector<ImGuiMenu*> m_Menus;
		std::vector<bool> m_Order;

		std::string m_Title;
	};

	class ImGuiMenuBar
	{
	public:
		ImGuiMenuBar() = default;
		~ImGuiMenuBar();
		void AddMenu(ImGuiMenu* menu);
		void Render();

	private:
		std::vector<ImGuiMenu*> m_Menus;
	};
}