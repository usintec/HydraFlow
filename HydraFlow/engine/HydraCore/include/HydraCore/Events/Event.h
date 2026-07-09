#pragma once

#include <HydraCore/Common/Platform.h>
#include <HydraCore/Common/Types.h>

#include <typeindex>
#include <typeinfo>
#include <chrono>

namespace Hydra {

// =============================================================================
// EventTypeId / EventPriority
// =============================================================================

/// Stable identity for a concrete Event subclass, derived from RTTI.
using EventTypeId = std::type_index;

/// Listener priority. Higher values run first when EventDispatcher invokes
/// listeners for a given event. Ties are broken by subscription order
/// (earlier subscribers run first).
enum class EventPriority : i32
{
    Lowest  = -100,
    Low     = -50,
    Normal  = 0,
    High    = 50,
    Highest = 100,
};

// =============================================================================
// Event
//
// Abstract base for all engine events. Concrete events should derive from
// EventType<Derived> (below) rather than Event directly — it fills in
// GetType()/GetName() automatically.
// =============================================================================

class HYDRA_API Event
{
public:
    virtual ~Event() = default;

    // -------------------------------------------------------------------------
    // Identity
    // -------------------------------------------------------------------------

    [[nodiscard]] virtual EventTypeId GetType() const noexcept = 0;
    [[nodiscard]] virtual StringView  GetName() const noexcept = 0;

    // -------------------------------------------------------------------------
    // Handled state
    //
    // A listener may call SetHandled() to mark the event as consumed. By
    // default this is purely informational; if StopPropagationWhenHandled()
    // is also enabled, EventDispatcher::Dispatch will stop invoking any
    // further (lower-priority) listeners once the event becomes handled.
    // -------------------------------------------------------------------------

    [[nodiscard]] bool IsHandled() const noexcept { return m_Handled; }
    void SetHandled(bool handled = true) noexcept { m_Handled = handled; }

    [[nodiscard]] bool StopsPropagationWhenHandled() const noexcept { return m_StopPropagation; }
    void SetStopPropagationWhenHandled(bool stop) noexcept { m_StopPropagation = stop; }

    // -------------------------------------------------------------------------
    // Metadata
    // -------------------------------------------------------------------------

    [[nodiscard]] std::chrono::steady_clock::time_point GetTimestamp() const noexcept { return m_Timestamp; }

protected:
    Event() = default;

private:
    std::chrono::steady_clock::time_point m_Timestamp = std::chrono::steady_clock::now();
    bool m_Handled         = false;
    bool m_StopPropagation = false;
};

// =============================================================================
// EventType<Derived> — CRTP base that implements GetType()/GetName().
//
// Usage:
//     class PlayerDiedEvent : public EventType<PlayerDiedEvent>
//     {
//     public:
//         explicit PlayerDiedEvent(i32 playerId) : playerId(playerId) {}
//         i32 playerId;
//     };
//
//     dispatcher.Subscribe<PlayerDiedEvent>([](PlayerDiedEvent& e) { ... });
// =============================================================================

template<typename Derived>
class EventType : public Event
{
public:
    [[nodiscard]] static EventTypeId StaticType() noexcept
    {
        return std::type_index(typeid(Derived));
    }

    [[nodiscard]] EventTypeId GetType() const noexcept override { return StaticType(); }
    [[nodiscard]] StringView  GetName() const noexcept override { return typeid(Derived).name(); }
};

} // namespace Hydra
