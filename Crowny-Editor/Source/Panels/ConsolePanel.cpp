#include "cwepch.h"

#include "Panels/ConsolePanel.h"

namespace Crowny
{

    ConsolePanel::ConsolePanel(const String& name) : ImGuiPanel(name) {}

    void ConsolePanel::Render()
    {
        m_RequestScrollToBottom = ImGuiConsoleBuffer::HasNewMessages();
        ImGui::Begin("Console", &m_Shown);
        UpdateState();
        RenderHeader();
        ImGui::Separator();
        RenderMessages();
        ImGui::End();
    }

    void ConsolePanel::RenderHeader()
    {
        ImGuiStyle& style = ImGui::GetStyle();
        const float spacing = style.ItemInnerSpacing.x;
        ImGui::AlignTextToFramePadding();

        for (int i = 0; i < ImGuiConsoleBuffer::Message::s_Levels.size(); i++)
        {
            ImGui::SameLine(0.0f, 2.0f * spacing);
            glm::vec4 color = GetRenderColor(ImGuiConsoleBuffer::Message::s_Levels[i]);
            // ImGui::PushStyleColor(ImGuiCol_Text, { color.r, color.g, color.b, color.a });
            ImGui::Checkbox(ImGuiConsoleBuffer::Message::GetLevelName(ImGuiConsoleBuffer::Message::s_Levels[i]),
                            &m_EnabledLevels[i]);
            // ImGui::PopStyleColor();
        }

        RenderSettings();
    }

    void ConsolePanel::RenderSettings()
    {
        const float maxWidth = ImGui::CalcTextSize("Scroll to bottom").x * 1.1f;
        const float spacing = ImGui::GetStyle().ItemInnerSpacing.x + ImGui::CalcTextSize(" ").x;
        const float checkboxSize = ImGui::GetFontSize() + ImGui::GetStyle().FramePadding.y * 2;

        ImGui::SameLine(ImGui::GetContentRegionAvail().x - checkboxSize - ImGui::CalcTextSize("Clear console").x +
                        1.0f - maxWidth - 2 * spacing);
        ImGui::AlignTextToFramePadding();
        ImGui::Text("Scroll to bottom");
        ImGui::SameLine(0.0f, spacing);
        ImGui::Checkbox("##ScrollToBottom", &m_AllowScrollingToBottom);
        ImGui::SameLine(0.0f, spacing);
        if (ImGui::Button("Clear console"))
            ImGuiConsoleBuffer::Clear();
    }

    void ConsolePanel::RenderMessages()
    {
        ImGui::BeginChild("ScrollRegion", ImVec2(0, 0), false, ImGuiWindowFlags_HorizontalScrollbar);
        {
            ImGui::SetWindowFontScale(m_DisplayScale);
            for (auto& message : ImGuiConsoleBuffer::GetBuffer())
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

    void ConsolePanel::RenderMessage(const Ref<ImGuiConsoleBuffer::Message>& message)
    {
        ImGuiConsoleBuffer::Message::Level level = message->GetLevel();
        if (level != ImGuiConsoleBuffer::Message::Level::Invalid && m_EnabledLevels[(int8_t)level])
        {
            glm::vec4 color = GetRenderColor(level);
            ImGui::PushStyleColor(ImGuiCol_Text, { color.r, color.g, color.b, color.a });
            ImGui::PushStyleVar(ImGuiStyleVar_SelectableTextAlign, ImVec2(0.0f, 0.5f));
            bool selected = m_SelectedMessage == message;
            ImGui::Selectable(
              message->GetMessage().c_str(), &selected, 0,
              { ImGui::GetContentRegionAvailWidth(), ImGui::CalcTextSize(message->GetMessage().c_str()).y * 2.0f });
            if (selected)
                m_SelectedMessage = message;
            ImGui::PopStyleVar();

            ImDrawList* draw_list = ImGui::GetWindowDrawList();
            draw_list->AddRect(ImGui::GetItemRectMin(), ImGui::GetItemRectMax(), IM_COL32(255, 255, 255, 255));
            ImGui::PopStyleColor(1);
        }
    }
} // namespace Crowny