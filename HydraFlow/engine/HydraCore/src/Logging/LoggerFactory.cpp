#include <HydraCore/Logging/LoggerFactory.h>
#include <HydraCore/Logging/ConsoleSink.h>
#include <HydraCore/Logging/FileSink.h>
#include <HydraCore/Logging/NullSink.h>
#include <HydraCore/Logging/Formatter.h>

#include <stdexcept>

namespace Hydra {

// =============================================================================
// Static member definitions
// =============================================================================

HashMap<String, SharedPtr<Logger>> LoggerFactory::s_Registry;
SharedPtr<Logger>                  LoggerFactory::s_Default;
std::mutex                         LoggerFactory::s_Mutex;
bool                               LoggerFactory::s_Initialized = false;

// =============================================================================
// Lifecycle
// =============================================================================

void LoggerFactory::Initialize(const LoggerConfig& config)
{
    // Phase 1: mark initialised under lock, then release immediately.
    // This prevents double-init without holding the mutex across the
    // Create() call that follows (which would re-acquire the same mutex
    // and deadlock).
    {
        std::lock_guard lock(s_Mutex);
        if (s_Initialized) return;
        s_Initialized = true;
    }

    // Phase 2: build the primary logger — lock is NOT held here.
    // Create() acquires s_Mutex internally; no deadlock.
    auto logger = Create(config);

    // Phase 3: publish as the default under lock.
    {
        std::lock_guard lock(s_Mutex);
        s_Default = logger;
    }
}

void LoggerFactory::Shutdown()
{
    // Flush everything before tearing down.
    FlushAll();

    std::lock_guard lock(s_Mutex);
    s_Registry.clear();
    s_Default.reset();
    s_Initialized = false;
}

bool LoggerFactory::IsInitialized() noexcept
{
    std::lock_guard lock(s_Mutex);
    return s_Initialized;
}

// =============================================================================
// Logger creation helpers
// =============================================================================

SharedPtr<Logger> LoggerFactory::Create(const LoggerConfig& config)
{
    std::lock_guard lock(s_Mutex);

    // Return the existing logger if one with the same name is already registered.
    auto it = s_Registry.find(config.name);
    if (it != s_Registry.end())
        return it->second;

    // Release lock before calling BuildLogger — BuildLogger re-acquires s_Mutex
    // via Create().  We call it via the non-locking path (BuildLogger directly)
    // to avoid recursion.
    // We already hold the lock here, so we inline the build.
    auto logger = std::make_shared<Logger>(config.name, config.level);

    if (config.enableConsole)
    {
        auto sink = std::make_shared<ConsoleSink>();
        sink->SetLevel(config.consoleLevel);
        logger->AddSink(std::move(sink));
    }

    if (config.enableFile && !config.filePath.empty())
    {
        auto sink = std::make_shared<FileSink>(
            std::filesystem::path(config.filePath),
            config.truncateFile);
        sink->SetLevel(config.fileLevel);
        sink->SetMaxBytes(config.maxFileBytes);
        sink->SetMaxFiles(config.maxFileCount);
        logger->AddSink(std::move(sink));
    }

    s_Registry[config.name] = logger;
    return logger;
}

SharedPtr<Logger> LoggerFactory::CreateConsoleLogger(String name, LogLevel level)
{
    LoggerConfig cfg;
    cfg.name           = std::move(name);
    cfg.level          = level;
    cfg.enableConsole  = true;
    cfg.consoleLevel   = LogLevel::Trace;
    cfg.enableFile     = false;
    return Create(cfg);
}

SharedPtr<Logger> LoggerFactory::CreateFileLogger(String name, String path, LogLevel level)
{
    LoggerConfig cfg;
    cfg.name           = std::move(name);
    cfg.level          = level;
    cfg.enableConsole  = false;
    cfg.enableFile     = true;
    cfg.filePath       = std::move(path);
    cfg.fileLevel      = LogLevel::Trace;
    return Create(cfg);
}

SharedPtr<Logger> LoggerFactory::CreateCompositeLogger(String name, String filePath, LogLevel level)
{
    LoggerConfig cfg;
    cfg.name           = std::move(name);
    cfg.level          = level;
    cfg.enableConsole  = true;
    cfg.consoleLevel   = LogLevel::Trace;
    cfg.enableFile     = true;
    cfg.filePath       = std::move(filePath);
    cfg.fileLevel      = LogLevel::Trace;
    return Create(cfg);
}

// =============================================================================
// Registry access
// =============================================================================

SharedPtr<Logger> LoggerFactory::Get(StringView name)
{
    std::lock_guard lock(s_Mutex);
    auto it = s_Registry.find(String(name));
    return it != s_Registry.end() ? it->second : nullptr;
}

SharedPtr<Logger> LoggerFactory::GetDefault()
{
    std::lock_guard lock(s_Mutex);
    return s_Default;
}

void LoggerFactory::SetDefault(StringView name)
{
    std::lock_guard lock(s_Mutex);
    auto it = s_Registry.find(String(name));
    if (it != s_Registry.end())
        s_Default = it->second;
}

// =============================================================================
// Bulk operations
// =============================================================================

void LoggerFactory::FlushAll()
{
    // Snapshot the registry under the lock, then flush without holding it
    // so sinks that call back into the factory don't deadlock.
    Vector<SharedPtr<Logger>> loggers;
    {
        std::lock_guard lock(s_Mutex);
        for (auto& [name, logger] : s_Registry)
            loggers.push_back(logger);
    }

    for (auto& logger : loggers)
        logger->Flush();
}

void LoggerFactory::SetLevelAll(LogLevel level)
{
    std::lock_guard lock(s_Mutex);
    for (auto& [name, logger] : s_Registry)
        logger->SetLevel(level);
}

// =============================================================================
// Internal build helper
//
// Called by Initialize() after the lock has already been released so it can
// call Create() safely.
// =============================================================================

SharedPtr<Logger> LoggerFactory::BuildLogger(const LoggerConfig& config)
{
    // Delegate to the public Create() which handles registry insertion and
    // duplicate detection under its own lock.
    return Create(config);
}

} // namespace Hydra
