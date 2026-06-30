#pragma once

#include <HydraCore/Common/Platform.h>
#include <HydraCore/Common/Types.h>
#include <HydraCore/Common/NonCopyable.h>
#include <HydraCore/Logging/Logger.h>
#include <HydraCore/Logging/LogLevel.h>

#include <mutex>

namespace Hydra {

struct LoggerConfig;

// =============================================================================
// LoggerConfig
//
// Plain configuration struct passed to LoggerFactory::Initialize() or
// LoggerFactory::Create().  Contains everything needed to build a fully-
// configured logger with console and/or file sinks.
// =============================================================================

struct LoggerConfig
{
    String  name          = "Hydra";   ///< Logger name (and registry key)

    // ---- Level --------------------------------------------------------------
    LogLevel level        = LogLevel::Debug; ///< Minimum level for the logger

    // ---- Console sink -------------------------------------------------------
    bool    enableConsole = true;      ///< Attach a ConsoleSink
    LogLevel consoleLevel = LogLevel::Trace; ///< Minimum level for the console sink

    // ---- File sink ----------------------------------------------------------
    bool    enableFile    = false;     ///< Attach a FileSink
    String  filePath      = "logs/hydra.log"; ///< Path to the log file
    bool    truncateFile  = true;      ///< Truncate on open (vs append)
    LogLevel fileLevel    = LogLevel::Trace;  ///< Minimum level for the file sink

    // ---- File rotation (only used when enableFile = true) ------------------
    usize   maxFileBytes  = 10 * 1024 * 1024; ///< 10 MiB rotation threshold
    u32     maxFileCount  = 5;                ///< Number of rotated backups to keep
};

// =============================================================================
// LoggerFactory
//
// Global registry and factory for Logger instances.
//
// Responsibilities:
//   - Create loggers with pre-configured sinks (console, file, or both).
//   - Maintain a named registry so any subsystem can retrieve a logger by name.
//   - Track a "default" logger used by the HYDRA_* convenience macros.
//   - Flush and destroy all loggers on Shutdown().
//
// All public methods are thread-safe (protected by a single registry mutex).
//
// Typical usage in StartupSequence:
//   LoggerConfig cfg;
//   cfg.enableConsole = true;
//   cfg.enableFile    = true;
//   cfg.filePath      = "logs/hydra.log";
//   LoggerFactory::Initialize(cfg);       // creates "Hydra" default logger
//
// Typical usage in application code:
//   auto logger = LoggerFactory::Get("MySystem");
//   if (!logger) logger = LoggerFactory::Create({.name = "MySystem"});
//   logger->Info("Hello from MySystem");
// =============================================================================

class HYDRA_API LoggerFactory final : private NonCopyableNonMovable
{
public:
    // =========================================================================
    // Lifecycle
    // =========================================================================

    /// Create and register the primary "Hydra" default logger from config.
    /// Safe to call multiple times — subsequent calls are ignored.
    static void Initialize(const LoggerConfig& config = {});

    /// Flush all loggers, clear the registry, and reset the default pointer.
    /// After this call Initialize() may be called again.
    static void Shutdown();

    [[nodiscard]] static bool IsInitialized() noexcept;

    // =========================================================================
    // Logger creation
    // =========================================================================

    /// Create a logger from a full config, register it, and return it.
    /// If a logger with the same name already exists it is returned unchanged.
    static SharedPtr<Logger> Create(const LoggerConfig& config);

    /// Convenience: create a logger with a console sink only.
    static SharedPtr<Logger> CreateConsoleLogger(
        String   name,
        LogLevel level = LogLevel::Debug);

    /// Convenience: create a logger with a file sink only.
    static SharedPtr<Logger> CreateFileLogger(
        String   name,
        String   path,
        LogLevel level = LogLevel::Debug);

    /// Convenience: create a logger with both console and file sinks.
    static SharedPtr<Logger> CreateCompositeLogger(
        String   name,
        String   filePath,
        LogLevel level = LogLevel::Debug);

    // =========================================================================
    // Registry access
    // =========================================================================

    /// Retrieve a previously registered logger by name.
    /// Returns nullptr if no logger with that name exists.
    [[nodiscard]] static SharedPtr<Logger> Get(StringView name);

    /// Returns the default logger (set during Initialize).
    /// Returns nullptr if the factory has not been initialised.
    [[nodiscard]] static SharedPtr<Logger> GetDefault();

    /// Change which registered logger serves as the default.
    /// Useful when different subsystems want different primary loggers.
    static void SetDefault(StringView name);

    // =========================================================================
    // Bulk operations
    // =========================================================================

    /// Flush every registered logger.
    static void FlushAll();

    /// Set the level on every registered logger simultaneously.
    static void SetLevelAll(LogLevel level);

private:
    // Construction is forbidden — factory is purely static.
    LoggerFactory() = delete;

    /// Internal helper that actually builds a Logger from a config.
    /// The registry mutex must NOT be held when this is called (it calls
    /// Create() which acquires the mutex internally).
    static SharedPtr<Logger> BuildLogger(const LoggerConfig& config);

    // ---- Registry -----------------------------------------------------------
    static HashMap<String, SharedPtr<Logger>> s_Registry; ///< name → logger map
    static SharedPtr<Logger>                  s_Default;  ///< Current default logger
    static std::mutex                         s_Mutex;    ///< Guards registry + default
    static bool                               s_Initialized;
};

} // namespace Hydra
