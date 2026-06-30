#include <HydraCore/Config/ApplicationSettings.h>
#include <HydraCore/Config/ConfigManager.h>
#include <HydraCore/Logging/LogLevel.h>

namespace Hydra {

// =============================================================================
// MakeDefault
// =============================================================================

ApplicationSettings ApplicationSettings::MakeDefault()
{
    ApplicationSettings s;

    // ---- Identity -----------------------------------------------------------
    s.appName        = "HydraFlow Application";
    s.appVersion     = "0.1.0";
    s.configFilePath = "config/hydra.yaml";

    // ---- Logging (Module 2 LoggerConfig) ------------------------------------
    s.logging.name          = "Hydra";
    s.logging.level         = LogLevel::Debug;
    s.logging.enableConsole = true;
    s.logging.consoleLevel  = LogLevel::Trace;
    s.logging.enableFile    = false;
    s.logging.filePath      = "logs/hydra.log";
    s.logging.maxFileBytes  = 10 * 1024 * 1024; // 10 MiB
    s.logging.maxFileCount  = 5;

    // ---- Update loop --------------------------------------------------------
    s.updateLoop.fixedTimeStep   = false;
    s.updateLoop.targetFrameRate = 60.0;
    s.updateLoop.maxDeltaTime    = 0.25;

    return s;
}

// =============================================================================
// LoadFromFile
// =============================================================================

ApplicationSettings ApplicationSettings::LoadFromFile(StringView path)
{
    // Start from sensible defaults; override only what the file specifies.
    ApplicationSettings s = MakeDefault();

    ConfigManager cfg;
    if (!cfg.LoadFile(std::filesystem::path(path)))
    {
        // Log to stderr because the Logger is not yet initialised at this stage.
        fprintf(stderr,
                "[HydraCore] ApplicationSettings: '%.*s' not found or invalid — using defaults\n",
                static_cast<int>(path.size()), path.data());
        return s;
    }

    // ---- Identity -----------------------------------------------------------
    s.appName    = cfg.GetOrDefault<std::string>("app.name",    s.appName);
    s.appVersion = cfg.GetOrDefault<std::string>("app.version", s.appVersion);

    // ---- Logging ------------------------------------------------------------
    s.logging.enableConsole = cfg.GetOrDefault<bool>("logging.console", s.logging.enableConsole);
    s.logging.enableFile    = cfg.GetOrDefault<bool>("logging.file",    s.logging.enableFile);
    s.logging.filePath      = cfg.GetOrDefault<std::string>("logging.path", s.logging.filePath);

    // Parse the level string ("trace" / "debug" / "info" / etc.)
    const std::string levelStr =
        cfg.GetOrDefault<std::string>("logging.level", "debug");
    s.logging.level = LogLevelFromString(levelStr);

    // File rotation settings
    s.logging.maxFileBytes =
        cfg.GetOrDefault<usize>("logging.max_file_bytes", s.logging.maxFileBytes);
    s.logging.maxFileCount =
        cfg.GetOrDefault<u32>("logging.max_file_count", s.logging.maxFileCount);

    // ---- Update loop --------------------------------------------------------
    s.updateLoop.targetFrameRate =
        cfg.GetOrDefault<double>("update_loop.target_fps",     s.updateLoop.targetFrameRate);
    s.updateLoop.maxDeltaTime    =
        cfg.GetOrDefault<double>("update_loop.max_delta_time", s.updateLoop.maxDeltaTime);
    s.updateLoop.fixedTimeStep   =
        cfg.GetOrDefault<bool>  ("update_loop.fixed",          s.updateLoop.fixedTimeStep);

    return s;
}

} // namespace Hydra
