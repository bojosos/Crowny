#pragma once

#include "Panels/ImGuiPanel.h"

#include <imgui.h>

namespace Crowny
{
    class ImGuiConsolePanel : public ImGuiPanel
    {
    public:
		ImGuiConsolePanel(const std::string& name);
		~ImGuiConsolePanel() = default;

		virtual void Render() override;
        void Clear();

    private:
        ImGuiTextBuffer m_Buffer;
        ImGuiTextFilter m_SearchFilter;
        ImGuiTextFilter m_Info { "] [info]" }, m_Warning { "] [warning]" }, m_Error { "] [error]" }, m_Critical { "] [critical]" };
        std::vector<int32_t> m_LineOffsets;
        bool m_AutoScroll = true;
    };

}