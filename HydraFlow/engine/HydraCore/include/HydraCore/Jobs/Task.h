#pragma once

// =============================================================================
// Task.h
//
// A Task is a *node in a TaskGraph* — a not-yet-scheduled description of
// work (name + function + priority + kind) plus the TaskId it's known by
// within that graph. It intentionally looks a lot like Job, because it
// basically becomes one: when a TaskGraph is submitted to a JobScheduler,
// every Task is turned into a real Job (with a scheduler-assigned JobId)
// and wired into the DependencyGraph according to the edges recorded on
// the TaskGraph.
//
// The reason Task exists as a separate type from Job rather than reusing
// Job directly: a TaskGraph is a reusable, inert *blueprint* — you can
// build one, add tasks and dependencies, and submit it to a scheduler
// multiple times (e.g. "run this same processing graph once per frame").
// Job, by contrast, carries live scheduling state (JobState, remaining
// dependency count, a completion condition variable) that only makes
// sense for one specific run. Keeping them separate means submitting a
// TaskGraph twice creates two independent sets of Jobs rather than
// reusing (and corrupting) the state of the previous run.
// =============================================================================

#include <HydraCore/Common/Types.h>
#include <HydraCore/Jobs/JobTypes.h>

namespace Hydra {

class Task
{
public:
    using TaskFunction = Function<void()>;

    Task(TaskId id, StringView name, TaskFunction function, JobPriority priority, JobKind kind)
        : m_Id(id)
        , m_Name(name)
        , m_Function(std::move(function))
        , m_Priority(priority)
        , m_Kind(kind)
    {
    }

    [[nodiscard]] TaskId GetId() const noexcept { return m_Id; }
    [[nodiscard]] const String& GetName() const noexcept { return m_Name; }
    [[nodiscard]] JobPriority GetPriority() const noexcept { return m_Priority; }
    [[nodiscard]] JobKind GetKind() const noexcept { return m_Kind; }

    /// Returns a copy of the stored function. TaskGraph::Submit() needs
    /// to hand a fresh copy to each new Job it creates (since a graph
    /// can be submitted more than once), so Task keeps its own copy
    /// rather than moving it out on first use.
    [[nodiscard]] const TaskFunction& GetFunction() const noexcept { return m_Function; }

private:
    TaskId       m_Id;
    String       m_Name;
    TaskFunction m_Function;
    JobPriority  m_Priority;
    JobKind      m_Kind;
};

} // namespace Hydra
