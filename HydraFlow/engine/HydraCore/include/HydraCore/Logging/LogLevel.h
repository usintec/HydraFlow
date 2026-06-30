#pragma once

#include <HydraCore/Common/Types.h>

namespace Hydra {

// =============================================================================
// LogLevel
//
// Severity levels for the HydraCore logging system, ordered from most verbose
// (Trace) to most severe (Fatal).  "Off" disables all output for a logger or
// sink.
//
// Levels are assigned contiguous integer values so callers can do simple
// comparisons:  if (msg.level >= LogLevel::Warn) ...
// =============================================================================

enum class LogLevel : i32
{
    Trace    = 0,   ///< Fine-grained diagnostic trace — extremely verbose
    Debug    = 1,   ///< Debug information useful during development
    Info     = 2,   ///< General informational messages about normal operation
    Warn     = 3,   ///< Potentially harmful situations that are not errors
    Error    = 4,   ///< Recoverable errors that need attention
    Fatal    = 5,   ///< Unrecoverable errors; engine should terminate after logging
    Off      = 6    ///< Sentinel: disables all logging on a logger or sink
};

// =============================================================================
// LogLevel Utilities
// =============================================================================

/// Returns a short uppercase C-string label for the given level, e.g. "INFO".
/// Always returns a non-null pointer.
[[nodiscard]] const char* LogLevelToString(LogLevel level) noexcept;

/// Returns the 5-character padded label used for aligned column output,
/// e.g. "INFO " or "WARN ".
[[nodiscard]] const char* LogLevelToStringPadded(LogLevel level) noexcept;

/// Parses a level from a case-insensitive string ("trace", "DEBUG", etc.).
/// Returns LogLevel::Info if the string is not recognised.
[[nodiscard]] LogLevel LogLevelFromString(StringView str) noexcept;

// =============================================================================
// ANSI Terminal Color Codes
//
// Used by ConsoleSink to colour-code output by severity.
// Color is automatically suppressed when stdout is not a TTY.
// =============================================================================

namespace AnsiColor {

/// Resets all terminal attributes back to default.
constexpr const char* Reset   = "\033[0m";

/// Per-level foreground colors (8-color, widely supported).
constexpr const char* Gray    = "\033[90m";  ///< Trace  — bright black / dark gray
constexpr const char* Cyan    = "\033[36m";  ///< Debug  — cyan
constexpr const char* Green   = "\033[32m";  ///< Info   — green
constexpr const char* Yellow  = "\033[33m";  ///< Warn   — yellow
constexpr const char* Red     = "\033[31m";  ///< Error  — red
constexpr const char* Magenta = "\033[1;35m"; ///< Fatal  — bold magenta

/// Returns the ANSI color code appropriate for the given log level.
[[nodiscard]] const char* ForLevel(LogLevel level) noexcept;

} // namespace AnsiColor

} // namespace Hydra
