#pragma once

#include "Crowny/ImGui/ImGuiConsoleBuffer.h"

#include <spdlog/sinks/base_sink.h>

namespace Crowny
{
    template <class Mutex> class ImGuiConsoleSink : public spdlog::sinks::base_sink<Mutex>
    {
    public:
        explicit ImGuiConsoleSink(bool forceFlush = false, uint8_t capacity = 10)
          : m_MessageBufferCapacity(forceFlush ? 1 : capacity), m_MessageBuffer(m_MessageBufferCapacity)
        {
        }

        ImGuiConsoleSink(const ImGuiConsoleSink&) = delete;
        ImGuiConsoleSink& operator=(const ImGuiConsoleSink&) = delete;
        virtual ~ImGuiConsoleSink() = default;

    protected:
        void sink_it_(const spdlog::details::log_msg& message) override
        {
            spdlog::memory_buf_t formatted;
            base_sink<Mutex>::formatter_->format(message, formatted);
            *(m_MessageBuffer.begin() + m_MessagesBuffered) =
              ImGuiConsoleBuffer::Message(fmt::to_string(formatted), GetMessageLevel(message.level));
            if (++m_MessagesBuffered == m_MessageBufferCapacity)
                flush_();
        }

        void flush_() override
        {
            for (int i = 0; i < m_MessagesBuffered; i++)
                ImGuiConsoleBuffer::Get().AddMessage(m_MessageBuffer[i]);
            m_MessagesBuffered = 0;
        }

    private:
        static ImGuiConsoleBuffer::Message::Level GetMessageLevel(const spdlog::level::level_enum level)
        {
            switch (level)
            {
            case spdlog::level::level_enum::info:
                return ImGuiConsoleBuffer::Message::Level::Info;
            case spdlog::level::level_enum::warn:
                return ImGuiConsoleBuffer::Message::Level::Warn;
            case spdlog::level::level_enum::err:
                return ImGuiConsoleBuffer::Message::Level::Error;
            case spdlog::level::level_enum::critical:
                return ImGuiConsoleBuffer::Message::Level::Critical;
            default:
                return ImGuiConsoleBuffer::Message::Level::Info; // trace info and off
            }

            return ImGuiConsoleBuffer::Message::Level::Info;
        }

    private:
        uint8_t m_MessagesBuffered = 0;
        uint8_t m_MessageBufferCapacity;
        Vector<ImGuiConsoleBuffer::Message> m_MessageBuffer;
    };
} // namespace Crowny

#include <mutex>
#include <spdlog/details/null_mutex.h>

namespace Crowny
{
    using ImGuiConsoleSink_mt = ImGuiConsoleSink<std::mutex>;
    using ImGuiConsoleSink_st = ImGuiConsoleSink<spdlog::details::null_mutex>;
} // namespace Crowny
