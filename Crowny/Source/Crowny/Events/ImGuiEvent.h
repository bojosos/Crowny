#pragma once

#include "Crowny/Events/Event.h"
#include <string>

namespace Crowny
{

    class ImGuiMenuItemClickedEvent : public Event
    {
    public:
        const String& GetTitle() const { return m_Title; }

        EVENT_CLASS_CATEGORY(EventCategoryImGui);
        EVENT_CLASS_TYPE(ImGuiMenuItemClicked);

        ImGuiMenuItemClickedEvent(const String& title) : m_Title(title) {}

    private:
        String m_Title;
    };

    class ImGuiViewportMeshDraggedEvent : public Event
    {
    public:
        const Path& GetSceneFilepath() const { return m_Filepath; }

        EVENT_CLASS_CATEGORY(EventCategoryImGui);
        EVENT_CLASS_TYPE(ImGuiViewportMeshDragged);

        ImGuiViewportMeshDraggedEvent(const Path& meshPath) : m_Filepath(meshPath) {}

    private:
        Path m_Filepath;
    };

} // namespace Crowny