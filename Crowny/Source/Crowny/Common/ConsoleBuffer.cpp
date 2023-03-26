#include "cwpch.h"

#include "Crowny/Common/Time.h"
#include "Crowny/Common/ConsoleBuffer.h"

namespace Crowny
{

    void ConsoleBuffer::AddMessage(const Message& message) // Binary search here
    {
        auto findIter = m_HashToIndex.find(message.Hash);
        if (findIter != m_HashToIndex.end())
            m_CollapsedMessageBuffer[findIter->second].RepeatCount++;
        else
        {
            m_CollapsedMessageBuffer.push_back(message);
            m_HashToIndex[message.Hash] = (uint32_t)m_CollapsedMessageBuffer.size() - 1;
        }
        m_NormalMessageBuffer.push_back(message);
        m_HasNewMessages = true;
    }

    void ConsoleBuffer::Clear()
    {
        m_NormalMessageBuffer.clear();
        m_HashToIndex.clear();
        m_CollapsedMessageBuffer.clear();
    }

    void ConsoleBuffer::Sort(uint32_t sortIdx, bool ascending)
    {
        if (m_Collapsed)
        {
            std::sort(m_CollapsedMessageBuffer.begin(), m_CollapsedMessageBuffer.end(),
                      [ascending, sortIdx](const ConsoleBuffer::Message& a, const ConsoleBuffer::Message& b) {
                          if (sortIdx == 1)
                              return ascending ? a.MessageText < b.MessageText : a.MessageText > b.MessageText;
                          else if (sortIdx == 0)
                              return ascending ? a.RepeatCount < b.RepeatCount : a.RepeatCount > b.RepeatCount;
                          return false;
                      });
        }
        else
        {
            std::sort(m_NormalMessageBuffer.begin(), m_NormalMessageBuffer.end(),
                      [ascending, sortIdx](const ConsoleBuffer::Message& a, const ConsoleBuffer::Message& b) {
                          if (sortIdx == 0)
                              return ascending ? a.Timestamp < b.Timestamp : a.Timestamp > b.Timestamp;
                          else if (sortIdx == 1)
                              return ascending ? a.MessageText < b.MessageText : a.MessageText > b.MessageText;
                          return false;
                      });
        }
    }

    ConsoleBuffer::Message::Message(const String& message, Level level) : MessageText(message), LogLevel(level)
    {
        Hash = Crowny::Hash(message);
        Timestamp = std::time(nullptr);
    }

    const Vector<ConsoleBuffer::Message>& ConsoleBuffer::GetBuffer()
    {
        m_HasNewMessages = false;
        if (m_Collapsed)
            return m_CollapsedMessageBuffer;
        return m_NormalMessageBuffer;
    }

    void ConsoleBuffer::Collapse() { m_Collapsed = true; }

    void ConsoleBuffer::Uncollapse() { m_Collapsed = false; }

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