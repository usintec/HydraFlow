#pragma once

// =============================================================================
// Semaphore.h
//
// Wraps C++20's std::counting_semaphore so the rest of HydraCore uses
// consistent Hydra:: naming instead of reaching into <semaphore> directly.
//
// A semaphore is a counter that threads can Acquire() (decrement, blocking
// if the count is already zero) and Release() (increment, waking a waiter
// if one is blocked). Unlike a mutex, a semaphore isn't "owned" by the
// thread that decremented it — any thread may release it. This makes
// semaphores the right tool for "N slots available" style problems, e.g.
// limiting how many worker threads may access a resource pool at once, or
// signaling "N items are now available to consume" between producer and
// consumer threads.
// =============================================================================

#include <HydraCore/Common/Platform.h>
#include <HydraCore/Common/Types.h>
#include <HydraCore/Common/NonCopyable.h>

#include <semaphore>
#include <chrono>
#include <cstddef>
#include <limits>

namespace Hydra {

// =============================================================================
// CountingSemaphore<LeastMaxValue>
//
// A semaphore whose internal counter may range from 0 up to at least
// LeastMaxValue (the exact ceiling is implementation-defined, matching
// std::counting_semaphore's own guarantee).
// =============================================================================
template<isize LeastMaxValue = std::numeric_limits<i32>::max()>
class CountingSemaphore : private NonCopyableNonMovable
{
public:
    /// Constructs the semaphore with `initialCount` permits already
    /// available (i.e. that many Acquire() calls can succeed immediately
    /// without blocking).
    explicit CountingSemaphore(isize initialCount = 0)
        : m_Semaphore(initialCount)
    {
    }

    ~CountingSemaphore() = default;

    /// Blocks the calling thread until a permit is available, then takes
    /// one (decrementing the internal counter).
    void Acquire() { m_Semaphore.acquire(); }

    /// Takes a permit only if one is immediately available, without
    /// blocking. Returns true if a permit was taken, false otherwise.
    [[nodiscard]] bool TryAcquire() { return m_Semaphore.try_acquire(); }

    /// Like TryAcquire(), but will wait up to `milliseconds` for a permit
    /// to become available before giving up. Returns true if a permit was
    /// taken, false on timeout.
    [[nodiscard]] bool TryAcquireFor(u64 milliseconds)
    {
        return m_Semaphore.try_acquire_for(std::chrono::milliseconds(milliseconds));
    }

    /// Returns `count` permits to the semaphore (default 1), waking up to
    /// `count` blocked waiters if any are waiting in Acquire().
    void Release(isize count = 1) { m_Semaphore.release(count); }

private:
    std::counting_semaphore<LeastMaxValue> m_Semaphore;
};

/// General-purpose semaphore with a very high permit ceiling — the
/// default choice when you just need "a counter threads can wait on".
using Semaphore = CountingSemaphore<>;

/// A semaphore restricted to 0 or 1 permits. Behaves like a lightweight,
/// non-recursive signal/flag between two threads (e.g. "producer thread,
/// tell the consumer thread exactly one item is ready").
using BinarySemaphore = CountingSemaphore<1>;

} // namespace Hydra
