#include <HydraCore/Application/StartupSequence.h>
#include <HydraCore/Application/HydraApplication.h>
#include <HydraCore/Application/EngineContext.h>
#include <HydraCore/Application/ModuleManager.h>
#include <HydraCore/Config/ConfigManager.h>
#include <HydraCore/Logging/Logger.h>

namespace Hydra {

bool StartupSequence::Execute(HydraApplication& app)
{
    if (!InitializeLogger(app))     return false;
    if (!LoadConfig(app))           return false;
    if (!RegisterCoreServices(app)) return false;

    app.OnPreInitialize();

    if (!InitializeModules(app))    return false;

    app.OnPostInitialize();

    return true;
}

bool StartupSequence::InitializeLogger(HydraApplication& app)
{
    Logger::Initialize(app.GetSettings().logging);
    HYDRA_LOG_INFO("=== HydraCore Startup ===");
    HYDRA_LOG_INFO("Application : {}", app.GetSettings().appName);
    HYDRA_LOG_INFO("Version     : {}", app.GetSettings().appVersion);
    return true;
}

bool StartupSequence::LoadConfig(HydraApplication& app)
{
    const auto& settingsPath = app.GetSettings().configFilePath;
    auto& cfg = app.GetConfig();

    if (!settingsPath.empty()) {
        if (cfg.LoadFile(settingsPath)) {
            HYDRA_LOG_INFO("[Startup] Config loaded from: {}", settingsPath);
        } else {
            HYDRA_LOG_WARN("[Startup] Config file '{}' not found or invalid — continuing with defaults",
                           settingsPath);
        }
    }
    return true;
}

bool StartupSequence::RegisterCoreServices(HydraApplication& app)
{
    auto& ctx = app.GetContext();
    ctx.RegisterService<ConfigManager>(&app.GetConfig());
    HYDRA_LOG_DEBUG("[Startup] Core services registered");
    return true;
}

bool StartupSequence::InitializeModules(HydraApplication& app)
{
    const bool ok = app.GetModules().InitializeAll(app.GetContext());
    if (!ok) {
        HYDRA_LOG_CRITICAL("[Startup] Module initialization failed — aborting startup");
    }
    return ok;
}

} // namespace Hydra
