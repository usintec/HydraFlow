#pragma once

#include <HydraCore/Common/Platform.h>
#include <HydraCore/Common/Types.h>
#include <HydraCore/Common/NonCopyable.h>
#include <HydraCore/Logging/LogLevel.h>
#include <HydraCore/Logging/LogMessage.h>
#include <HydraCore/Logging/Formatter.h>

#include <memory>
#include <mutex>

namespace Hydra {

// =============================================================================
// ILogSink
//
// Abstract base class for all log output destinations.
//
// A sink is responsible for:
//   1. Filtering messages by minimum level (m_Level).
//   2. Formatting the message using its owned IFormatter.
//   3. Writing the formatted string to the output destination.
//   4. Flushing pending writes when requested.
//
// Thread-safety contract:
//   Derived classes must implement Sink() and Flush() in a thread-safe manner.
//   The m_Mutex member is available for use in derived classes.
//
// Ownership:
//   Sinks are shared via shared_ptr<ILogSink>.  Multiple Logger instances may
//   share a single sink (e.g. a global console sink).
// =============================================================================

class HYDRA_API ILogSink : private NonCopyable
{
public:
    ILogSink();
    virtual ~ILogSink() = default;

    // Allow moving into containers
    ILogSink(ILogSink&&)            = default;
    ILogSink& operator=(ILogSink&&) = default;

    // -------------------------------------------------------------------------
    // Core Interface — must be implemented by every sink
    // -------------------------------------------------------------------------

    /// Write msg to the output destination.
    /// Called only when ShouldLog(msg.level) returns true.
    /// Implementations must be thread-safe.
    virtual void Sink(const LogMessage& msg) = 0;

    /// Flush any buffered output immediately.
    /// Called at shutdown or after Fatal-level messages.
    virtual void Flush() = 0;

    // -------------------------------------------------------------------------
    // Level Filtering
    // -------------------------------------------------------------------------

    /// Set the minimum severity this sink will process.
    /// Messages below this level are silently discarded.
    void SetLevel(LogLevel level) noexcept;

    [[nodiscard]] LogLevel GetLevel() const noexcept;

    /// Returns true if a message at this level should be forwarded to Sink().
    [[nodiscard]] bool ShouldLog(LogLevel level) const noexcept;

    // -------------------------------------------------------------------------
    // Formatter
    // -------------------------------------------------------------------------

    /// Replace the formatter.  Passing nullptr restores the default PatternFormatter.
    void SetFormatter(SharedPtr<IFormatter> formatter);

    [[nodiscard]] const SharedPtr<IFormatter>& GetFormatter() const noexcept;

protected:
    /// Convenience: format a message using the owned formatter.
    [[nodiscard]] String FormatMessage(const LogMessage& msg) const;

    /// Mutex available to derived classes for write serialisation.
    mutable std::mutex m_Mutex;

private:
    LogLevel              m_Level     = LogLevel::Trace; ///< Minimum level for this sink
    SharedPtr<IFormatter> m_Formatter;                   ///< Owned formatter instance
};

} // namespace Hydra
