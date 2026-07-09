#include <HydraCore/Threading/ThreadPool.h>
#include <HydraCore/Threading/ThreadUtils.h>

namespace Hydra {

ThreadPool::ThreadPool(usize threadCount, StringView name)
    : m_Name(name)
{
    // 0 means "figure it out yourself" — use one worker per logical CPU
    // core so we can actually achieve CPU parallelism on this machine.
    const usize workerCount = (threadCount == 0) ? static_cast<usize>(Thread::GetHardwareConcurrency())
                                                  : threadCount;

    m_Workers.reserve(workerCount);
    for (usize i = 0; i < workerCount; ++i)
    {
        // Each worker gets a distinguishable name like "ThreadPool-2" so
        // it's obvious in a debugger/profiler which pool a given thread
        // belongs to.
        String workerName = m_Name + "-" + std::to_string(i);
        m_Workers.push_back(MakeUnique<WorkerThread>(workerName, [this] { WorkerLoop(); }));
    }
}

ThreadPool::~ThreadPool()
{
    Shutdown();
}

void ThreadPool::EnqueueTask(Task task)
{
    {
        ScopedLock<Mutex> lock(m_QueueMutex);
        m_Tasks.push(std::move(task));
    }
    // One new task means (at most) one sleeping worker needs to wake up
    // to claim it — NotifyOne() is cheaper than waking every worker just
    // to have all-but-one immediately go back to sleep.
    m_QueueCv.NotifyOne();
}

void ThreadPool::WorkerLoop()
{
    // Runs on a worker's dedicated OS thread (see WorkerThread) for as
    // long as the pool exists and hasn't been asked to stop.
    for (;;)
    {
        Task task;
        {
            UniqueLock<Mutex> lock(m_QueueMutex);

            // Sleep until there's either a task to do or we've been told
            // to stop — avoids burning CPU busy-polling an empty queue.
            m_QueueCv.Wait(lock, [this] { return m_Stopping.load(std::memory_order_acquire) || !m_Tasks.empty(); });

            // Even when stopping, keep draining whatever is already
            // queued so submitted work isn't silently dropped — only
            // exit once *both* conditions hold: stopping AND empty.
            if (m_Stopping.load(std::memory_order_acquire) && m_Tasks.empty())
            {
                return;
            }

            task = std::move(m_Tasks.front());
            m_Tasks.pop();
        }

        // Track "in-flight" tasks (as opposed to merely queued ones) so
        // WaitIdle() can tell the difference between "queue is empty" and
        // "queue is empty AND nothing is still running".
        m_ActiveTaskCount.fetch_add(1, std::memory_order_acq_rel);
        task();
        const usize remaining = m_ActiveTaskCount.fetch_sub(1, std::memory_order_acq_rel) - 1;

        // Only bother grabbing the idle lock/notifying if we might
        // actually have just become idle (cheap early-out for the
        // common case of a busy pool with more tasks still queued).
        if (remaining == 0)
        {
            ScopedLock<Mutex> idleLock(m_IdleMutex);
            m_IdleCv.NotifyAll();
        }
    }
}

void ThreadPool::WaitIdle()
{
    UniqueLock<Mutex> idleLock(m_IdleMutex);
    m_IdleCv.Wait(idleLock, [this]
    {
        // "Idle" means nothing is running AND nothing is left queued.
        // The queue check needs its own lock since it's a different
        // mutex than the idle one.
        if (m_ActiveTaskCount.load(std::memory_order_acquire) != 0)
        {
            return false;
        }
        ScopedLock<Mutex> queueLock(m_QueueMutex);
        return m_Tasks.empty();
    });
}

void ThreadPool::Shutdown()
{
    // Guard against double-shutdown (e.g. caller calls Shutdown()
    // explicitly and then the destructor calls it again) — without this,
    // the second call would try to join already-joined threads, which is
    // harmless here since WorkerThread::Join() is itself idempotent, but
    // we still want to avoid redundant notify storms.
    const bool alreadyStopping = m_Stopping.exchange(true, std::memory_order_acq_rel);
    if (alreadyStopping)
    {
        return;
    }

    // Wake every worker so they all notice m_Stopping and exit their wait
    // (each will still finish draining any tasks left in the queue
    // before actually returning — see the check in WorkerLoop()).
    m_QueueCv.NotifyAll();

    for (auto& worker : m_Workers)
    {
        worker->Join();
    }
}

usize ThreadPool::GetPendingTaskCount()
{
    ScopedLock<Mutex> lock(m_QueueMutex);
    return m_Tasks.size();
}

} // namespace Hydra
