#include <HydraCore/Application/ShutdownSequence.h>
#include <HydraCore/Application/HydraApplication.h>
#include <HydraCore/Application/EngineContext.h>
#include <HydraCore/Application/ModuleManager.h>
#include <HydraCore/Config/ConfigManager.h>
#include <HydraCore/Logging/LoggerFactory.h>
#include <HydraCore/Logging/LoggingMacros.h>

namespace Hydra {

// =============================================================================
// ShutdownSequence::Execute
//
// Tears down the engine in the reverse order of StartupSequence::Execute:
//   modules → core services → logger
//
// This order guarantees that:
//   - Modules can still call HYDRA_LOG_* during their OnShutdown() hooks.
//   - The logger is the last thing to go, capturing any final diagnostics.
// =============================================================================

void ShutdownSequence::Execute(HydraApplication& app)
{
    HYDRA_LOG_INFO("=== HydraCore Shutdown ===");

    app.OnPreShutdown();   // user hook — runs before any module is touched

    ShutdownModules(app);
    UnregisterCoreServices(app);
    ShutdownLogger();      // must be last — logs are valid up to this point

    app.OnPostShutdown();  // user hook — runs after the logger is gone (no logging!)
}

// =============================================================================
// ShutdownModules
//
// Drives ModuleManager through OnShutdown → OnUnregister for every module
// in reverse registration order.  Reverse order ensures that modules with
// dependencies on earlier modules are torn down before their dependencies.
// =============================================================================

void ShutdownSequence::ShutdownModules(HydraApplication& app)
{
    app.GetModules().ShutdownAll(app.GetContext());
}

// =============================================================================
// UnregisterCoreServices
//
// Removes raw service pointers from the EngineContext.  Must happen after
// modules are shut down so no module's OnShutdown tries to use a service
// that has already been de-registered.
// =============================================================================

void ShutdownSequence::UnregisterCoreServices(HydraApplication& app)
{
    app.GetContext().UnregisterService<ConfigManager>();
    HYDRA_LOG_DEBUG("[Shutdown] Core services unregistered");
}

// =============================================================================
// ShutdownLogger
//
// Flushes all sinks and tears down the LoggerFactory registry.  This is the
// point of no return — no log calls must be made after this returns.
// =============================================================================

void ShutdownSequence::ShutdownLogger()
{
    HYDRA_LOG_INFO("[Shutdown] Logger shutting down — goodbye");
    LoggerFactory::Shutdown();
}

} // namespace Hydra
