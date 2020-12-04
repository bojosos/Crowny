#include "cwepch.h"

#include "Panels/ImGuiConsolePanel.h"

namespace Crowny
{

    ImGuiConsolePanel::ImGuiConsolePanel(const std::string& name) : ImGuiPanel(name)
	{

	}

    void ImGuiConsolePanel::Clear()
    {
        Log::s_Output.flush();
    }

	void ImGuiConsolePanel::Render()
	{
        ImGui::Begin("Console");
        
        m_Buffer.clear();
        m_LineOffsets.clear();
		m_Buffer.append(Log::s_Output.str().c_str());
        for (int i = 0; i < m_Buffer.size(); i++)
            if (m_Buffer[i] == '\n')
                m_LineOffsets.push_back(i + 1);

        if (ImGui::Button("Clear"))
            Clear();
        //if (ImGui::Button("Copy"))
            //ImGui::LogToClipboard();

        ImGui::Separator();
        ImGui::BeginChild("scrolling", ImVec2(0, 0), false, ImGuiWindowFlags_HorizontalScrollbar);

        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0, 0));
        const char* buf = m_Buffer.begin();
        const char* buf_end = m_Buffer.end();
        //ImGui::TextUnformatted(buf, buf_end);
        
        if (m_SearchFilter.IsActive())
        {
            for (int line_no = 0; line_no < m_LineOffsets.size(); line_no++)
            {
                const char* line_start = buf + m_LineOffsets[line_no];
                const char* line_end = (line_no + 1 < m_LineOffsets.size()) ? (buf + m_LineOffsets[line_no + 1] - 1) : buf_end;
                if (m_SearchFilter.PassFilter(line_start, line_end))
                {
                    if (m_Info.PassFilter(line_start, line_end))
                    {
                        ImGui::PushStyleColor(ImGuiCol_Text, { 0, 128, 0, 255 });
                        ImGui::TextUnformatted(line_start, line_end);
                        ImGui::PopStyleColor();
                    } else if (m_Warning.PassFilter(line_start, line_end))
                    {
                        ImGui::PushStyleColor(ImGuiCol_Text, { 255, 255, 0, 255 });
                        ImGui::TextUnformatted(line_start, line_end);
                        ImGui::PopStyleColor();
                    } else if (m_Error.PassFilter(line_start, line_end))
                    {
                        ImGui::PushStyleColor(ImGuiCol_Text, { 220, 0, 0, 255 });
                        ImGui::TextUnformatted(line_start, line_end);
                        ImGui::PopStyleColor();
                    }
                    else if (m_Critical.PassFilter(line_start, line_end))
                    {
                        ImGui::PushStyleColor(ImGuiCol_Text, { 139, 0, 0, 255 });
                        ImGui::TextUnformatted(line_start, line_end);
                        ImGui::PopStyleColor();
                    } else ImGui::TextUnformatted(line_start, line_end);
                }
            }
        }
        else
        {
            ImGuiListClipper clipper;
            clipper.Begin(m_LineOffsets.size());
            while (clipper.Step())
            {
                for (int line_no = clipper.DisplayStart; line_no < clipper.DisplayEnd; line_no++)
                {
                    const char* line_start = buf + m_LineOffsets[line_no];
                    const char* line_end = (line_no + 1 < m_LineOffsets.size()) ? (buf + m_LineOffsets[line_no + 1] - 1) : buf_end;
                    if (m_SearchFilter.PassFilter(line_start, line_end))
                    {
                        if (m_Info.PassFilter(line_start, line_end))
                        {
                            ImGui::PushStyleColor(ImGuiCol_Text, { 0, 128, 0, 255 });
                            ImGui::TextUnformatted(line_start, line_end);
                            ImGui::PopStyleColor();
                        } else if (m_Warning.PassFilter(line_start, line_end))
                        {
                            ImGui::PushStyleColor(ImGuiCol_Text, { 255, 255, 0, 255 });
                            ImGui::TextUnformatted(line_start, line_end);
                            ImGui::PopStyleColor();
                        } else if (m_Error.PassFilter(line_start, line_end))
                        {
                            ImGui::PushStyleColor(ImGuiCol_Text, { 220, 0, 0, 255 });
                            ImGui::TextUnformatted(line_start, line_end);
                            ImGui::PopStyleColor();
                        }
                        else if (m_Critical.PassFilter(line_start, line_end))
                        {
                            ImGui::PushStyleColor(ImGuiCol_Text, { 139, 0, 0, 255 });
                            ImGui::TextUnformatted(line_start, line_end);
                            ImGui::PopStyleColor();
                        } else ImGui::TextUnformatted(line_start, line_end);
                    }
                }
            }
            clipper.End();
        }
        //ImGui::PopStyleColor();
        ImGui::PopStyleVar();

        if (m_AutoScroll && ImGui::GetScrollY() >= ImGui::GetScrollMaxY())
            ImGui::SetScrollHereY(1.0f);

        ImGui::EndChild();
        ImGui::End();
	}
}