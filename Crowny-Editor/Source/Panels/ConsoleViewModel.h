#pragma once

#include "Crowny/Common/ConsoleBuffer.h"

namespace Crowny
{
    class ConsoleViewModel
    {
    public:
        static constexpr size_t HeaderLabelCapacity = 32u;

        struct LevelSummary
        {
            Array<char, HeaderLabelCapacity> Label{};
            uint32_t Count = 0;
        };

        struct Summary
        {
            Array<LevelSummary, ConsoleBuffer::Message::Levels.size()> Levels{};
            uint32_t ShownCount = 0;
        };

        void UpdateMessages(const Vector<ConsoleBuffer::Message>& messages, const Vector<uint32_t>& visibleIndices, bool collapsed);
        void UpdateSelection(const ConsoleBuffer::Message* message);

        const Summary& GetSummary() const { return m_Summary; }
        const Vector<String>& GetCallstackSourceLabels() const { return m_CallstackSourceLabels; }

    private:
        Summary m_Summary;
        Vector<String> m_CallstackSourceLabels;
        uint64_t m_SelectedSequence = 0u;
        bool m_HasSelection = false;
    };
} // namespace Crowny
