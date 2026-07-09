#pragma once

#include <HydraCore/Common/Platform.h>
#include <HydraCore/Common/Types.h>
#include <HydraCore/Common/NonCopyable.h>
#include <HydraCore/Events/Event.h>
#include <HydraCore/Events/EventListener.h>
#include <HydraCore/Events/Subscription.h>
#include <HydraCore/Events/EventDispatcher.h>
#include <HydraCore/Events/EventQueue.h>

#include <type_traits>
#include <utility>

namespace Hydra {

// =============================================================================
// EventBus
//
// High-level façade combining an EventDispatcher (synchronous delivery) with
// an EventQueue (deferred/asynchronous delivery) behind a single API:
//
//   Publish / Emit           -> dispatched immediately, on the calling thread.
//   PublishAsync / EmitAsync -> queued; delivered on the next ProcessQueue().
//
// Typically one EventBus is owned by EngineContext, and ProcessQueue() is
// called once per frame from the application's update loop.
// =============================================================================

class HYDRA_API EventBus final : private NonCopyable
{
public:
    EventBus()  = default;
    ~EventBus() = default;

    // -------------------------------------------------------------------------
    // Subscription
    // -------------------------------------------------------------------------

    template<typename TEvent>
    [[nodiscard]] Subscription Subscribe(EventCallback<TEvent> callback,
                                          i32 priority = static_cast<i32>(EventPriority::Normal))
    {
        return m_Dispatcher.Subscribe<TEvent>(std::move(callback), priority);
    }

    // -------------------------------------------------------------------------
    // Synchronous publish
    // -------------------------------------------------------------------------

    /// Dispatches an already-constructed event immediately.
    template<typename TEvent>
    usize Publish(TEvent& event)
    {
        static_assert(std::is_base_of_v<Event, TEvent>, "TEvent must derive from Hydra::Event");
        return m_Dispatcher.Dispatch(event);
    }

    /// Constructs TEvent on the stack and dispatches it immediately.
    template<typename TEvent, typename... Args>
    usize Emit(Args&&... args)
    {
        TEvent event(std::forward<Args>(args)...);
        return Publish(event);
    }

    // -------------------------------------------------------------------------
    // Asynchronous publish
    // -------------------------------------------------------------------------

    /// Queues a heap-allocated event for delivery on the next ProcessQueue().
    template<typename TEvent>
    void PublishAsync(UniquePtr<TEvent> event)
    {
        static_assert(std::is_base_of_v<Event, TEvent>, "TEvent must derive from Hydra::Event");
        m_Queue.Enqueue(std::move(event));
    }

    /// Constructs TEvent on the heap and queues it for deferred delivery.
    template<typename TEvent, typename... Args>
    void EmitAsync(Args&&... args)
    {
        m_Queue.template Emplace<TEvent>(std::forward<Args>(args)...);
    }

    // -------------------------------------------------------------------------
    // Queue processing
    // -------------------------------------------------------------------------

    /// Drains and dispatches queued async events (0 = unlimited). Returns
    /// the number of events dispatched.
    usize ProcessQueue(usize maxEvents = 0);

    [[nodiscard]] usize GetPendingCount() const;

    // -------------------------------------------------------------------------
    // Introspection / management
    // -------------------------------------------------------------------------

    [[nodiscard]] usize GetListenerCount(EventTypeId type) const;
    void UnsubscribeAll(EventTypeId type);
    void ClearListeners();
    void ClearQueue();

private:
    EventDispatcher m_Dispatcher;
    EventQueue      m_Queue;
};

} // namespace Hydra
