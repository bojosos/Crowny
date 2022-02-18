#pragma once

#include "Crowny/ImGui/ImGuiMenu.h"

namespace Crowny
{
    class ImGuiMenuItem;
    typedef int ImGuiWindowFlags;

    class ImGuiPanel
    {
    public:
        ImGuiPanel(const String& name);
        virtual ~ImGuiPanel() = default;

        virtual void Show() { m_Shown = true; };
        virtual void Hide() { m_Shown = false; };

        virtual void Render() = 0;
        virtual const String& GetName() const { return m_Name; }
        virtual bool IsFocused() { return m_Focused; }
        virtual bool IsHovered() { return m_Hovered; }

        void RegisterInMenu(ImGuiMenu* menu);
        bool IsShown() { return m_Shown; }

    protected:
        void UpdateState();
        void BeginPanel(ImGuiWindowFlags flags = 0);
        void EndPanel();

    protected:
        friend class ImGuiMenuItem;
        bool m_Focused = false, m_Hovered = false;
        String m_Name;
        bool m_Shown = true;
    };
} // namespace Crowny
