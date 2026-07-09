#pragma once

// =============================================================================
// ConditionVariable.h
//
// Wraps std::condition_variable_any so it works with Hydra::Mutex (and
// Hydra::RecursiveMutex) out of the box.
//
// We deliberately use condition_variable_any instead of the plain
// condition_variable: the plain version only works with std::unique_lock
// bound to *exactly* std::mutex, but Hydra::Mutex is our own class (see
// Mutex.h). condition_variable_any is a little slower in the rare
// high-contention benchmark, but it accepts *any* type satisfying the
// BasicLockable concept, so ThreadPool/WorkerThread can wait on
// Hydra::Mutex without extra glue code.
//
// A condition variable is how one thread says "wake me up when something
// changes" and another thread says "something changed, wake everyone
// waiting" — used here so ThreadPool worker threads can sleep instead of
// spin-checking an empty task queue.
// =============================================================================

#include <HydraCore/Common/Platform.h>
#include <HydraCore/Common/Types.h>
#include <HydraCore/Common/NonCopyable.h>

#include <condition_variable>
#include <chrono>

namespace Hydra {

class HYDRA_API ConditionVariable : private NonCopyableNonMovable
{
public:
    ConditionVariable()  = default;
    ~ConditionVariable() = default;

    /// Wakes exactly one thread currently blocked in Wait()/WaitFor()
    /// (if any). Use when only one waiter needs to react (e.g. "one new
    /// task was added, one idle worker should pick it up").
    void NotifyOne() noexcept { m_Cv.notify_one(); }

    /// Wakes every thread currently blocked in Wait()/WaitFor(). Use for
    /// "state changed in a way that could matter to everyone waiting"
    /// (e.g. "shutdown requested" — every worker needs to notice).
    void NotifyAll() noexcept { m_Cv.notify_all(); }

    /// Atomically unlocks `lock` and blocks the calling thread until
    /// notified. Re-acquires `lock` before returning. `lock` must already
    /// be held by the calling thread.
    template<typename LockType>
    void Wait(LockType& lock)
    {
        m_Cv.wait(lock);
    }

    /// Same as Wait(lock), but keeps re-waiting until `predicate()`
    /// returns true — this is the recommended way to use a condition
    /// variable, because it protects against spurious wakeups (the OS is
    /// allowed to wake a waiting thread even when nobody called Notify*).
    template<typename LockType, typename Predicate>
    void Wait(LockType& lock, Predicate predicate)
    {
        m_Cv.wait(lock, predicate);
    }

    /// Like Wait(lock), but gives up and returns false after `milliseconds`
    /// if no notification arrives in time. Returns true if woken by a
    /// notification (or if it happened to already be true), false on
    /// timeout.
    template<typename LockType>
    [[nodiscard]] bool WaitFor(LockType& lock, u64 milliseconds)
    {
        return m_Cv.wait_for(lock, std::chrono::milliseconds(milliseconds)) == std::cv_status::no_timeout;
    }

    /// Predicate + timeout combined: keeps waiting (re-checking predicate
    /// on every wakeup) until either the predicate becomes true or the
    /// timeout elapses. Returns the final value of predicate() — i.e.
    /// true means "condition met", false means "gave up after timeout".
    template<typename LockType, typename Predicate>
    [[nodiscard]] bool WaitFor(LockType& lock, u64 milliseconds, Predicate predicate)
    {
        return m_Cv.wait_for(lock, std::chrono::milliseconds(milliseconds), predicate);
    }

private:
    std::condition_variable_any m_Cv;
};

} // namespace Hydra
