#pragma once

// =============================================================================
// LoggingMacros.h
//
// Provides all HYDRA_* logging macros.
//
// Design goals:
//   1. Zero overhead when a message is below the logger's level — the format
//      string is never evaluated.
//   2. Source location (file, line, function) is captured automatically at the
//      call site without any extra typing.
//   3. Type-safe format strings via std::format_string (C++20).
//   4. Two families of macros:
//        HYDRA_LOG_*  — lower-level, used internally by HydraCore modules
//        HYDRA_*      — user-facing aliases matching the spec (HYDRA_INFO, etc.)
//
// All macros use LoggerFactory::GetDefault() as their target logger.
// For named loggers use logger->Info("...") directly.
// =============================================================================

#include <HydraCore/Logging/LogLevel.h>
#include <HydraCore/Logging/LogMessage.h>
#include <HydraCore/Logging/LoggerFactory.h>

#include <format>

namespace Hydra::detail {

// ---------------------------------------------------------------------------
// LogImpl
//
// Internal free function invoked by all macros.  Separating this from the
// macro body means we can call LoggerFactory inside a header-only inline
// without pulling in the full Logger implementation everywhere.
// ---------------------------------------------------------------------------

template<typename... Args>
inline void LogImpl(
    LogLevel                    level,
    SourceLocation              loc,
    std::format_string<Args...> fmt,
    Args&&...                   args)
{
    // Fetch the default logger.  The macro ShouldLog guard already ran, but
    // the factory could theoretically return a different logger between the
    // two calls — this is safe because GetDefault() is just a shared_ptr load.
    auto logger = ::Hydra::LoggerFactory::GetDefault();
    if (!logger) return;

    // Format the message at the call site (where Args... are known) so
    // std::format_string can enforce compile-time format-string checking.
    logger->Dispatch(level, loc, std::format(fmt, std::forward<Args>(args)...));
}

} // namespace Hydra::detail

// =============================================================================
// Internal helper — captures source location, guards on level, formats, logs
// =============================================================================

// clang-format off
#define HYDRA_LOG_AT(level, ...)                                                \
    do {                                                                        \
        /* Cheap level check before building the format string */               \
        auto _hydra_logger = ::Hydra::LoggerFactory::GetDefault();              \
        if (_hydra_logger && _hydra_logger->ShouldLog(level)) {                 \
            ::Hydra::detail::LogImpl(                                           \
                (level),                                                        \
                ::Hydra::SourceLocation{__FILE__, __LINE__, __func__},           \
                __VA_ARGS__);                                                   \
        }                                                                       \
    } while (false)
// clang-format on

// =============================================================================
// HYDRA_LOG_* — internal / HydraCore module family
// =============================================================================

/// Extremely verbose tracing; stripped from Release builds.
#ifdef HYDRA_RELEASE
    #define HYDRA_LOG_TRACE(...)    do {} while(false)
#else
    #define HYDRA_LOG_TRACE(...)    HYDRA_LOG_AT(::Hydra::LogLevel::Trace,   __VA_ARGS__)
#endif

#define HYDRA_LOG_DEBUG(...)        HYDRA_LOG_AT(::Hydra::LogLevel::Debug,   __VA_ARGS__)
#define HYDRA_LOG_INFO(...)         HYDRA_LOG_AT(::Hydra::LogLevel::Info,    __VA_ARGS__)
#define HYDRA_LOG_WARN(...)         HYDRA_LOG_AT(::Hydra::LogLevel::Warn,    __VA_ARGS__)
#define HYDRA_LOG_ERROR(...)        HYDRA_LOG_AT(::Hydra::LogLevel::Error,   __VA_ARGS__)
#define HYDRA_LOG_FATAL(...)        HYDRA_LOG_AT(::Hydra::LogLevel::Fatal,   __VA_ARGS__)

// Keep Module 1 compatibility alias
#define HYDRA_LOG_CRITICAL(...)     HYDRA_LOG_FATAL(__VA_ARGS__)

// =============================================================================
// HYDRA_* — user-facing family (matches Module 2 spec)
// =============================================================================

/// Informational message.  Use for normal operational milestones.
#define HYDRA_INFO(...)             HYDRA_LOG_AT(::Hydra::LogLevel::Info,    __VA_ARGS__)

/// Warning: something unexpected happened but the engine can continue.
#define HYDRA_WARNING(...)          HYDRA_LOG_AT(::Hydra::LogLevel::Warn,    __VA_ARGS__)

/// Recoverable error: log it and let the caller decide how to handle it.
#define HYDRA_ERROR(...)            HYDRA_LOG_AT(::Hydra::LogLevel::Error,   __VA_ARGS__)

/// Fatal error: the engine cannot safely continue.  Flushes all sinks.
/// Typically followed immediately by an abort or controlled shutdown.
#define HYDRA_FATAL(...)                                                        \
    do {                                                                        \
        HYDRA_LOG_AT(::Hydra::LogLevel::Fatal, __VA_ARGS__);                   \
        ::Hydra::LoggerFactory::FlushAll();                                     \
    } while (false)

/// Alias for completeness — matches HYDRA_LOG_TRACE.
#define HYDRA_TRACE(...)            HYDRA_LOG_AT(::Hydra::LogLevel::Trace,   __VA_ARGS__)

// Note: HYDRA_DEBUG is intentionally not defined here because CMake emits
// -DHYDRA_DEBUG as a compile-time flag in Debug builds (see CMakeLists.txt).
// Use HYDRA_LOG_DEBUG(...) for debug-level logging instead.
