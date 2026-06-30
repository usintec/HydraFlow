#include <HydraCore/Logging/Formatter.h>
#include <HydraCore/Logging/LogLevel.h>

#include <chrono>
#include <ctime>
#include <format>
#include <sstream>

namespace Hydra {

// =============================================================================
// PatternFormatter
// =============================================================================

/// Default pattern mirrors what most HydraCore modules expect to see in the
/// console: timestamp, logger name, padded level, thread ID, then the message.
static constexpr const char* kDefaultPattern =
    "[{datetime}] [{name}] [{level}] [t:{thread}] {message}";

PatternFormatter::PatternFormatter()
    : m_Pattern(kDefaultPattern)
    , m_ColorEnabled(false)
{}

PatternFormatter::PatternFormatter(String pattern)
    : m_Pattern(std::move(pattern))
    , m_ColorEnabled(false)
{}

void PatternFormatter::SetPattern(String pattern)
{
    m_Pattern = std::move(pattern);
}

const String& PatternFormatter::GetPattern() const noexcept
{
    return m_Pattern;
}

void PatternFormatter::SetColorEnabled(bool enabled) noexcept
{
    m_ColorEnabled = enabled;
}

bool PatternFormatter::IsColorEnabled() const noexcept
{
    return m_ColorEnabled;
}

// ---------------------------------------------------------------------------
// Format — walk the pattern string and replace recognised {token} sequences
// ---------------------------------------------------------------------------

String PatternFormatter::Format(const LogMessage& msg) const
{
    String result;
    result.reserve(m_Pattern.size() + msg.message.size() + 64);

    std::size_t i = 0;
    while (i < m_Pattern.size())
    {
        if (m_Pattern[i] == '{')
        {
            // Find the closing brace
            std::size_t end = m_Pattern.find('}', i + 1);
            if (end == String::npos)
            {
                // Malformed token — output the rest literally
                result.append(m_Pattern, i, String::npos);
                break;
            }

            // Extract the token name between { and }
            StringView token = StringView(m_Pattern).substr(i + 1, end - i - 1);
            result += ResolveToken(token, msg);
            i = end + 1;
        }
        else
        {
            result += m_Pattern[i];
            ++i;
        }
    }

    return result;
}

// ---------------------------------------------------------------------------
// ResolveToken — map a token name to its runtime value
// ---------------------------------------------------------------------------

String PatternFormatter::ResolveToken(StringView token, const LogMessage& msg) const
{
    if (token == "time")
        return FormatTimestamp(msg.timestamp, /*dateOnly=*/false, /*timeOnly=*/true);

    if (token == "date")
        return FormatTimestamp(msg.timestamp, /*dateOnly=*/true, /*timeOnly=*/false);

    if (token == "datetime")
        return FormatTimestamp(msg.timestamp, /*dateOnly=*/false, /*timeOnly=*/false);

    if (token == "level")
        return LogLevelToStringPadded(msg.level);

    if (token == "LEVEL")
    {
        // Colourised level label — only meaningful on ConsoleSink
        if (m_ColorEnabled)
        {
            return std::string(AnsiColor::ForLevel(msg.level))
                 + LogLevelToStringPadded(msg.level)
                 + AnsiColor::Reset;
        }
        return LogLevelToStringPadded(msg.level);
    }

    if (token == "name")
        return msg.loggerName;

    if (token == "thread")
    {
        // Convert std::thread::id to a decimal string
        std::ostringstream oss;
        oss << msg.threadId;
        return oss.str();
    }

    if (token == "message")
        return msg.message;

    if (token == "file")
    {
        if (!msg.source.IsValid()) return "<unknown>";
        // Return only the base filename, not the full path
        StringView path = msg.source.file;
        auto slash = path.find_last_of("/\\");
        return String(slash == StringView::npos ? path : path.substr(slash + 1));
    }

    if (token == "filepath")
        return msg.source.IsValid() ? String(msg.source.file) : "<unknown>";

    if (token == "line")
        return msg.source.IsValid() ? std::to_string(msg.source.line) : "0";

    if (token == "func")
        return msg.source.IsValid() ? String(msg.source.function) : "<unknown>";

    // Unknown token — output it back literally so it's visible in the output
    return "{" + String(token) + "}";
}

// ---------------------------------------------------------------------------
// FormatTimestamp — convert a time_point to a human-readable string
// ---------------------------------------------------------------------------

String PatternFormatter::FormatTimestamp(
    const std::chrono::system_clock::time_point& tp,
    bool dateOnly,
    bool timeOnly)
{
    // Break into calendar fields via std::time_t
    auto tt    = std::chrono::system_clock::to_time_t(tp);
    std::tm tm_buf{};

    // Use thread-safe variants
#if defined(HYDRA_PLATFORM_WINDOWS)
    localtime_s(&tm_buf, &tt);
#else
    localtime_r(&tt, &tm_buf);
#endif

    // Milliseconds sub-second component
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                  tp.time_since_epoch()) % 1000;

    if (dateOnly)
    {
        return std::format("{:04d}-{:02d}-{:02d}",
                           tm_buf.tm_year + 1900,
                           tm_buf.tm_mon  + 1,
                           tm_buf.tm_mday);
    }

    if (timeOnly)
    {
        return std::format("{:02d}:{:02d}:{:02d}.{:03d}",
                           tm_buf.tm_hour,
                           tm_buf.tm_min,
                           tm_buf.tm_sec,
                           static_cast<int>(ms.count()));
    }

    // Full datetime
    return std::format("{:04d}-{:02d}-{:02d} {:02d}:{:02d}:{:02d}.{:03d}",
                       tm_buf.tm_year + 1900,
                       tm_buf.tm_mon  + 1,
                       tm_buf.tm_mday,
                       tm_buf.tm_hour,
                       tm_buf.tm_min,
                       tm_buf.tm_sec,
                       static_cast<int>(ms.count()));
}

// =============================================================================
// JsonFormatter
// =============================================================================

String JsonFormatter::Format(const LogMessage& msg) const
{
    // Build a single-line JSON object.  We manually escape the message string
    // to avoid pulling in a full JSON library just for the formatter.
    auto escapeJson = [](const String& s) -> String
    {
        String out;
        out.reserve(s.size() + 4);
        for (char c : s)
        {
            switch (c)
            {
                case '"':  out += "\\\""; break;
                case '\\': out += "\\\\"; break;
                case '\n': out += "\\n";  break;
                case '\r': out += "\\r";  break;
                case '\t': out += "\\t";  break;
                default:   out += c;      break;
            }
        }
        return out;
    };

    // Timestamp as ISO-8601
    auto tt = std::chrono::system_clock::to_time_t(msg.timestamp);
    std::tm tm_buf{};
#if defined(HYDRA_PLATFORM_WINDOWS)
    gmtime_s(&tm_buf, &tt);
#else
    gmtime_r(&tt, &tm_buf);
#endif
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                  msg.timestamp.time_since_epoch()) % 1000;

    std::string ts = std::format("{:04d}-{:02d}-{:02d}T{:02d}:{:02d}:{:02d}.{:03d}Z",
                                 tm_buf.tm_year + 1900, tm_buf.tm_mon + 1, tm_buf.tm_mday,
                                 tm_buf.tm_hour, tm_buf.tm_min, tm_buf.tm_sec,
                                 static_cast<int>(ms.count()));

    std::ostringstream tid;
    tid << msg.threadId;

    std::string result;
    result.reserve(256);
    result += "{\"time\":\"";   result += ts;
    result += "\",\"level\":\""; result += LogLevelToString(msg.level);
    result += "\",\"name\":\"";  result += escapeJson(msg.loggerName);
    result += "\",\"thread\":\""; result += tid.str();
    result += "\",\"message\":\""; result += escapeJson(msg.message);

    if (msg.source.IsValid())
    {
        result += "\",\"file\":\"";
        result += escapeJson(msg.source.file);
        result += "\",\"line\":";
        result += std::to_string(msg.source.line);
        result += ",\"func\":\"";
        result += escapeJson(msg.source.function);
        result += "\"";
    }
    else
    {
        result += "\"";
    }

    result += "}";
    return result;
}

} // namespace Hydra
