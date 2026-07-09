#pragma once

// =============================================================================
// SharedState.h  (internal detail — not part of the public API surface)
//
// The piece of memory a Promise<T> writes into and a Future<T> reads from.
// Both Promise and Future hold a SharedPtr to the *same* SharedState<T>
// instance, so:
//
//   Promise::SetValue(x)  -->  writes x into the shared state, marks it
//                              "ready", and wakes anyone blocked in
//                              Future::Get()/Wait().
//
//   Future::Get()          -->  blocks (via the condition variable) until
//                              the shared state becomes ready, then
//                              returns the value (or re-throws the stored
//                              exception, if SetException() was used
//                              instead of SetValue()).
//
// This mirrors how std::promise/std::future work internally, but is
// implemented from scratch here (rather than aliasing std::promise) so
// HydraCore's Future/Promise can add engine-specific conveniences like
// IsReady() without blocking, and so the whole Threading module is
// self-consistent (built on Hydra::Mutex/Hydra::ConditionVariable).
// =============================================================================

#include <HydraCore/Common/Types.h>
#include <HydraCore/Threading/Mutex.h>
#include <HydraCore/Threading/ConditionVariable.h>

#include <exception>
#include <optional>
#include <utility>

namespace Hydra::detail {

// -----------------------------------------------------------------------
// Primary template — shared state for Future<T>/Promise<T> where T is a
// real value type.
// -----------------------------------------------------------------------
template<typename T>
struct SharedState
{
    Mutex             mutex;      ///< Guards every field below.
    ConditionVariable cv;         ///< Signaled when `ready` becomes true.
    Optional<T>       value;      ///< The result, once SetValue() is called.
    std::exception_ptr exception; ///< The error, if SetException() is called instead.
    bool              ready = false; ///< True once value or exception is set.
};

// -----------------------------------------------------------------------
// Specialization for T = void — there is no value to store, only "did it
// finish" and "did it throw". Kept as a separate specialization rather
// than Optional<void> (which isn't a valid type) to keep Future<void>
// ergonomic (Get() just waits and rethrows, returning nothing).
// -----------------------------------------------------------------------
template<>
struct SharedState<void>
{
    Mutex               mutex;
    ConditionVariable   cv;
    std::exception_ptr  exception;
    bool                ready = false;
};

} // namespace Hydra::detail
