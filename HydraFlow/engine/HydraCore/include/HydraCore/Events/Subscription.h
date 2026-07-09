#pragma once

#include <HydraCore/Common/Platform.h>
#include <HydraCore/Common/Types.h>

#include <utility>

namespace Hydra {

// =============================================================================
// Subscription
//
// RAII handle returned by EventDispatcher::Subscribe / EventBus::Subscribe.
// The listener is automatically removed when the Subscription is destroyed
// or Reset(), unless Release() has been called first to detach ownership
// (in which case the listener remains registered until manually removed,
// e.g. via EventDispatcher::UnsubscribeAll).
//
// Move-only, cannot be copied.
// =============================================================================
class HYDRA_API Subscription
{
public:
    Subscription() = default;

    explicit Subscription(Function<void()> unsubscribeFn) noexcept
        : m_UnsubscribeFn(std::move(unsubscribeFn))
        , m_Active(true)
    {
    }

    ~Subscription() { Reset(); }

    Subscription(const Subscription&)            = delete;
    Subscription& operator=(const Subscription&) = delete;

    Subscription(Subscription&& other) noexcept
        : m_UnsubscribeFn(std::move(other.m_UnsubscribeFn))
        , m_Active(other.m_Active)
    {
        other.m_Active = false;
        other.m_UnsubscribeFn = nullptr;
    }

    Subscription& operator=(Subscription&& other) noexcept
    {
        if (this != &other)
        {
            Reset();
            m_UnsubscribeFn       = std::move(other.m_UnsubscribeFn);
            m_Active              = other.m_Active;
            other.m_Active        = false;
            other.m_UnsubscribeFn = nullptr;
        }
        return *this;
    }

    /// Immediately unsubscribes. Safe to call multiple times / on an
    /// already-inactive subscription (no-op).
    void Reset()
    {
        if (m_Active && m_UnsubscribeFn)
        {
            m_UnsubscribeFn();
        }
        m_Active        = false;
        m_UnsubscribeFn = nullptr;
    }

    /// Detaches without unsubscribing — the listener stays registered
    /// indefinitely (or until removed some other way).
    void Release() noexcept
    {
        m_Active        = false;
        m_UnsubscribeFn = nullptr;
    }

    [[nodiscard]] bool IsValid() const noexcept { return m_Active; }

private:
    Function<void()> m_UnsubscribeFn;
    bool             m_Active = false;
};

} // namespace Hydra
