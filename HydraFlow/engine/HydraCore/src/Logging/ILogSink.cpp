#include <HydraCore/Logging/ILogSink.h>
#include <HydraCore/Logging/Formatter.h>

namespace Hydra {

// =============================================================================
// ILogSink — constructor
// =============================================================================

ILogSink::ILogSink()
{
    // Provide a sensible default formatter so derived classes do not need to
    // call SetFormatter() in their own constructors unless they want a custom
    // pattern or format.
    m_Formatter = std::make_shared<PatternFormatter>();
}

// =============================================================================
// Level filtering
// =============================================================================

void ILogSink::SetLevel(LogLevel level) noexcept
{
    // No lock needed: a single enum assignment is atomic on all supported
    // platforms and the worst outcome of a torn read is a missed message —
    // acceptable for a logging subsystem.
    m_Level = level;
}

LogLevel ILogSink::GetLevel() const noexcept
{
    return m_Level;
}

bool ILogSink::ShouldLog(LogLevel level) const noexcept
{
    return level >= m_Level && m_Level != LogLevel::Off;
}

// =============================================================================
// Formatter
// =============================================================================

void ILogSink::SetFormatter(SharedPtr<IFormatter> formatter)
{
    // Null formatter → restore the default.
    if (!formatter)
        formatter = std::make_shared<PatternFormatter>();

    std::lock_guard lock(m_Mutex);
    m_Formatter = std::move(formatter);
}

const SharedPtr<IFormatter>& ILogSink::GetFormatter() const noexcept
{
    return m_Formatter;
}

// =============================================================================
// FormatMessage — convenience for derived classes
// =============================================================================

String ILogSink::FormatMessage(const LogMessage& msg) const
{
    // Safe to call without m_Mutex: shared_ptr copy is thread-safe in C++11+
    // and the formatter itself must be internally thread-safe per its contract.
    return m_Formatter->Format(msg);
}

} // namespace Hydra
