#include "cwpch.h"

#include "Crowny/Common/ConsoleBuffer.h"
#include "Crowny/Common/Time.h"

namespace Crowny
{

    void ConsoleBuffer::AddMessage(Message::Level logLevel, const String& messageText,
                                   const Vector<Message::FunctionCall>& callstack) // Binary search here
    {
        Message message;
        message.MessageText = messageText;
        message.Timestamp = std::time(nullptr);
        message.Callstack = callstack;
        size_t hash = Hash(message.MessageText);
        HashCombine(hash, message.Timestamp, (int32_t)message.LogLevel);
        for (const Message::FunctionCall& call : message.Callstack)
            HashCombine(hash, call.FunctionSignature, call.Line, call.SourceFilePath);
        message.Hash = hash;
        message.LogLevel = logLevel;

        auto findIter = m_HashToIndex.find(message.Hash);
        if (findIter != m_HashToIndex.end())
            m_CollapsedMessageBuffer[findIter->second].RepeatCount++;
        else
        {
            m_CollapsedMessageBuffer.push_back(message);
            m_HashToIndex[message.Hash] = (uint32_t)m_CollapsedMessageBuffer.size() - 1;
        }
        m_NormalMessageBuffer.push_back(std::move(message));
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
                              return ascending ? (a.Timestamp < b.Timestamp) : (a.Timestamp > b.Timestamp);
                          else if (sortIdx == 1)
                              return ascending ? (a.MessageText < b.MessageText) : (a.MessageText > b.MessageText);
                          return false;
                      });
        }
    }

    ConsoleBuffer::Message::Message(const String& message, Level level)
      : MessageText(message), LogLevel(level), Hash(0), Timestamp(0)
    {
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