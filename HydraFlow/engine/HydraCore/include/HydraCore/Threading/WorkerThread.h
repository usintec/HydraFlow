#pragma once

// =============================================================================
// WorkerThread.h
//
// A thin, named wrapper around std::thread.
//
// WorkerThread doesn't know anything about tasks or queues by itself —
// you give it one function ("the entry point") to run when it starts, and
// it runs that function on a dedicated OS thread until the function
// returns. ThreadPool (see ThreadPool.h) builds on top of WorkerThread by
// giving each one an entry point that loops "pull a task from the shared
// queue, run it, repeat until told to stop".
//
// Keeping WorkerThread generic like this means it's also directly useful
// outside of the pool — e.g. a single always-on background thread (audio
// mixing, asset streaming) that isn't part of a pool at all.
// =============================================================================

#include <HydraCore/Common/Platform.h>
#include <HydraCore/Common/Types.h>
#include <HydraCore/Common/NonCopyable.h>
#include <HydraCore/Threading/ThreadUtils.h>

#include <thread>
#include <atomic>

namespace Hydra {

class HYDRA_API WorkerThread final : private NonCopyableNonMovable
{
public:
    /// The function this thread will run. Should itself contain any
    /// looping logic (WorkerThread just calls it once, on a new thread).
    using EntryPoint = Function<void()>;

    /// Immediately spawns an OS thread running `entryPoint`. `name` is
    /// applied to the new thread via Thread::SetCurrentThreadName() for
    /// easier debugging/profiling.
    WorkerThread(StringView name, EntryPoint entryPoint);

    /// Joins the thread if it's still joinable, so a WorkerThread never
    /// outlives the object that owns it (and never gets silently
    /// terminated when the process exits mid-task, which std::thread
    /// would otherwise `std::terminate()` on if left joinable).
    ~WorkerThread();

    /// Blocks the calling thread until this worker's entry point returns.
    /// Safe to call even if already joined (no-op in that case).
    void Join();

    /// True from just before the entry point starts running until just
    /// after it returns. Mainly useful for diagnostics/introspection.
    [[nodiscard]] bool IsRunning() const noexcept { return m_Running.load(std::memory_order_acquire); }

    [[nodiscard]] StringView GetName() const noexcept { return m_Name; }

    /// The OS-level id of this worker's thread, as reported by
    /// Thread::GetCurrentThreadId() from *inside* the thread. Useful for
    /// matching log lines ("[thread 1234] ...") back to a specific
    /// WorkerThread instance.
    [[nodiscard]] u64 GetThreadId() const noexcept { return m_ThreadId.load(std::memory_order_acquire); }

private:
    std::thread       m_Thread;
    String            m_Name;
    std::atomic<bool> m_Running{false};
    std::atomic<u64>  m_ThreadId{0};
};

} // namespace Hydra
