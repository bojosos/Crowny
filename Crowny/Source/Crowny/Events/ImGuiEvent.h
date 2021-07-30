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

	class ImGuiViewportSceneDraggedEvent : public Event
	{
	public:
		const std::string& GetSceneFilepath() const { return m_Filepath; }

		EVENT_CLASS_CATEGORY(EventCategoryImGui);
		EVENT_CLASS_TYPE(ImGuiViewportSceneDragged);

		ImGuiViewportSceneDraggedEvent(const std::string& title) : m_Filepath(title) { }
		
	private:
		std::string m_Filepath;
	};

	class ImGuiViewportMeshDraggedEvent : public Event
	{
	public:
		const std::string& GetSceneFilepath() const { return m_Filepath; }

		EVENT_CLASS_CATEGORY(EventCategoryImGui);
		EVENT_CLASS_TYPE(ImGuiViewportMeshDragged);

		ImGuiViewportMeshDraggedEvent(const std::string& title) : m_Filepath(title) { }

	private:
		std::string m_Filepath;
	};


}