#pragma once

#include "Crowny/Common/Common.h"

namespace Crowny
{
    typedef int ImGuiWindowFlags;

    class ImGuiPanel
    {
    public:
        ImGuiPanel(const String& name);
        virtual ~ImGuiPanel() = default;

        void Show() { m_Shown = true; }
        void Hide() { m_Shown = false; }
        void Toggle() { m_Shown = !m_Shown; }
        void SetShown(bool shown) { m_Shown = shown; }

        virtual void Render() = 0;

        const String& GetName() const { return m_Name; }
        bool IsFocused() const { return m_Focused; }
        bool IsHovered() const { return m_Hovered; }

        bool IsShown() const { return m_Shown; }

    protected:
        void UpdateState();
        bool BeginPanel(ImGuiWindowFlags flags = 0);
        void EndPanel();

    protected:
        bool m_Focused = false, m_Hovered = false;
        String m_Name;
        bool m_Shown = true;
        bool m_BeginCalled = false;
    };
} // namespace Crowny
