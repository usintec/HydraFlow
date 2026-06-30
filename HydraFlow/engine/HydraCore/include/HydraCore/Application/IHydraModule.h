#pragma once

#include <HydraCore/Common/Platform.h>
#include <HydraCore/Common/Types.h>
#include <HydraCore/Common/NonCopyable.h>

namespace Hydra {

class EngineContext;

/// ===========================================================================
/// IHydraModule
///
/// Interface that every pluggable engine module must implement.
/// Modules are registered with ModuleManager and driven through a
/// well-defined lifecycle:
///
///   Register -> Initialize -> OnUpdate* -> Shutdown -> Unregister
///
/// All methods have default no-op implementations so concrete modules
/// only override what they need.
/// ===========================================================================
class HYDRA_API IHydraModule : private NonCopyable
{
public:
    IHydraModule()          = default;
    virtual ~IHydraModule() = default;

    // Allow move so modules can be stored in containers
    IHydraModule(IHydraModule&&)            = default;
    IHydraModule& operator=(IHydraModule&&) = default;

    // -------------------------------------------------------------------------
    // Identity
    // -------------------------------------------------------------------------

    /// Human-readable name used for logging and diagnostics.
    [[nodiscard]] virtual StringView GetName() const noexcept = 0;

    /// Semantic version of this module (e.g. "1.0.0").
    [[nodiscard]] virtual StringView GetVersion() const noexcept { return "0.0.0"; }

    // -------------------------------------------------------------------------
    // Lifecycle hooks — called by ModuleManager in order
    // -------------------------------------------------------------------------

    /// Called once when the module is first registered before initialization.
    virtual void OnRegister(EngineContext& ctx) { (void)ctx; }

    /// Called during engine startup; acquire resources here.
    /// Return false to abort startup.
    virtual bool OnInitialize(EngineContext& ctx) { (void)ctx; return true; }

    /// Called every frame. deltaTime is in seconds.
    virtual void OnUpdate(EngineContext& ctx, f64 deltaTime) { (void)ctx; (void)deltaTime; }

    /// Called after all modules have updated (post-update).
    virtual void OnLateUpdate(EngineContext& ctx, f64 deltaTime) { (void)ctx; (void)deltaTime; }

    /// Called during engine shutdown; release resources here.
    virtual void OnShutdown(EngineContext& ctx) { (void)ctx; }

    /// Called after shutdown when the module is removed from the manager.
    virtual void OnUnregister(EngineContext& ctx) { (void)ctx; }

    // -------------------------------------------------------------------------
    // State queries
    // -------------------------------------------------------------------------

    [[nodiscard]] bool IsInitialized() const noexcept { return m_Initialized; }

private:
    friend class ModuleManager;
    bool m_Initialized = false;
};

} // namespace Hydra
