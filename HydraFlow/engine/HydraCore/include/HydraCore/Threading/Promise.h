#pragma once

// =============================================================================
// Promise.h
//
// Promise<T> is the "write side" of a Promise<T>/Future<T> pair: whoever
// is doing the actual work (e.g. code running inside a ThreadPool worker)
// calls SetValue() or SetException() exactly once when it's done, and
// whoever is waiting for the result (holding the matching Future<T> from
// GetFuture()) wakes up and receives it.
//
// You will rarely construct a Promise by hand outside of ThreadPool's own
// implementation — normally you just call pool.Submit(...) and get a
// Future back. Promise is exposed publicly because it's still a useful,
// general building block for "produce a result on one thread, consume it
// on another" outside of the pool (e.g. bridging a callback-based API
// into something you can Wait()/Get() on).
// =============================================================================

#include <HydraCore/Common/Types.h>
#include <HydraCore/Threading/Mutex.h>
#include <HydraCore/Threading/SharedState.h>
#include <HydraCore/Threading/Future.h>

#include <utility>

namespace Hydra {

// =============================================================================
// Promise<T> — primary template.
// =============================================================================
template<typename T>
class Promise
{
public:
    /// Creates a brand-new, unfulfilled promise with its own shared state.
    Promise() : m_State(MakeShared<detail::SharedState<T>>()) {}

    // Promises are move-only for the same reason Futures are: only one
    // "producer" should ever be responsible for fulfilling a given
    // shared state.
    Promise(const Promise&)            = delete;
    Promise& operator=(const Promise&) = delete;
    Promise(Promise&&)                 = default;
    Promise& operator=(Promise&&)      = default;

    /// Returns the Future<T> that reads whatever this Promise eventually
    /// writes. May be called any number of times (each call shares the
    /// same underlying state) — though typically you only need it once.
    [[nodiscard]] Future<T> GetFuture() const { return Future<T>(m_State); }

    /// Fulfills the promise with a successful result. Wakes any thread
    /// blocked in the matching Future's Wait()/WaitFor()/Get().
    /// Calling this more than once on the same Promise is a programming
    /// error (asserted in debug builds).
    void SetValue(T value)
    {
        HYDRA_ASSERT(m_State && "SetValue() called on a moved-from Promise");
        {
            ScopedLock<Mutex> lock(m_State->mutex);
            HYDRA_ASSERT(!m_State->ready && "Promise fulfilled more than once");
            m_State->value = std::move(value);
            m_State->ready = true;
        }
        // Notify outside the lock: it's not strictly required for
        // correctness with condition_variable_any, but avoids waking a
        // thread just to have it immediately block again on the mutex we
        // are still holding.
        m_State->cv.NotifyAll();
    }

    /// Fulfills the promise with a failure. The matching Future's Get()
    /// will re-throw this exception instead of returning a value.
    void SetException(std::exception_ptr exception)
    {
        HYDRA_ASSERT(m_State && "SetException() called on a moved-from Promise");
        {
            ScopedLock<Mutex> lock(m_State->mutex);
            HYDRA_ASSERT(!m_State->ready && "Promise fulfilled more than once");
            m_State->exception = std::move(exception);
            m_State->ready     = true;
        }
        m_State->cv.NotifyAll();
    }

private:
    SharedPtr<detail::SharedState<T>> m_State;
};

// =============================================================================
// Promise<void> — specialization for tasks with no return value.
// =============================================================================
template<>
class Promise<void>
{
public:
    Promise() : m_State(MakeShared<detail::SharedState<void>>()) {}

    Promise(const Promise&)            = delete;
    Promise& operator=(const Promise&) = delete;
    Promise(Promise&&)                 = default;
    Promise& operator=(Promise&&)      = default;

    [[nodiscard]] Future<void> GetFuture() const { return Future<void>(m_State); }

    /// Marks the task as successfully completed (nothing to store).
    void SetValue()
    {
        HYDRA_ASSERT(m_State && "SetValue() called on a moved-from Promise<void>");
        {
            ScopedLock<Mutex> lock(m_State->mutex);
            HYDRA_ASSERT(!m_State->ready && "Promise<void> fulfilled more than once");
            m_State->ready = true;
        }
        m_State->cv.NotifyAll();
    }

    void SetException(std::exception_ptr exception)
    {
        HYDRA_ASSERT(m_State && "SetException() called on a moved-from Promise<void>");
        {
            ScopedLock<Mutex> lock(m_State->mutex);
            HYDRA_ASSERT(!m_State->ready && "Promise<void> fulfilled more than once");
            m_State->exception = std::move(exception);
            m_State->ready     = true;
        }
        m_State->cv.NotifyAll();
    }

private:
    SharedPtr<detail::SharedState<void>> m_State;
};

} // namespace Hydra
