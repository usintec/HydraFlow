#pragma once

#include <HydraCore/Common/Platform.h>
#include <HydraCore/Common/Types.h>
#include <HydraCore/Common/NonCopyable.h>

#include <spdlog/spdlog.h>
#include <spdlog/logger.h>

#include <memory>
#include <string>

namespace Hydra {

/// Log severity levels, mirrors spdlog::level for forward-compatibility.
enum class LogLevel : i32
{
    Trace    = 0,
    Debug    = 1,
    Info     = 2,
    Warn     = 3,
    Error    = 4,
    Critical = 5,
    Off      = 6
};

/// Configuration passed to Logger::Initialize.
struct LoggerConfig
{
    String  loggerName    = "Hydra";
    String  logFilePath   = "hydra.log";
    bool    enableConsole = true;
    bool    enableFile    = false;
    LogLevel level        = LogLevel::Debug;
};

/// ===========================================================================
/// Logger
///
/// Thin wrapper around spdlog that owns a named logger instance.
/// Call Logger::Initialize() early in startup before any log calls.
/// The class is not instantiable — use the static interface.
/// ===========================================================================
class HYDRA_API Logger final : private NonCopyableNonMovable
{
public:
    static void Initialize(const LoggerConfig& config = {});
    static void Shutdown();

    [[nodiscard]] static bool IsInitialized() noexcept;

    static void SetLevel(LogLevel level) noexcept;
    [[nodiscard]] static LogLevel GetLevel() noexcept;

    // --- Raw log methods (prefer macros in production code) -----------------
    template<typename... Args>
    static void Trace(spdlog::format_string_t<Args...> fmt, Args&&... args);

    template<typename... Args>
    static void Debug(spdlog::format_string_t<Args...> fmt, Args&&... args);

    template<typename... Args>
    static void Info(spdlog::format_string_t<Args...> fmt, Args&&... args);

    template<typename... Args>
    static void Warn(spdlog::format_string_t<Args...> fmt, Args&&... args);

    template<typename... Args>
    static void Error(spdlog::format_string_t<Args...> fmt, Args&&... args);

    template<typename... Args>
    static void Critical(spdlog::format_string_t<Args...> fmt, Args&&... args);

private:
    static SharedPtr<spdlog::logger> s_Logger;
};

// ===========================================================================
// Template implementations
// ===========================================================================

template<typename... Args>
void Logger::Trace(spdlog::format_string_t<Args...> fmt, Args&&... args)
{
    if (s_Logger) s_Logger->trace(fmt, std::forward<Args>(args)...);
}

template<typename... Args>
void Logger::Debug(spdlog::format_string_t<Args...> fmt, Args&&... args)
{
    if (s_Logger) s_Logger->debug(fmt, std::forward<Args>(args)...);
}

template<typename... Args>
void Logger::Info(spdlog::format_string_t<Args...> fmt, Args&&... args)
{
    if (s_Logger) s_Logger->info(fmt, std::forward<Args>(args)...);
}

template<typename... Args>
void Logger::Warn(spdlog::format_string_t<Args...> fmt, Args&&... args)
{
    if (s_Logger) s_Logger->warn(fmt, std::forward<Args>(args)...);
}

template<typename... Args>
void Logger::Error(spdlog::format_string_t<Args...> fmt, Args&&... args)
{
    if (s_Logger) s_Logger->error(fmt, std::forward<Args>(args)...);
}

template<typename... Args>
void Logger::Critical(spdlog::format_string_t<Args...> fmt, Args&&... args)
{
    if (s_Logger) s_Logger->critical(fmt, std::forward<Args>(args)...);
}

} // namespace Hydra

// ===========================================================================
// Logging Macros
// ===========================================================================

#define HYDRA_LOG_TRACE(...)    ::Hydra::Logger::Trace(__VA_ARGS__)
#define HYDRA_LOG_DEBUG(...)    ::Hydra::Logger::Debug(__VA_ARGS__)
#define HYDRA_LOG_INFO(...)     ::Hydra::Logger::Info(__VA_ARGS__)
#define HYDRA_LOG_WARN(...)     ::Hydra::Logger::Warn(__VA_ARGS__)
#define HYDRA_LOG_ERROR(...)    ::Hydra::Logger::Error(__VA_ARGS__)
#define HYDRA_LOG_CRITICAL(...) ::Hydra::Logger::Critical(__VA_ARGS__)
