#pragma once

// =============================================================================
// TaskGraph.h
//
// A reusable *blueprint* for a batch of interdependent work: add tasks
// with AddTask(), wire up ordering constraints with AddDependency(), then
// hand the whole thing to JobScheduler::Schedule(TaskGraph&) to actually
// run it. Building the graph does not touch any scheduler or spawn any
// threads — it's pure bookkeeping, so it's cheap to construct, inspect,
// validate, and even submit multiple times.
//
// Example — a 3-stage pipeline where C depends on both A and B:
//
//     TaskGraph graph;
//     TaskId a = graph.AddTask("LoadData",   [] { LoadData(); });
//     TaskId b = graph.AddTask("LoadConfig", [] { LoadConfig(); });
//     TaskId c = graph.AddTask("Process",    [] { Process(); });
//     graph.AddDependency(c, a); // Process depends on LoadData
//     graph.AddDependency(c, b); // Process depends on LoadConfig
//
//     Vector<JobHandle> handles = scheduler.Schedule(graph);
//     handles.back().Wait(); // wait for "Process" (the last task added)
// =============================================================================

#include <HydraCore/Common/Types.h>
#include <HydraCore/Jobs/JobTypes.h>
#include <HydraCore/Jobs/Task.h>

#include <utility>

namespace Hydra {

class TaskGraph
{
public:
    TaskGraph()  = default;
    ~TaskGraph() = default;

    /// Adds a new task to the graph and returns the TaskId used to refer
    /// to it in AddDependency() calls. Tasks start with no dependencies
    /// (i.e. ready to run immediately) unless AddDependency() says
    /// otherwise.
    TaskId AddTask(StringView name, Task::TaskFunction function,
                    JobPriority priority = JobPriority::Normal, JobKind kind = JobKind::CPU)
    {
        const TaskId id = static_cast<TaskId>(m_Tasks.size());
        m_Tasks.emplace_back(id, name, std::move(function), priority, kind);
        return id;
    }

    /// Declares that `dependent` must not run until `dependsOn` has
    /// completed. Order of AddDependency() calls doesn't matter; cycles
    /// (e.g. A depends on B which depends on A) are only detected later,
    /// at Submit()/Schedule() time via Validate(), since that's the
    /// first point a cycle would actually be a problem.
    void AddDependency(TaskId dependent, TaskId dependsOn)
    {
        HYDRA_ASSERT(dependent < m_Tasks.size() && "AddDependency(): unknown dependent TaskId");
        HYDRA_ASSERT(dependsOn < m_Tasks.size() && "AddDependency(): unknown dependsOn TaskId");
        m_Edges.emplace_back(dependent, dependsOn);
    }

    [[nodiscard]] usize GetTaskCount() const noexcept { return m_Tasks.size(); }
    [[nodiscard]] const Vector<Task>& GetTasks() const noexcept { return m_Tasks; }

    /// Every dependency edge as (dependent, dependsOn) pairs — "dependent
    /// runs after dependsOn". Consumed by JobScheduler when translating
    /// this graph into real Jobs wired into a DependencyGraph.
    [[nodiscard]] const Vector<std::pair<TaskId, TaskId>>& GetEdges() const noexcept { return m_Edges; }

    /// Checks the graph for cycles using Kahn's algorithm (repeatedly
    /// remove nodes with zero remaining in-degree; if any nodes are left
    /// over once no more can be removed, they're part of a cycle).
    /// Returns true if the graph is a valid DAG (safe to schedule),
    /// false if it contains a cycle.
    ///
    /// JobScheduler calls this before creating any Jobs from the graph —
    /// a cyclic TaskGraph would otherwise produce Jobs that can never
    /// reach zero remaining dependencies and would sit Pending forever.
    [[nodiscard]] bool Validate() const
    {
        if (m_Tasks.empty())
        {
            return true; // An empty graph is trivially acyclic.
        }

        Vector<u32> inDegree(m_Tasks.size(), 0);
        HashMap<TaskId, Vector<TaskId>> dependents;
        for (const auto& [dependent, dependsOn] : m_Edges)
        {
            inDegree[dependent] += 1;
            dependents[dependsOn].push_back(dependent);
        }

        // Seed the queue with every task that has no unmet dependencies.
        Vector<TaskId> ready;
        for (usize i = 0; i < m_Tasks.size(); ++i)
        {
            if (inDegree[i] == 0)
            {
                ready.push_back(static_cast<TaskId>(i));
            }
        }

        usize processedCount = 0;
        while (!ready.empty())
        {
            const TaskId current = ready.back();
            ready.pop_back();
            ++processedCount;

            const auto it = dependents.find(current);
            if (it == dependents.end())
            {
                continue;
            }
            for (const TaskId next : it->second)
            {
                if (--inDegree[next] == 0)
                {
                    ready.push_back(next);
                }
            }
        }

        // If Kahn's algorithm couldn't process every task, whatever's
        // left over is stuck in a cycle (each still has >0 in-degree
        // that can never reach zero because its remaining dependencies
        // are, transitively, waiting on it).
        return processedCount == m_Tasks.size();
    }

private:
    Vector<Task> m_Tasks;
    Vector<std::pair<TaskId, TaskId>> m_Edges; ///< (dependent, dependsOn)
};

} // namespace Hydra
