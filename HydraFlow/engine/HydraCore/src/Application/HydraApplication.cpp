#include <HydraCore/Application/HydraApplication.h>
#include <HydraCore/Application/StartupSequence.h>
#include <HydraCore/Application/ShutdownSequence.h>
#include <HydraCore/Logging/Logger.h>

#include <chrono>
#include <thread>

namespace Hydra {

HydraApplication::HydraApplication(ApplicationSettings settings)
    : m_Settings(std::move(settings))
{
}

HydraApplication::~HydraApplication()
{
    if (m_Initialized) {
        Shutdown();
    }
}

int HydraApplication::Run()
{
    if (!Initialize()) {
        return EXIT_FAILURE;
    }

    UpdateLoop();
    Shutdown();

    return EXIT_SUCCESS;
}

void HydraApplication::RequestShutdown() noexcept
{
    m_Running = false;
}

bool HydraApplication::Initialize()
{
    StartupSequence startup;
    if (!startup.Execute(*this)) {
        return false;
    }
    m_Initialized = true;
    m_Running     = true;
    return true;
}

void HydraApplication::UpdateLoop()
{
    using Clock     = std::chrono::high_resolution_clock;
    using TimePoint = Clock::time_point;
    using Duration  = std::chrono::duration<f64>;

    const f64 targetDt = 1.0 / m_Settings.updateLoop.targetFrameRate;
    const f64 maxDt    = m_Settings.updateLoop.maxDeltaTime;

    HYDRA_LOG_INFO("[Application] Entering update loop (target fps={:.0f})",
                   m_Settings.updateLoop.targetFrameRate);

    TimePoint lastTime = Clock::now();

    while (m_Running) {
        TimePoint now      = Clock::now();
        f64       deltaTime = Duration(now - lastTime).count();
        lastTime = now;

        // Clamp to prevent spiral-of-death on large hitches
        if (deltaTime > maxDt)
            deltaTime = maxDt;

        OnPreUpdate(deltaTime);
        m_ModuleManager.UpdateAll(m_Context, deltaTime);
        m_ModuleManager.LateUpdateAll(m_Context, deltaTime);
        OnPostUpdate(deltaTime);

        // Sleep to maintain target framerate (headless mode)
        const f64 elapsed = Duration(Clock::now() - lastTime).count() + deltaTime;
        if (elapsed < targetDt) {
            const auto sleepDuration = std::chrono::duration<f64>(targetDt - elapsed);
            std::this_thread::sleep_for(sleepDuration);
        }
    }

    HYDRA_LOG_INFO("[Application] Update loop exited");
}

void HydraApplication::Shutdown()
{
    if (!m_Initialized)
        return;

    ShutdownSequence shutdown;
    shutdown.Execute(*this);

    m_Initialized = false;
    m_Running     = false;
}

EngineContext& HydraApplication::GetContext() noexcept
{
    return m_Context;
}

ModuleManager& HydraApplication::GetModules() noexcept
{
    return m_ModuleManager;
}

ConfigManager& HydraApplication::GetConfig() noexcept
{
    return m_ConfigManager;
}

const ApplicationSettings& HydraApplication::GetSettings() const noexcept
{
    return m_Settings;
}

bool HydraApplication::IsRunning() const noexcept
{
    return m_Running;
}

} // namespace Hydra
