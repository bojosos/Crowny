#include "cwpch.h"

#include "Crowny/ImGui/ImGuiConsoleBuffer.h"
#include "Crowny/Common/Time.h"

namespace Crowny
{

    void ImGuiConsoleBuffer::AddMessage(const Message& message)
    {
        // if (m_Collapsed)
        // {
            auto findIter = m_HashToIndex.find(message.Hash);
            if (findIter != m_HashToIndex.end())
                m_CollapsedMessageBuffer[findIter->second].RepeatCount++;
            else
            {
                m_CollapsedMessageBuffer.push_back(message);
                m_HashToIndex[message.Hash] = m_CollapsedMessageBuffer.size() - 1;
            }
        // }
        // else
            m_NormalMessageBuffer.push_back(message);
        m_HasNewMessages = true;
    }

    void ImGuiConsoleBuffer::Clear() { m_NormalMessageBuffer.clear(); m_HashToIndex.clear(); m_CollapsedMessageBuffer.clear(); }

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

    void ImGuiConsoleBuffer::Collapse()
    {
        m_Collapsed = true;
    }

    void ImGuiConsoleBuffer::Uncollapse()
    { 
        m_Collapsed = false;
    }

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