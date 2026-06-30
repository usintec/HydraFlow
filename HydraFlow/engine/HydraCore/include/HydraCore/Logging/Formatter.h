#pragma once

#include <HydraCore/Common/Types.h>
#include <HydraCore/Common/NonCopyable.h>
#include <HydraCore/Logging/LogMessage.h>

namespace Hydra {

// =============================================================================
// IFormatter
//
// Abstract interface for converting a LogMessage into a human- or
// machine-readable string.  Sinks hold a shared_ptr<IFormatter>; swapping
// the formatter at runtime changes the output format without touching the sink.
//
// Implementors must be thread-safe: Format() may be called concurrently from
// multiple threads when a sink is shared across loggers.
// =============================================================================

class IFormatter
{
public:
    virtual ~IFormatter() = default;

    /// Convert a LogMessage into its final string representation.
    /// Must be thread-safe (pure functions or internally synchronised).
    [[nodiscard]] virtual String Format(const LogMessage& msg) const = 0;
};

// =============================================================================
// PatternFormatter
//
// A token-based formatter driven by a user-supplied pattern string.
// Tokens are replaced at format time with fields from the LogMessage.
//
// Supported tokens:
//   {time}     — wall-clock time as HH:MM:SS.mmm
//   {date}     — date as YYYY-MM-DD
//   {datetime} — full ISO-8601-like: YYYY-MM-DD HH:MM:SS.mmm
//   {level}    — padded severity label, e.g. "INFO "
//   {LEVEL}    — same as {level} but surrounded by color codes (console only)
//   {name}     — logger name
//   {thread}   — thread ID (decimal)
//   {message}  — formatted log payload
//   {file}     — source file (basename only)
//   {filepath} — source file (full path)
//   {line}     — source line number
//   {func}     — enclosing function name
//
// Default pattern: "[{datetime}] [{name}] [{level}] [{thread}] {message}"
// =============================================================================

class PatternFormatter final : public IFormatter
{
public:
    /// Default constructor uses the standard HydraCore pattern.
    PatternFormatter();

    /// Construct with a custom pattern string.
    explicit PatternFormatter(String pattern);

    // -------------------------------------------------------------------------
    // Configuration
    // -------------------------------------------------------------------------

    /// Replace the active pattern string.
    void SetPattern(String pattern);

    /// Returns the current pattern.
    [[nodiscard]] const String& GetPattern() const noexcept;

    /// Enable or disable ANSI color codes in the {LEVEL} token.
    /// The ConsoleSink enables this automatically when stdout is a TTY.
    void SetColorEnabled(bool enabled) noexcept;

    [[nodiscard]] bool IsColorEnabled() const noexcept;

    // -------------------------------------------------------------------------
    // IFormatter
    // -------------------------------------------------------------------------

    /// Thread-safe: reads m_Pattern but never mutates during formatting.
    [[nodiscard]] String Format(const LogMessage& msg) const override;

private:
    /// Resolves a single recognised token name to its value from msg.
    /// Returns an empty string for unknown tokens.
    [[nodiscard]] String ResolveToken(StringView token, const LogMessage& msg) const;

    /// Formats the timestamp portion of a message.
    [[nodiscard]] static String FormatTimestamp(
        const std::chrono::system_clock::time_point& tp,
        bool dateOnly, bool timeOnly);

    String m_Pattern;
    bool   m_ColorEnabled = false;
};

// =============================================================================
// JsonFormatter
//
// Emits each log record as a single-line JSON object.
// Useful for structured logging pipelines (log aggregators, ELK, etc.).
//
// Example output:
//   {"time":"2026-06-30T12:00:00.123Z","level":"INFO","name":"Hydra",
//    "thread":"140234","message":"Engine started","file":"main.cpp","line":42}
// =============================================================================

class JsonFormatter final : public IFormatter
{
public:
    JsonFormatter()  = default;
    ~JsonFormatter() = default;

    [[nodiscard]] String Format(const LogMessage& msg) const override;
};

} // namespace Hydra
