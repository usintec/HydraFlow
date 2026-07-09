#include <HydraCore/Jobs/JobScheduler.h>
#include <HydraCore/Threading/ThreadUtils.h>

#include <stdexcept>

namespace Hydra {

JobScheduler::JobScheduler(usize threadCount, StringView name)
    : m_Name(name)
{
    const usize workerCount = (threadCount == 0) ? static_cast<usize>(Thread::GetHardwareConcurrency())
                                                  : threadCount;

    m_Workers.reserve(workerCount);
    for (usize i = 0; i < workerCount; ++i)
    {
        String workerName = m_Name + "-" + std::to_string(i);
        m_Workers.push_back(MakeUnique<WorkerThread>(workerName, [this] { WorkerLoop(); }));
    }
}

JobScheduler::~JobScheduler()
{
    Shutdown();
}

SharedPtr<Job> JobScheduler::CreateJob(Job::JobFunction function, StringView name, JobPriority priority, JobKind kind)
{
    const JobId id = m_NextJobId.fetch_add(1, std::memory_order_relaxed);
    auto job = MakeShared<Job>(id, name, std::move(function), priority, kind);

    {
        ScopedLock<Mutex> lock(m_JobRegistryMutex);
        m_JobRegistry.emplace(id, job);
    }

    return job;
}

JobHandle JobScheduler::Schedule(Job::JobFunction function, StringView name, JobPriority priority, JobKind kind)
{
    SharedPtr<Job> job = CreateJob(std::move(function), name, priority, kind);

    // A job scheduled this way has no dependencies at all, so it's
    // registered with the DependencyGraph (mainly so a *later* TaskGraph
    // could theoretically depend on it — not required for correctness
    // today, but keeps every job's bookkeeping consistent) and pushed
    // straight onto the ready queue.
    m_DependencyGraph.AddNode(job->GetId());
    m_ActiveJobCount.fetch_add(1, std::memory_order_acq_rel);
    m_ReadyQueue.Push(job);

    return JobHandle(job);
}

Vector<JobHandle> JobScheduler::Schedule(const TaskGraph& graph)
{
    if (!graph.Validate())
    {
        throw std::runtime_error("JobScheduler::Schedule(): TaskGraph contains a dependency cycle");
    }

    const Vector<Task>& tasks = graph.GetTasks();

    // Create every Job up front (all Pending) before wiring any
    // dependencies or enqueueing anything, so partial failures can't
    // leave some jobs ready and others missing.
    Vector<SharedPtr<Job>> jobs;
    jobs.reserve(tasks.size());
    for (const Task& task : tasks)
    {
        // Copy the task's function since the same TaskGraph may be
        // submitted again later — Task::GetFunction() intentionally
        // returns a const reference rather than moving it out.
        jobs.push_back(CreateJob(task.GetFunction(), task.GetName(), task.GetPriority(), task.GetKind()));
        m_DependencyGraph.AddNode(jobs.back()->GetId());
    }

    // Wire up dependency edges using the real JobIds we just assigned
    // (TaskGraph's edges are expressed in TaskIds, which map 1:1 by
    // index into `jobs` since AddTask() hands out ids in insertion order).
    for (const auto& [dependentTaskId, dependsOnTaskId] : graph.GetEdges())
    {
        const JobId dependentJobId  = jobs[dependentTaskId]->GetId();
        const JobId dependsOnJobId  = jobs[dependsOnTaskId]->GetId();
        m_DependencyGraph.AddDependency(dependentJobId, dependsOnJobId);
    }

    // Now that every edge is registered, ask each job's Job object to
    // reflect its true remaining-dependency count and enqueue the ones
    // that are already ready (i.e. had no dependencies at all).
    Vector<JobHandle> handles;
    handles.reserve(jobs.size());
    for (const SharedPtr<Job>& job : jobs)
    {
        const u32 remaining = m_DependencyGraph.GetRemainingDependencyCount(job->GetId());
        job->RemainingDependencies().store(remaining, std::memory_order_release);

        handles.emplace_back(job);

        if (remaining == 0)
        {
            m_ActiveJobCount.fetch_add(1, std::memory_order_acq_rel);
            m_ReadyQueue.Push(job);
        }
    }

    return handles;
}

void JobScheduler::SetGpuExecutor(GpuExecutor executor)
{
    ScopedLock<Mutex> lock(m_GpuExecutorMutex);
    m_GpuExecutor = std::move(executor);
}

void JobScheduler::WorkerLoop()
{
    for (;;)
    {
        SharedPtr<Job> job = m_ReadyQueue.WaitAndPop();
        if (!job)
        {
            // WaitAndPop() only returns null once the queue has been
            // Close()'d and fully drained — time for this worker to exit.
            return;
        }

        if (job->GetKind() == JobKind::CPU)
        {
            // The common case today: just run it right here on this
            // worker thread, synchronously.
            job->Execute();
            OnJobFinished(job);
        }
        else // JobKind::GPU
        {
            GpuExecutor executor;
            {
                ScopedLock<Mutex> lock(m_GpuExecutorMutex);
                executor = m_GpuExecutor;
            }

            if (!executor)
            {
                // No GPU backend has been wired up yet — fail loudly and
                // immediately, *without* running the job's function.
                // Silently falling back to running GPU-intended work on
                // the CPU would be a confusing surprise once real GPU
                // jobs start assuming GPU-only resources are available;
                // failing fast makes the missing SetGpuExecutor() call
                // obvious instead.
                job->FailWithoutRunning(std::make_exception_ptr(
                    std::runtime_error("JobScheduler: no GPU executor registered for job '" + job->GetName() + "'")));
                OnJobFinished(job);
                continue;
            }

            // The GPU executor is responsible for calling this exactly
            // once, whenever the GPU work it kicked off actually
            // finishes — which may be well after this worker thread has
            // moved on to pick up the next ready CPU job.
            SharedPtr<Job> jobForCallback = job;
            executor(*job, [this, jobForCallback] { OnJobFinished(jobForCallback); });
        }
    }
}

void JobScheduler::OnJobFinished(const SharedPtr<Job>& job)
{
    // Ask the dependency graph who was waiting on this job and is now
    // unblocked; enqueue each of those immediately.
    const Vector<JobId> newlyReady = m_DependencyGraph.OnNodeCompleted(job->GetId());

    for (const JobId readyId : newlyReady)
    {
        SharedPtr<Job> readyJob;
        {
            ScopedLock<Mutex> lock(m_JobRegistryMutex);
            const auto it = m_JobRegistry.find(readyId);
            if (it != m_JobRegistry.end())
            {
                readyJob = it->second;
            }
        }

        if (readyJob)
        {
            m_ActiveJobCount.fetch_add(1, std::memory_order_acq_rel);
            m_ReadyQueue.Push(readyJob);
        }
    }

    const u32 remaining = m_ActiveJobCount.fetch_sub(1, std::memory_order_acq_rel) - 1;
    if (remaining == 0)
    {
        ScopedLock<Mutex> idleLock(m_IdleMutex);
        m_IdleCv.NotifyAll();
    }
}

void JobScheduler::WaitAll()
{
    UniqueLock<Mutex> idleLock(m_IdleMutex);
    m_IdleCv.Wait(idleLock, [this]
    {
        return m_ActiveJobCount.load(std::memory_order_acquire) == 0 && m_ReadyQueue.Size() == 0;
    });
}

void JobScheduler::Shutdown()
{
    const bool alreadyStopping = m_Stopping.exchange(true, std::memory_order_acq_rel);
    if (alreadyStopping)
    {
        return;
    }

    m_ReadyQueue.Close();

    for (auto& worker : m_Workers)
    {
        worker->Join();
    }
}

} // namespace Hydra
