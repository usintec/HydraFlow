#pragma once

#include <HydraCore/Common/Platform.h>
#include <HydraCore/Common/Types.h>
#include <HydraCore/Common/NonCopyable.h>
#include <HydraCore/Application/IHydraModule.h>

#include <vector>
#include <memory>

namespace Hydra {

class EngineContext;

/// ===========================================================================
/// ModuleManager
///
/// Owns and drives all IHydraModule instances through their full lifecycle.
/// Modules are stored in insertion order, which also determines the order in
/// which Initialize, Update, and Shutdown are called.
///
/// Typical usage:
///   manager.Register<MyModule>();
///   manager.InitializeAll(ctx);          // called by StartupSequence
///   while (running) manager.UpdateAll(ctx, dt);
///   manager.ShutdownAll(ctx);            // called by ShutdownSequence
/// ===========================================================================
class HYDRA_API ModuleManager final : private NonCopyableNonMovable
{
public:
    ModuleManager()  = default;
    ~ModuleManager() = default;

    // -------------------------------------------------------------------------
    // Registration
    // -------------------------------------------------------------------------

    /// Construct a module in-place and register it.
    template<typename T, typename... Args>
    T& Register(Args&&... args);

    /// Register an already-constructed module (transfer ownership).
    void Register(UniquePtr<IHydraModule> module);

    /// Remove a module by name (calls OnShutdown + OnUnregister if initialized).
    bool Unregister(StringView name, EngineContext& ctx);

    // -------------------------------------------------------------------------
    // Lifecycle
    // -------------------------------------------------------------------------

    /// Initialize all registered modules in order.
    /// Returns false and halts if any module returns false from OnInitialize.
    bool InitializeAll(EngineContext& ctx);

    /// Call OnUpdate on all initialized modules.
    void UpdateAll(EngineContext& ctx, f64 deltaTime);

    /// Call OnLateUpdate on all initialized modules.
    void LateUpdateAll(EngineContext& ctx, f64 deltaTime);

    /// Shutdown all initialized modules in reverse order.
    void ShutdownAll(EngineContext& ctx);

    // -------------------------------------------------------------------------
    // Queries
    // -------------------------------------------------------------------------

    [[nodiscard]] usize Count() const noexcept;
    [[nodiscard]] bool  IsEmpty() const noexcept;

    /// Find a module by name; returns nullptr if not found.
    [[nodiscard]] IHydraModule* Find(StringView name) const noexcept;

    /// Find and cast to a concrete type; returns nullptr on failure.
    template<typename T>
    [[nodiscard]] T* FindAs(StringView name) const noexcept;

private:
    Vector<UniquePtr<IHydraModule>> m_Modules;
};

// ===========================================================================
// Template Implementations
// ===========================================================================

template<typename T, typename... Args>
T& ModuleManager::Register(Args&&... args)
{
    static_assert(std::is_base_of_v<IHydraModule, T>,
                  "T must derive from IHydraModule");
    auto module = MakeUnique<T>(std::forward<Args>(args)...);
    T&   ref    = *module;
    m_Modules.push_back(std::move(module));
    return ref;
}

template<typename T>
T* ModuleManager::FindAs(StringView name) const noexcept
{
    return dynamic_cast<T*>(Find(name));
}

} // namespace Hydra
