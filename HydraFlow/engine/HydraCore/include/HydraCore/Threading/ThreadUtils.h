#pragma once

// =============================================================================
// ThreadUtils.h
//
// Small collection of free functions for working with the *current* thread:
// naming it (for debuggers/profilers), reading its id, sleeping, yielding,
// and asking the OS how many hardware threads are available (used by
// ThreadPool to size itself for CPU parallelism).
//
// These are plain functions (not a class) because they operate on
// "whichever thread called them" — there is no object to own.
// =============================================================================

#include <HydraCore/Common/Platform.h>
#include <HydraCore/Common/Types.h>

namespace Hydra::Thread {

// -----------------------------------------------------------------------
// GetHardwareConcurrency
//
// Returns the number of concurrent threads the hardware can *usefully*
// run (roughly: logical CPU core count). ThreadPool uses this to decide
// how many worker threads to spawn when the caller doesn't specify a
// count explicitly. Falls back to 1 if the OS can't tell us (rare, but
// std::thread::hardware_concurrency() is technically allowed to return 0).
// -----------------------------------------------------------------------
[[nodiscard]] HYDRA_API u32 GetHardwareConcurrency() noexcept;

// -----------------------------------------------------------------------
// SetCurrentThreadName / GetCurrentThreadName
//
// Gives the *calling* thread a human-readable name. This shows up in
// debuggers (gdb "info threads"), profilers, and tools like `htop -H`,
// which makes multi-threaded debugging far less painful than staring at
// numeric thread ids. On Linux this is implemented with pthread's
// name API; the name is silently truncated to 15 characters because
// that's the kernel-imposed limit (TASK_COMM_LEN - 1).
// -----------------------------------------------------------------------
HYDRA_API void SetCurrentThreadName(StringView name);
[[nodiscard]] HYDRA_API String GetCurrentThreadName();

// -----------------------------------------------------------------------
// GetCurrentThreadId
//
// Returns a process-wide-unique numeric id for the calling thread.
// std::thread::id is not an integer by design, so we hash it down to a
// u64 — good enough for logging ("[thread 140234] ...") and equality
// comparisons, not guaranteed stable across runs or platforms.
// -----------------------------------------------------------------------
[[nodiscard]] HYDRA_API u64 GetCurrentThreadId() noexcept;

// -----------------------------------------------------------------------
// SleepForMilliseconds / SleepForMicroseconds
//
// Thin wrappers over std::this_thread::sleep_for so call sites don't need
// to spell out <chrono> durations everywhere.
// -----------------------------------------------------------------------
HYDRA_API void SleepForMilliseconds(u64 milliseconds);
HYDRA_API void SleepForMicroseconds(u64 microseconds);

// -----------------------------------------------------------------------
// YieldThread
//
// Politely asks the OS scheduler to run some other ready thread instead
// of this one. Useful in short spin-wait loops (e.g. while polling a
// lock-free flag) to avoid burning a full CPU core for no reason.
// -----------------------------------------------------------------------
HYDRA_API void YieldThread() noexcept;

} // namespace Hydra::Thread
