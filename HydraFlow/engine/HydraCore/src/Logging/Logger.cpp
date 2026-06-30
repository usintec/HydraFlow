#include <HydraCore/Logging/Logger.h>

namespace Hydra {

// =============================================================================
// Construction
// =============================================================================

Logger::Logger(String name, LogLevel level)
    : m_Name(std::move(name))
    , m_Level(level)
{}

// =============================================================================
// Identity
// =============================================================================

const String& Logger::GetName() const noexcept
{
    return m_Name;
}

// =============================================================================
// Level filtering
// =============================================================================

void Logger::SetLevel(LogLevel level) noexcept
{
    std::lock_guard lock(m_SinkMutex);
    m_Level = level;
}

LogLevel Logger::GetLevel() const noexcept
{
    std::lock_guard lock(m_SinkMutex);
    return m_Level;
}

bool Logger::ShouldLog(LogLevel level) const noexcept
{
    // Intentionally not locking here — reading a single enum value that is
    // written by a single atomic-width store is safe on all mainstream
    // architectures.  Using a lock here would add contention on the hot path.
    return level >= m_Level && m_Level != LogLevel::Off;
}

// =============================================================================
// Sink management
// =============================================================================

void Logger::AddSink(SharedPtr<ILogSink> sink)
{
    if (!sink) return;
    std::lock_guard lock(m_SinkMutex);
    m_Sinks.push_back(std::move(sink));
}

void Logger::RemoveSink(const SharedPtr<ILogSink>& sink)
{
    std::lock_guard lock(m_SinkMutex);
    m_Sinks.erase(
        std::remove(m_Sinks.begin(), m_Sinks.end(), sink),
        m_Sinks.end());
}

void Logger::ClearSinks()
{
    std::lock_guard lock(m_SinkMutex);
    m_Sinks.clear();
}

Vector<SharedPtr<ILogSink>> Logger::GetSinks() const
{
    // Return a snapshot so the caller does not hold the lock.
    std::lock_guard lock(m_SinkMutex);
    return m_Sinks;
}

// =============================================================================
// Core dispatch
// =============================================================================

void Logger::Dispatch(LogLevel level, const SourceLocation& source, String message)
{
    // Assemble the immutable LogMessage record.
    LogMessage record = LogMessage::Make(m_Name, level, std::move(message), source);

    // Take a snapshot of the sink list under the lock, then release the lock
    // before calling into each sink.  This prevents deadlocks if a sink's
    // Sink() method calls back into this logger.
    Vector<SharedPtr<ILogSink>> sinks;
    {
        std::lock_guard lock(m_SinkMutex);
        sinks = m_Sinks;
    }

    for (const auto& sink : sinks)
    {
        // Each sink has its own level filter; only forward if it passes.
        if (sink->ShouldLog(record.level))
            sink->Sink(record);
    }
}

// =============================================================================
// Flush
// =============================================================================

void Logger::Flush()
{
    std::lock_guard lock(m_SinkMutex);
    for (auto& sink : m_Sinks)
        sink->Flush();
}

} // namespace Hydra
