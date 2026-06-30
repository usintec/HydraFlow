#pragma once

#include <HydraCore/Common/Platform.h>
#include <HydraCore/Common/Types.h>
#include <HydraCore/Common/NonCopyable.h>
#include <HydraCore/Application/EngineContext.h>
#include <HydraCore/Application/ModuleManager.h>
#include <HydraCore/Config/ApplicationSettings.h>
#include <HydraCore/Config/ConfigManager.h>

namespace Hydra {

class StartupSequence;
class ShutdownSequence;

/// ===========================================================================
/// HydraApplication
///
/// The root object of every HydraFlow application.  One instance must exist
/// for the lifetime of the process.  It owns the EngineContext, the
/// ModuleManager, and all core services.
///
/// Usage pattern:
///
///   ApplicationSettings settings = ApplicationSettings::MakeDefault();
///   settings.appName = "My Simulation";
///
///   HydraApplication app(settings);
///   app.GetModules().Register<MySimModule>();
///
///   return app.Run();
/// ===========================================================================
class HYDRA_API HydraApplication : private NonCopyableNonMovable
{
public:
    explicit HydraApplication(ApplicationSettings settings = ApplicationSettings::MakeDefault());
    ~HydraApplication();

    // -------------------------------------------------------------------------
    // Main Entry Point
    // -------------------------------------------------------------------------

    /// Runs the full startup → update loop → shutdown sequence.
    /// Returns an OS exit code (0 = success).
    int Run();

    /// Request a clean shutdown at the end of the current frame.
    void RequestShutdown() noexcept;

    // -------------------------------------------------------------------------
    // Accessors
    // -------------------------------------------------------------------------

    [[nodiscard]] EngineContext&            GetContext()  noexcept;
    [[nodiscard]] ModuleManager&            GetModules()  noexcept;
    [[nodiscard]] ConfigManager&            GetConfig()   noexcept;
    [[nodiscard]] const ApplicationSettings& GetSettings() const noexcept;

    [[nodiscard]] bool IsRunning() const noexcept;

    // -------------------------------------------------------------------------
    // Overridable hooks (for subclassing)
    // -------------------------------------------------------------------------

    /// Called after core services are up but before modules are initialized.
    virtual void OnPreInitialize()  {}

    /// Called after all modules are initialized, before the update loop.
    virtual void OnPostInitialize() {}

    /// Called each frame before module updates.
    virtual void OnPreUpdate(f64 /*deltaTime*/)  {}

    /// Called each frame after module updates.
    virtual void OnPostUpdate(f64 /*deltaTime*/) {}

    /// Called after modules are shut down, before core teardown.
    virtual void OnPreShutdown()  {}

    /// Called at the very end of shutdown.
    virtual void OnPostShutdown() {}

private:
    bool Initialize();
    void UpdateLoop();
    void Shutdown();

    ApplicationSettings          m_Settings;
    EngineContext                m_Context;
    ConfigManager                m_ConfigManager;
    ModuleManager                m_ModuleManager;

    bool m_Running    = false;
    bool m_Initialized = false;
};

} // namespace Hydra
