#pragma once

// =============================================================================
// DependencyGraph.h
//
// Tracks "job A must finish before job B may start" edges as a directed
// graph, keyed by plain integer ids (works for both JobId and TaskId,
// since they're both u64 under the hood).
//
// This class only knows about ids and edge counts — it has no idea what
// a Job or Task actually *is* or *does*. That separation keeps it simple,
// reusable, and easy to unit test: JobScheduler is the one that reacts to
// "these ids just became ready" by looking up and enqueueing the real
// Job objects.
//
// Thread-safety: all public methods lock an internal mutex, because
// JobScheduler calls OnNodeCompleted() concurrently from multiple worker
// threads as jobs finish at the same time.
// =============================================================================

#include <HydraCore/Common/Platform.h>
#include <HydraCore/Common/Types.h>
#include <HydraCore/Threading/Mutex.h>
#include <HydraCore/Jobs/JobTypes.h>

namespace Hydra {

class HYDRA_API DependencyGraph
{
public:
    DependencyGraph()  = default;
    ~DependencyGraph() = default;

    /// Registers a node with no dependencies yet. Safe to call more than
    /// once for the same id (subsequent calls are a no-op) — AddEdge()
    /// below also implicitly registers both ids it references, so this
    /// is mainly useful for nodes that have no dependencies at all and
    /// need to exist in the graph anyway.
    void AddNode(JobId id);

    /// Records that `dependent` must not run until `dependency` has
    /// completed. Both ids are implicitly registered as nodes if they
    /// aren't already. Safe to call before or after AddNode().
    void AddDependency(JobId dependent, JobId dependency);

    /// How many not-yet-completed dependencies `id` currently has. Zero
    /// means the node is ready to run right now.
    [[nodiscard]] u32 GetRemainingDependencyCount(JobId id) const;

    /// Marks `id` as completed and returns the list of dependent ids
    /// whose remaining-dependency count just dropped to zero as a
    /// result (i.e. the set of nodes that became ready *because of* this
    /// completion). The caller (JobScheduler) is responsible for
    /// actually enqueueing those ids for execution.
    [[nodiscard]] Vector<JobId> OnNodeCompleted(JobId id);

    /// Depth-first cycle check across every edge currently in the graph.
    /// JobScheduler calls this before committing a TaskGraph to
    /// execution — a cyclic dependency would otherwise mean some jobs
    /// can *never* reach zero remaining dependencies and would sit in
    /// Pending forever.
    [[nodiscard]] bool HasCycle() const;

    /// Removes all nodes and edges, returning the graph to a fresh
    /// state. JobScheduler doesn't currently reuse a single
    /// DependencyGraph across unrelated batches of work, but this is
    /// provided for callers/tests that want to.
    void Clear();

private:
    // DFS helper for HasCycle(): standard white/gray/black recursion-stack
    // coloring to detect back-edges (the hallmark of a cycle in a
    // directed graph).
    enum class VisitState : u8 { Unvisited, InProgress, Done };
    bool VisitForCycleCheck(JobId id,
                             HashMap<JobId, VisitState>& visitState) const;

    mutable Mutex m_Mutex;

    // How many unmet dependencies each node still has left.
    HashMap<JobId, u32> m_RemainingDependencyCount;

    // Adjacency list: id -> the ids that depend on it (i.e. the edges
    // pointing *out* of a dependency, towards everything waiting on it).
    HashMap<JobId, Vector<JobId>> m_Dependents;
};

} // namespace Hydra
