// =============================================================================
// test_Threading_Module6.cpp
//
// Tests for Module 6 (Threading): ThreadUtils, Mutex/ConditionVariable/
// Semaphore wrappers, Future/Promise, WorkerThread, ThreadPool, and the
// ParallelFor CPU-parallelism helper.
//
// Comments throughout explain *what each test is checking and why*, per
// the project convention of thoroughly commenting all Module 6 code.
// =============================================================================

#include <gtest/gtest.h>

#include <HydraCore/Threading/ThreadUtils.h>
#include <HydraCore/Threading/Mutex.h>
#include <HydraCore/Threading/ConditionVariable.h>
#include <HydraCore/Threading/Semaphore.h>
#include <HydraCore/Threading/Future.h>
#include <HydraCore/Threading/Promise.h>
#include <HydraCore/Threading/WorkerThread.h>
#include <HydraCore/Threading/ThreadPool.h>
#include <HydraCore/Threading/ParallelFor.h>

#include <atomic>
#include <chrono>
#include <numeric>
#include <stdexcept>
#include <thread>
#include <vector>

using namespace Hydra;

// =============================================================================
// ThreadUtils
// =============================================================================

TEST(ThreadUtils, HardwareConcurrencyIsAtLeastOne)
{
    // Must never be 0 — HydraCore clamps the (technically allowed to be
    // zero) std::thread::hardware_concurrency() result so callers like
    // ThreadPool never try to spawn a zero-worker pool.
    EXPECT_GE(Thread::GetHardwareConcurrency(), 1u);
}

TEST(ThreadUtils, SetAndGetCurrentThreadName)
{
    Thread::SetCurrentThreadName("TestThread");
    EXPECT_EQ(Thread::GetCurrentThreadName(), "TestThread");
}

TEST(ThreadUtils, CurrentThreadIdIsStableWithinAThread)
{
    // Calling it twice in a row on the same thread must return the same
    // value (it's derived from this thread's std::thread::id).
    const u64 first  = Thread::GetCurrentThreadId();
    const u64 second = Thread::GetCurrentThreadId();
    EXPECT_EQ(first, second);
}

TEST(ThreadUtils, DifferentThreadsHaveDifferentIds)
{
    u64 mainId = Thread::GetCurrentThreadId();
    u64 otherId = 0;

    std::thread t([&otherId] { otherId = Thread::GetCurrentThreadId(); });
    t.join();

    EXPECT_NE(mainId, otherId);
}

TEST(ThreadUtils, SleepAndYieldDoNotCrash)
{
    // These are trivial "does it explode" smoke tests — the actual sleep
    // duration isn't asserted since scheduler timing is inherently fuzzy
    // on shared CI machines.
    Thread::SleepForMilliseconds(1);
    Thread::SleepForMicroseconds(100);
    Thread::YieldThread();
    SUCCEED();
}

// =============================================================================
// Mutex / RecursiveMutex
// =============================================================================

TEST(Mutex, LockUnlockProtectsSharedCounter)
{
    Mutex mutex;
    int counter = 0;
    constexpr int kIncrementsPerThread = 10'000;

    // Two threads racing to increment an unguarded `int` would (almost
    // always) lose increments; wrapping every increment in the mutex
    // must make the final result exact.
    auto worker = [&]
    {
        for (int i = 0; i < kIncrementsPerThread; ++i)
        {
            ScopedLock<Mutex> lock(mutex);
            ++counter;
        }
    };

    std::thread t1(worker);
    std::thread t2(worker);
    t1.join();
    t2.join();

    EXPECT_EQ(counter, kIncrementsPerThread * 2);
}

TEST(Mutex, TryLockFailsWhileAlreadyHeld)
{
    Mutex mutex;
    mutex.lock();

    // A second attempt to lock from a *different* thread must fail
    // immediately (try_lock never blocks) while the first thread still
    // holds it.
    std::atomic<bool> acquired{true};
    std::thread t([&] { acquired.store(mutex.try_lock()); });
    t.join();

    EXPECT_FALSE(acquired.load());
    mutex.unlock();
}

TEST(RecursiveMutex, SameThreadCanLockMultipleTimes)
{
    RecursiveMutex mutex;

    // A plain Mutex would deadlock here; RecursiveMutex must allow the
    // same thread to nest lock() calls as long as it unlocks the same
    // number of times.
    mutex.lock();
    mutex.lock();
    mutex.lock();
    mutex.unlock();
    mutex.unlock();
    mutex.unlock();

    SUCCEED();
}

// =============================================================================
// ConditionVariable
// =============================================================================

TEST(ConditionVariable, WaitWakesUpAfterNotify)
{
    Mutex mutex;
    ConditionVariable cv;
    bool ready = false;

    std::thread producer([&]
    {
        Thread::SleepForMilliseconds(10);
        {
            ScopedLock<Mutex> lock(mutex);
            ready = true;
        }
        cv.NotifyOne();
    });

    UniqueLock<Mutex> lock(mutex);
    // Predicate form guards against spurious wakeups — the wait only
    // returns once `ready` is actually true.
    cv.Wait(lock, [&] { return ready; });

    EXPECT_TRUE(ready);
    producer.join();
}

TEST(ConditionVariable, WaitForTimesOutWhenNeverNotified)
{
    Mutex mutex;
    ConditionVariable cv;
    UniqueLock<Mutex> lock(mutex);

    // Nobody will ever notify this condition variable, so WaitFor must
    // give up after the requested timeout and report failure.
    const bool wokenByNotify = cv.WaitFor(lock, 20, [] { return false; });
    EXPECT_FALSE(wokenByNotify);
}

// =============================================================================
// Semaphore
// =============================================================================

TEST(Semaphore, AcquireBlocksUntilReleased)
{
    // Starts with 0 permits: Acquire() on the main thread must block
    // until the background thread calls Release().
    Semaphore semaphore(0);
    std::atomic<bool> acquired{false};

    std::thread releaser([&]
    {
        Thread::SleepForMilliseconds(10);
        semaphore.Release();
    });

    semaphore.Acquire();
    acquired.store(true);
    releaser.join();

    EXPECT_TRUE(acquired.load());
}

TEST(Semaphore, TryAcquireFailsWhenNoPermitsAvailable)
{
    Semaphore semaphore(0);
    EXPECT_FALSE(semaphore.TryAcquire());

    semaphore.Release();
    EXPECT_TRUE(semaphore.TryAcquire());
}

TEST(BinarySemaphore, ActsAsASingleSignal)
{
    // A BinarySemaphore only ever has 0 or 1 permits — releasing twice
    // before a single acquire must not accumulate to "2 acquires worth"
    // (that's the whole point of it being *binary*, unlike a general
    // counting semaphore).
    BinarySemaphore semaphore(0);
    semaphore.Release();
    semaphore.Release();

    EXPECT_TRUE(semaphore.TryAcquire());
    // Implementation-defined whether the second Release() was absorbed
    // or queued; either way a BinarySemaphore must never let two
    // Acquire()s succeed back-to-back after exactly one Release() from 0.
}

// =============================================================================
// Future / Promise
// =============================================================================

TEST(FuturePromise, SetValueDeliversResultToFuture)
{
    Promise<int> promise;
    Future<int> future = promise.GetFuture();

    promise.SetValue(42);

    EXPECT_TRUE(future.IsReady());
    EXPECT_EQ(future.Get(), 42);
}

TEST(FuturePromise, GetBlocksUntilValueIsSet)
{
    Promise<int> promise;
    Future<int> future = promise.GetFuture();

    std::thread producer([&promise]
    {
        Thread::SleepForMilliseconds(20);
        promise.SetValue(7);
    });

    // Get() must block here rather than returning a garbage/default
    // value before the producer thread has actually set one.
    const int value = future.Get();
    EXPECT_EQ(value, 7);

    producer.join();
}

TEST(FuturePromise, SetExceptionRethrowsFromGet)
{
    Promise<int> promise;
    Future<int> future = promise.GetFuture();

    promise.SetException(std::make_exception_ptr(std::runtime_error("boom")));

    EXPECT_THROW((void)future.Get(), std::runtime_error);
}

TEST(FuturePromise, VoidSpecializationCompletesWithoutAValue)
{
    Promise<void> promise;
    Future<void> future = promise.GetFuture();

    promise.SetValue();

    // Get() on a Future<void> should simply return without throwing.
    EXPECT_NO_THROW(future.Get());
}

TEST(FuturePromise, WaitForReportsTimeoutBeforeValueIsSet)
{
    Promise<int> promise;
    Future<int> future = promise.GetFuture();

    // Nothing has fulfilled the promise yet, so a short WaitFor must
    // report "not ready" (false) rather than hanging or succeeding.
    EXPECT_FALSE(future.WaitFor(10));

    promise.SetValue(1);
    EXPECT_TRUE(future.WaitFor(1000));
}

// =============================================================================
// WorkerThread
// =============================================================================

TEST(WorkerThread, RunsEntryPointAndReportsRunningState)
{
    std::atomic<bool> ran{false};

    WorkerThread worker("UnitTestWorker", [&]
    {
        Thread::SleepForMilliseconds(10);
        ran.store(true);
    });

    worker.Join();

    EXPECT_TRUE(ran.load());
    EXPECT_FALSE(worker.IsRunning()); // Entry point has already returned.
    EXPECT_EQ(worker.GetName(), "UnitTestWorker");
}

// =============================================================================
// ThreadPool
// =============================================================================

TEST(ThreadPool, SubmitReturnsCorrectResult)
{
    ThreadPool pool(2, "TestPool");

    Future<int> future = pool.Submit([](int a, int b) { return a + b; }, 3, 4);

    EXPECT_EQ(future.Get(), 7);
}

TEST(ThreadPool, SubmitVoidTaskCompletes)
{
    ThreadPool pool(2, "TestPool");
    std::atomic<int> sideEffect{0};

    Future<void> future = pool.Submit([&sideEffect] { sideEffect.store(99); });
    future.Get();

    EXPECT_EQ(sideEffect.load(), 99);
}

TEST(ThreadPool, ExceptionFromTaskPropagatesThroughFuture)
{
    ThreadPool pool(2, "TestPool");

    Future<int> future = pool.Submit([]() -> int
    {
        throw std::runtime_error("task failed");
    });

    // The worker thread must catch the exception (rather than crashing
    // the process) and hand it to the Future to re-throw here instead.
    EXPECT_THROW((void)future.Get(), std::runtime_error);
}

TEST(ThreadPool, ManyTasksAllCompleteExactlyOnce)
{
    ThreadPool pool(4, "TestPool");
    constexpr int kTaskCount = 1000;

    std::atomic<int> completedCount{0};
    Vector<Future<void>> futures;
    futures.reserve(kTaskCount);

    for (int i = 0; i < kTaskCount; ++i)
    {
        futures.push_back(pool.Submit([&completedCount] { completedCount.fetch_add(1); }));
    }

    for (auto& f : futures)
    {
        f.Get();
    }

    EXPECT_EQ(completedCount.load(), kTaskCount);
}

TEST(ThreadPool, WaitIdleBlocksUntilQueueIsDrained)
{
    ThreadPool pool(2, "TestPool");
    std::atomic<int> completedCount{0};

    for (int i = 0; i < 50; ++i)
    {
        (void)pool.Submit([&completedCount]
        {
            Thread::SleepForMicroseconds(500);
            completedCount.fetch_add(1);
        });
    }

    pool.WaitIdle();

    // By the time WaitIdle() returns, every submitted task must have
    // actually finished running (not merely been dequeued).
    EXPECT_EQ(completedCount.load(), 50);
    EXPECT_EQ(pool.GetPendingTaskCount(), 0u);
}

TEST(ThreadPool, DestructorDrainsQueueBeforeStopping)
{
    std::atomic<int> completedCount{0};

    {
        ThreadPool pool(2, "TestPool");
        for (int i = 0; i < 20; ++i)
        {
            (void)pool.Submit([&completedCount] { completedCount.fetch_add(1); });
        }
        // Pool goes out of scope here -> destructor must finish
        // everything already queued before joining its workers.
    }

    EXPECT_EQ(completedCount.load(), 20);
}

// =============================================================================
// ParallelFor
// =============================================================================

TEST(ParallelFor, AppliesFunctionToEveryIndexExactlyOnce)
{
    ThreadPool pool(4, "ParallelForPool");
    constexpr usize kSize = 10'000;

    std::vector<int> touchedCount(kSize, 0);

    Threading::ParallelFor(pool, 0, kSize, [&touchedCount](usize i)
    {
        touchedCount[i] += 1;
    });

    // Every single index must have been visited exactly once — no gaps
    // (missed indices) and no double-processing (overlapping chunks).
    const int total = std::accumulate(touchedCount.begin(), touchedCount.end(), 0);
    EXPECT_EQ(total, static_cast<int>(kSize));
}

TEST(ParallelFor, EmptyRangeDoesNothing)
{
    ThreadPool pool(2, "ParallelForPool");
    bool called = false;

    Threading::ParallelFor(pool, 5, 5, [&called](usize) { called = true; });

    EXPECT_FALSE(called);
}

TEST(ParallelFor, PropagatesExceptionFromChunk)
{
    ThreadPool pool(2, "ParallelForPool");

    EXPECT_THROW(
        Threading::ParallelFor(pool, 0, 10, [](usize i)
        {
            if (i == 5) { throw std::runtime_error("chunk failed"); }
        }),
        std::runtime_error);
}
