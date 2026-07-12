---
name: JobScheduler two-pass enqueue
description: Critical race in Schedule(TaskGraph&) — single-loop that both reads remaining-counts AND pushes ready jobs allows workers to cascade-complete before all counts are snapshotted, causing double-push of dependents.
---

## The rule
`JobScheduler::Schedule(TaskGraph&)` must snapshot all dependency counts into each `Job::RemainingDependencies()` in **one pass** (no queue pushes), then push initially-ready jobs in a **separate second pass** only after all counts are stable.

**Why:** If jobs are pushed to the `WorkerQueue` during the same loop that reads remaining-counts, a fast worker can execute a just-pushed job and call `OnJobFinished` — which decrements dependent counts and pushes newly-ready jobs — while the host thread's loop is still reading the remaining-count for that same dependent. The host thread may then read 0 (cascade already fired) and push the dependent a second time, executing it twice.

**How to apply:** Any time you add a batch-submission path that (1) wires dependency edges and (2) pushes some ready jobs, enforce the two-pass invariant:
- Pass 1: for every job, call `m_DependencyGraph.GetRemainingDependencyCount()` and store it via `job->RemainingDependencies().store(...)`. No `m_ReadyQueue.Push()` calls.
- Pass 2: iterate the same jobs again, check `job->RemainingDependencies().load() == 0`, and push those. Workers may now start running, but all counts are already frozen from Pass 1, so `OnJobFinished` cascade decrements are safe.
