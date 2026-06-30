#include <HydraCore/Logging/LogLevel.h>

#include <algorithm>
#include <cctype>
#include <string>

namespace Hydra {

// =============================================================================
// LogLevel string conversion
// =============================================================================

const char* LogLevelToString(LogLevel level) noexcept
{
    switch (level)
    {
        case LogLevel::Trace:    return "TRACE";
        case LogLevel::Debug:    return "DEBUG";
        case LogLevel::Info:     return "INFO";
        case LogLevel::Warn:     return "WARN";
        case LogLevel::Error:    return "ERROR";
        case LogLevel::Fatal:    return "FATAL";
        case LogLevel::Off:      return "OFF";
        default:                 return "UNKNOWN";
    }
}

const char* LogLevelToStringPadded(LogLevel level) noexcept
{
    // Each string is exactly 5 characters (padded with a trailing space where needed)
    // so log columns stay aligned in fixed-width output.
    switch (level)
    {
        case LogLevel::Trace:    return "TRACE";
        case LogLevel::Debug:    return "DEBUG";
        case LogLevel::Info:     return "INFO ";
        case LogLevel::Warn:     return "WARN ";
        case LogLevel::Error:    return "ERROR";
        case LogLevel::Fatal:    return "FATAL";
        case LogLevel::Off:      return "OFF  ";
        default:                 return "?????";
    }
}

LogLevel LogLevelFromString(StringView str) noexcept
{
    // Copy to a local buffer and convert to lower-case for comparison.
    std::string lower(str);
    std::transform(lower.begin(), lower.end(), lower.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });

    if (lower == "trace")   return LogLevel::Trace;
    if (lower == "debug")   return LogLevel::Debug;
    if (lower == "info")    return LogLevel::Info;
    if (lower == "warn"  || lower == "warning") return LogLevel::Warn;
    if (lower == "error")   return LogLevel::Error;
    if (lower == "fatal" || lower == "critical") return LogLevel::Fatal;
    if (lower == "off")     return LogLevel::Off;

    // Unknown string — fall back to Info so the logger is not silenced.
    return LogLevel::Info;
}

// =============================================================================
// AnsiColor
// =============================================================================

namespace AnsiColor {

const char* ForLevel(LogLevel level) noexcept
{
    switch (level)
    {
        case LogLevel::Trace:    return Gray;
        case LogLevel::Debug:    return Cyan;
        case LogLevel::Info:     return Green;
        case LogLevel::Warn:     return Yellow;
        case LogLevel::Error:    return Red;
        case LogLevel::Fatal:    return Magenta;
        default:                 return Reset;
    }
}

} // namespace AnsiColor

} // namespace Hydra
