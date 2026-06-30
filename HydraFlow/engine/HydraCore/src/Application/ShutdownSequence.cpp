#include <HydraCore/Application/ShutdownSequence.h>
#include <HydraCore/Application/HydraApplication.h>
#include <HydraCore/Application/EngineContext.h>
#include <HydraCore/Application/ModuleManager.h>
#include <HydraCore/Config/ConfigManager.h>
#include <HydraCore/Logging/Logger.h>

namespace Hydra {

void ShutdownSequence::Execute(HydraApplication& app)
{
    HYDRA_LOG_INFO("=== HydraCore Shutdown ===");

    app.OnPreShutdown();

    ShutdownModules(app);
    UnregisterCoreServices(app);
    ShutdownLogger();

    app.OnPostShutdown();
}

void ShutdownSequence::ShutdownModules(HydraApplication& app)
{
    app.GetModules().ShutdownAll(app.GetContext());
}

void ShutdownSequence::UnregisterCoreServices(HydraApplication& app)
{
    auto& ctx = app.GetContext();
    ctx.UnregisterService<ConfigManager>();
    HYDRA_LOG_DEBUG("[Shutdown] Core services unregistered");
}

void ShutdownSequence::ShutdownLogger()
{
    HYDRA_LOG_INFO("[Shutdown] Logger shutting down — goodbye");
    Logger::Shutdown();
}

} // namespace Hydra
