#pragma once

#include "Crowny/Events/Event.h"
#include <string>

namespace Crowny
{
	class ImGuiMenuItemClickedEvent : public Event
	{
	public:
		const std::string& GetTitle() const { return m_Title; }

		EVENT_CLASS_CATEGORY(EventCategoryImGui);
		EVENT_CLASS_TYPE(ImGuiMenuItemClicked);

		ImGuiMenuItemClickedEvent(const std::string& title) : m_Title(title) { }
	private:
		std::string m_Title;
	};

}