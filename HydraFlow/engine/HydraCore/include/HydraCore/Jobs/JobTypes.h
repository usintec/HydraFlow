#pragma once

// =============================================================================
// JobTypes.h
//
// Shared small types used across the whole Job System module: ids,
// priority levels, lifecycle states, and the "kind" of hardware a job
// wants to run on. Kept in one file because every other Jobs/ header
// needs at least one of these, and they're too small to justify their
// own files.
// =============================================================================

#include <HydraCore/Common/Types.h>

namespace Hydra {

// -----------------------------------------------------------------------
// JobId / TaskId
//
// JobId identifies a Job once it has been handed to a JobScheduler.
// TaskId identifies a Task *within a single TaskGraph*, before it has
// been turned into a real, scheduled Job (a TaskGraph can be built and
// thrown away without ever touching the scheduler). They are separate
// aliases — even though both happen to be plain integers today — so
// call sites (and future refactors) can tell "graph-local id" apart
// from "scheduler-global id" instead of silently mixing the two up.
// -----------------------------------------------------------------------
using JobId  = u64;
using TaskId = u64;

/// Reserved value meaning "no id" / "not scheduled" — 0 is never handed
/// out as a real id by JobScheduler or TaskGraph (both start counting at 1).
constexpr JobId  kInvalidJobId  = 0;
constexpr TaskId kInvalidTaskId = static_cast<TaskId>(-1);

// -----------------------------------------------------------------------
// JobPriority
//
// Hint used by WorkerQueue to decide which ready job to hand to a free
// worker thread first when several are ready at once. Does not affect
// correctness (dependency order is always respected regardless of
// priority) — only scheduling order among jobs that are equally ready.
// -----------------------------------------------------------------------
enum class JobPriority : u8
{
    Low = 0,
    Normal,
    High,
    Critical
};

// -----------------------------------------------------------------------
// JobKind
//
// What kind of hardware executes this job's function. Only CPU is
// implemented today (Module 6's ThreadPool machinery underneath), but
// having this enum from day one means the scheduling/dependency
// machinery (JobScheduler, DependencyGraph, TaskGraph) never has to
// change when a real GPU backend is added later — only the executor
// that a GPU-kind job is dispatched to needs to be written.
// -----------------------------------------------------------------------
enum class JobKind : u8
{
    CPU = 0, ///< Runs on a JobScheduler worker thread (fully supported today).
    GPU       ///< Reserved for a future GPU command-queue backend (see JobScheduler::SetGpuExecutor).
};

// -----------------------------------------------------------------------
// JobState
//
// Where a Job currently is in its lifecycle:
//
//   Pending   -> created, but still waiting on one or more dependencies.
//   Ready     -> all dependencies satisfied, sitting in the WorkerQueue.
//   Running   -> a worker thread is currently executing it.
//   Completed -> finished successfully.
//   Failed    -> finished by throwing an exception (see Job::GetException()).
// -----------------------------------------------------------------------
enum class JobState : u8
{
    Pending = 0,
    Ready,
    Running,
    Completed,
    Failed
};

} // namespace Hydra
