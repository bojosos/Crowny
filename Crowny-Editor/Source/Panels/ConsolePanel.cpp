#include "cwepch.h"

#include "Panels/ConsolePanel.h"

#include <imgui.h>

// TODO: Add binary search for placing new messages in the right place, for collapsed mode I would need to add the count
// and then check if the message has to be moved up
// TODO: Move localtime call to buffer
// TODO: Consider sorting case insensitive
// TODO: Consider displaying newer messages first in collapsed mode when no sorting is used with std::max(timestamp1,
// timestamp2)

#include "Vendor/FontAwesome/IconsFontAwesome6.h"

namespace Crowny
{

    ConsolePanel::ConsolePanel(const String& name) : ImGuiPanel(name), m_SelectedMessageHash(0) {}

    void ConsolePanel::Render()
    {
        BeginPanel();
        m_RequestScrollToBottom = m_AllowScrollingToBottom && ImGuiConsoleBuffer::Get().HasNewMessages();
        RenderHeader();
		ImGui::SetCursorPosY(ImGui::GetCursorPosY() - 4.0f);
        ImGui::Separator();
        RenderMessages();
        EndPanel();
    }

    void ConsolePanel::RenderHeader()
    {
        ImGuiStyle& style = ImGui::GetStyle();

		ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(2, 2));
        ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0.0f, 0.0f, 0.0f, 0.0f));

		auto drawButton = [&](const char* icon, ImGuiConsoleBuffer::Message::Level level)
		{
			if (m_EnabledLevels[(uint32_t)level])
			{
				ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.33333334f, 0.3529412f, 0.36078432f, 0.5f));
				ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.7f, 0.7f, 0.7f, 1.00f));
			}
			else
			{
				ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.0f, 0.0f, 0.0f, 0.0f));
				ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.7f, 0.7f, 0.7f, 1.00f));
			}
			if (ImGui::Button(icon))
				m_EnabledLevels[(uint32_t)level] = !m_EnabledLevels[(uint32_t)level];
			ImGui::PopStyleColor(2);
		};
		
		ImGui::PushFont(ImGui::GetIO().Fonts->Fonts[1]);
		glm::vec4 color = GetRenderColor(ImGuiConsoleBuffer::Message::Levels[0]);
		ImGui::PushStyleColor(ImGuiCol_Text, { color.r, color.g, color.b, color.a });
        drawButton(ICON_FA_EXCLAMATION, ImGuiConsoleBuffer::Message::Level::Info);
		ImGui::PopStyleColor();
		ImGui::SameLine(0.0f, 2.0f * ImGui::GetStyle().ItemInnerSpacing.x);
		color = GetRenderColor(ImGuiConsoleBuffer::Message::Levels[1]);
		ImGui::PushStyleColor(ImGuiCol_Text, { color.r, color.g, color.b, color.a });
        drawButton(ICON_FA_TRIANGLE_EXCLAMATION, ImGuiConsoleBuffer::Message::Level::Warn);
		ImGui::PopStyleColor();
		ImGui::SameLine(0.0f, 2.0f * ImGui::GetStyle().ItemInnerSpacing.x);
		color = GetRenderColor(ImGuiConsoleBuffer::Message::Levels[2]);
		ImGui::PushStyleColor(ImGuiCol_Text, { color.r, color.g, color.b, color.a });
        drawButton(ICON_FA_CIRCLE_EXCLAMATION, ImGuiConsoleBuffer::Message::Level::Error);
		ImGui::PopStyleColor();
		ImGui::PopFont();
		ImGui::PopStyleColor();
        ImGui::PopStyleVar(1);

        RenderSettings();
    }

    void ConsolePanel::RenderSettings()
    {
        const float maxWidth = ImGui::CalcTextSize("Scroll to bottom").x * 1.1f;
        const float spacing = ImGui::GetStyle().ItemInnerSpacing.x + ImGui::CalcTextSize(" ").x;
        const float checkboxSize = ImGui::GetFontSize() + ImGui::GetStyle().FramePadding.y * 2;
        const float collapseWidth = ImGui::CalcTextSize("Collapse").x + ImGui::GetStyle().ItemInnerSpacing.x;
        ImGui::SameLine(ImGui::GetContentRegionAvail().x - checkboxSize - ImGui::CalcTextSize("Clear console").x +
                        1.0f - maxWidth - 2 * spacing - collapseWidth - spacing - checkboxSize);
		
        ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 4.0f);
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
        ImGui::SetWindowFontScale(m_DisplayScale);
        ImGuiTableFlags flags = ImGuiTableFlags_Borders | ImGuiTableFlags_SortMulti | ImGuiTableFlags_Sortable |
                                ImGuiTableFlags_Resizable | ImGuiTableFlags_Hideable | ImGuiTableFlags_ScrollY;
        if (ImGui::BeginTable("##consoleTable", 2, flags))
        {
            if (!m_Collapse)
                ImGui::TableSetupColumn("Time", ImGuiTableColumnFlags_WidthStretch | ImGuiTableColumnFlags_DefaultSort,
                                        0.03f);
            if (m_Collapse)
                ImGui::TableSetupColumn("#", ImGuiTableColumnFlags_WidthStretch | ImGuiTableColumnFlags_DefaultSort,
                                        0.03f);
            ImGui::TableSetupColumn("Message", ImGuiTableColumnFlags_WidthStretch, 0.97f);
            ImGui::TableHeadersRow();

            const auto& buffer = ImGuiConsoleBuffer::Get().GetBuffer();
			Vector<uint32_t> messageIndices;
			messageIndices.resize(buffer.size());
			uint32_t messageIdx = 0;
            for (uint32_t i = 0; i < (uint32_t)buffer.size(); i++)
            {
				const auto& message = buffer[i];
				if (m_EnabledLevels[(uint8_t)message.LogLevel])
					messageIndices[messageIdx++] = i;
            }
            ImGuiListClipper clipper;
            clipper.Begin(messageIdx);
            while (clipper.Step())
            {
                ImGui::TableNextRow();
                for (int row = clipper.DisplayStart; row < clipper.DisplayEnd; row++)
                    RenderMessage(buffer[messageIndices[row]]);
            }

            bool needSort = false;
            ImGuiTableSortSpecs* sortSpec = ImGui::TableGetSortSpecs();
            if (sortSpec && sortSpec->SpecsDirty)
                needSort = true;
            if (sortSpec && needSort)
            {
                ImGuiConsoleBuffer::Get().Sort(sortSpec->Specs[0].ColumnIndex,
                                               sortSpec->Specs[0].SortDirection == ImGuiSortDirection_Ascending);
                sortSpec->SpecsDirty = false;
            }

            if (m_RequestScrollToBottom && ImGui::GetScrollMaxY() > 0)
            {
                ImGui::SetScrollHereY(1.0f);
                m_RequestScrollToBottom = false;
            }
            ImGui::EndTable();
        }
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
            ImGui::TableNextColumn();
            if (!m_Collapse) // timestamp
            {
                char res[9];
                tm* timeinfo;
                timeinfo = localtime(&message.Timestamp);
                strftime(res, 9, "%T", timeinfo);
                ImGui::Text(res);
            }
            else // repeats
            {
                ImGui::AlignTextToFramePadding();
                ImGui::Text("%d", message.RepeatCount);
            }
            ImGui::TableNextColumn();
            if (ImGui::Selectable(message.MessageText.c_str(), &selected))
                m_SelectedMessageHash = message.Hash;
            ImGui::PopStyleVar();
            ImGui::PopStyleColor(1);

            // ImGui::TableNextRow(); // Do it here, due to severity check
        }
    }
} // namespace Crowny