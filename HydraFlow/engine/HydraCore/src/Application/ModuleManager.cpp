#include <HydraCore/Application/ModuleManager.h>
#include <HydraCore/Application/IHydraModule.h>
#include <HydraCore/Application/EngineContext.h>
#include <HydraCore/Logging/LoggingMacros.h>

#include <algorithm>

namespace Hydra {

// =============================================================================
// Registration
// =============================================================================

void ModuleManager::Register(UniquePtr<IHydraModule> module)
{
    HYDRA_ASSERT_MSG(module != nullptr, "Cannot register a null module");
    HYDRA_LOG_DEBUG("[ModuleManager] Registering module: {}", module->GetName());
    m_Modules.push_back(std::move(module));
}

bool ModuleManager::Unregister(StringView name, EngineContext& ctx)
{
    auto it = std::find_if(m_Modules.begin(), m_Modules.end(),
        [&name](const UniquePtr<IHydraModule>& m) {
            return m->GetName() == name;
        });

    if (it == m_Modules.end())
    {
        HYDRA_LOG_WARN("[ModuleManager] Unregister: module '{}' not found", name);
        return false;
    }

    // If the module was fully initialized we must give it a chance to clean up
    // before we destroy it.
    if ((*it)->IsInitialized())
    {
        HYDRA_LOG_DEBUG("[ModuleManager] Shutting down '{}' before unregistering", name);
        (*it)->OnShutdown(ctx);
    }
    (*it)->OnUnregister(ctx);

    HYDRA_LOG_DEBUG("[ModuleManager] Unregistered module: {}", name);
    m_Modules.erase(it);
    return true;
}

// =============================================================================
// Lifecycle — InitializeAll
//
// Two-phase approach:
//   Phase 1 — OnRegister: every module is notified it has been added to the
//              manager.  At this point no module is yet fully initialized.
//   Phase 2 — OnInitialize: modules allocate resources and validate their
//              dependencies.  If any module fails, startup is aborted before
//              touching subsequent modules (fail-fast).
// =============================================================================

bool ModuleManager::InitializeAll(EngineContext& ctx)
{
    HYDRA_LOG_INFO("[ModuleManager] Initializing {} module(s)...", m_Modules.size());

    // Phase 1: notification that the module has been registered
    for (auto& module : m_Modules)
    {
        HYDRA_LOG_DEBUG("[ModuleManager]   + OnRegister  : {}", module->GetName());
        module->OnRegister(ctx);
    }

    // Phase 2: resource acquisition / validation
    for (auto& module : m_Modules)
    {
        HYDRA_LOG_DEBUG("[ModuleManager]   + OnInitialize: {}", module->GetName());
        if (!module->OnInitialize(ctx))
        {
            HYDRA_LOG_ERROR("[ModuleManager] Module '{}' failed OnInitialize — aborting",
                            module->GetName());
            return false;
        }
        // Mark as initialized so ShutdownAll knows to call OnShutdown on it.
        module->m_Initialized = true;
    }

    HYDRA_LOG_INFO("[ModuleManager] All modules initialized successfully");
    return true;
}

// =============================================================================
// Lifecycle — UpdateAll / LateUpdateAll
//
// Only initialized modules receive update ticks.  A module that failed
// OnInitialize is never updated.
// =============================================================================

void ModuleManager::UpdateAll(EngineContext& ctx, f64 deltaTime)
{
    for (auto& module : m_Modules)
    {
        if (module->IsInitialized())
            module->OnUpdate(ctx, deltaTime);
    }
}

void ModuleManager::LateUpdateAll(EngineContext& ctx, f64 deltaTime)
{
    for (auto& module : m_Modules)
    {
        if (module->IsInitialized())
            module->OnLateUpdate(ctx, deltaTime);
    }
}

// =============================================================================
// Lifecycle — ShutdownAll
//
// Iterates in reverse registration order so modules are torn down in the
// opposite sequence from initialization.  This respects implicit dependencies:
// if module B depends on module A, B was registered after A and should be
// shut down before A.
// =============================================================================

void ModuleManager::ShutdownAll(EngineContext& ctx)
{
    HYDRA_LOG_INFO("[ModuleManager] Shutting down {} module(s) (reverse order)...",
                   m_Modules.size());

    for (auto it = m_Modules.rbegin(); it != m_Modules.rend(); ++it)
    {
        auto& module = *it;
        if (module->IsInitialized())
        {
            HYDRA_LOG_DEBUG("[ModuleManager]   - OnShutdown  : {}", module->GetName());
            module->OnShutdown(ctx);
            module->m_Initialized = false;
        }
        HYDRA_LOG_DEBUG("[ModuleManager]   - OnUnregister: {}", module->GetName());
        module->OnUnregister(ctx);
    }

    m_Modules.clear();
    HYDRA_LOG_INFO("[ModuleManager] All modules shut down");
}

// =============================================================================
// Queries
// =============================================================================

usize ModuleManager::Count() const noexcept
{
    return m_Modules.size();
}

bool ModuleManager::IsEmpty() const noexcept
{
    return m_Modules.empty();
}

IHydraModule* ModuleManager::Find(StringView name) const noexcept
{
    for (const auto& module : m_Modules)
    {
        if (module->GetName() == name)
            return module.get();
    }
    return nullptr;
}

} // namespace Hydra
