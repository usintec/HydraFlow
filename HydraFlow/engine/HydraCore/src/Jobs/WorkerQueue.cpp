#include <HydraCore/Jobs/WorkerQueue.h>
#include <cstdio>

namespace Hydra {

void WorkerQueue::Push(SharedPtr<Job> job)
{
    {
        ScopedLock<Mutex> lock(m_Mutex);
        const u64 sequence = m_NextSequence.fetch_add(1, std::memory_order_relaxed);
        fprintf(stderr, "[DBG] Push job id=%llu seq=%llu\n", (unsigned long long)job->GetId(), (unsigned long long)sequence);
        m_Queue.push(Entry{std::move(job), sequence});
    }
    // One new ready job means at most one sleeping worker needs waking.
    m_Cv.NotifyOne();
}

SharedPtr<Job> WorkerQueue::WaitAndPop()
{
    UniqueLock<Mutex> lock(m_Mutex);

    // Sleep until there's something to hand out or we've been told to
    // shut down (and there's genuinely nothing left to drain).
    m_Cv.Wait(lock, [this] { return m_Closed || !m_Queue.empty(); });

    if (m_Queue.empty())
    {
        // Only reachable when m_Closed is true and the queue has been
        // fully drained — tell the caller (a worker thread) it's time
        // to stop.
        return nullptr;
    }

    // priority_queue only exposes const access to its top element, so we
    // copy the shared_ptr out before popping — cheap (just a refcount
    // bump), and avoids needing const_cast gymnastics.
    SharedPtr<Job> job = m_Queue.top().job;
    m_Queue.pop();
    return job;
}

SharedPtr<Job> WorkerQueue::TryPop()
{
    ScopedLock<Mutex> lock(m_Mutex);
    if (m_Queue.empty())
    {
        return nullptr;
    }
    SharedPtr<Job> job = m_Queue.top().job;
    m_Queue.pop();
    return job;
}

void WorkerQueue::Close()
{
    {
        ScopedLock<Mutex> lock(m_Mutex);
        m_Closed = true;
    }
    // Every worker sleeping in WaitAndPop() needs to wake up and notice
    // the shutdown, not just one — hence NotifyAll rather than NotifyOne.
    m_Cv.NotifyAll();
}

usize WorkerQueue::Size() const
{
    ScopedLock<Mutex> lock(m_Mutex);
    return m_Queue.size();
}

} // namespace Hydra
