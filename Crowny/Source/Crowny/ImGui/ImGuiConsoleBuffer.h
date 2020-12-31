#pragma once

namespace Crowny
{
    class ImGuiConsoleBuffer
    {
    public:
        class Message
        {
        public:
            enum class Level : int8_t
            {
                Invalid = -1,
                Info = 0,
                Warn = 1,
                Error = 2,
                Critical = 3,
            };
        public:
            Message(const std::string& message = "", Level level = Level::Invalid);
            static const char* GetLevelName(Level level);
        public:
            const std::string& GetMessage() const { return m_Message; }
            Level GetLevel() const { return m_Level; }
        private:
            std::string m_Message;
            Level m_Level;
        public:
            static std::vector<Level> s_Levels;
        };

    public:
        ~ImGuiConsoleBuffer() = default;
        static void AddMessage(const Ref<Message>& message);
        static void Clear();
        static std::vector<Ref<Message>> GetBuffer() { s_HasNewMessages = false; return s_MessageBuffer; }
        static bool HasNewMessages() { return s_HasNewMessages; }
    protected:
        ImGuiConsoleBuffer() = default;
    private:
        static bool s_HasNewMessages;
        static std::vector<Ref<Message>> s_MessageBuffer;
    };
}