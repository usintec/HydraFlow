#include <HydraCore/HydraCore.h>

#include <atomic>
#include <csignal>

// ==============================================================================
// Example: Minimal HydraCore application with a custom module
// ==============================================================================

static std::atomic<bool> g_ShutdownRequested{false};

static void SignalHandler(int) noexcept
{
    g_ShutdownRequested = true;
}

// ------------------------------------------------------------------------------
// ExampleSimModule — a simple simulation module that counts frames and logs state
// ------------------------------------------------------------------------------

class ExampleSimModule : public Hydra::IHydraModule
{
public:
    explicit ExampleSimModule(Hydra::HydraApplication& app)
        : m_App(app) {}

    Hydra::StringView GetName()    const noexcept override { return "ExampleSimModule"; }
    Hydra::StringView GetVersion() const noexcept override { return "0.1.0"; }

    bool OnInitialize(Hydra::EngineContext& ctx) override
    {
        HYDRA_LOG_INFO("[ExampleSim] Initialized — ready to simulate");
        return true;
    }

    void OnUpdate(Hydra::EngineContext& ctx, Hydra::f64 dt) override
    {
        ++m_FrameCount;

        if (m_FrameCount % 60 == 0) {
            HYDRA_LOG_INFO("[ExampleSim] Frame {:>6} | dt={:.4f}s | elapsed={:.2f}s",
                           m_FrameCount, dt, m_Elapsed);
        }

        m_Elapsed += dt;

        // Respond to OS signal or run for max 300 frames
        if (g_ShutdownRequested || m_FrameCount >= 300) {
            HYDRA_LOG_INFO("[ExampleSim] Requesting shutdown after {} frames", m_FrameCount);
            m_App.RequestShutdown();
        }
    }

    void OnShutdown(Hydra::EngineContext&) override
    {
        HYDRA_LOG_INFO("[ExampleSim] Shutdown — ran {} frames, elapsed {:.3f}s",
                       m_FrameCount, m_Elapsed);
    }

private:
    Hydra::HydraApplication& m_App;
    int                      m_FrameCount = 0;
    Hydra::f64               m_Elapsed    = 0.0;
};

// ------------------------------------------------------------------------------
// CustomApplication — demonstrates lifecycle hook overrides
// ------------------------------------------------------------------------------

class BasicApplication : public Hydra::HydraApplication
{
public:
    using HydraApplication::HydraApplication;

    void OnPreInitialize() override
    {
        HYDRA_LOG_INFO("[BasicApp] Pre-initialize hook");
        GetModules().Register<ExampleSimModule>(*this);
    }

    void OnPostInitialize() override
    {
        HYDRA_LOG_INFO("[BasicApp] All systems go!");
    }

    void OnPreShutdown() override
    {
        HYDRA_LOG_INFO("[BasicApp] Pre-shutdown hook");
    }
};

// ------------------------------------------------------------------------------
// Entry Point
// ------------------------------------------------------------------------------

int main()
{
    std::signal(SIGINT,  SignalHandler);
    std::signal(SIGTERM, SignalHandler);

    Hydra::ApplicationSettings settings = Hydra::ApplicationSettings::MakeDefault();
    settings.appName                    = "BasicApplication";
    settings.appVersion                 = "0.1.0";
    settings.configFilePath             = ""; // no config file needed for this demo
    settings.logging.enableConsole      = true;
    settings.logging.level              = Hydra::LogLevel::Debug;
    settings.updateLoop.targetFrameRate = 60.0;

    BasicApplication app(settings);
    return app.Run();
}
