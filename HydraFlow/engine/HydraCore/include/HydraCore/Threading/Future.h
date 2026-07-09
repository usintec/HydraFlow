#pragma once

// =============================================================================
// Future.h
//
// Future<T> is the "read side" of a Promise<T>/Future<T> pair: a handle
// that lets one thread wait for, and retrieve, a result that another
// thread (or a ThreadPool worker) will eventually produce.
//
// Typical usage (usually you get one of these back from
// ThreadPool::Submit() rather than constructing it directly):
//
//     Future<int> f = pool.Submit([]{ return 42; });
//     ...
//     int result = f.Get();   // blocks until the task finishes
//
// A Future is single-use: once Get() has been called, the value has been
// moved out and calling Get() again is not meaningful (this mirrors
// std::future's contract).
// =============================================================================

#include <HydraCore/Common/Types.h>
#include <HydraCore/Threading/Mutex.h>
#include <HydraCore/Threading/SharedState.h>

#include <utility>

namespace Hydra {

// =============================================================================
// Future<T> — primary template, for tasks that produce a value of type T.
// =============================================================================
template<typename T>
class Future
{
public:
    /// Default-constructed Future is "empty" — not attached to any
    /// Promise. Calling any method other than IsValid() on it is a bug
    /// (asserted in debug builds via the shared-state null check).
    Future() = default;

    // Futures are move-only: copying would let two callers both try to
    // "consume" the same single-use result, which makes no sense.
    Future(const Future&)            = delete;
    Future& operator=(const Future&) = delete;
    Future(Future&&)                 = default;
    Future& operator=(Future&&)      = default;

    /// True if this Future is attached to a Promise's shared state
    /// (i.e. it wasn't default-constructed and hasn't been moved-from).
    [[nodiscard]] bool IsValid() const noexcept { return static_cast<bool>(m_State); }

    /// Non-blocking check: has the producer called SetValue()/SetException() yet?
    [[nodiscard]] bool IsReady() const
    {
        HYDRA_ASSERT(m_State && "IsReady() called on an empty Future");
        ScopedLock<Mutex> lock(m_State->mutex);
        return m_State->ready;
    }

    /// Blocks the calling thread until the result is ready. Does not
    /// consume/return the value — use Get() for that. Useful when you
    /// only care "has it finished" without needing the value itself.
    void Wait() const
    {
        HYDRA_ASSERT(m_State && "Wait() called on an empty Future");
        UniqueLock<Mutex> lock(m_State->mutex);
        m_State->cv.Wait(lock, [this] { return m_State->ready; });
    }

    /// Like Wait(), but gives up after `milliseconds`. Returns true if the
    /// result became ready in time, false on timeout.
    [[nodiscard]] bool WaitFor(u64 milliseconds) const
    {
        HYDRA_ASSERT(m_State && "WaitFor() called on an empty Future");
        UniqueLock<Mutex> lock(m_State->mutex);
        return m_State->cv.WaitFor(lock, milliseconds, [this] { return m_State->ready; });
    }

    /// Blocks until ready, then returns the value (moved out — a Future
    /// is single-use) or re-throws whatever exception the task threw.
    [[nodiscard]] T Get()
    {
        HYDRA_ASSERT(m_State && "Get() called on an empty Future");
        UniqueLock<Mutex> lock(m_State->mutex);
        m_State->cv.Wait(lock, [this] { return m_State->ready; });

        if (m_State->exception)
        {
            std::rethrow_exception(m_State->exception);
        }
        // Moving out of Optional<T> leaves the Future's copy of the state
        // empty; that's fine since Get() is meant to be called once.
        return std::move(*m_State->value);
    }

private:
    // Only Promise<T> may construct a "real" (non-empty) Future, via
    // GetFuture() — this keeps the wiring between the two classes
    // explicit and prevents users from fabricating a Future that isn't
    // backed by any Promise.
    template<typename U> friend class Promise;
    explicit Future(SharedPtr<detail::SharedState<T>> state) : m_State(std::move(state)) {}

    SharedPtr<detail::SharedState<T>> m_State;
};

// =============================================================================
// Future<void> — specialization for tasks that don't produce a value,
// only "did it complete" / "did it throw".
// =============================================================================
template<>
class Future<void>
{
public:
    Future() = default;

    Future(const Future&)            = delete;
    Future& operator=(const Future&) = delete;
    Future(Future&&)                 = default;
    Future& operator=(Future&&)      = default;

    [[nodiscard]] bool IsValid() const noexcept { return static_cast<bool>(m_State); }

    [[nodiscard]] bool IsReady() const
    {
        HYDRA_ASSERT(m_State && "IsReady() called on an empty Future<void>");
        ScopedLock<Mutex> lock(m_State->mutex);
        return m_State->ready;
    }

    void Wait() const
    {
        HYDRA_ASSERT(m_State && "Wait() called on an empty Future<void>");
        UniqueLock<Mutex> lock(m_State->mutex);
        m_State->cv.Wait(lock, [this] { return m_State->ready; });
    }

    [[nodiscard]] bool WaitFor(u64 milliseconds) const
    {
        HYDRA_ASSERT(m_State && "WaitFor() called on an empty Future<void>");
        UniqueLock<Mutex> lock(m_State->mutex);
        return m_State->cv.WaitFor(lock, milliseconds, [this] { return m_State->ready; });
    }

    /// Blocks until the task finishes; re-throws its exception if it
    /// threw one. Returns nothing on success (there's no value for void).
    void Get()
    {
        HYDRA_ASSERT(m_State && "Get() called on an empty Future<void>");
        UniqueLock<Mutex> lock(m_State->mutex);
        m_State->cv.Wait(lock, [this] { return m_State->ready; });

        if (m_State->exception)
        {
            std::rethrow_exception(m_State->exception);
        }
    }

private:
    template<typename U> friend class Promise;
    explicit Future(SharedPtr<detail::SharedState<void>> state) : m_State(std::move(state)) {}

    SharedPtr<detail::SharedState<void>> m_State;
};

} // namespace Hydra
