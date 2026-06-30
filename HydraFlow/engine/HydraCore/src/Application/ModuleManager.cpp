#include <HydraCore/Application/ModuleManager.h>
#include <HydraCore/Application/IHydraModule.h>
#include <HydraCore/Application/EngineContext.h>
#include <HydraCore/Logging/Logger.h>

#include <algorithm>

namespace Hydra {

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

    if (it == m_Modules.end()) {
        HYDRA_LOG_WARN("[ModuleManager] Unregister: module '{}' not found", name);
        return false;
    }

    if ((*it)->IsInitialized()) {
        HYDRA_LOG_DEBUG("[ModuleManager] Shutting down module '{}' before unregistering", name);
        (*it)->OnShutdown(ctx);
    }
    (*it)->OnUnregister(ctx);

    HYDRA_LOG_DEBUG("[ModuleManager] Unregistered module: {}", name);
    m_Modules.erase(it);
    return true;
}

bool ModuleManager::InitializeAll(EngineContext& ctx)
{
    HYDRA_LOG_INFO("[ModuleManager] Initializing {} module(s)...", m_Modules.size());

    for (auto& module : m_Modules) {
        HYDRA_LOG_DEBUG("[ModuleManager]   + OnRegister  : {}", module->GetName());
        module->OnRegister(ctx);
    }

    for (auto& module : m_Modules) {
        HYDRA_LOG_DEBUG("[ModuleManager]   + OnInitialize: {}", module->GetName());
        if (!module->OnInitialize(ctx)) {
            HYDRA_LOG_ERROR("[ModuleManager] Module '{}' failed to initialize — aborting",
                            module->GetName());
            return false;
        }
        module->m_Initialized = true;
    }

    HYDRA_LOG_INFO("[ModuleManager] All modules initialized successfully");
    return true;
}

void ModuleManager::UpdateAll(EngineContext& ctx, f64 deltaTime)
{
    for (auto& module : m_Modules) {
        if (module->IsInitialized())
            module->OnUpdate(ctx, deltaTime);
    }
}

void ModuleManager::LateUpdateAll(EngineContext& ctx, f64 deltaTime)
{
    for (auto& module : m_Modules) {
        if (module->IsInitialized())
            module->OnLateUpdate(ctx, deltaTime);
    }
}

void ModuleManager::ShutdownAll(EngineContext& ctx)
{
    HYDRA_LOG_INFO("[ModuleManager] Shutting down {} module(s) (reverse order)...",
                   m_Modules.size());

    for (auto it = m_Modules.rbegin(); it != m_Modules.rend(); ++it) {
        auto& module = *it;
        if (module->IsInitialized()) {
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
    for (const auto& module : m_Modules) {
        if (module->GetName() == name)
            return module.get();
    }
    return nullptr;
}

} // namespace Hydra
