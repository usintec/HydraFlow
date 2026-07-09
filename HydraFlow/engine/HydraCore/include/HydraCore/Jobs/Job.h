#pragma once

// =============================================================================
// Job.h
//
// A Job is a single unit of work: a name (for debugging), a priority (for
// scheduling order), a "kind" (CPU today, GPU reserved for later), and the
// callable that actually does the work. It also owns its own completion
// signal (a Mutex + ConditionVariable pair) so a JobHandle can efficiently
// Wait() for it without polling.
//
// Jobs are almost always created *for* you by JobScheduler::Schedule() or
// by translating a TaskGraph — you rarely construct one directly, but the
// type is public because JobHandle, WorkerQueue, and DependencyGraph all
// need to see it.
// =============================================================================

#include <HydraCore/Common/Platform.h>
#include <HydraCore/Common/Types.h>
#include <HydraCore/Common/NonCopyable.h>
#include <HydraCore/Threading/Mutex.h>
#include <HydraCore/Threading/ConditionVariable.h>
#include <HydraCore/Jobs/JobTypes.h>

#include <atomic>
#include <exception>

namespace Hydra {

class HYDRA_API Job final : private NonCopyableNonMovable
{
public:
    /// The work a Job performs. Takes no arguments and returns nothing —
    /// any inputs/outputs should be captured by the lambda/closure
    /// (e.g. by reference into data the caller keeps alive, or by
    /// pointer/shared_ptr for longer-lived state).
    using JobFunction = Function<void()>;

    Job(JobId id, StringView name, JobFunction function, JobPriority priority, JobKind kind)
        : m_Id(id)
        , m_Name(name)
        , m_Function(std::move(function))
        , m_Priority(priority)
        , m_Kind(kind)
    {
    }

    ~Job() = default;

    [[nodiscard]] JobId GetId() const noexcept { return m_Id; }
    [[nodiscard]] const String& GetName() const noexcept { return m_Name; }
    [[nodiscard]] JobPriority GetPriority() const noexcept { return m_Priority; }
    [[nodiscard]] JobKind GetKind() const noexcept { return m_Kind; }

    [[nodiscard]] JobState GetState() const noexcept { return m_State.load(std::memory_order_acquire); }
    [[nodiscard]] bool IsComplete() const noexcept
    {
        const JobState state = GetState();
        return state == JobState::Completed || state == JobState::Failed;
    }
    [[nodiscard]] bool HasFailed() const noexcept { return GetState() == JobState::Failed; }

    /// The exception captured if this job failed, or null if it
    /// succeeded (or hasn't finished yet). Safe to call any time; only
    /// meaningful once IsComplete() is true.
    [[nodiscard]] std::exception_ptr GetException() const
    {
        ScopedLock<Mutex> lock(m_CompletionMutex);
        return m_Exception;
    }

    /// Runs the job's function on whichever thread calls this (normally
    /// a JobScheduler worker thread), catching any exception instead of
    /// letting it escape and crash that worker. Marks the job Running
    /// before the call and Completed/Failed after, waking anyone blocked
    /// in Wait()/WaitFor().
    ///
    /// The scheduler — not the job itself — is responsible for calling
    /// this only once all of the job's dependencies have finished; Job
    /// has no idea what its dependencies are (that bookkeeping lives in
    /// DependencyGraph).
    void Execute()
    {
        m_State.store(JobState::Running, std::memory_order_release);

        std::exception_ptr caught;
        try
        {
            m_Function();
        }
        catch (...)
        {
            caught = std::current_exception();
        }

        {
            ScopedLock<Mutex> lock(m_CompletionMutex);
            m_Exception = caught;
            m_State.store(caught ? JobState::Failed : JobState::Completed, std::memory_order_release);
        }
        // Wake outside the lock so a waiting thread doesn't immediately
        // re-block on a mutex we're still holding.
        m_CompletionCv.NotifyAll();
    }

    /// Marks the job as Failed *without* running its function — used by
    /// JobScheduler when a job can't legitimately be executed at all
    /// (e.g. a GPU-kind job submitted with no GPU executor registered).
    /// Wakes anyone blocked in Wait()/WaitFor(), same as a normal
    /// Execute() failure would.
    void FailWithoutRunning(std::exception_ptr exception)
    {
        {
            ScopedLock<Mutex> lock(m_CompletionMutex);
            m_Exception = std::move(exception);
            m_State.store(JobState::Failed, std::memory_order_release);
        }
        m_CompletionCv.NotifyAll();
    }

    /// Blocks the calling thread until this job has finished (either
    /// Completed or Failed). Does not re-throw — use GetException()/
    /// HasFailed() after Wait() returns if you need to react to failure.
    void Wait() const
    {
        UniqueLock<Mutex> lock(m_CompletionMutex);
        m_CompletionCv.Wait(lock, [this] { return IsComplete(); });
    }

    /// Like Wait(), but gives up after `milliseconds`. Returns true if
    /// the job finished in time, false on timeout.
    [[nodiscard]] bool WaitFor(u64 milliseconds) const
    {
        UniqueLock<Mutex> lock(m_CompletionMutex);
        return m_CompletionCv.WaitFor(lock, milliseconds, [this] { return IsComplete(); });
    }

    // -------------------------------------------------------------------
    // Dependency bookkeeping (used exclusively by DependencyGraph /
    // JobScheduler — not part of the "public" Job API a job author would
    // use). Kept here rather than in DependencyGraph itself because each
    // Job needs exactly one counter and it's simpler for the scheduler to
    // decrement it directly on the Job than to look it up in a separate
    // map on every completion.
    // -------------------------------------------------------------------

    /// Number of not-yet-completed dependencies this job is still
    /// waiting on. JobScheduler decrements this as dependencies finish;
    /// the job becomes Ready (eligible to run) once it reaches zero.
    [[nodiscard]] std::atomic<u32>& RemainingDependencies() noexcept { return m_RemainingDependencies; }

private:
    JobId       m_Id;
    String      m_Name;
    JobFunction m_Function;
    JobPriority m_Priority;
    JobKind     m_Kind;

    std::atomic<JobState> m_State{JobState::Pending};
    std::atomic<u32>      m_RemainingDependencies{0};

    // Guards m_Exception and pairs with m_CompletionCv so Wait()/WaitFor()
    // can block efficiently instead of spin-polling GetState().
    mutable Mutex             m_CompletionMutex;
    mutable ConditionVariable m_CompletionCv;
    std::exception_ptr        m_Exception;
};

} // namespace Hydra
