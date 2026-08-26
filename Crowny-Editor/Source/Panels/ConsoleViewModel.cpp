#include "cwepch.h"

#include "Panels/ConsoleViewModel.h"

#include <cstdio>

namespace Crowny
{
    void ConsoleViewModel::UpdateMessages(const Vector<ConsoleBuffer::Message>& messages, const Vector<uint32_t>& visibleIndices, bool collapsed)
    {
        Array<uint32_t, ConsoleBuffer::Message::Levels.size()> counts{};
        for (const ConsoleBuffer::Message& message : messages)
            counts[static_cast<uint8_t>(message.LogLevel)] += collapsed ? message.RepeatCount : 1u;

        uint32_t shownCount = 0;
        for (const uint32_t messageIndex : visibleIndices)
            shownCount += collapsed ? messages[messageIndex].RepeatCount : 1u;

        for (size_t index = 0; index < ConsoleBuffer::Message::Levels.size(); index++)
        {
            LevelSummary& summary = m_Summary.Levels[index];
            summary.Count = counts[index];
            std::snprintf(summary.Label.data(), summary.Label.size(), "%s  %u",
                          ConsoleBuffer::Message::GetLevelName(ConsoleBuffer::Message::Levels[index]), summary.Count);
        }
        m_Summary.ShownCount = shownCount;
    }

    void ConsoleViewModel::UpdateSelection(const ConsoleBuffer::Message* message)
    {
        m_CallstackSourceLabels.clear();
        if (message == nullptr)
            return;

        m_CallstackSourceLabels.reserve(message->Callstack.size());
        for (const ConsoleBuffer::Message::FunctionCall& call : message->Callstack)
            m_CallstackSourceLabels.push_back(call.SourceFilePath.string() + ":" + std::to_string(call.Line));
    }
} // namespace Crowny
