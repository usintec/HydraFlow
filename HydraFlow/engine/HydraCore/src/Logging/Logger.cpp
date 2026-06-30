#include <HydraCore/Logging/Logger.h>

#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/sinks/basic_file_sink.h>
#include <spdlog/spdlog.h>

#include <vector>
#include <stdexcept>

namespace Hydra {

SharedPtr<spdlog::logger> Logger::s_Logger;

static spdlog::level::level_enum ToSpdlogLevel(LogLevel level) noexcept
{
    switch (level) {
        case LogLevel::Trace:    return spdlog::level::trace;
        case LogLevel::Debug:    return spdlog::level::debug;
        case LogLevel::Info:     return spdlog::level::info;
        case LogLevel::Warn:     return spdlog::level::warn;
        case LogLevel::Error:    return spdlog::level::err;
        case LogLevel::Critical: return spdlog::level::critical;
        case LogLevel::Off:      return spdlog::level::off;
        default:                 return spdlog::level::info;
    }
}

void Logger::Initialize(const LoggerConfig& config)
{
    if (s_Logger)
        return; // Already initialized

    std::vector<spdlog::sink_ptr> sinks;

    if (config.enableConsole) {
        auto consoleSink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
        consoleSink->set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%n] [%^%l%$] %v");
        sinks.push_back(std::move(consoleSink));
    }

    if (config.enableFile && !config.logFilePath.empty()) {
        try {
            auto fileSink = std::make_shared<spdlog::sinks::basic_file_sink_mt>(
                config.logFilePath, /*truncate=*/true);
            fileSink->set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%n] [%l] %v");
            sinks.push_back(std::move(fileSink));
        } catch (const spdlog::spdlog_ex& ex) {
            // Can't log yet — print to stderr directly
            fprintf(stderr, "[HydraCore] Logger: failed to open log file '%s': %s\n",
                    config.logFilePath.c_str(), ex.what());
        }
    }

    s_Logger = std::make_shared<spdlog::logger>(
        std::string(config.loggerName), sinks.begin(), sinks.end());

    s_Logger->set_level(ToSpdlogLevel(config.level));
    s_Logger->flush_on(spdlog::level::warn);

    spdlog::register_logger(s_Logger);

    s_Logger->info("Logger initialized (level={})", spdlog::level::to_string_view(ToSpdlogLevel(config.level)));
}

void Logger::Shutdown()
{
    if (s_Logger) {
        s_Logger->info("Logger shutting down");
        s_Logger->flush();
        spdlog::drop(s_Logger->name());
        s_Logger.reset();
    }
}

bool Logger::IsInitialized() noexcept
{
    return s_Logger != nullptr;
}

void Logger::SetLevel(LogLevel level) noexcept
{
    if (s_Logger)
        s_Logger->set_level(ToSpdlogLevel(level));
}

LogLevel Logger::GetLevel() noexcept
{
    if (!s_Logger)
        return LogLevel::Off;

    switch (s_Logger->level()) {
        case spdlog::level::trace:    return LogLevel::Trace;
        case spdlog::level::debug:    return LogLevel::Debug;
        case spdlog::level::info:     return LogLevel::Info;
        case spdlog::level::warn:     return LogLevel::Warn;
        case spdlog::level::err:      return LogLevel::Error;
        case spdlog::level::critical: return LogLevel::Critical;
        default:                      return LogLevel::Off;
    }
}

} // namespace Hydra
