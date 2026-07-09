#pragma once

// =============================================================================
// ThreadPool.h
//
// The main entry point for CPU parallelism in HydraCore: a fixed set of
// WorkerThreads that pull tasks off a shared queue and run them, so
// callers don't have to spawn/join raw threads themselves for every
// piece of parallel work.
//
//     ThreadPool pool;                      // sizes itself to hardware_concurrency()
//     Future<int> f = pool.Submit([]{ return 1 + 1; });
//     int result = f.Get();                 // blocks until the task runs, returns 2
//
// Any callable + arguments can be submitted; Submit() returns a Future
// matching the callable's return type (including Future<void> for
// callables that return nothing), so callers get the result back the
// same way regardless of what kind of task they queued.
//
// The pool owns its workers for its entire lifetime: destroying a
// ThreadPool signals every worker to stop (once its current task and any
// already-queued tasks are drained) and joins all of them, so it's always
// safe to let a ThreadPool go out of scope.
// =============================================================================

#include <HydraCore/Common/Platform.h>
#include <HydraCore/Common/Types.h>
#include <HydraCore/Common/NonCopyable.h>
#include <HydraCore/Threading/Mutex.h>
#include <HydraCore/Threading/ConditionVariable.h>
#include <HydraCore/Threading/WorkerThread.h>
#include <HydraCore/Threading/Future.h>
#include <HydraCore/Threading/Promise.h>

#include <queue>
#include <atomic>
#include <type_traits>
#include <utility>

namespace Hydra {

class HYDRA_API ThreadPool final : private NonCopyableNonMovable
{
public:
    /// Creates a pool with `threadCount` worker threads. Passing 0 (the
    /// default) asks Thread::GetHardwareConcurrency() how many logical
    /// CPU cores are available and uses that — the usual choice for
    /// "use all the CPU parallelism this machine has to offer".
    explicit ThreadPool(usize threadCount = 0, StringView name = "ThreadPool");

    /// Requests shutdown (see Shutdown()) and joins every worker thread.
    ~ThreadPool();

    /// Queues `fn(args...)` to run on the next available worker thread
    /// and immediately returns a Future for its result — it does *not*
    /// block the calling thread. If `fn` throws, the exception is
    /// captured and re-thrown from the Future's Get() instead of
    /// crashing the worker thread.
    template<typename Fn, typename... Args>
    [[nodiscard]] auto Submit(Fn&& fn, Args&&... args)
        -> Future<std::invoke_result_t<std::decay_t<Fn>, std::decay_t<Args>...>>
    {
        using ReturnType = std::invoke_result_t<std::decay_t<Fn>, std::decay_t<Args>...>;

        // The Promise has to outlive this Submit() call (it's fulfilled
        // later, on a worker thread), so it's heap-allocated and shared
        // between this function and the queued task lambda below.
        auto promise = MakeShared<Promise<ReturnType>>();
        Future<ReturnType> future = promise->GetFuture();

        // C++20 allows capturing a parameter pack by init-capture
        // (`...args = std::forward<Args>(args)`), which is what lets us
        // store an arbitrary, already-bound argument list inside the
        // lambda below without an extra std::bind/tuple dance.
        auto boundTask = [fn = std::forward<Fn>(fn), ... capturedArgs = std::forward<Args>(args), promise]() mutable
        {
            try
            {
                if constexpr (std::is_void_v<ReturnType>)
                {
                    std::invoke(fn, capturedArgs...);
                    promise->SetValue();
                }
                else
                {
                    promise->SetValue(std::invoke(fn, capturedArgs...));
                }
            }
            catch (...)
            {
                // Never let an exception escape a worker thread — that
                // would call std::terminate() and take the whole process
                // down. Instead, hand it to the Future so the *caller*
                // decides what to do with it.
                promise->SetException(std::current_exception());
            }
        };

        EnqueueTask(std::move(boundTask));
        return future;
    }

    /// Blocks the calling thread until the task queue is empty AND no
    /// worker is currently mid-task. Useful for "wait for this batch of
    /// work to finish" without shutting the pool down (unlike
    /// Shutdown(), the pool remains usable afterwards).
    void WaitIdle();

    /// Signals every worker to stop once it has drained the queue, then
    /// joins them. Safe to call more than once (subsequent calls are a
    /// no-op). Automatically called by the destructor if not called
    /// explicitly.
    void Shutdown();

    [[nodiscard]] usize GetThreadCount() const noexcept { return m_Workers.size(); }

    /// Snapshot of how many tasks are currently sitting in the queue
    /// (not counting ones already picked up and running). Useful for
    /// diagnostics/telemetry, not for precise synchronization (the
    /// number can change the instant after you read it).
    [[nodiscard]] usize GetPendingTaskCount();

private:
    using Task = Function<void()>;

    // Pushes a task onto the shared queue and wakes one sleeping worker
    // to come pick it up.
    void EnqueueTask(Task task);

    // The function each WorkerThread runs: loop forever, pulling tasks
    // off the queue and executing them, until told to stop.
    void WorkerLoop();

    Vector<UniquePtr<WorkerThread>> m_Workers;

    // --- Task queue -----------------------------------------------------
    std::queue<Task>  m_Tasks;
    Mutex             m_QueueMutex;
    ConditionVariable m_QueueCv;      ///< Signaled when a task is enqueued or on shutdown.

    // --- Idle tracking (for WaitIdle()) ----------------------------------
    std::atomic<usize> m_ActiveTaskCount{0}; ///< Tasks currently being executed by a worker.
    Mutex               m_IdleMutex;
    ConditionVariable   m_IdleCv;      ///< Signaled whenever the pool might have become idle.

    std::atomic<bool> m_Stopping{false};
    String            m_Name;
};

} // namespace Hydra
