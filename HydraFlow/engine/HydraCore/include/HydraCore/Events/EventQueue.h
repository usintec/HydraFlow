#pragma once

#include <HydraCore/Common/Platform.h>
#include <HydraCore/Common/Types.h>
#include <HydraCore/Common/NonCopyable.h>
#include <HydraCore/Events/Event.h>
#include <HydraCore/Events/EventDispatcher.h>

#include <deque>
#include <mutex>
#include <type_traits>
#include <utility>

namespace Hydra {

// =============================================================================
// EventQueue
//
// Thread-safe FIFO buffer of pending events for deferred / asynchronous
// dispatch. Producers call Enqueue()/Emplace() from any thread; a single
// consumer (typically the main/update thread) periodically calls
// ProcessAll()/ProcessUpTo() to drain the queue and dispatch each event
// synchronously through an EventDispatcher.
//
// Events enqueued *during* a Process* call (e.g. by a listener publishing a
// follow-up event) are left in the queue for the next call — a single
// Process* invocation never dispatches more than what was pending when it
// started, which bounds per-frame work and avoids infinite loops.
// =============================================================================

class HYDRA_API EventQueue final : private NonCopyable
{
public:
    EventQueue()  = default;
    ~EventQueue() = default;

    /// Enqueues a pre-constructed event for later dispatch. Ignored if null.
    void Enqueue(UniquePtr<Event> event);

    /// Constructs TEvent in place on the heap and enqueues it.
    template<typename TEvent, typename... Args>
    void Emplace(Args&&... args)
    {
        static_assert(std::is_base_of_v<Event, TEvent>, "TEvent must derive from Hydra::Event");
        Enqueue(MakeUnique<TEvent>(std::forward<Args>(args)...));
    }

    /// Dispatches every event that was pending at the start of this call, in
    /// FIFO order. Returns the number of events dispatched.
    usize ProcessAll(EventDispatcher& dispatcher);

    /// Dispatches at most maxEvents pending events (0 means unlimited, same
    /// as ProcessAll). Returns the number of events actually dispatched.
    usize ProcessUpTo(EventDispatcher& dispatcher, usize maxEvents);

    [[nodiscard]] usize GetPendingCount() const;

    /// Discards all pending events without dispatching them.
    void Clear();

private:
    mutable std::mutex           m_Mutex;
    std::deque<UniquePtr<Event>> m_Queue;
};

} // namespace Hydra
