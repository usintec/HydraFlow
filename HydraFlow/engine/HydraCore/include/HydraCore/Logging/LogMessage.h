#pragma once

#include <HydraCore/Common/Types.h>
#include <HydraCore/Logging/LogLevel.h>

#include <chrono>
#include <thread>

namespace Hydra {

// =============================================================================
// SourceLocation
//
// Captures the C++ source location (file, line, function) at the point where
// a log macro is invoked.  This is intentionally a lightweight POD-like struct
// — pointers are into string literals, so no heap allocation occurs.
//
// Usage: always populated by the logging macros via __FILE__, __LINE__, __func__
// rather than constructed manually in user code.
// =============================================================================

struct SourceLocation
{
    const char* file     = nullptr;  ///< Source file path (__FILE__)
    int         line     = 0;        ///< Source line number (__LINE__)
    const char* function = nullptr;  ///< Enclosing function name (__func__)

    /// Returns true if this location carries valid source info.
    [[nodiscard]] bool IsValid() const noexcept
    {
        return file != nullptr && line > 0;
    }
};

// =============================================================================
// LogMessage
//
// An immutable value type that represents one fully-assembled log record.
// Sinks receive a const reference to this type; they must not store it past
// their Sink() call without copying.
//
// All fields are populated by Logger::Log() before dispatch to sinks, so
// sinks never need to call system APIs themselves.
// =============================================================================

struct LogMessage
{
    // ---- Identity -----------------------------------------------------------

    String      loggerName;      ///< Name of the logger that emitted this message

    // ---- Severity -----------------------------------------------------------

    LogLevel    level;           ///< Severity level of this record

    // ---- Payload ------------------------------------------------------------

    String      message;         ///< The fully-formatted (via std::format) log text

    // ---- Metadata -----------------------------------------------------------

    /// Wall-clock timestamp captured at the moment Log() was called.
    std::chrono::system_clock::time_point timestamp;

    /// OS thread ID of the thread that called Log().
    std::thread::id threadId;

    /// Source location captured by the logging macro (optional — may be empty).
    SourceLocation  source;

    // -------------------------------------------------------------------------
    // Factory
    // -------------------------------------------------------------------------

    /// Convenience constructor that populates timestamp and threadId automatically.
    static LogMessage Make(
        String           loggerName,
        LogLevel         level,
        String           message,
        SourceLocation   source = {})
    {
        LogMessage m;
        m.loggerName = std::move(loggerName);
        m.level      = level;
        m.message    = std::move(message);
        m.timestamp  = std::chrono::system_clock::now();
        m.threadId   = std::this_thread::get_id();
        m.source     = source;
        return m;
    }
};

} // namespace Hydra
