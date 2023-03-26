#include "cwepch.h"

#include "Editor/EditorAssets.h"
#include "Panels/ConsolePanel.h"
#include "UI/UIUtils.h"

#include <imgui.h>

// TODO: Add binary search for placing new messages in the right place, for collapsed mode I would need to add the count
// and then check if the message has to be moved up
// TODO: Move localtime call to buffer
// TODO: Consider sorting case insensitive
// TODO: Consider displaying newer messages first in collapsed mode when no sorting is used with std::max(timestamp1,
// timestamp2)

namespace Crowny
{

    ConsolePanel::ConsolePanel(const String& name) : ImGuiPanel(name), m_SelectedMessageHash(0)
    {
        m_RequestScrollToBottom = m_AllowScrollingToBottom;
    }

    void ConsolePanel::Render()
    {
        UI::ScopedStyle windowPadding(ImGuiStyleVar_WindowPadding, ImVec2(8.0f, 2.0f));
        BeginPanel();
        m_RequestScrollToBottom = m_AllowScrollingToBottom && ConsoleBuffer::Get().HasNewMessages();
        RenderHeader();
        ImGui::Separator();
        RenderMessages();
        EndPanel();
    }

    void ConsolePanel::RenderHeader()
    {
        UI::ScopedStyle style(ImGuiStyleVar_ItemSpacing, ImVec2(6, 2));
        UI::ScopedColor color(ImGuiCol_Border, ImVec4(0.0f, 0.0f, 0.0f, 0.0f));

        auto drawButton = [](const Ref<Texture>& icon, const ImColor& tint, float paddingY = 0.0f) {
            const float height = std::min((float)icon->GetHeight(), 24.0f) - paddingY * 2.0f;
            const float width = (float)icon->GetWidth() / (float)icon->GetHeight() * height;
            const bool clicked = ImGui::InvisibleButton(UI::GenerateID(), ImVec2(width, height));
            ImColor hover = ImGui::GetStyle().Colors[ImGuiCol_ButtonHovered];
            ImColor active = ImGui::GetStyle().Colors[ImGuiCol_ButtonActive];
            UI::DrawButtonImage(icon, tint, hover, active, UI::RectOffset(UI::GetItemRect(), 0.0f, paddingY));
            return clicked;
        };
        ImGui::BeginVertical("##consolePanelV", { ImGui::GetContentRegionAvailWidth(), 0.0f });
        ImGui::Spring();
        ImGui::BeginHorizontal("##consolePanelH", { ImGui::GetContentRegionAvailWidth(), 0.0f });
        ImColor tint = m_EnabledLevels[(uint32_t)ConsoleBuffer::Message::Level::Info]
                         ? IM_COL32(236, 158, 36, 255)
                         : IM_COL32(192, 192, 192, 255);
        if (drawButton(EditorAssets::Get().ConsoleInfo, tint))
            m_EnabledLevels[(uint32_t)ConsoleBuffer::Message::Level::Info] =
              !m_EnabledLevels[(uint32_t)ConsoleBuffer::Message::Level::Info];
        tint = m_EnabledLevels[(uint32_t)ConsoleBuffer::Message::Level::Warn] ? IM_COL32(236, 158, 36, 255)
                                                                                   : IM_COL32(192, 192, 192, 255);
        if (drawButton(EditorAssets::Get().ConsoleWarn, tint))
            m_EnabledLevels[(uint32_t)ConsoleBuffer::Message::Level::Warn] =
              !m_EnabledLevels[(uint32_t)ConsoleBuffer::Message::Level::Warn];
        tint = m_EnabledLevels[(uint32_t)ConsoleBuffer::Message::Level::Error] ? IM_COL32(236, 158, 36, 255)
                                                                                    : IM_COL32(192, 192, 192, 255);
        if (drawButton(EditorAssets::Get().ConsoleError, tint))
            m_EnabledLevels[(uint32_t)ConsoleBuffer::Message::Level::Error] =
              !m_EnabledLevels[(uint32_t)ConsoleBuffer::Message::Level::Error];

        UI::ScopedStyle layoutRight(ImGuiStyleVar_LayoutAlign, 1.0f);
        ImGui::Spring();
        RenderSettings();
        ImGui::EndHorizontal();
        ImGui::Spring();
        ImGui::EndVertical();
    }

    void ConsolePanel::RenderSettings()
    {
        UI::ShiftCursorY(3.0f); // For some reason I need to shift the cursor to make the text align properly
        ImGui::Text("Scroll to bottom");
        UI::ShiftCursorY(-3.0f);
        ImGui::Checkbox("##ScrollToBottom", &m_AllowScrollingToBottom);
        ImGui::Text("Collapse");
        if (ImGui::Checkbox("##Collapse", &m_Collapse))
        {
            if (m_Collapse)
                ConsoleBuffer::Get().Collapse();
            else
                ConsoleBuffer::Get().Uncollapse();
        }
        if (ImGui::Button("Clear console"))
            ConsoleBuffer::Get().Clear();
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

            const auto& buffer = ConsoleBuffer::Get().GetBuffer();

            m_MessageIndices.clear();
            m_MessageIndices.resize(buffer.size());
            uint32_t messageIdx = 0;
            for (uint32_t i = 0; i < (uint32_t)buffer.size(); i++)
            {
                const auto& message = buffer[i];
                if (m_EnabledLevels[(uint8_t)message.LogLevel])
                    m_MessageIndices[messageIdx++] = i;
            }
            ImGuiListClipper clipper;
            clipper.Begin(messageIdx);
            while (clipper.Step())
            {
                ImGui::TableNextRow();
                for (int row = clipper.DisplayStart; row < clipper.DisplayEnd; row++)
                    RenderMessage(buffer[m_MessageIndices[row]]);
            }

            bool needSort = false;
            ImGuiTableSortSpecs* sortSpec = ImGui::TableGetSortSpecs();
            if (sortSpec && sortSpec->SpecsDirty)
                needSort = true;
            if (sortSpec && needSort)
            {
                ConsoleBuffer::Get().Sort(sortSpec->Specs[0].ColumnIndex,
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

    void ConsolePanel::RenderMessage(const ConsoleBuffer::Message& message)
    {
        ConsoleBuffer::Message::Level level = message.LogLevel;
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