#pragma once

#include <HydraCore/Common/Platform.h>
#include <HydraCore/Common/Types.h>
#include <HydraCore/Common/NonCopyable.h>

#include <any>
#include <typeindex>

namespace Hydra {

class ConfigManager;
class Logger;

/// ===========================================================================
/// EngineContext
///
/// Central service locator / dependency injection container passed to every
/// module.  Services are registered by type and retrieved by type, keeping
/// module code decoupled from concrete service implementations.
///
/// Ownership model:
///   - Services stored as raw (non-owning) pointers — the lifetime of each
///     service is managed by whoever created it (typically HydraApplication).
///   - EngineContext itself does NOT own the services.
/// ===========================================================================
class HYDRA_API EngineContext final : private NonCopyableNonMovable
{
public:
    EngineContext()  = default;
    ~EngineContext() = default;

    // -------------------------------------------------------------------------
    // Service Registration / Removal
    // -------------------------------------------------------------------------

    /// Register a service by its static type.
    /// Overwrites any previously registered service of the same type.
    template<typename T>
    void RegisterService(T* service);

    /// Unregister a service by its static type.
    template<typename T>
    void UnregisterService();

    // -------------------------------------------------------------------------
    // Service Lookup
    // -------------------------------------------------------------------------

    /// Returns a raw pointer to the registered service, or nullptr if absent.
    template<typename T>
    [[nodiscard]] T* GetService() const noexcept;

    /// Returns a reference; throws std::bad_any_cast if service is absent.
    template<typename T>
    [[nodiscard]] T& RequireService() const;

    /// Returns true if a service of type T is currently registered.
    template<typename T>
    [[nodiscard]] bool HasService() const noexcept;

    // -------------------------------------------------------------------------
    // Convenience accessors for mandatory core services
    // -------------------------------------------------------------------------

    [[nodiscard]] ConfigManager& GetConfig() const;

private:
    HashMap<std::type_index, void*> m_Services;
};

// ===========================================================================
// Template Implementations
// ===========================================================================

template<typename T>
void EngineContext::RegisterService(T* service)
{
    m_Services[std::type_index(typeid(T))] = static_cast<void*>(service);
}

template<typename T>
void EngineContext::UnregisterService()
{
    m_Services.erase(std::type_index(typeid(T)));
}

template<typename T>
T* EngineContext::GetService() const noexcept
{
    auto it = m_Services.find(std::type_index(typeid(T)));
    if (it == m_Services.end())
        return nullptr;
    return static_cast<T*>(it->second);
}

template<typename T>
T& EngineContext::RequireService() const
{
    T* service = GetService<T>();
    HYDRA_ASSERT_MSG(service != nullptr, "Required service is not registered in EngineContext");
    return *service;
}

template<typename T>
bool EngineContext::HasService() const noexcept
{
    return m_Services.contains(std::type_index(typeid(T)));
}

} // namespace Hydra
