#pragma once

#include "Crowny/Common/Module.h"

namespace Crowny
{
    class ConsoleBuffer : public Module<ConsoleBuffer>
    {
    public:
        static constexpr size_t DefaultMaxMessages = 10000u;

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

            static const char* GetLevelName(Level level);
            static constexpr Array<Level, 4> Levels = { Level::Info, Level::Warn, Level::Error, Level::Critical };

        public:
            String MessageText;
            String TimestampText;
            String SearchText;
            String SourceSearchText;
            Level LogLevel = Level::Info;
            size_t Hash = 0; // for collapse
            std::time_t Timestamp = 0;
            uint64_t Sequence = 0;      // Stable row identity; first occurrence for a collapsed group.
            uint64_t GroupSequence = 0; // Stable identity shared by normal rows in the same collapsed group.
            uint64_t LastSequence = 0;  // Most recent occurrence, used to order equally sorted collapsed groups.
            uint32_t RepeatCount = 1;

            struct FunctionCall
            {
                String FunctionSignature;
                Path SourceFilePath;
                uint32_t Line;
            };

            Vector<Message::FunctionCall> Callstack;
        };

        using CallstackBuffer = Vector<Message::FunctionCall>;

        class SearchQuery
        {
        public:
            void Set(StringView query);
            bool Matches(const Message& message) const;
            bool Empty() const { return m_Terms.empty(); }

        private:
            enum class Field : uint8_t
            {
                Any,
                Text,
                Source,
                Level,
                Time
            };

            struct Term
            {
                Field SearchField = Field::Any;
                String Value;
                bool Exclude = false;
            };

            Vector<Term> m_Terms;
        };

    public:
        explicit ConsoleBuffer(size_t maxMessages = DefaultMaxMessages);
        ~ConsoleBuffer() = default;
        void AddMessage(Message::Level logLevel, const String& messageText, const Vector<Message::FunctionCall>& callstack = {});

        void Sort(uint32_t sortIdx, bool ascending);
        void Clear();
        uint64_t CopyBuffer(Vector<Message>& output);
        /** Copies the current view only when revision is stale. Stable calls take the atomic fast path. */
        bool CopyBufferIfChanged(Vector<Message>& output, uint64_t& revision);
        uint64_t GetRevision() const { return m_Revision.load(std::memory_order_acquire); }
        uint64_t GetDroppedMessageCount() const { return m_DroppedMessageCount.load(std::memory_order_acquire); }
        void Collapse();
        void Uncollapse();
        bool HasNewMessages() const { return m_HasNewMessages.load(std::memory_order_acquire); }

    private:
        void ApplySort();
        void TrimToCapacity();
        void RebuildCollapsedBuffer();
        void RebuildCollapsedIndices();

        mutable Mutex m_Mutex;
        std::atomic<bool> m_HasNewMessages{ false };
        std::atomic<uint64_t> m_Revision{ 0 };
        std::atomic<uint64_t> m_DroppedMessageCount{ 0 };
        size_t m_MaxMessages = DefaultMaxMessages;
        bool m_Collapsed = false;
        bool m_HasSort = false;
        bool m_SortDirty = false;
        uint32_t m_SortIndex = 0;
        bool m_SortAscending = true;
        uint64_t m_NextSequence = 1;
        bool m_HasCachedTimestamp = false;
        std::time_t m_CachedTimestamp = 0;
        String m_CachedTimestampText;

        Vector<Message> m_NormalMessageBuffer;
        Vector<Message> m_CollapsedMessageBuffer;
        UnorderedMap<size_t, Vector<uint32_t>> m_HashToIndices;
    };
} // namespace Crowny
