#pragma once

#include <HydraCore/Common/Platform.h>
#include <HydraCore/Common/Types.h>
#include <HydraCore/Common/NonCopyable.h>
#include <HydraCore/Logging/LogLevel.h>
#include <HydraCore/Logging/LogMessage.h>
#include <HydraCore/Logging/ILogSink.h>

#include <format>
#include <mutex>
#include <vector>

namespace Hydra {

// =============================================================================
// Logger
//
// The central logging object.  Each Logger has:
//   - A unique name (used in the log output and for lookup in LoggerFactory).
//   - A minimum level: messages below this level are dropped before reaching
//     any sink, making early rejection very cheap.
//   - An ordered list of sinks.  Messages are forwarded to every sink whose
//     own level filter also passes.
//
// Loggers are not constructed directly; use LoggerFactory to create and
// retrieve named instances.
//
// Thread safety:
//   Logger is fully thread-safe.  The sink list is protected by m_SinkMutex.
//   Individual sinks are responsible for their own write serialisation.
//
// Lifecycle:
//   Loggers are owned by LoggerFactory (via shared_ptr).  Destroying a Logger
//   does not flush its sinks — call Flush() explicitly if needed.
// =============================================================================

class HYDRA_API Logger final : private NonCopyable
{
public:
    /// Loggers are created by LoggerFactory, but the constructor is public so
    /// tests and custom factories can build them directly.
    explicit Logger(String name, LogLevel level = LogLevel::Trace);

    ~Logger() = default;

    // Allow move so LoggerFactory can emplace into containers
    Logger(Logger&&)            = default;
    Logger& operator=(Logger&&) = default;

    // =========================================================================
    // Identity
    // =========================================================================

    [[nodiscard]] const String& GetName()  const noexcept;

    // =========================================================================
    // Level filtering
    // =========================================================================

    /// Set the minimum level.  Any message with a lower severity is discarded
    /// immediately without touching sinks.
    void     SetLevel(LogLevel level) noexcept;

    [[nodiscard]] LogLevel GetLevel() const noexcept;

    /// Quick check used by macros to avoid building the format string when the
    /// message would be discarded anyway.
    [[nodiscard]] bool ShouldLog(LogLevel level) const noexcept;

    // =========================================================================
    // Sink management
    // =========================================================================

    /// Append a sink to the end of the sink list.
    void AddSink(SharedPtr<ILogSink> sink);

    /// Remove all sinks that compare equal (by pointer) to the given sink.
    void RemoveSink(const SharedPtr<ILogSink>& sink);

    /// Remove all sinks.
    void ClearSinks();

    /// Returns a snapshot of the current sink list (copy, not a live view).
    [[nodiscard]] Vector<SharedPtr<ILogSink>> GetSinks() const;

    // =========================================================================
    // Core logging — accepting a pre-formatted string
    // =========================================================================

    /// Dispatch a pre-formatted message to all sinks.
    /// This is the lowest-level entry point; all other Log overloads call this.
    void Dispatch(LogLevel level, const SourceLocation& source, String message);

    // =========================================================================
    // Template logging — format string + arguments (C++20 std::format)
    // =========================================================================

    /// Log with explicit level and source location.
    /// Formatting occurs here; if ShouldLog() returns false the format call is
    /// skipped entirely.
    template<typename... Args>
    void Log(LogLevel                      level,
             const SourceLocation&         source,
             std::format_string<Args...>   fmt,
             Args&&...                     args)
    {
        if (!ShouldLog(level)) return;
        Dispatch(level, source, std::format(fmt, std::forward<Args>(args)...));
    }

    // ---- Convenience overloads (no source location) -------------------------

    template<typename... Args>
    void Trace(std::format_string<Args...> fmt, Args&&... args)
    {
        Log(LogLevel::Trace, {}, fmt, std::forward<Args>(args)...);
    }

    template<typename... Args>
    void Debug(std::format_string<Args...> fmt, Args&&... args)
    {
        Log(LogLevel::Debug, {}, fmt, std::forward<Args>(args)...);
    }

    template<typename... Args>
    void Info(std::format_string<Args...> fmt, Args&&... args)
    {
        Log(LogLevel::Info, {}, fmt, std::forward<Args>(args)...);
    }

    template<typename... Args>
    void Warn(std::format_string<Args...> fmt, Args&&... args)
    {
        Log(LogLevel::Warn, {}, fmt, std::forward<Args>(args)...);
    }

    template<typename... Args>
    void Error(std::format_string<Args...> fmt, Args&&... args)
    {
        Log(LogLevel::Error, {}, fmt, std::forward<Args>(args)...);
    }

    template<typename... Args>
    void Fatal(std::format_string<Args...> fmt, Args&&... args)
    {
        Log(LogLevel::Fatal, {}, fmt, std::forward<Args>(args)...);
    }

    // =========================================================================
    // Flush
    // =========================================================================

    /// Flush every sink attached to this logger.
    void Flush();

private:
    String                    m_Name;      ///< Unique logger name
    LogLevel                  m_Level;     ///< Minimum level (atomic-like via mutex)
    Vector<SharedPtr<ILogSink>> m_Sinks;   ///< Ordered list of output sinks
    mutable std::mutex        m_SinkMutex; ///< Guards m_Sinks and m_Level
};

} // namespace Hydra
