// =============================================================================
// test_Jobs_Module7.cpp
//
// Tests for Module 7 (Job System): Job/JobHandle, DependencyGraph,
// WorkerQueue, Task/TaskGraph, and JobScheduler — parallel execution,
// dependency-ordered scheduling, and the GPU-executor extension point.
//
// Comments throughout explain *what each test is checking and why*, per
// the project convention of thoroughly commenting all code.
// =============================================================================

#include <gtest/gtest.h>

#include <HydraCore/Jobs/JobTypes.h>
#include <HydraCore/Jobs/Job.h>
#include <HydraCore/Jobs/JobHandle.h>
#include <HydraCore/Jobs/DependencyGraph.h>
#include <HydraCore/Jobs/WorkerQueue.h>
#include <HydraCore/Jobs/Task.h>
#include <HydraCore/Jobs/TaskGraph.h>
#include <HydraCore/Jobs/JobScheduler.h>

#include <atomic>
#include <chrono>
#include <mutex>
#include <stdexcept>
#include <vector>

using namespace Hydra;

// =============================================================================
// Job
// =============================================================================

TEST(Job, ExecuteRunsFunctionAndMarksCompleted)
{
    bool ran = false;
    Job job(1, "TestJob", [&ran] { ran = true; }, JobPriority::Normal, JobKind::CPU);

    EXPECT_EQ(job.GetState(), JobState::Pending);

    job.Execute();

    EXPECT_TRUE(ran);
    EXPECT_EQ(job.GetState(), JobState::Completed);
    EXPECT_TRUE(job.IsComplete());
    EXPECT_FALSE(job.HasFailed());
    EXPECT_EQ(job.GetException(), nullptr);
}

TEST(Job, ExecuteCatchesExceptionAndMarksFailed)
{
    Job job(2, "ThrowingJob", [] { throw std::runtime_error("boom"); }, JobPriority::Normal, JobKind::CPU);

    job.Execute();

    EXPECT_EQ(job.GetState(), JobState::Failed);
    EXPECT_TRUE(job.IsComplete());
    EXPECT_TRUE(job.HasFailed());
    ASSERT_NE(job.GetException(), nullptr);

    // The captured exception should be re-throwable and carry the
    // original message.
    try
    {
        std::rethrow_exception(job.GetException());
        FAIL() << "Expected exception to be re-thrown";
    }
    catch (const std::runtime_error& e)
    {
        EXPECT_STREQ(e.what(), "boom");
    }
}

TEST(Job, WaitBlocksUntilExecuteFinishes)
{
    // Runs Execute() on another thread after a short delay; Wait() on
    // this thread must not return until that happens.
    auto job = MakeShared<Job>(3, "AsyncJob", []
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(30));
    }, JobPriority::Normal, JobKind::CPU);

    std::thread runner([job] { job->Execute(); });
    job->Wait();
    EXPECT_TRUE(job->IsComplete());
    runner.join();
}

TEST(Job, WaitForTimesOutBeforeCompletion)
{
    auto job = MakeShared<Job>(4, "SlowJob", []
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }, JobPriority::Normal, JobKind::CPU);

    std::thread runner([job] { job->Execute(); });

    // A very short timeout should elapse before the 100ms job finishes.
    EXPECT_FALSE(job->WaitFor(5));

    job->Wait();
    runner.join();
}

// =============================================================================
// JobHandle
// =============================================================================

TEST(JobHandle, DefaultConstructedIsInvalid)
{
    JobHandle handle;
    EXPECT_FALSE(handle.IsValid());
}

TEST(JobHandle, ReflectsUnderlyingJobState)
{
    auto job = MakeShared<Job>(5, "HandledJob", [] {}, JobPriority::Normal, JobKind::CPU);
    JobHandle handle(job);

    EXPECT_TRUE(handle.IsValid());
    EXPECT_EQ(handle.GetId(), 5u);
    EXPECT_FALSE(handle.IsComplete());

    job->Execute();

    EXPECT_TRUE(handle.IsComplete());
    EXPECT_FALSE(handle.HasFailed());
}

TEST(JobHandle, WaitAndRethrowPropagatesFailure)
{
    auto job = MakeShared<Job>(6, "FailingJob", [] { throw std::runtime_error("handle boom"); },
                                JobPriority::Normal, JobKind::CPU);
    JobHandle handle(job);

    std::thread runner([job] { job->Execute(); });
    runner.join();

    EXPECT_THROW(handle.WaitAndRethrow(), std::runtime_error);
}

// =============================================================================
// DependencyGraph
// =============================================================================

TEST(DependencyGraph, NodeWithNoDependenciesStartsAtZero)
{
    DependencyGraph graph;
    graph.AddNode(1);
    EXPECT_EQ(graph.GetRemainingDependencyCount(1), 0u);
}

TEST(DependencyGraph, AddDependencyIncrementsDependentCount)
{
    DependencyGraph graph;
    // Job 2 depends on job 1; job 3 also depends on job 1.
    graph.AddDependency(2, 1);
    graph.AddDependency(3, 1);

    EXPECT_EQ(graph.GetRemainingDependencyCount(1), 0u);
    EXPECT_EQ(graph.GetRemainingDependencyCount(2), 1u);
    EXPECT_EQ(graph.GetRemainingDependencyCount(3), 1u);
}

TEST(DependencyGraph, OnNodeCompletedReturnsNewlyReadyDependents)
{
    DependencyGraph graph;
    // 2 depends on 1; 3 depends on both 1 and 2.
    graph.AddDependency(2, 1);
    graph.AddDependency(3, 1);
    graph.AddDependency(3, 2);

    // Completing 1 frees up 2 (count 1 -> 0) but not 3 (still waiting on 2).
    Vector<JobId> readyAfterOne = graph.OnNodeCompleted(1);
    ASSERT_EQ(readyAfterOne.size(), 1u);
    EXPECT_EQ(readyAfterOne[0], 2u);
    EXPECT_EQ(graph.GetRemainingDependencyCount(3), 1u);

    // Completing 2 now frees up 3.
    Vector<JobId> readyAfterTwo = graph.OnNodeCompleted(2);
    ASSERT_EQ(readyAfterTwo.size(), 1u);
    EXPECT_EQ(readyAfterTwo[0], 3u);
}

TEST(DependencyGraph, HasCycleDetectsCircularDependency)
{
    DependencyGraph acyclic;
    acyclic.AddDependency(2, 1);
    acyclic.AddDependency(3, 2);
    EXPECT_FALSE(acyclic.HasCycle());

    DependencyGraph cyclic;
    cyclic.AddDependency(2, 1);
    cyclic.AddDependency(3, 2);
    cyclic.AddDependency(1, 3); // Closes the loop: 1 -> 2 -> 3 -> 1.
    EXPECT_TRUE(cyclic.HasCycle());
}

TEST(DependencyGraph, ClearResetsGraph)
{
    DependencyGraph graph;
    graph.AddDependency(2, 1);
    graph.Clear();

    EXPECT_EQ(graph.GetRemainingDependencyCount(2), 0u);
    EXPECT_TRUE(graph.OnNodeCompleted(1).empty());
}

// =============================================================================
// WorkerQueue
// =============================================================================

TEST(WorkerQueue, PushThenWaitAndPopReturnsSameJob)
{
    WorkerQueue queue;
    auto job = MakeShared<Job>(10, "QueuedJob", [] {}, JobPriority::Normal, JobKind::CPU);

    queue.Push(job);
    SharedPtr<Job> popped = queue.WaitAndPop();

    ASSERT_NE(popped, nullptr);
    EXPECT_EQ(popped->GetId(), 10u);
}

TEST(WorkerQueue, HigherPriorityJobsArePoppedFirst)
{
    WorkerQueue queue;
    auto low      = MakeShared<Job>(1, "Low", [] {}, JobPriority::Low, JobKind::CPU);
    auto critical = MakeShared<Job>(2, "Critical", [] {}, JobPriority::Critical, JobKind::CPU);
    auto normal   = MakeShared<Job>(3, "Normal", [] {}, JobPriority::Normal, JobKind::CPU);

    // Pushed in low -> critical -> normal order, but should pop out in
    // priority order regardless of insertion order.
    queue.Push(low);
    queue.Push(critical);
    queue.Push(normal);

    EXPECT_EQ(queue.WaitAndPop()->GetId(), 2u); // Critical
    EXPECT_EQ(queue.WaitAndPop()->GetId(), 3u); // Normal
    EXPECT_EQ(queue.WaitAndPop()->GetId(), 1u); // Low
}

TEST(WorkerQueue, EqualPriorityIsFifo)
{
    WorkerQueue queue;
    auto first  = MakeShared<Job>(1, "First", [] {}, JobPriority::Normal, JobKind::CPU);
    auto second = MakeShared<Job>(2, "Second", [] {}, JobPriority::Normal, JobKind::CPU);
    auto third  = MakeShared<Job>(3, "Third", [] {}, JobPriority::Normal, JobKind::CPU);

    queue.Push(first);
    queue.Push(second);
    queue.Push(third);

    EXPECT_EQ(queue.WaitAndPop()->GetId(), 1u);
    EXPECT_EQ(queue.WaitAndPop()->GetId(), 2u);
    EXPECT_EQ(queue.WaitAndPop()->GetId(), 3u);
}

TEST(WorkerQueue, TryPopReturnsNullOnEmptyQueue)
{
    WorkerQueue queue;
    EXPECT_EQ(queue.TryPop(), nullptr);
}

TEST(WorkerQueue, CloseWakesWaitersWithNullptr)
{
    WorkerQueue queue;
    SharedPtr<Job> result;
    std::thread waiter([&queue, &result] { result = queue.WaitAndPop(); });

    // Give the waiter thread a moment to actually start blocking before
    // closing the queue.
    std::this_thread::sleep_for(std::chrono::milliseconds(20));
    queue.Close();
    waiter.join();

    EXPECT_EQ(result, nullptr);
}

TEST(WorkerQueue, SizeReflectsQueuedJobCount)
{
    WorkerQueue queue;
    EXPECT_EQ(queue.Size(), 0u);
    queue.Push(MakeShared<Job>(1, "A", [] {}, JobPriority::Normal, JobKind::CPU));
    queue.Push(MakeShared<Job>(2, "B", [] {}, JobPriority::Normal, JobKind::CPU));
    EXPECT_EQ(queue.Size(), 2u);
}

// =============================================================================
// TaskGraph
// =============================================================================

TEST(TaskGraph, EmptyGraphIsValid)
{
    TaskGraph graph;
    EXPECT_TRUE(graph.Validate());
}

TEST(TaskGraph, LinearChainIsValid)
{
    TaskGraph graph;
    TaskId a = graph.AddTask("A", [] {});
    TaskId b = graph.AddTask("B", [] {});
    TaskId c = graph.AddTask("C", [] {});
    graph.AddDependency(b, a);
    graph.AddDependency(c, b);

    EXPECT_TRUE(graph.Validate());
    EXPECT_EQ(graph.GetTaskCount(), 3u);
    EXPECT_EQ(graph.GetEdges().size(), 2u);
}

TEST(TaskGraph, CyclicGraphFailsValidation)
{
    TaskGraph graph;
    TaskId a = graph.AddTask("A", [] {});
    TaskId b = graph.AddTask("B", [] {});
    graph.AddDependency(a, b); // A depends on B...
    graph.AddDependency(b, a); // ...and B depends on A: a cycle.

    EXPECT_FALSE(graph.Validate());
}

// =============================================================================
// JobScheduler
// =============================================================================

TEST(JobScheduler, ScheduleSingleJobRunsToCompletion)
{
    JobScheduler scheduler(2, "TestScheduler");
    std::atomic<bool> ran{false};

    JobHandle handle = scheduler.Schedule([&ran] { ran = true; }, "SingleJob");
    handle.Wait();

    EXPECT_TRUE(ran.load());
    EXPECT_TRUE(handle.IsComplete());
    EXPECT_FALSE(handle.HasFailed());
}

TEST(JobScheduler, ScheduleFailingJobReportsFailure)
{
    JobScheduler scheduler(2, "TestScheduler");
    JobHandle handle = scheduler.Schedule([] { throw std::runtime_error("scheduler boom"); }, "FailingJob");
    handle.Wait();

    EXPECT_TRUE(handle.HasFailed());
    EXPECT_THROW(handle.WaitAndRethrow(), std::runtime_error);
}

TEST(JobScheduler, ManyIndependentJobsAllComplete)
{
    JobScheduler scheduler(4, "ParallelScheduler");
    constexpr int kJobCount = 200;

    std::atomic<int> counter{0};
    Vector<JobHandle> handles;
    handles.reserve(kJobCount);

    for (int i = 0; i < kJobCount; ++i)
    {
        handles.push_back(scheduler.Schedule([&counter] { counter.fetch_add(1, std::memory_order_relaxed); }));
    }

    scheduler.WaitAll();

    EXPECT_EQ(counter.load(), kJobCount);
    for (const JobHandle& handle : handles)
    {
        EXPECT_TRUE(handle.IsComplete());
    }
}

TEST(JobScheduler, TaskGraphRespectsDependencyOrder)
{
    JobScheduler scheduler(4, "GraphScheduler");

    // Records the order in which tasks actually ran; a mutex protects it
    // since multiple worker threads could in principle append at once
    // (though the dependency chain here forces strict ordering).
    std::mutex orderMutex;
    Vector<String> executionOrder;

    TaskGraph graph;
    TaskId a = graph.AddTask("A", [&]
    {
        std::scoped_lock lock(orderMutex);
        executionOrder.push_back("A");
    });
    TaskId b = graph.AddTask("B", [&]
    {
        std::scoped_lock lock(orderMutex);
        executionOrder.push_back("B");
    });
    TaskId c = graph.AddTask("C", [&]
    {
        std::scoped_lock lock(orderMutex);
        executionOrder.push_back("C");
    });

    // C depends on both A and B; A and B are independent of each other.
    graph.AddDependency(c, a);
    graph.AddDependency(c, b);

    Vector<JobHandle> handles = scheduler.Schedule(graph);
    ASSERT_EQ(handles.size(), 3u);

    handles[c].Wait();
    scheduler.WaitAll();

    ASSERT_EQ(executionOrder.size(), 3u);
    // C must be last regardless of whether A or B ran first.
    EXPECT_EQ(executionOrder.back(), "C");
    EXPECT_TRUE((executionOrder[0] == "A" && executionOrder[1] == "B") ||
                (executionOrder[0] == "B" && executionOrder[1] == "A"));
}

TEST(JobScheduler, ScheduleCyclicTaskGraphThrows)
{
    JobScheduler scheduler(2, "CyclicScheduler");

    TaskGraph graph;
    TaskId a = graph.AddTask("A", [] {});
    TaskId b = graph.AddTask("B", [] {});
    graph.AddDependency(a, b);
    graph.AddDependency(b, a);

    EXPECT_THROW(scheduler.Schedule(graph), std::runtime_error);
}

TEST(JobScheduler, GpuJobFailsWithoutRegisteredExecutor)
{
    JobScheduler scheduler(2, "NoGpuScheduler");

    JobHandle handle = scheduler.Schedule([] {}, "GpuJob", JobPriority::Normal, JobKind::GPU);
    handle.Wait();

    EXPECT_TRUE(handle.HasFailed());
    EXPECT_THROW(handle.WaitAndRethrow(), std::runtime_error);
}

TEST(JobScheduler, GpuJobRunsThroughRegisteredExecutor)
{
    JobScheduler scheduler(2, "GpuScheduler");
    std::atomic<bool> executorInvoked{false};

    // A trivial "GPU executor" that just runs the job's function inline
    // and immediately signals completion — enough to prove the hook is
    // wired correctly without needing a real GPU backend.
    scheduler.SetGpuExecutor([&executorInvoked](Job& job, Function<void()> onComplete)
    {
        executorInvoked = true;
        job.Execute();
        onComplete();
    });

    std::atomic<bool> ran{false};
    JobHandle handle = scheduler.Schedule([&ran] { ran = true; }, "GpuJob", JobPriority::Normal, JobKind::GPU);
    handle.Wait();

    EXPECT_TRUE(executorInvoked.load());
    EXPECT_TRUE(ran.load());
    EXPECT_TRUE(handle.IsComplete());
    EXPECT_FALSE(handle.HasFailed());
}

TEST(JobScheduler, WaitAllReturnsOnceQueueIsIdle)
{
    JobScheduler scheduler(2, "IdleScheduler");
    std::atomic<int> completed{0};

    for (int i = 0; i < 20; ++i)
    {
        scheduler.Schedule([&completed]
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
            completed.fetch_add(1, std::memory_order_relaxed);
        });
    }

    scheduler.WaitAll();
    EXPECT_EQ(completed.load(), 20);
    EXPECT_EQ(scheduler.GetPendingJobCount(), 0u);
}
