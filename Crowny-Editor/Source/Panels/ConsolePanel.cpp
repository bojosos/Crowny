#include "cwepch.h"

#include "Panels/ConsolePanel.h"

#include <imgui.h>

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
        ImGui::BeginChild("ScrollRegion", ImVec2(0, 0), false, ImGuiWindowFlags_HorizontalScrollbar);
        {
            ImGui::SetWindowFontScale(m_DisplayScale);
            for (auto& message : ImGuiConsoleBuffer::Get().GetBuffer())
                RenderMessage(message);

            if (m_RequestScrollToBottom && ImGui::GetScrollMaxY() > 0)
            {
                // ImGui::SetScrollY(ImGui::GetScrollMaxY());
                ImGui::SetScrollHereY(1.0f);
                m_RequestScrollToBottom = false;
            }
        }
        ImGui::EndChild();
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
            if (ImGui::Selectable(message.MessageText.c_str(), &selected, 0, { ImGui::GetContentRegionAvail().x, ImGui::CalcTextSize(message.MessageText.c_str()).y * 2.0f }))
                m_SelectedMessageHash = message.Hash;
            ImGui::PopStyleVar();

            ImDrawList* draw_list = ImGui::GetWindowDrawList(); // border
            draw_list->AddRect(ImGui::GetItemRectMin(), ImGui::GetItemRectMax(), IM_COL32(255, 255, 255, 255));
            ImGui::PopStyleColor(1);
            if (m_Collapse)
            {
                ImGui::AlignTextToFramePadding();
                ImGui::SameLine(ImGui::GetContentRegionAvail().x - ImGui::GetFontSize() * 5);
                ImGui::Text("%d", message.RepeatCount);
                // ImGui::SameLine();
            }
        }
    }
} // namespace Crowny