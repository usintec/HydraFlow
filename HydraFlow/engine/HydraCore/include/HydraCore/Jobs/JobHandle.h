#pragma once

// =============================================================================
// JobHandle.h
//
// What JobScheduler::Schedule() hands back to the caller: a lightweight,
// copyable reference to a Job that lets you check on / wait for it
// without exposing the full Job API (or letting callers accidentally
// call Execute() themselves, which is the scheduler's job — pun intended).
//
//     JobHandle handle = scheduler.Schedule([]{ DoWork(); }, "MyJob");
//     handle.Wait();
//     if (handle.HasFailed()) { std::rethrow_exception(handle.GetException()); }
//
// Deliberately copyable (unlike Future<T>) — many independent pieces of
// code may all want to hold a handle to the same job (e.g. a TaskGraph
// caller and, internally, the scheduler's own dependency bookkeeping)
// without fighting over single ownership.
// =============================================================================

#include <HydraCore/Common/Types.h>
#include <HydraCore/Jobs/Job.h>
#include <HydraCore/Jobs/JobTypes.h>

#include <exception>

namespace Hydra {

class JobHandle
{
public:
    /// A default-constructed handle is "empty" (IsValid() == false) and
    /// isn't attached to any Job — returned e.g. when a lookup fails.
    JobHandle() = default;
    explicit JobHandle(SharedPtr<Job> job) : m_Job(std::move(job)) {}

    [[nodiscard]] bool IsValid() const noexcept { return static_cast<bool>(m_Job); }

    [[nodiscard]] JobId GetId() const noexcept { return m_Job ? m_Job->GetId() : kInvalidJobId; }
    [[nodiscard]] StringView GetName() const noexcept { return m_Job ? StringView(m_Job->GetName()) : StringView(); }

    [[nodiscard]] bool IsComplete() const
    {
        HYDRA_ASSERT(m_Job && "IsComplete() called on an empty JobHandle");
        return m_Job->IsComplete();
    }

    [[nodiscard]] bool HasFailed() const
    {
        HYDRA_ASSERT(m_Job && "HasFailed() called on an empty JobHandle");
        return m_Job->HasFailed();
    }

    [[nodiscard]] std::exception_ptr GetException() const
    {
        HYDRA_ASSERT(m_Job && "GetException() called on an empty JobHandle");
        return m_Job->GetException();
    }

    /// Blocks until the job finishes. Does not throw even if the job
    /// failed — check HasFailed()/GetException() afterwards, or use
    /// WaitAndRethrow() below if you want the exception to propagate
    /// automatically.
    void Wait() const
    {
        HYDRA_ASSERT(m_Job && "Wait() called on an empty JobHandle");
        m_Job->Wait();
    }

    /// Like Wait(), but returns false instead of blocking forever if the
    /// job doesn't finish within `milliseconds`.
    [[nodiscard]] bool WaitFor(u64 milliseconds) const
    {
        HYDRA_ASSERT(m_Job && "WaitFor() called on an empty JobHandle");
        return m_Job->WaitFor(milliseconds);
    }

    /// Convenience: waits for completion, then re-throws the job's
    /// exception if it failed. Mirrors Future<void>::Get()'s contract.
    void WaitAndRethrow() const
    {
        Wait();
        if (const std::exception_ptr exception = GetException())
        {
            std::rethrow_exception(exception);
        }
    }

private:
    SharedPtr<Job> m_Job;
};

} // namespace Hydra
