#include "cwpch.h"

#include "Crowny/Common/Time.h"
#include "Crowny/ImGui/ImGuiConsoleBuffer.h"

namespace Crowny
{

    void ImGuiConsoleBuffer::AddMessage(const Message& message) // Binary search here
    {
        // if (m_Collapsed)
        // {
        auto findIter = m_HashToIndex.find(message.Hash);
        if (findIter != m_HashToIndex.end())
            m_CollapsedMessageBuffer[findIter->second].RepeatCount++;
        else
        {
            m_CollapsedMessageBuffer.push_back(message);
            m_HashToIndex[message.Hash] = (uint32_t)m_CollapsedMessageBuffer.size() - 1;
        }
        // }
        // else
        m_NormalMessageBuffer.push_back(message);
        m_HasNewMessages = true;
    }

    void ImGuiConsoleBuffer::Clear()
    {
        m_NormalMessageBuffer.clear();
        m_HashToIndex.clear();
        m_CollapsedMessageBuffer.clear();
    }

    void ImGuiConsoleBuffer::Sort(uint32_t sortIdx, bool ascending)
    {
        if (m_Collapsed)
        {
            std::sort(m_CollapsedMessageBuffer.begin(), m_CollapsedMessageBuffer.end(),
                      [ascending, sortIdx](const ImGuiConsoleBuffer::Message& a, const ImGuiConsoleBuffer::Message& b) {
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
                      [ascending, sortIdx](const ImGuiConsoleBuffer::Message& a, const ImGuiConsoleBuffer::Message& b) {
                          if (sortIdx == 0)
                              return ascending ? a.Timestamp < b.Timestamp : a.Timestamp > b.Timestamp;
                          else if (sortIdx == 1)
                              return ascending ? a.MessageText < b.MessageText : a.MessageText > b.MessageText;
                          return false;
                      });
        }
    }

    ImGuiConsoleBuffer::Message::Message(const String& message, Level level) : MessageText(message), LogLevel(level)
    {
        Hash = Crowny::Hash(message);
        Timestamp = std::time(nullptr);
    }

    const Vector<ImGuiConsoleBuffer::Message>& ImGuiConsoleBuffer::GetBuffer()
    {
        m_HasNewMessages = false;
        if (m_Collapsed)
            return m_CollapsedMessageBuffer;
        return m_NormalMessageBuffer;
    }

    void ImGuiConsoleBuffer::Collapse() { m_Collapsed = true; }

    void ImGuiConsoleBuffer::Uncollapse() { m_Collapsed = false; }

    const char* ImGuiConsoleBuffer::Message::GetLevelName(Level level)
    {
        switch (level)
        {
        case ImGuiConsoleBuffer::Message::Level::Critical:
            return "Critical";
        case ImGuiConsoleBuffer::Message::Level::Error:
            return "Error";
        case ImGuiConsoleBuffer::Message::Level::Warn:
            return "Warn";
        case ImGuiConsoleBuffer::Message::Level::Info:
            return "Info";
        }

        return "Unknown";
    }
} // namespace Crowny