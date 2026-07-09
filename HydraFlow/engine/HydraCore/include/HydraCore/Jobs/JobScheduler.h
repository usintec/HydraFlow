#pragma once

// =============================================================================
// JobScheduler.h
//
// The central façade of the Job System: owns a pool of worker threads
// (built on Module 6's WorkerThread), a WorkerQueue of ready-to-run jobs,
// and a DependencyGraph tracking what's still waiting on what.
//
//     JobScheduler scheduler;                          // sizes itself to hardware_concurrency()
//     JobHandle h = scheduler.Schedule([]{ Work(); }, "MyJob");
//     h.Wait();
//
//     TaskGraph graph;
//     TaskId a = graph.AddTask("A", []{ ... });
//     TaskId b = graph.AddTask("B", []{ ... });
//     graph.AddDependency(b, a);                        // B runs after A
//     Vector<JobHandle> handles = scheduler.Schedule(graph);
//
// -----------------------------------------------------------------------
// Future GPU expansion
// -----------------------------------------------------------------------
// Every Job carries a JobKind (CPU or GPU). Today, only CPU jobs are
// actually runnable — WorkerLoop() executes them directly on a worker
// thread. A GPU job is instead handed to whatever callback was
// registered via SetGpuExecutor(); if none has been registered, the
// job fails with a clear "no GPU executor registered" exception rather
// than silently running on the CPU or hanging forever. This means the
// entire scheduling/dependency machinery (WorkerQueue, DependencyGraph,
// TaskGraph) already works unchanged for GPU work — a future renderer
// module just needs to call SetGpuExecutor() with something that submits
// the job's function to a GPU command queue and signals completion.
// =============================================================================

#include <HydraCore/Common/Platform.h>
#include <HydraCore/Common/Types.h>
#include <HydraCore/Common/NonCopyable.h>
#include <HydraCore/Threading/Mutex.h>
#include <HydraCore/Threading/ConditionVariable.h>
#include <HydraCore/Threading/WorkerThread.h>
#include <HydraCore/Jobs/JobTypes.h>
#include <HydraCore/Jobs/Job.h>
#include <HydraCore/Jobs/JobHandle.h>
#include <HydraCore/Jobs/WorkerQueue.h>
#include <HydraCore/Jobs/DependencyGraph.h>
#include <HydraCore/Jobs/TaskGraph.h>

#include <atomic>

namespace Hydra {

class HYDRA_API JobScheduler final : private NonCopyableNonMovable
{
public:
    /// Executes a single GPU-kind job's function, then must arrange for
    /// `onComplete` to be invoked exactly once (immediately, or later
    /// once the GPU work actually finishes) so the scheduler can cascade
    /// dependency completion and wake anyone waiting on the JobHandle.
    using GpuExecutor = Function<void(Job& job, Function<void()> onComplete)>;

    /// Creates the scheduler and immediately spawns `threadCount` worker
    /// threads (0 = one per logical CPU core, via
    /// Thread::GetHardwareConcurrency() — see Module 6).
    explicit JobScheduler(usize threadCount = 0, StringView name = "JobScheduler");

    /// Signals shutdown and joins every worker thread. Any jobs still
    /// sitting in the ready queue when this runs are simply never
    /// executed — call WaitAll() first if you need everything to finish.
    ~JobScheduler();

    /// Schedules a single, dependency-free unit of work and returns a
    /// handle to it immediately (non-blocking). Equivalent to building a
    /// one-task TaskGraph and submitting it, but without the ceremony.
    JobHandle Schedule(Job::JobFunction function, StringView name = "Job",
                        JobPriority priority = JobPriority::Normal, JobKind kind = JobKind::CPU);

    /// Translates an entire TaskGraph into real Jobs, wires them into the
    /// scheduler's DependencyGraph according to the graph's edges, and
    /// immediately enqueues every task that has no dependencies. Returns
    /// one JobHandle per task, in the same order as TaskGraph::AddTask()
    /// was called (i.e. `result[taskId]` is the handle for that task).
    ///
    /// Throws std::runtime_error if the graph contains a dependency
    /// cycle (see TaskGraph::Validate()) — nothing is scheduled in that
    /// case.
    Vector<JobHandle> Schedule(const TaskGraph& graph);

    /// Registers (or replaces) the callback used to execute GPU-kind
    /// jobs. See the "Future GPU expansion" note above.
    void SetGpuExecutor(GpuExecutor executor);

    /// Blocks the calling thread until the ready queue is empty and no
    /// worker is currently executing a job. Note this does *not* mean
    /// every job has completed if some are still Pending on dependencies
    /// that were never satisfied (e.g. a job outside this batch) —
    /// ordinary use (schedule a graph, then WaitAll()) doesn't hit that
    /// edge case.
    void WaitAll();

    /// Signals every worker to stop (after finishing whatever is already
    /// queued) and joins them. Safe to call more than once. Automatically
    /// invoked by the destructor if not called explicitly.
    void Shutdown();

    [[nodiscard]] usize GetThreadCount() const noexcept { return m_Workers.size(); }
    [[nodiscard]] usize GetPendingJobCount() const { return m_ReadyQueue.Size(); }

private:
    // The function every worker thread runs in a loop: pull a ready job,
    // run it (or hand it to the GPU executor), cascade completion.
    void WorkerLoop();

    // Shared by Schedule(Job::JobFunction, ...) and Schedule(TaskGraph&):
    // registers a brand-new Job under a fresh JobId and, if it has no
    // unmet dependencies, immediately pushes it onto the ready queue.
    SharedPtr<Job> CreateJob(Job::JobFunction function, StringView name, JobPriority priority, JobKind kind);

    // Called once a job's function has actually finished executing
    // (successfully or not): looks up which dependents just became ready
    // via the DependencyGraph and enqueues them, then updates idle
    // tracking for WaitAll().
    void OnJobFinished(const SharedPtr<Job>& job);

    Vector<UniquePtr<WorkerThread>> m_Workers;
    WorkerQueue                     m_ReadyQueue;
    DependencyGraph                 m_DependencyGraph;

    // Registry so OnJobFinished() (given only a JobId from the
    // DependencyGraph) can look up the actual Job object to enqueue.
    Mutex                     m_JobRegistryMutex;
    HashMap<JobId, SharedPtr<Job>> m_JobRegistry;

    std::atomic<u64> m_NextJobId{1}; // 0 is reserved as kInvalidJobId.

    // --- Idle tracking (for WaitAll()) -----------------------------------
    std::atomic<u32> m_ActiveJobCount{0};
    Mutex             m_IdleMutex;
    ConditionVariable m_IdleCv;

    Mutex       m_GpuExecutorMutex;
    GpuExecutor m_GpuExecutor; ///< Empty until SetGpuExecutor() is called.

    std::atomic<bool> m_Stopping{false};
    String            m_Name;
};

} // namespace Hydra
