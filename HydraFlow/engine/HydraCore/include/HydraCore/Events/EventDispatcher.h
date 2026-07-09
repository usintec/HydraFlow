#pragma once

#include <HydraCore/Common/Platform.h>
#include <HydraCore/Common/Types.h>
#include <HydraCore/Common/NonCopyable.h>
#include <HydraCore/Events/Event.h>
#include <HydraCore/Events/EventListener.h>
#include <HydraCore/Events/Subscription.h>

#include <atomic>
#include <mutex>
#include <type_traits>
#include <utility>

namespace Hydra {

// =============================================================================
// EventDispatcher
//
// Synchronous, type-keyed publish/subscribe hub.
//
//   Subscribe<TEvent>(callback, priority) registers a callback for a
//   specific concrete event type and returns a Subscription (RAII handle).
//
//   Dispatch(event) immediately invokes every listener registered for
//   event.GetType(), highest-priority first (insertion order breaks ties).
//   If the event has StopPropagationWhenHandled() enabled and a listener
//   marks it handled, remaining lower-priority listeners are skipped.
//
// Thread-safe: Subscribe/Unsubscribe/Dispatch may be called concurrently
// from different threads. Listener callbacks themselves execute on whatever
// thread calls Dispatch().
// =============================================================================

class HYDRA_API EventDispatcher final : private NonCopyable
{
public:
    EventDispatcher()  = default;
    ~EventDispatcher() = default;

    // -------------------------------------------------------------------------
    // Subscription
    // -------------------------------------------------------------------------

    /// Subscribes to a specific event type, resolved from TEvent at compile
    /// time. Higher `priority` listeners are invoked first.
    template<typename TEvent>
    [[nodiscard]] Subscription Subscribe(EventCallback<TEvent> callback,
                                          i32 priority = static_cast<i32>(EventPriority::Normal))
    {
        static_assert(std::is_base_of_v<Event, TEvent>, "TEvent must derive from Hydra::Event");

        RawEventCallback raw = [cb = std::move(callback)](Event& e)
        {
            cb(static_cast<TEvent&>(e));
        };
        return SubscribeRaw(TEvent::StaticType(), std::move(raw), priority);
    }

    /// Type-erased subscribe, for callers that only have an EventTypeId at
    /// hand (e.g. generic/reflection-driven code).
    [[nodiscard]] Subscription SubscribeRaw(EventTypeId type, RawEventCallback callback, i32 priority = 0);

    /// Removes every listener registered for `type`.
    void UnsubscribeAll(EventTypeId type);

    /// Removes every listener for every type.
    void Clear();

    // -------------------------------------------------------------------------
    // Dispatch
    // -------------------------------------------------------------------------

    /// Synchronously invokes all listeners for event.GetType(). Returns the
    /// number of listeners actually invoked.
    usize Dispatch(Event& event);

    // -------------------------------------------------------------------------
    // Introspection
    // -------------------------------------------------------------------------

    [[nodiscard]] usize GetListenerCount(EventTypeId type) const;
    [[nodiscard]] usize GetTotalListenerCount() const;

private:
    void Unsubscribe(EventTypeId type, ListenerId id);

    mutable std::mutex                               m_Mutex;
    HashMap<EventTypeId, Vector<EventListenerEntry>> m_Listeners;
    std::atomic<ListenerId>                          m_NextListenerId{1};
    std::atomic<u64>                                 m_NextInsertionOrder{0};
};

} // namespace Hydra
