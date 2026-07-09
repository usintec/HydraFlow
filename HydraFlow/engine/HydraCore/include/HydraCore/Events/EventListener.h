#pragma once

#include <HydraCore/Common/Types.h>
#include <HydraCore/Events/Event.h>

namespace Hydra {

/// Opaque identifier for a registered listener, unique per EventDispatcher.
using ListenerId = u64;

/// Strongly-typed callback signature for a listener of TEvent.
template<typename TEvent>
using EventCallback = Function<void(TEvent&)>;

/// Type-erased callback signature used internally once a typed callback has
/// been wrapped for storage in EventDispatcher.
using RawEventCallback = Function<void(Event&)>;

// =============================================================================
// EventListenerEntry
//
// Internal bookkeeping record kept by EventDispatcher for each subscription:
// identity (for unsubscription), priority ordering, and the type-erased
// callback itself.
// =============================================================================
struct EventListenerEntry
{
    ListenerId       id             = 0;
    i32              priority       = static_cast<i32>(EventPriority::Normal);
    u64              insertionOrder = 0;
    RawEventCallback callback;
};

} // namespace Hydra
