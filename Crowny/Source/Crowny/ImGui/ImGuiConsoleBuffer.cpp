#include "cwpch.h"

#include "Crowny/ImGui/ImGuiConsoleBuffer.h"

namespace Crowny
{
    Vector<Ref<ImGuiConsoleBuffer::Message>> ImGuiConsoleBuffer::s_MessageBuffer;
    bool ImGuiConsoleBuffer::s_HasNewMessages = true;

    void ImGuiConsoleBuffer::AddMessage(const Ref<Message>& message)
    {
        if (message->GetLevel() == Message::Level::Invalid)
            return;
        s_HasNewMessages = true;
        s_MessageBuffer.push_back(message);
    }

    void ImGuiConsoleBuffer::Clear() { s_MessageBuffer.clear(); }

    Vector<ImGuiConsoleBuffer::Message::Level> ImGuiConsoleBuffer::Message::s_Levels{
        ImGuiConsoleBuffer::Message::Level::Info, ImGuiConsoleBuffer::Message::Level::Warn,
        ImGuiConsoleBuffer::Message::Level::Error, ImGuiConsoleBuffer::Message::Level::Critical
    };

    ImGuiConsoleBuffer::Message::Message(const String& message, Level level) : m_Message(message), m_Level(level) {}

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