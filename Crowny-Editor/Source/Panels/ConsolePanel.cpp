#include "cwepch.h"

#include "Panels/ConsolePanel.h"

#include <imgui.h>

// TODO: Use ImGuiListClipper
// This causes some problems. Can't use severity filters for now.
// Solutions: 
//   Console buffer with a list for every message type. Then when creating the Clipper give it the size of all enabled. Getting the next message would be slower.
//      Also using the clipper I would need to go trough the first clipper.DisplayStart elements, defeating the purpose
//   Add another buffer to console buffer and reconstruct every time some setting changes? Maybe a buffer with ints only could work?
// TODO: Add binary search for placing new messages in the right place, for collapsed mode I would need to add the count and then check if the message has to be moved up
// TODO: Move localtime call to buffer
// TODO: Consider sorting case insensitive
// TODO: Consider displaying newer messages first in collapsed mode when no sorting is used with std::max(timestamp1, timestamp2)
// TODO: Fix scroll to bottom

namespace Crowny
{

    ConsolePanel::ConsolePanel(const String& name) : ImGuiPanel(name) { }

    void ConsolePanel::Render()
    {
        BeginPanel();
        m_RequestScrollToBottom = m_AllowScrollingToBottom && ImGuiConsoleBuffer::Get().HasNewMessages();
        RenderHeader();
        ImGui::Separator();
        RenderMessages();
        EndPanel();
    }

    void ConsolePanel::RenderHeader()
    {
        ImGuiStyle& style = ImGui::GetStyle();
        const float spacing = style.ItemInnerSpacing.x;
        ImGui::AlignTextToFramePadding();

        for (int i = 0; i < ImGuiConsoleBuffer::Message::Levels.size(); i++)
        {
            ImGui::SameLine(0.0f, 2.0f * spacing);
            glm::vec4 color = GetRenderColor(ImGuiConsoleBuffer::Message::Levels[i]);
            // ImGui::PushStyleColor(ImGuiCol_Text, { color.r, color.g, color.b, color.a });
            ImGui::Checkbox(ImGuiConsoleBuffer::Message::GetLevelName(ImGuiConsoleBuffer::Message::Levels[i]),
                            &m_EnabledLevels[i]);
            // ImGui::PopStyleColor();
        }

        RenderSettings();
    }

    void ConsolePanel::RenderSettings()
    {
        const float collapseWidth = ImGui::CalcTextSize("Collapse").x * 1.1f;

        const float maxWidth = ImGui::CalcTextSize("Scroll to bottom").x * 1.1f;
        const float spacing = ImGui::GetStyle().ItemInnerSpacing.x + ImGui::CalcTextSize(" ").x;
        const float checkboxSize = ImGui::GetFontSize() + ImGui::GetStyle().FramePadding.y * 2;

        ImGui::SameLine(ImGui::GetContentRegionAvail().x - checkboxSize - ImGui::CalcTextSize("Clear console").x +
                        1.0f - maxWidth - 2 * spacing - collapseWidth - 2 * spacing - checkboxSize);
        ImGui::AlignTextToFramePadding();
        ImGui::Text("Scroll to bottom");
        ImGui::SameLine(0.0f, spacing);
        ImGui::Checkbox("##ScrollToBottom", &m_AllowScrollingToBottom);
        ImGui::SameLine(0.0f, spacing);
        ImGui::Text("Collapse");
        ImGui::SameLine(0.0f, spacing);
        if (ImGui::Checkbox("##Collapse", &m_Collapse))
        {
            if (m_Collapse)
                ImGuiConsoleBuffer::Get().Collapse();
            else
                ImGuiConsoleBuffer::Get().Uncollapse();
        }
        ImGui::SameLine(0.0f, spacing);
        if (ImGui::Button("Clear console"))
            ImGuiConsoleBuffer::Get().Clear();
    }

    void ConsolePanel::RenderMessages()
    {
       // ImGui::BeginChild("ScrollRegion", ImVec2(0, 0), false, ImGuiWindowFlags_HorizontalScrollbar);
        {
            ImGui::SetWindowFontScale(m_DisplayScale);
            ImGuiTableFlags flags = ImGuiTableFlags_Borders | ImGuiTableFlags_SortMulti | ImGuiTableFlags_Sortable | ImGuiTableFlags_Resizable | ImGuiTableFlags_Hideable | ImGuiTableFlags_ScrollY;
            if (ImGui::BeginTable("##consoleTable", 2, flags))
            {
                float width = ImGui::GetContentRegionAvailWidth();
                if (!m_Collapse)
                    ImGui::TableSetupColumn("Time", ImGuiTableColumnFlags_WidthStretch | ImGuiTableColumnFlags_DefaultSort, 0.03f);
                if (m_Collapse)
                    ImGui::TableSetupColumn("#", ImGuiTableColumnFlags_WidthStretch | ImGuiTableColumnFlags_DefaultSort, 0.03f);
                ImGui::TableSetupColumn("Message", ImGuiTableColumnFlags_WidthStretch, 0.97f);
                ImGui::TableHeadersRow();

                const auto& buffer = ImGuiConsoleBuffer::Get().GetBuffer();
				ImGuiListClipper clipper;
				clipper.Begin(buffer.size());
				while (clipper.Step())
				{
                    ImGui::TableNextRow();
					for (int row = clipper.DisplayStart; row < clipper.DisplayEnd; row++)
                        RenderMessage(buffer[row]);
                }
				
                bool needSort = false;
				ImGuiTableSortSpecs* sortSpec = ImGui::TableGetSortSpecs();
				if (sortSpec && sortSpec->SpecsDirty)
					needSort = true;
				if (sortSpec && needSort)
				{
                    ImGuiConsoleBuffer::Get().Sort(sortSpec->Specs[0].ColumnIndex, sortSpec->Specs[0].SortDirection == ImGuiSortDirection_Ascending);
					sortSpec->SpecsDirty = false;
				}

                ImGui::EndTable();
            }
            if (m_RequestScrollToBottom && ImGui::GetScrollMaxY() > 0)
            {
                ImGui::SetScrollY(ImGui::GetScrollMaxY());
                // ImGui::SetScrollHereY(1.0f);
                m_RequestScrollToBottom = false;
            }
        }
       // ImGui::EndChild();
    }

    void ConsolePanel::RenderMessage(const ImGuiConsoleBuffer::Message& message)
    {
        ImGuiConsoleBuffer::Message::Level level = message.LogLevel;
        if (m_EnabledLevels[(uint8_t)level])
        {
            glm::vec4 color = GetRenderColor(level);
            ImGui::PushStyleColor(ImGuiCol_Text, { color.r, color.g, color.b, color.a });
            ImGui::PushStyleVar(ImGuiStyleVar_SelectableTextAlign, ImVec2(0.0f, 0.5f));
            bool selected = m_SelectedMessageHash == message.Hash;
            ImGui::TableNextColumn(); // Draw timestamp here
			if (!m_Collapse)
			{
                char res[9];
			    tm* timeinfo;
			    timeinfo = localtime(&message.Timestamp);
                strftime(res, 9, "%T", timeinfo);
                ImGui::Text(res);
            }
            else
			{
                ImGui::AlignTextToFramePadding();
                ImGui::Text("%d", message.RepeatCount);
			}
            ImGui::TableNextColumn();
            if (ImGui::Selectable(message.MessageText.c_str(), &selected))
                m_SelectedMessageHash = message.Hash;
            ImGui::PopStyleVar();
            ImGui::PopStyleColor(1);
            
//			ImGui::TableNextRow(); // Do it here, due to severity check
        }
    }
} // namespace Crowny