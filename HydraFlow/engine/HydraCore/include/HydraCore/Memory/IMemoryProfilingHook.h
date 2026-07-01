#pragma once

#include <HydraCore/Common/Platform.h>
#include <HydraCore/Common/Types.h>
#include <HydraCore/Memory/AllocationRecord.h>

namespace Hydra {

// =============================================================================
// IMemoryProfilingHook  (interface only)
//
// Implement and register with MemoryTracker::AddProfilingHook() to receive
// callbacks on every tracked allocation and deallocation.
//
// Lifetime contract:
//   The hook object must outlive the MemoryTracker (or be removed via
//   RemoveProfilingHook() before destruction).  The tracker holds raw,
//   non-owning pointers.
//
// Threading:
//   Callbacks are invoked on the thread that called Allocate() / Deallocate().
//   Implementations are responsible for their own thread-safety if needed.
// =============================================================================

class HYDRA_API IMemoryProfilingHook
{
public:
    virtual ~IMemoryProfilingHook() = default;

    // -------------------------------------------------------------------------
    // Callbacks
    // -------------------------------------------------------------------------

    /// Called immediately after an allocation is recorded in the tracker.
    /// @param record  Complete record for the new allocation.
    virtual void OnAllocate(const AllocationRecord& record) = 0;

    /// Called immediately after a deallocation is processed by the tracker.
    /// @param address        The pointer being freed.
    /// @param size           The size that was freed (may be 0 if unknown).
    /// @param allocatorName  Name of the allocator that originally allocated this.
    virtual void OnDeallocate(void*       address,
                              usize       size,
                              StringView  allocatorName) = 0;
};

} // namespace Hydra
