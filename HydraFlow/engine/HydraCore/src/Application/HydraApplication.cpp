#include <HydraCore/Application/HydraApplication.h>
#include <HydraCore/Application/StartupSequence.h>
#include <HydraCore/Application/ShutdownSequence.h>
#include <HydraCore/Logging/LoggingMacros.h>

#include <chrono>
#include <thread>

namespace Hydra {

// =============================================================================
// Construction / Destruction
// =============================================================================

HydraApplication::HydraApplication(ApplicationSettings settings)
    : m_Settings(std::move(settings))
{
    // The application object is valid but not yet initialized at this point.
    // Initialization (and therefore logging) begins in Run() → Initialize().
}

HydraApplication::~HydraApplication()
{
    // Guard against double-shutdown.  This path is hit when the destructor
    // runs after an exception escapes Run() or if the user never calls Run().
    if (m_Initialized)
        Shutdown();
}

// =============================================================================
// Run — main entry point
//
// Executes the full lifecycle:  Initialize → UpdateLoop → Shutdown.
// Returns a POSIX exit code (0 = success) suitable for main().
// =============================================================================

int HydraApplication::Run()
{
    if (!Initialize())
        return EXIT_FAILURE;

    UpdateLoop();
    Shutdown();

    return EXIT_SUCCESS;
}

// =============================================================================
// RequestShutdown
//
// Thread-safe (write to a bool is atomic on all supported platforms when the
// update loop only reads it once per frame at the top of the loop body).
// Modules call this to request a clean exit at the end of the current frame.
// =============================================================================

void HydraApplication::RequestShutdown() noexcept
{
    m_Running = false;
}

// =============================================================================
// Initialize (private)
// =============================================================================

bool HydraApplication::Initialize()
{
    StartupSequence startup;
    if (!startup.Execute(*this))
        return false;

    m_Initialized = true;
    m_Running     = true;
    return true;
}

// =============================================================================
// UpdateLoop (private)
//
// Headless fixed- or variable-rate update loop.  The engine has no window or
// GPU; the loop simply drives module updates and then sleeps for the remainder
// of the frame budget.
//
// Delta-time clamping prevents the "spiral of death" that occurs when a
// temporary hitch (e.g. disk I/O) produces a huge dt that causes every
// simulation in the next frame to overshoot.
// =============================================================================

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

    while (m_Running)
    {
        const TimePoint now       = Clock::now();
        f64             deltaTime = Duration(now - lastTime).count();
        lastTime = now;

        // Clamp spikes so simulation stays stable even during hiccups
        if (deltaTime > maxDt)
            deltaTime = maxDt;

        // User pre-update hook (e.g. poll OS events in a future window module)
        OnPreUpdate(deltaTime);

        // Drive all registered modules
        m_ModuleManager.UpdateAll(m_Context, deltaTime);
        m_ModuleManager.LateUpdateAll(m_Context, deltaTime);

        // User post-update hook (e.g. present / swap-buffers in a window module)
        OnPostUpdate(deltaTime);

        // Sleep for the remainder of the frame budget to avoid burning CPU
        const f64 elapsed = Duration(Clock::now() - lastTime).count() + deltaTime;
        if (elapsed < targetDt)
        {
            std::this_thread::sleep_for(std::chrono::duration<f64>(targetDt - elapsed));
        }
    }

    HYDRA_LOG_INFO("[Application] Update loop exited");
}

// =============================================================================
// Shutdown (private)
// =============================================================================

void HydraApplication::Shutdown()
{
    if (!m_Initialized) return;

    ShutdownSequence shutdown;
    shutdown.Execute(*this);

    m_Initialized = false;
    m_Running     = false;
}

// =============================================================================
// Accessors
// =============================================================================

EngineContext& HydraApplication::GetContext() noexcept  { return m_Context; }
ModuleManager& HydraApplication::GetModules() noexcept  { return m_ModuleManager; }
ConfigManager& HydraApplication::GetConfig()  noexcept  { return m_ConfigManager; }

const ApplicationSettings& HydraApplication::GetSettings() const noexcept
{
    return m_Settings;
}

bool HydraApplication::IsRunning() const noexcept { return m_Running; }

} // namespace Hydra
