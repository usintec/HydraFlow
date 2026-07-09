#include <HydraCore/Events/EventDispatcher.h>

#include <algorithm>

namespace Hydra {

Subscription EventDispatcher::SubscribeRaw(EventTypeId type, RawEventCallback callback, i32 priority)
{
    const ListenerId id    = m_NextListenerId.fetch_add(1, std::memory_order_relaxed);
    const u64         order = m_NextInsertionOrder.fetch_add(1, std::memory_order_relaxed);

    {
        std::lock_guard<std::mutex> lock(m_Mutex);
        auto& list = m_Listeners[type];
        list.push_back(EventListenerEntry{id, priority, order, std::move(callback)});

        // Stable sort by priority descending; ties keep insertion order
        // because insertionOrder is monotonically increasing and the sort
        // is stable with respect to equal keys.
        std::stable_sort(list.begin(), list.end(),
                          [](const EventListenerEntry& a, const EventListenerEntry& b)
                          {
                              return a.priority > b.priority;
                          });
    }

    return Subscription([this, type, id]() { Unsubscribe(type, id); });
}

void EventDispatcher::Unsubscribe(EventTypeId type, ListenerId id)
{
    std::lock_guard<std::mutex> lock(m_Mutex);
    auto it = m_Listeners.find(type);
    if (it == m_Listeners.end())
    {
        return;
    }

    auto& list = it->second;
    list.erase(std::remove_if(list.begin(), list.end(),
                               [id](const EventListenerEntry& e) { return e.id == id; }),
               list.end());
}

void EventDispatcher::UnsubscribeAll(EventTypeId type)
{
    std::lock_guard<std::mutex> lock(m_Mutex);
    m_Listeners.erase(type);
}

void EventDispatcher::Clear()
{
    std::lock_guard<std::mutex> lock(m_Mutex);
    m_Listeners.clear();
}

usize EventDispatcher::Dispatch(Event& event)
{
    // Snapshot the listener list under lock, then invoke callbacks outside
    // the lock so a listener is free to Subscribe/Unsubscribe (including
    // unsubscribing itself) without deadlocking or invalidating iterators.
    Vector<EventListenerEntry> snapshot;
    {
        std::lock_guard<std::mutex> lock(m_Mutex);
        auto it = m_Listeners.find(event.GetType());
        if (it == m_Listeners.end())
        {
            return 0;
        }
        snapshot = it->second;
    }

    usize invoked = 0;
    for (auto& entry : snapshot)
    {
        entry.callback(event);
        ++invoked;

        if (event.IsHandled() && event.StopsPropagationWhenHandled())
        {
            break;
        }
    }
    return invoked;
}

usize EventDispatcher::GetListenerCount(EventTypeId type) const
{
    std::lock_guard<std::mutex> lock(m_Mutex);
    auto it = m_Listeners.find(type);
    return it == m_Listeners.end() ? 0 : it->second.size();
}

usize EventDispatcher::GetTotalListenerCount() const
{
    std::lock_guard<std::mutex> lock(m_Mutex);
    usize total = 0;
    for (const auto& [type, list] : m_Listeners)
    {
        total += list.size();
    }
    return total;
}

} // namespace Hydra
