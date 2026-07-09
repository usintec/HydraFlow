#include <HydraCore/Events/EventBus.h>

namespace Hydra {

usize EventBus::ProcessQueue(usize maxEvents)
{
    return m_Queue.ProcessUpTo(m_Dispatcher, maxEvents);
}

usize EventBus::GetPendingCount() const
{
    return m_Queue.GetPendingCount();
}

usize EventBus::GetListenerCount(EventTypeId type) const
{
    return m_Dispatcher.GetListenerCount(type);
}

void EventBus::UnsubscribeAll(EventTypeId type)
{
    m_Dispatcher.UnsubscribeAll(type);
}

void EventBus::ClearListeners()
{
    m_Dispatcher.Clear();
}

void EventBus::ClearQueue()
{
    m_Queue.Clear();
}

} // namespace Hydra
