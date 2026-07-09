#pragma once

// =============================================================================
// Mutex.h
//
// Thin HydraCore wrappers around std::mutex / std::recursive_mutex.
//
// Why wrap the standard mutex at all instead of using std::mutex directly?
//   1. Consistency: every HydraCore subsystem includes <HydraCore/...> types
//      rather than reaching into <mutex> directly, which keeps the public
//      API surface uniform and gives us one place to add instrumentation
//      later (e.g. contention counters, deadlock detection) without
//      touching every call site.
//   2. Safety: NonCopyableNonMovable is baked in, so a Mutex can never be
//      accidentally copied into another object (a classic bug that silently
//      creates two "independent" locks guarding the same data).
//
// NOTE ON NAMING: lock()/unlock()/try_lock() are deliberately lowercase
// (breaking HydraCore's usual PascalCase convention) because that is the
// exact spelling the C++ standard library requires for the "BasicLockable"
// / "Lockable" concepts. Using these lowercase names means Hydra::Mutex
// and Hydra::RecursiveMutex work transparently with std::lock_guard,
// std::unique_lock, std::scoped_lock, and Hydra::ConditionVariable below —
// no adapter code needed.
// =============================================================================

#include <HydraCore/Common/Platform.h>
#include <HydraCore/Common/NonCopyable.h>

#include <mutex>

namespace Hydra {

// =============================================================================
// Mutex — non-recursive mutual exclusion lock.
//
// Use this for the common case: a lock that is acquired and released
// within a single call stack, never re-entered by the same thread while
// already held (re-entering would deadlock — use RecursiveMutex if you
// genuinely need re-entrancy).
// =============================================================================
class HYDRA_API Mutex : private NonCopyableNonMovable
{
public:
    Mutex()  = default;
    ~Mutex() = default;

    /// Blocks the calling thread until the lock is acquired.
    void lock() { m_Mutex.lock(); }

    /// Attempts to acquire the lock without blocking.
    /// Returns true if the lock was acquired, false if it was already held.
    [[nodiscard]] bool try_lock() { return m_Mutex.try_lock(); }

    /// Releases the lock. Undefined behaviour if the calling thread does
    /// not currently hold it (same rule as std::mutex).
    void unlock() { m_Mutex.unlock(); }

    /// Escape hatch for code that needs the raw std::mutex directly
    /// (e.g. to pass to a third-party API that expects one explicitly).
    [[nodiscard]] std::mutex& Native() noexcept { return m_Mutex; }

private:
    std::mutex m_Mutex;
};

// =============================================================================
// RecursiveMutex — mutual exclusion lock that the *same* thread may
// re-acquire multiple times without deadlocking (it must release it the
// same number of times before another thread can acquire it).
//
// Prefer plain Mutex when possible — recursive locking is usually a sign
// that a function isn't sure whether it's already holding the lock, which
// often points to a design that could be simplified. Reach for this when
// you genuinely need it (e.g. a class whose public methods call each
// other and each independently locks for safety).
// =============================================================================
class HYDRA_API RecursiveMutex : private NonCopyableNonMovable
{
public:
    RecursiveMutex()  = default;
    ~RecursiveMutex() = default;

    void lock() { m_Mutex.lock(); }
    [[nodiscard]] bool try_lock() { return m_Mutex.try_lock(); }
    void unlock() { m_Mutex.unlock(); }

    [[nodiscard]] std::recursive_mutex& Native() noexcept { return m_Mutex; }

private:
    std::recursive_mutex m_Mutex;
};

// =============================================================================
// Convenience RAII aliases
//
//   ScopedLock<MutexType>  — locks in the constructor, unlocks in the
//                            destructor. Use for the common "lock for the
//                            rest of this scope" pattern.
//   UniqueLock<MutexType>  — like ScopedLock but movable and unlockable
//                            early; required by Hydra::ConditionVariable's
//                            Wait()/WaitFor(), which need to temporarily
//                            release the lock while waiting.
// =============================================================================

template<typename MutexType>
using ScopedLock = std::lock_guard<MutexType>;

template<typename MutexType>
using UniqueLock = std::unique_lock<MutexType>;

} // namespace Hydra
