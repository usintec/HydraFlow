#pragma once

// =============================================================================
// ParallelFor.h
//
// A small convenience helper built on top of ThreadPool that splits a
// simple index range [begin, end) into contiguous chunks, one per worker
// thread, and runs them in parallel — the classic "data-parallel loop"
// pattern used all over scientific computing (e.g. applying an operation
// to every element of a large array).
//
//     ThreadPool pool;
//     std::vector<float> data(1'000'000);
//     Threading::ParallelFor(pool, 0, data.size(), [&](usize i)
//     {
//         data[i] = std::sqrt(data[i]);
//     });
//     // blocks until every chunk has finished
//
// This is intentionally simple (fixed chunking, no work-stealing) — for
// HydraCore's needs it turns "one big loop" into "hardware_concurrency()
// chunks running on the pool", which is enough to get real CPU
// parallelism without adding a second, more complex scheduler.
// =============================================================================

#include <HydraCore/Common/Types.h>
#include <HydraCore/Threading/ThreadPool.h>
#include <HydraCore/Threading/Future.h>

#include <algorithm>

namespace Hydra::Threading {

/// Calls `fn(i)` for every index `i` in [begin, end), splitting the range
/// into roughly `pool.GetThreadCount()` contiguous chunks and running one
/// chunk per Submit()'d task. Blocks the calling thread until every chunk
/// has finished (and re-throws the first exception seen, if any chunk's
/// `fn` threw).
///
/// `minChunkSize` prevents over-splitting tiny ranges into more chunks
/// than is worth the task-submission overhead (e.g. a 4-element range on
/// a 32-thread pool should run as 1 chunk, not 32 one-element chunks).
template<typename Fn>
void ParallelFor(ThreadPool& pool, usize begin, usize end, Fn&& fn, usize minChunkSize = 1)
{
    if (begin >= end)
    {
        return; // Empty range — nothing to do.
    }

    const usize total       = end - begin;
    const usize workerCount = std::max<usize>(1, pool.GetThreadCount());

    // Round up so we cover the full range even when `total` doesn't
    // divide evenly by `workerCount`.
    const usize chunkSize = std::max<usize>(minChunkSize, (total + workerCount - 1) / workerCount);

    // One Future per chunk; we hold onto all of them so we can wait for
    // every chunk to finish before returning (making ParallelFor behave
    // like a normal, blocking for-loop from the caller's point of view).
    Vector<Future<void>> chunkFutures;
    chunkFutures.reserve((total + chunkSize - 1) / chunkSize);

    for (usize chunkStart = begin; chunkStart < end; chunkStart += chunkSize)
    {
        const usize chunkEnd = std::min(end, chunkStart + chunkSize);

        chunkFutures.push_back(pool.Submit([chunkStart, chunkEnd, &fn]()
        {
            for (usize i = chunkStart; i < chunkEnd; ++i)
            {
                fn(i);
            }
        }));
    }

    // Get() (rather than Wait()) so an exception thrown inside any chunk
    // propagates out to the caller instead of being silently swallowed.
    for (auto& future : chunkFutures)
    {
        future.Get();
    }
}

} // namespace Hydra::Threading
