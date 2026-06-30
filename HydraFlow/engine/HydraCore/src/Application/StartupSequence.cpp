#include <HydraCore/Application/StartupSequence.h>
#include <HydraCore/Application/HydraApplication.h>
#include <HydraCore/Application/EngineContext.h>
#include <HydraCore/Application/ModuleManager.h>
#include <HydraCore/Config/ConfigManager.h>
#include <HydraCore/Logging/LoggerFactory.h>
#include <HydraCore/Logging/LoggingMacros.h>

namespace Hydra {

// =============================================================================
// StartupSequence::Execute
//
// Runs all startup steps in order.  Returns false on the first failure so the
// caller (HydraApplication::Run) can exit before entering the update loop.
// =============================================================================

bool StartupSequence::Execute(HydraApplication& app)
{
    // Steps run in this exact order — changing the order is a breaking change.
    if (!InitializeLogger(app))     return false;  // must be first; all later steps log
    if (!LoadConfig(app))           return false;
    if (!RegisterCoreServices(app)) return false;

    app.OnPreInitialize();                          // user hook before modules start

    if (!InitializeModules(app))    return false;

    app.OnPostInitialize();                         // user hook after modules are up

    return true;
}

// =============================================================================
// Step 1: InitializeLogger
//
// Brings up the LoggerFactory and the default "Hydra" logger.  Every
// subsequent step can use HYDRA_LOG_* macros once this returns true.
// =============================================================================

bool StartupSequence::InitializeLogger(HydraApplication& app)
{
    // Delegate entirely to LoggerFactory.  The config carries sink settings,
    // level, file path, and rotation parameters.
    LoggerFactory::Initialize(app.GetSettings().logging);

    HYDRA_LOG_INFO("=== HydraCore Startup ===");
    HYDRA_LOG_INFO("Application : {}", app.GetSettings().appName);
    HYDRA_LOG_INFO("Version     : {}", app.GetSettings().appVersion);

    return true;
}

// =============================================================================
// Step 2: LoadConfig
//
// Attempts to load the optional YAML configuration file.  A missing or
// malformed file is treated as a soft warning, not a hard failure, because
// defaults are already embedded in ApplicationSettings.
// =============================================================================

bool StartupSequence::LoadConfig(HydraApplication& app)
{
    const auto& settingsPath = app.GetSettings().configFilePath;
    auto& cfg = app.GetConfig();

    if (!settingsPath.empty())
    {
        if (cfg.LoadFile(settingsPath))
        {
            HYDRA_LOG_INFO("[Startup] Config loaded from: {}", settingsPath);
        }
        else
        {
            // Not an error — the file is optional.
            HYDRA_LOG_WARN("[Startup] Config '{}' not found — using defaults", settingsPath);
        }
    }
    return true;
}

// =============================================================================
// Step 3: RegisterCoreServices
//
// Publishes built-in HydraCore services into the EngineContext so modules can
// retrieve them via ctx.RequireService<T>() without knowing the concrete owner.
// =============================================================================

bool StartupSequence::RegisterCoreServices(HydraApplication& app)
{
    auto& ctx = app.GetContext();

    // ConfigManager is owned by HydraApplication; EngineContext holds a non-
    // owning pointer.  The service is unregistered in ShutdownSequence before
    // the application object is destroyed.
    ctx.RegisterService<ConfigManager>(&app.GetConfig());

    HYDRA_LOG_DEBUG("[Startup] Core services registered");
    return true;
}

// =============================================================================
// Step 4: InitializeModules
//
// Drives ModuleManager through the OnRegister → OnInitialize phase for every
// registered module.  A single module failure aborts the whole startup.
// =============================================================================

bool StartupSequence::InitializeModules(HydraApplication& app)
{
    const bool ok = app.GetModules().InitializeAll(app.GetContext());
    if (!ok)
        HYDRA_LOG_FATAL("[Startup] Module initialization failed — engine cannot start");

    return ok;
}

} // namespace Hydra
