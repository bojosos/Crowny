#include "cwepch.h"

#include "Editor/Script/CodeEditor.h"
#include "Panels/ConsolePanel.h"
#include "UI/UIUtils.h"

#include "Crowny/Common/PlatformUtils.h"
#include "Crowny/Input/Input.h"

#include <imgui.h>

namespace Crowny
{

    ConsolePanel::ConsolePanel(const String& name) : ImGuiPanel(name) { m_RequestScrollToBottom = m_AllowScrollingToBottom; }

    void ConsolePanel::Render()
    {
        UI::ScopedStyle windowPadding(ImGuiStyleVar_WindowPadding, ImVec2(8.0f, 2.0f));
        if (!BeginPanel(ImGuiWindowFlags_NoScrollWithMouse | ImGuiWindowFlags_NoScrollbar))
        {
            EndPanel();
            return;
        }
        const float availableHeight = ImGui::GetContentRegionAvail().y;
        const float splitterHeight = 6.0f;
        const float minimumDetailsHeight = std::min(100.0f, availableHeight * 0.4f);
        const float maximumMessageHeight = std::max(1.0f, availableHeight - minimumDetailsHeight - splitterHeight);
        const float minimumMessageHeight = std::min(120.0f, maximumMessageHeight);
        if (m_MessageHeight <= 0.0f)
            m_MessageHeight = maximumMessageHeight;
        m_MessageHeight = std::clamp(m_MessageHeight, minimumMessageHeight, maximumMessageHeight);

        ConsoleBuffer& console = ConsoleBuffer::Get();
        const bool hasNewMessages = console.HasNewMessages();
        if (hasNewMessages && m_AllowScrollingToBottom)
            m_RequestScrollToBottom = true;
        RefreshMessages();

        ImGui::BeginChild("##consoleMessages", ImVec2(0, m_MessageHeight), true);
        RenderHeader();
        ImGui::Separator();
        RenderMessages();

        // Ctrl+C copies the selected message text + callstack
        const bool controlPressed = Input::IsKeyPressed(Key::LeftControl) || Input::IsKeyPressed(Key::RightControl);
        if (m_Focused && m_SelectedMessageId != 0 && controlPressed && Input::IsKeyDown(Key::C))
            CopySelectedMessage();

        ImGui::EndChild();
        ImGui::InvisibleButton("##consoleSplitter", ImVec2(-1, splitterHeight));
        if (ImGui::IsItemHovered())
            ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeNS);
        if (ImGui::IsItemActive())
            m_MessageHeight = std::clamp(m_MessageHeight + ImGui::GetIO().MouseDelta.y, minimumMessageHeight, maximumMessageHeight);
        ImGui::BeginChild("##consoleDetails", ImVec2(0, 0), true);
        RenderFooter();
        ImGui::EndChild();

        EndPanel();
    }

    void ConsolePanel::RenderHeader()
    {
        const float availableWidth = ImGui::GetContentRegionAvail().x;
        if (availableWidth >= 620.0f && ImGui::BeginTable("##consoleTopBar", 2, ImGuiTableFlags_SizingStretchProp))
        {
            ImGui::TableSetupColumn("Search", ImGuiTableColumnFlags_WidthStretch);
            ImGui::TableSetupColumn("Actions", ImGuiTableColumnFlags_WidthFixed, 260.0f);
            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            ImGui::SetNextItemWidth(-1.0f);
            if (UIUtils::SearchWidget(m_SearchString, "Search (text:, source:, level:, time:, \"phrase\", -exclude)"))
            {
                m_RequestScrollToBottom = false;
                RebuildSearchTerms();
            }
            ImGui::TableNextColumn();
            RenderSettings();
            ImGui::EndTable();
        }
        else
        {
            ImGui::SetNextItemWidth(-1.0f);
            if (UIUtils::SearchWidget(m_SearchString, "Search console..."))
            {
                m_RequestScrollToBottom = false;
                RebuildSearchTerms();
            }
            RenderSettings();
        }

        RebuildFilteredIndices();
        const ConsoleViewModel::Summary& summary = m_ViewModel.GetSummary();
        for (size_t i = 0; i < ConsoleBuffer::Message::Levels.size(); i++)
        {
            const ConsoleBuffer::Message::Level level = ConsoleBuffer::Message::Levels[i];
            const size_t levelIndex = static_cast<uint8_t>(level);
            const char* label = summary.Levels[levelIndex].Label.data();
            const float width = ImGui::CalcTextSize(label).x + ImGui::GetStyle().FramePadding.x * 2.0f;
            const glm::vec4 color = GetRenderColor(level);
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(color.r, color.g, color.b, color.a));
            if (ImGui::Selectable(label, m_EnabledLevels[levelIndex], ImGuiSelectableFlags_None, ImVec2(width, 0.0f)))
            {
                m_EnabledLevels[levelIndex] = !m_EnabledLevels[levelIndex];
                m_FilterDirty = true;
            }
            ImGui::PopStyleColor();
            if (ImGui::IsItemHovered())
                ImGui::SetTooltip("Show or hide %s messages", ConsoleBuffer::Message::GetLevelName(level));
            if (i + 1 < ConsoleBuffer::Message::Levels.size())
                ImGui::SameLine();
        }

        ImGui::SameLine();
        ImGui::TextDisabled("%u shown", summary.ShownCount);
    }

    void ConsolePanel::RenderSettings()
    {
        ImGui::Checkbox("Follow", &m_AllowScrollingToBottom);
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip("Scroll to new messages");
        ImGui::SameLine();
        if (ImGui::Checkbox("Collapse", &m_Collapse))
        {
            if (m_Collapse)
                ConsoleBuffer::Get().Collapse();
            else
                ConsoleBuffer::Get().Uncollapse();
        }
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip("Group repeated messages");
        ImGui::SameLine();
        if (ImGui::Button("Clear"))
        {
            ConsoleBuffer::Get().Clear();
            m_MessageIndices.clear();
            m_MessageSnapshot.clear();
            m_MessageRevision = std::numeric_limits<uint64_t>::max();
            SetSelectedMessage(nullptr);
        }
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip("Remove all console messages");
    }

    void ConsolePanel::RenderMessages()
    {
        ImGui::SetWindowFontScale(m_DisplayScale);
        ImGuiTableFlags flags = ImGuiTableFlags_BordersOuter | ImGuiTableFlags_BordersInnerH | ImGuiTableFlags_RowBg | ImGuiTableFlags_Sortable |
                                ImGuiTableFlags_Resizable | ImGuiTableFlags_ScrollY;
        if (ImGui::BeginTable("##consoleTable", 2, flags))
        {
            if (!m_Collapse)
                ImGui::TableSetupColumn("Time", ImGuiTableColumnFlags_WidthFixed | ImGuiTableColumnFlags_DefaultSort, 76.0f);
            if (m_Collapse)
                ImGui::TableSetupColumn("Count", ImGuiTableColumnFlags_WidthFixed | ImGuiTableColumnFlags_DefaultSort, 58.0f);
            ImGui::TableSetupColumn("Message", ImGuiTableColumnFlags_WidthStretch);
            ImGui::TableHeadersRow();

            RebuildFilteredIndices();

            if (m_MessageIndices.empty())
            {
                ImGui::TableNextRow();
                ImGui::TableSetColumnIndex(1);
                if (m_MessageSnapshot.empty())
                    ImGui::TextDisabled("The console is empty.");
                else if (!m_SearchString.empty())
                    ImGui::TextDisabled("No messages match \"%s\".", m_SearchString.c_str());
                else
                    ImGui::TextDisabled("All message levels are hidden.");
            }

            ImGuiListClipper clipper;
            clipper.Begin(static_cast<int>(m_MessageIndices.size()));
            while (clipper.Step())
            {
                for (int row = clipper.DisplayStart; row < clipper.DisplayEnd; row++)
                {
                    ImGui::TableNextRow();
                    ImGui::PushID(row);
                    RenderMessage(m_MessageSnapshot[m_MessageIndices[row]]);
                    ImGui::PopID();
                }
            }

            bool needSort = false;
            ImGuiTableSortSpecs* sortSpec = ImGui::TableGetSortSpecs();
            if (sortSpec && sortSpec->SpecsDirty)
                needSort = true;
            if (sortSpec && needSort)
            {
                ConsoleBuffer::Get().Sort(sortSpec->Specs[0].ColumnIndex, sortSpec->Specs[0].SortDirection == ImGuiSortDirection_Ascending);
                sortSpec->SpecsDirty = false;
                RefreshMessages();
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
        const ConsoleBuffer::Message::Level level = message.LogLevel;
        const glm::vec4 color = GetRenderColor(level);
        ImGui::PushStyleColor(ImGuiCol_Text, { color.r, color.g, color.b, color.a });
        ImGui::PushStyleVar(ImGuiStyleVar_SelectableTextAlign, ImVec2(0.0f, 0.5f));
        bool selected = m_SelectedMessageId == message.Sequence;
        ImGui::TableNextColumn();
        if (!m_Collapse)
            ImGui::TextUnformatted(message.TimestampText.c_str());
        else
            ImGui::Text("%u", message.RepeatCount);
        ImGui::TableNextColumn();
        if (ImGui::Selectable(message.MessageText.c_str(), &selected))
            SetSelectedMessage(&message);
        if (ImGui::IsItemHovered())
        {
            ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
            if (ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left) && !message.Callstack.empty())
                CodeEditorManager::Get().OpenFile(message.Callstack[0].SourceFilePath, message.Callstack[0].Line);
        }

        ImGui::PopStyleVar();
        ImGui::PopStyleColor();
    }

    void ConsolePanel::RenderFooter()
    {
        if (m_SelectedMessageId == 0)
        {
            ImGui::TextDisabled("Select a message to view its details.");
            return;
        }

        const ConsoleBuffer::Message::Level level = m_SelectedMessage.LogLevel;
        const glm::vec4 levelColor = GetRenderColor(level);
        if (ImGui::BeginTable("##consoleDetailHeader", 2, ImGuiTableFlags_SizingStretchProp))
        {
            ImGui::TableSetupColumn("Message", ImGuiTableColumnFlags_WidthStretch);
            ImGui::TableSetupColumn("Copy", ImGuiTableColumnFlags_WidthFixed, 54.0f);
            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            ImGui::TextColored(ImVec4(levelColor.r, levelColor.g, levelColor.b, levelColor.a), "%s", ConsoleBuffer::Message::GetLevelName(level));
            ImGui::SameLine();
            ImGui::TextDisabled("%s", m_SelectedMessage.TimestampText.c_str());
            ImGui::TableNextColumn();
            if (ImGui::Button("Copy", ImVec2(-1.0f, 0.0f)))
                CopySelectedMessage();
            ImGui::EndTable();
        }

        ImGui::Separator();
        ImGui::TextWrapped("%s", m_SelectedMessage.MessageText.c_str());

        if (m_SelectedMessage.Callstack.empty())
        {
            ImGui::Spacing();
            ImGui::TextDisabled("No call stack was recorded.");
            return;
        }

        ImGui::SeparatorText("Call stack");
        if (ImGui::BeginTable("##consoleCallstack", 2, ImGuiTableFlags_BordersInnerH | ImGuiTableFlags_RowBg | ImGuiTableFlags_Resizable))
        {
            const Vector<String>& sourceLabels = m_ViewModel.GetCallstackSourceLabels();
            ImGui::TableSetupColumn("Function", ImGuiTableColumnFlags_WidthStretch, 0.58f);
            ImGui::TableSetupColumn("Source", ImGuiTableColumnFlags_WidthStretch, 0.42f);
            for (size_t i = 0; i < m_SelectedMessage.Callstack.size(); i++)
            {
                const ConsoleBuffer::Message::FunctionCall& call = m_SelectedMessage.Callstack[i];
                ImGui::PushID(static_cast<int>(i));
                ImGui::TableNextRow();
                ImGui::TableNextColumn();
                ImGui::TextUnformatted(call.FunctionSignature.c_str());
                ImGui::TableNextColumn();
                UI::ScopedColor color(ImGuiCol_Text, ImVec4(0.15f, 0.72f, 0.95f, 1.0f));
                if (ImGui::Selectable(sourceLabels[i].c_str()))
                    CodeEditorManager::Get().OpenFile(call.SourceFilePath, call.Line);
                if (ImGui::IsItemHovered())
                {
                    ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
                    ImGui::SetTooltip("Open in code editor");
                }
                ImGui::PopID();
            }
            ImGui::EndTable();
        }
    }

    bool ConsolePanel::MatchesSearch(const ConsoleBuffer::Message& message) const { return m_SearchQuery.Matches(message); }

    void ConsolePanel::RefreshMessages()
    {
        ConsoleBuffer& console = ConsoleBuffer::Get();
        if (m_MessageRevision == console.GetRevision())
            return;

        m_MessageRevision = console.CopyBuffer(m_MessageSnapshot);
        m_FilterDirty = true;

        if (m_SelectedMessageId == 0)
            return;

        auto selected = std::find_if(m_MessageSnapshot.begin(), m_MessageSnapshot.end(),
                                     [this](const ConsoleBuffer::Message& message) { return message.Sequence == m_SelectedMessageId; });
        if (selected == m_MessageSnapshot.end() && m_SelectedMessage.GroupSequence != 0)
        {
            selected = std::find_if(m_MessageSnapshot.begin(), m_MessageSnapshot.end(), [this](const ConsoleBuffer::Message& message) {
                return message.GroupSequence == m_SelectedMessage.GroupSequence;
            });
        }
        if (selected != m_MessageSnapshot.end())
            SetSelectedMessage(&*selected);
        else
            SetSelectedMessage(nullptr);
    }

    void ConsolePanel::SetSelectedMessage(const ConsoleBuffer::Message* message)
    {
        if (message != nullptr)
        {
            m_SelectedMessageId = message->Sequence;
            m_SelectedMessage = *message;
            m_ViewModel.UpdateSelection(&m_SelectedMessage);
            return;
        }

        m_SelectedMessageId = 0u;
        m_SelectedMessage = {};
        m_ViewModel.UpdateSelection(nullptr);
    }

    void ConsolePanel::RebuildSearchTerms()
    {
        m_SearchQuery.Set(m_SearchString);
        m_FilterDirty = true;
    }

    void ConsolePanel::RebuildFilteredIndices()
    {
        if (!m_FilterDirty)
            return;

        m_MessageIndices.clear();
        m_MessageIndices.reserve(m_MessageSnapshot.size());
        for (uint32_t index = 0; index < static_cast<uint32_t>(m_MessageSnapshot.size()); index++)
        {
            const ConsoleBuffer::Message& message = m_MessageSnapshot[index];
            if (m_EnabledLevels[static_cast<uint8_t>(message.LogLevel)] && MatchesSearch(message))
                m_MessageIndices.push_back(index);
        }
        m_ViewModel.UpdateMessages(m_MessageSnapshot, m_MessageIndices, m_Collapse);
        m_FilterDirty = false;
    }

    void ConsolePanel::SetMessageLevelEnabled(ConsoleBuffer::Message::Level level, bool enabled)
    {
        m_EnabledLevels[static_cast<uint32_t>(level)] = enabled;
        m_FilterDirty = true;
    }

    void ConsolePanel::SetCollapseEnabled(bool collapse)
    {
        m_Collapse = collapse;
        if (!ConsoleBuffer::IsStartedUp())
            return;
        if (collapse)
            ConsoleBuffer::Get().Collapse();
        else
            ConsoleBuffer::Get().Uncollapse();
        m_MessageRevision = std::numeric_limits<uint64_t>::max();
        m_FilterDirty = true;
    }

    void ConsolePanel::CopySelectedMessage() const
    {
        String clipboardText = m_SelectedMessage.MessageText;
        for (const ConsoleBuffer::Message::FunctionCall& call : m_SelectedMessage.Callstack)
        {
            clipboardText += "\n  " + call.FunctionSignature + " (at " + call.SourceFilePath.string() + ":" + std::to_string(call.Line) + ")";
        }
        ImGui::SetClipboardText(clipboardText.c_str());
    }
} // namespace Crowny
