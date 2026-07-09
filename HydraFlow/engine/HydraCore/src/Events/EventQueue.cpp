#include <HydraCore/Events/EventQueue.h>

#include <algorithm>

namespace Hydra {

void EventQueue::Enqueue(UniquePtr<Event> event)
{
    if (!event)
    {
        return;
    }
    std::lock_guard<std::mutex> lock(m_Mutex);
    m_Queue.push_back(std::move(event));
}

usize EventQueue::ProcessAll(EventDispatcher& dispatcher)
{
    return ProcessUpTo(dispatcher, 0);
}

usize EventQueue::ProcessUpTo(EventDispatcher& dispatcher, usize maxEvents)
{
    // Pull out only what's currently pending so events enqueued by a
    // listener during dispatch are deferred to the next call.
    std::deque<UniquePtr<Event>> batch;
    {
        std::lock_guard<std::mutex> lock(m_Mutex);
        const usize count = (maxEvents == 0) ? m_Queue.size() : std::min(maxEvents, m_Queue.size());
        for (usize i = 0; i < count; ++i)
        {
            batch.push_back(std::move(m_Queue.front()));
            m_Queue.pop_front();
        }
    }

    usize processed = 0;
    for (auto& event : batch)
    {
        dispatcher.Dispatch(*event);
        ++processed;
    }
    return processed;
}

usize EventQueue::GetPendingCount() const
{
    std::lock_guard<std::mutex> lock(m_Mutex);
    return m_Queue.size();
}

void EventQueue::Clear()
{
    std::lock_guard<std::mutex> lock(m_Mutex);
    m_Queue.clear();
}

} // namespace Hydra
