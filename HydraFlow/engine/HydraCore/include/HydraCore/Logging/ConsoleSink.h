#pragma once

#include <HydraCore/Common/Platform.h>
#include <HydraCore/Logging/ILogSink.h>

namespace Hydra {

// =============================================================================
// ConsoleSink
//
// A thread-safe log sink that writes formatted messages to stdout (Info and
// below) or stderr (Error and Fatal).
//
// Color support:
//   ANSI escape codes are applied automatically when the output stream is
//   attached to an interactive terminal (TTY).  On Windows, virtual terminal
//   sequences are enabled via SetConsoleMode().  When piped/redirected, color
//   is suppressed to keep log files clean.
//
// Thread safety:
//   A mutex serialises all writes; concurrent calls from different threads
//   will produce complete, non-interleaved lines.
//
// Usage:
//   auto sink = std::make_shared<ConsoleSink>();
//   sink->SetLevel(LogLevel::Debug);
//   logger->AddSink(sink);
// =============================================================================

class HYDRA_API ConsoleSink final : public ILogSink
{
public:
    /// Constructs the sink and detects TTY/color support automatically.
    ConsoleSink();
    ~ConsoleSink() override = default;

    // -------------------------------------------------------------------------
    // Color control
    // -------------------------------------------------------------------------

    /// Override automatic TTY detection to force color on or off.
    void SetColorEnabled(bool enabled) noexcept;

    [[nodiscard]] bool IsColorEnabled() const noexcept;

    // -------------------------------------------------------------------------
    // ILogSink
    // -------------------------------------------------------------------------

    /// Writes the formatted record to stdout (or stderr for Error/Fatal).
    /// Wraps the level label in ANSI color codes when color is enabled.
    /// Thread-safe via internal mutex.
    void Sink(const LogMessage& msg) override;

    /// Flushes both stdout and stderr.
    void Flush() override;

private:
    /// Detects whether the given file descriptor is connected to a TTY.
    static bool DetectTty(int fd) noexcept;

    bool m_ColorEnabled = false; ///< Whether ANSI codes are currently active
};

} // namespace Hydra
