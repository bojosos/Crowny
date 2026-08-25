#include "cwpch.h"

#include "Crowny/Common/ConsoleBuffer.h"
#include "Crowny/Common/Time.h"

#include <cctype>

namespace Crowny
{
    namespace
    {
        char FoldSearchCharacter(unsigned char character) { return static_cast<char>(std::tolower(character)); }

        bool EqualsSearchToken(StringView value, StringView expected)
        {
            return value.size() == expected.size() &&
                   std::equal(value.begin(), value.end(), expected.begin(),
                              [](unsigned char lhs, unsigned char rhs) {
                                  return FoldSearchCharacter(lhs) == FoldSearchCharacter(rhs);
                              });
        }

        void AppendSearchText(String& output, const String& value)
        {
            if (!output.empty())
                output.push_back(' ');

            output.reserve(output.size() + value.size());
            for (const unsigned char character : value)
                output.push_back(FoldSearchCharacter(character));
        }

        bool IsSameMessage(const ConsoleBuffer::Message& lhs, const ConsoleBuffer::Message& rhs)
        {
            if (lhs.LogLevel != rhs.LogLevel || lhs.MessageText != rhs.MessageText || lhs.Callstack.size() != rhs.Callstack.size())
                return false;

            for (size_t i = 0; i < lhs.Callstack.size(); ++i)
            {
                const ConsoleBuffer::Message::FunctionCall& lhsCall = lhs.Callstack[i];
                const ConsoleBuffer::Message::FunctionCall& rhsCall = rhs.Callstack[i];
                if (lhsCall.FunctionSignature != rhsCall.FunctionSignature || lhsCall.Line != rhsCall.Line ||
                    lhsCall.SourceFilePath != rhsCall.SourceFilePath)
                    return false;
            }
            return true;
        }
    } // namespace

    void ConsoleBuffer::SearchQuery::Set(StringView query)
    {
        m_Terms.clear();

        size_t cursor = 0;
        while (cursor < query.size())
        {
            while (cursor < query.size() && std::isspace(static_cast<unsigned char>(query[cursor])))
                cursor++;
            if (cursor == query.size())
                break;

            bool exclude = false;
            if (query[cursor] == '-' && cursor + 1 < query.size() &&
                !std::isspace(static_cast<unsigned char>(query[cursor + 1])))
            {
                exclude = true;
                cursor++;
            }

            const size_t tokenStart = cursor;
            Field field = Field::Any;
            size_t prefixEnd = cursor;
            while (prefixEnd < query.size() && query[prefixEnd] != ':' && query[prefixEnd] != '"' &&
                   !std::isspace(static_cast<unsigned char>(query[prefixEnd])))
            {
                prefixEnd++;
            }

            bool recognizedPrefix = false;
            if (prefixEnd < query.size() && query[prefixEnd] == ':')
            {
                const StringView prefix = query.substr(tokenStart, prefixEnd - tokenStart);
                if (EqualsSearchToken(prefix, "text"))
                    field = Field::Text;
                else if (EqualsSearchToken(prefix, "source"))
                    field = Field::Source;
                else if (EqualsSearchToken(prefix, "level"))
                    field = Field::Level;
                else if (EqualsSearchToken(prefix, "time"))
                    field = Field::Time;
                else
                    field = Field::Any;

                recognizedPrefix = field != Field::Any;
                if (recognizedPrefix)
                    cursor = prefixEnd + 1;
            }
            if (!recognizedPrefix)
                cursor = tokenStart;

            String value;
            bool quoted = false;
            while (cursor < query.size())
            {
                const unsigned char character = static_cast<unsigned char>(query[cursor]);
                if (character == '\\' && cursor + 1 < query.size() &&
                    (query[cursor + 1] == '\\' || query[cursor + 1] == '"'))
                {
                    value.push_back(FoldSearchCharacter(static_cast<unsigned char>(query[cursor + 1])));
                    cursor += 2;
                    continue;
                }
                if (character == '"')
                {
                    quoted = !quoted;
                    cursor++;
                    continue;
                }
                if (!quoted && std::isspace(character))
                    break;

                value.push_back(FoldSearchCharacter(character));
                cursor++;
            }

            if (!value.empty())
                m_Terms.push_back({ field, std::move(value), exclude });
        }
    }

    bool ConsoleBuffer::SearchQuery::Matches(const Message& message) const
    {
        const StringView level = [&message]() -> StringView {
            switch (message.LogLevel)
            {
            case Message::Level::Info: return "info";
            case Message::Level::Warn: return "warn warning";
            case Message::Level::Error: return "error";
            case Message::Level::Critical: return "critical";
            }
            return {};
        }();

        for (const Term& term : m_Terms)
        {
            bool matches = false;
            switch (term.SearchField)
            {
            case Field::Text: matches = message.SearchText.find(term.Value) != String::npos; break;
            case Field::Source: matches = message.SourceSearchText.find(term.Value) != String::npos; break;
            case Field::Level: matches = level.find(term.Value) != StringView::npos; break;
            case Field::Time: matches = message.TimestampText.find(term.Value) != String::npos; break;
            case Field::Any:
                matches = message.SearchText.find(term.Value) != String::npos ||
                          message.SourceSearchText.find(term.Value) != String::npos ||
                          message.TimestampText.find(term.Value) != String::npos || level.find(term.Value) != StringView::npos;
                break;
            }

            if (matches == term.Exclude)
                return false;
        }
        return true;
    }

    void ConsoleBuffer::AddMessage(Message::Level logLevel, const String& messageText, const Vector<Message::FunctionCall>& callstack)
    {
        Message message;
        message.MessageText = messageText;
        message.Timestamp = std::time(nullptr);
        message.Callstack = callstack;
        message.LogLevel = logLevel;

        AppendSearchText(message.SearchText, message.MessageText);
        for (const Message::FunctionCall& call : message.Callstack)
        {
            AppendSearchText(message.SourceSearchText, call.FunctionSignature);
            AppendSearchText(message.SourceSearchText, call.SourceFilePath.string());
            AppendSearchText(message.SourceSearchText, std::to_string(call.Line));
        }

        size_t hash = Hash(message.MessageText);
        HashCombine(hash, static_cast<int32_t>(message.LogLevel));
        for (const Message::FunctionCall& call : message.Callstack)
            HashCombine(hash, call.FunctionSignature, call.Line, call.SourceFilePath);
        message.Hash = hash;

        ScopedLock lock(m_Mutex);
        message.Sequence = m_NextSequence++;
        message.GroupSequence = message.Sequence;
        if (m_CachedTimestamp != message.Timestamp)
        {
            char formattedTime[9] = {};
            tm timeInfo = {};
#ifdef CW_PLATFORM_WIN32
            localtime_s(&timeInfo, &message.Timestamp);
#else
            localtime_r(&message.Timestamp, &timeInfo);
#endif
            strftime(formattedTime, sizeof(formattedTime), "%T", &timeInfo);
            m_CachedTimestamp = message.Timestamp;
            m_CachedTimestampText = formattedTime;
        }
        message.TimestampText = m_CachedTimestampText;

        Vector<uint32_t>& candidates = m_HashToIndices[message.Hash];
        const auto matchingCandidate = std::find_if(candidates.begin(), candidates.end(), [&](uint32_t index) {
            return IsSameMessage(m_CollapsedMessageBuffer[index], message);
        });
        if (matchingCandidate != candidates.end())
        {
            Message& collapsedMessage = m_CollapsedMessageBuffer[*matchingCandidate];
            message.GroupSequence = collapsedMessage.GroupSequence;
            collapsedMessage.RepeatCount++;
            collapsedMessage.Timestamp = message.Timestamp;
            collapsedMessage.TimestampText = message.TimestampText;
            collapsedMessage.SearchText = message.SearchText;
            collapsedMessage.SourceSearchText = message.SourceSearchText;
        }
        else
        {
            m_CollapsedMessageBuffer.push_back(message);
            candidates.push_back(static_cast<uint32_t>(m_CollapsedMessageBuffer.size() - 1));
        }
        m_NormalMessageBuffer.push_back(std::move(message));
        m_SortDirty = m_HasSort;
        m_HasNewMessages.store(true, std::memory_order_release);
        m_Revision.fetch_add(1, std::memory_order_release);
    }

    void ConsoleBuffer::Clear()
    {
        ScopedLock lock(m_Mutex);
        m_NormalMessageBuffer.clear();
        m_HashToIndices.clear();
        m_CollapsedMessageBuffer.clear();
        m_SortDirty = false;
        m_HasNewMessages.store(false, std::memory_order_release);
        m_Revision.fetch_add(1, std::memory_order_release);
    }

    void ConsoleBuffer::Sort(uint32_t sortIdx, bool ascending)
    {
        ScopedLock lock(m_Mutex);
        m_SortIndex = sortIdx;
        m_SortAscending = ascending;
        m_HasSort = true;
        m_SortDirty = true;
        ApplySort();
        m_Revision.fetch_add(1, std::memory_order_release);
    }

    void ConsoleBuffer::ApplySort()
    {
        if (!m_HasSort || !m_SortDirty)
            return;

        const uint32_t sortIdx = m_SortIndex;
        const bool ascending = m_SortAscending;
        if (m_Collapsed)
        {
            std::stable_sort(m_CollapsedMessageBuffer.begin(), m_CollapsedMessageBuffer.end(),
                             [ascending, sortIdx](const Message& lhs, const Message& rhs) {
                                 if (sortIdx == 1 && lhs.MessageText != rhs.MessageText)
                                     return ascending ? lhs.MessageText < rhs.MessageText : lhs.MessageText > rhs.MessageText;
                                 if (sortIdx == 0 && lhs.RepeatCount != rhs.RepeatCount)
                                     return ascending ? lhs.RepeatCount < rhs.RepeatCount : lhs.RepeatCount > rhs.RepeatCount;
                                 return ascending ? lhs.Sequence < rhs.Sequence : lhs.Sequence > rhs.Sequence;
                             });
            RebuildCollapsedIndices();
        }
        else
        {
            std::stable_sort(m_NormalMessageBuffer.begin(), m_NormalMessageBuffer.end(),
                             [ascending, sortIdx](const Message& lhs, const Message& rhs) {
                                 if (sortIdx == 0 && lhs.Timestamp != rhs.Timestamp)
                                     return ascending ? lhs.Timestamp < rhs.Timestamp : lhs.Timestamp > rhs.Timestamp;
                                 if (sortIdx == 1 && lhs.MessageText != rhs.MessageText)
                                     return ascending ? lhs.MessageText < rhs.MessageText : lhs.MessageText > rhs.MessageText;
                                 return ascending ? lhs.Sequence < rhs.Sequence : lhs.Sequence > rhs.Sequence;
                             });
        }
        m_SortDirty = false;
    }

    void ConsoleBuffer::RebuildCollapsedIndices()
    {
        m_HashToIndices.clear();
        for (uint32_t index = 0; index < static_cast<uint32_t>(m_CollapsedMessageBuffer.size()); index++)
            m_HashToIndices[m_CollapsedMessageBuffer[index].Hash].push_back(index);
    }

    ConsoleBuffer::Message::Message(const String& message, Level level) : MessageText(message), LogLevel(level), Hash(0), Timestamp(0) {}

    uint64_t ConsoleBuffer::CopyBuffer(Vector<Message>& output)
    {
        ScopedLock lock(m_Mutex);
        ApplySort();
        output = m_Collapsed ? m_CollapsedMessageBuffer : m_NormalMessageBuffer;
        m_HasNewMessages.store(false, std::memory_order_release);
        return m_Revision.load(std::memory_order_acquire);
    }

    void ConsoleBuffer::Collapse()
    {
        ScopedLock lock(m_Mutex);
        if (m_Collapsed)
            return;
        m_Collapsed = true;
        m_SortDirty = m_HasSort;
        m_Revision.fetch_add(1, std::memory_order_release);
    }

    void ConsoleBuffer::Uncollapse()
    {
        ScopedLock lock(m_Mutex);
        if (!m_Collapsed)
            return;
        m_Collapsed = false;
        m_SortDirty = m_HasSort;
        m_Revision.fetch_add(1, std::memory_order_release);
    }

    const char* ConsoleBuffer::Message::GetLevelName(Level level)
    {
        switch (level)
        {
        case ConsoleBuffer::Message::Level::Critical:
            return "Critical";
        case ConsoleBuffer::Message::Level::Error:
            return "Error";
        case ConsoleBuffer::Message::Level::Warn:
            return "Warn";
        case ConsoleBuffer::Message::Level::Info:
            return "Info";
        }

        return "Unknown";
    }
} // namespace Crowny
