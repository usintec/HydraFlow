#pragma once

// =============================================================================
// WorkerQueue.h
//
// The thread-safe, priority-ordered queue of "ready to run right now"
// jobs that JobScheduler's worker threads pull from. A job only ever
// enters this queue once every dependency it has is already complete
// (see DependencyGraph) — the queue itself has no concept of
// dependencies, it just hands out the highest-priority job available,
// oldest-first among equal priorities.
//
// This mirrors ThreadPool's internal task queue (Module 6) but is
// job-aware (priority ordering, closes cleanly on shutdown) and exposed
// as its own type per the Job System's design, since JobScheduler and
// tests both need to reason about "how many jobs are ready to run".
// =============================================================================

#include <HydraCore/Common/Platform.h>
#include <HydraCore/Common/Types.h>
#include <HydraCore/Common/NonCopyable.h>
#include <HydraCore/Threading/Mutex.h>
#include <HydraCore/Threading/ConditionVariable.h>
#include <HydraCore/Jobs/Job.h>

#include <queue>
#include <vector>

namespace Hydra {

class HYDRA_API WorkerQueue final : private NonCopyableNonMovable
{
public:
    WorkerQueue()  = default;
    ~WorkerQueue() = default;

    /// Adds a job that is ready to run. Wakes one worker thread blocked
    /// in WaitAndPop() (if any) to come pick it up.
    void Push(SharedPtr<Job> job);

    /// Blocks the calling thread until either a job is available (which
    /// it removes and returns) or the queue is Close()'d and drained, in
    /// which case it returns nullptr. This is how a JobScheduler worker
    /// thread sleeps instead of busy-polling when there's nothing to do.
    [[nodiscard]] SharedPtr<Job> WaitAndPop();

    /// Non-blocking variant: removes and returns the highest-priority
    /// job if one is immediately available, or nullptr if the queue is
    /// currently empty. Does not distinguish "empty" from "closed" —
    /// use WaitAndPop() if that distinction matters to you.
    [[nodiscard]] SharedPtr<Job> TryPop();

    /// Signals that no more jobs will ever be pushed and wakes every
    /// thread blocked in WaitAndPop() so they can notice and exit their
    /// wait loop. Jobs already in the queue are still returned by
    /// WaitAndPop()/TryPop() after Close() — only once the queue is both
    /// closed *and* empty does WaitAndPop() start returning nullptr.
    void Close();

    /// Snapshot of how many jobs are currently waiting to be picked up.
    /// Purely informational — the real count can change immediately
    /// after this returns.
    [[nodiscard]] usize Size() const;

private:
    // Wraps a job together with a monotonically increasing sequence
    // number purely so the priority_queue below can break ties between
    // equal-priority jobs in FIFO (insertion) order — std::priority_queue
    // is not stable on its own.
    struct Entry
    {
        SharedPtr<Job> job;
        u64            sequence;
    };

    // std::priority_queue is a max-heap by default using operator< —
    // we want the *highest* JobPriority popped first, and among equal
    // priorities the *lowest* sequence number (i.e. the oldest) popped
    // first. Comparator returns true when `lhs` should be considered
    // "less urgent" than `rhs` (i.e. sorted to come out later).
    struct EntryComparator
    {
        bool operator()(const Entry& lhs, const Entry& rhs) const noexcept
        {
            if (lhs.job->GetPriority() != rhs.job->GetPriority())
            {
                return lhs.job->GetPriority() < rhs.job->GetPriority();
            }
            // Equal priority: the entry with the *larger* sequence
            // number arrived later, so it should be considered "less
            // urgent" (come out of the heap after the older one).
            return lhs.sequence > rhs.sequence;
        }
    };

    mutable Mutex     m_Mutex;
    ConditionVariable m_Cv;
    std::priority_queue<Entry, std::vector<Entry>, EntryComparator> m_Queue;
    std::atomic<u64>  m_NextSequence{0};
    bool              m_Closed = false;
};

} // namespace Hydra
