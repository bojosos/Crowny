#include "cwpch.h"

#include "Crowny/ImGui/ImGuiConsoleSink.h"
#include "Log.h"

#include <spdlog/common-inl.h>
#include <spdlog/sinks/basic_file_sink.h>
#include <spdlog/sinks/stdout_color_sinks.h>

namespace Crowny
{

    Ref<spdlog::logger> Log::s_EngineLogger = nullptr;
    Ref<spdlog::logger> Log::s_ClientLogger = nullptr;
    Vector<spdlog::sink_ptr> Log::s_LogSinks = {};

    void Log::Init(const String& clientLoggerName)
    {
        s_LogSinks.emplace_back(CreateRef<spdlog::sinks::stdout_color_sink_mt>());
        s_LogSinks.emplace_back(CreateRef<spdlog::sinks::basic_file_sink_mt>(clientLoggerName + ".log", true));
        s_LogSinks.emplace_back(CreateRef<ImGuiConsoleSink_mt>(true));

        s_LogSinks[0]->set_pattern("%^[%T] %n: %v%$");
        s_LogSinks[1]->set_pattern("[%T] [%l] %n: %v");
        s_LogSinks[2]->set_pattern("%v");

        s_EngineLogger = CreateRef<spdlog::logger>("CROWNY", std::begin(s_LogSinks), std::end(s_LogSinks));
        spdlog::register_logger(s_EngineLogger);
        s_EngineLogger->set_level(spdlog::level::trace);
        s_EngineLogger->flush_on(spdlog::level::trace);

        s_ClientLogger = CreateRef<spdlog::logger>(clientLoggerName, std::cbegin(s_LogSinks), std::cend(s_LogSinks));
        spdlog::register_logger(s_ClientLogger);
        s_ClientLogger->set_level(spdlog::level::trace);
        s_ClientLogger->flush_on(spdlog::level::trace);
    }

    void Log::RenameClientLogger(const StringView loggerName)
    {
        CW_ENGINE_ASSERT(s_ClientLogger != nullptr);
        s_ClientLogger = CreateRef<spdlog::logger>("CLIENT", std::cbegin(s_LogSinks), std::cend(s_LogSinks));
        spdlog::drop(s_ClientLogger->name());
        spdlog::register_logger(s_ClientLogger);
        s_ClientLogger->set_level(spdlog::level::trace);
        s_ClientLogger->flush_on(spdlog::level::trace);
    }

} // namespace Crowny