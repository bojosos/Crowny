#pragma once

#include "Crowny/Common/Module.h"

namespace Crowny
{
    class ImGuiConsoleBuffer : public Module<ImGuiConsoleBuffer>
    {
    public:
        struct Message
        {
        public:
            enum class Level : uint8_t
            {
                Info = 0,
                Warn = 1,
                Error = 2,
                Critical = 3,
            };

        public:
            Message() = default;
            Message(const String& message, Level level);

            Vector<Level> s_Levels;

            static const char* GetLevelName(Level level);
            static constexpr Array<Level, 4> Levels = { Level::Info, Level::Warn, Level::Error, Level::Critical };

        public:
            String MessageText;
            Level LogLevel;
            size_t Hash; // for collapse
            std::time_t Timestamp;
            String Source;
            uint32_t RepeatCount = 1;
        };

    public:
        ImGuiConsoleBuffer() = default;
        ~ImGuiConsoleBuffer() = default;
        void AddMessage(const Message& message);

        void Sort(uint32_t sortIdx, bool ascending);
        void Clear();
        const Vector<Message>& GetBuffer();
        void Collapse();
        void Uncollapse();
        bool HasNewMessages() { return m_HasNewMessages; }

    private:
        bool m_HasNewMessages = false;
        bool m_Collapsed = false;

        Vector<Message> m_NormalMessageBuffer;
        Vector<Message> m_CollapsedMessageBuffer;
        UnorderedMap<size_t, uint32_t> m_HashToIndex;
    };
} // namespace Crowny