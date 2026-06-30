#include <HydraCore/Config/ApplicationSettings.h>
#include <HydraCore/Config/ConfigManager.h>
#include <HydraCore/Logging/Logger.h>

namespace Hydra {

ApplicationSettings ApplicationSettings::MakeDefault()
{
    ApplicationSettings s;
    s.appName        = "HydraFlow Application";
    s.appVersion     = "0.1.0";
    s.configFilePath = "config/hydra.yaml";

    s.logging.loggerName    = "Hydra";
    s.logging.enableConsole = true;
    s.logging.enableFile    = false;
    s.logging.level         = LogLevel::Debug;

    s.updateLoop.fixedTimeStep   = false;
    s.updateLoop.targetFrameRate = 60.0;
    s.updateLoop.maxDeltaTime    = 0.25;

    return s;
}

ApplicationSettings ApplicationSettings::LoadFromFile(StringView path)
{
    ApplicationSettings s = MakeDefault();

    ConfigManager cfg;
    if (!cfg.LoadFile(std::filesystem::path(path))) {
        // Logger may not be initialized yet — use stderr
        fprintf(stderr, "[HydraCore] ApplicationSettings: failed to load '%.*s', using defaults\n",
                static_cast<int>(path.size()), path.data());
        return s;
    }

    s.appName    = cfg.GetOrDefault<std::string>("app.name",    s.appName);
    s.appVersion = cfg.GetOrDefault<std::string>("app.version", s.appVersion);

    s.logging.enableConsole = cfg.GetOrDefault<bool>("logging.console", s.logging.enableConsole);
    s.logging.enableFile    = cfg.GetOrDefault<bool>("logging.file",    s.logging.enableFile);
    s.logging.logFilePath   = cfg.GetOrDefault<std::string>("logging.path", s.logging.logFilePath);

    const std::string levelStr = cfg.GetOrDefault<std::string>("logging.level", "debug");
    if      (levelStr == "trace")    s.logging.level = LogLevel::Trace;
    else if (levelStr == "debug")    s.logging.level = LogLevel::Debug;
    else if (levelStr == "info")     s.logging.level = LogLevel::Info;
    else if (levelStr == "warn")     s.logging.level = LogLevel::Warn;
    else if (levelStr == "error")    s.logging.level = LogLevel::Error;
    else if (levelStr == "critical") s.logging.level = LogLevel::Critical;

    s.updateLoop.targetFrameRate = cfg.GetOrDefault<double>("update_loop.target_fps",    s.updateLoop.targetFrameRate);
    s.updateLoop.maxDeltaTime    = cfg.GetOrDefault<double>("update_loop.max_delta_time", s.updateLoop.maxDeltaTime);
    s.updateLoop.fixedTimeStep   = cfg.GetOrDefault<bool>  ("update_loop.fixed",          s.updateLoop.fixedTimeStep);

    return s;
}

} // namespace Hydra
