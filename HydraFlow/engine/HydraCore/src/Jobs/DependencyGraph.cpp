#include <HydraCore/Jobs/DependencyGraph.h>
#include <cstdio>

namespace Hydra {

void DependencyGraph::AddNode(JobId id)
{
    ScopedLock<Mutex> lock(m_Mutex);

    // operator[] default-constructs to 0/empty-vector if not already
    // present, which is exactly "register this node with no
    // dependencies/dependents yet" — so a plain lookup is enough, no
    // separate "does it exist" branch needed.
    (void)m_RemainingDependencyCount[id];
    (void)m_Dependents[id];
}

void DependencyGraph::AddDependency(JobId dependent, JobId dependency)
{
    ScopedLock<Mutex> lock(m_Mutex);

    // `dependent` gains one more thing it has to wait for...
    m_RemainingDependencyCount[dependent] += 1;
    // ...and `dependency` needs to know to notify `dependent` once it
    // finishes. Also make sure `dependency` itself is registered even if
    // nothing else referenced it yet.
    m_Dependents[dependency].push_back(dependent);
    (void)m_RemainingDependencyCount[dependency];
}

u32 DependencyGraph::GetRemainingDependencyCount(JobId id) const
{
    ScopedLock<Mutex> lock(m_Mutex);
    const auto it = m_RemainingDependencyCount.find(id);
    return (it != m_RemainingDependencyCount.end()) ? it->second : 0;
}

Vector<JobId> DependencyGraph::OnNodeCompleted(JobId id)
{
    ScopedLock<Mutex> lock(m_Mutex);

    Vector<JobId> newlyReady;

    const auto dependentsIt = m_Dependents.find(id);
    if (dependentsIt == m_Dependents.end())
    {
        return newlyReady; // Nothing depended on this node.
    }

    fprintf(stderr, "[DBG] OnNodeCompleted(%llu) dependents.size=%zu\n", (unsigned long long)id, dependentsIt->second.size());
    for (const JobId dependentId : dependentsIt->second)
    {
        auto countIt = m_RemainingDependencyCount.find(dependentId);
        u32 before = (countIt != m_RemainingDependencyCount.end()) ? countIt->second : 999;
        if (countIt != m_RemainingDependencyCount.end() && countIt->second > 0)
        {
            countIt->second -= 1;
            fprintf(stderr, "[DBG]   dependent=%llu before=%u after=%u\n", (unsigned long long)dependentId, before, countIt->second);
            if (countIt->second == 0)
            {
                newlyReady.push_back(dependentId);
            }
        }
    }

    return newlyReady;
}

bool DependencyGraph::HasCycle() const
{
    ScopedLock<Mutex> lock(m_Mutex);

    HashMap<JobId, VisitState> visitState;
    for (const auto& [id, _] : m_RemainingDependencyCount)
    {
        visitState.emplace(id, VisitState::Unvisited);
    }

    for (const auto& [id, state] : visitState)
    {
        if (state == VisitState::Unvisited)
        {
            // Re-fetch from the map each time since VisitForCycleCheck
            // mutates `visitState` as it recurses.
            if (VisitForCycleCheck(id, visitState))
            {
                return true;
            }
        }
    }

    return false;
}

bool DependencyGraph::VisitForCycleCheck(JobId id, HashMap<JobId, VisitState>& visitState) const
{
    visitState[id] = VisitState::InProgress;

    const auto dependentsIt = m_Dependents.find(id);
    if (dependentsIt != m_Dependents.end())
    {
        for (const JobId nextId : dependentsIt->second)
        {
            const VisitState nextState = visitState[nextId];

            // Reaching a node that's still "InProgress" (i.e. an
            // ancestor of itself in the current DFS path) is exactly
            // what a cycle looks like — a back-edge.
            if (nextState == VisitState::InProgress)
            {
                return true;
            }
            if (nextState == VisitState::Unvisited && VisitForCycleCheck(nextId, visitState))
            {
                return true;
            }
        }
    }

    visitState[id] = VisitState::Done;
    return false;
}

void DependencyGraph::Clear()
{
    ScopedLock<Mutex> lock(m_Mutex);
    m_RemainingDependencyCount.clear();
    m_Dependents.clear();
}

} // namespace Hydra
