#pragma once

#include <HydraCore/Common/Platform.h>
#include <HydraCore/Common/Types.h>
#include <HydraCore/Memory/AllocationRecord.h>

namespace Hydra {

// =============================================================================
// ILeakDetectionHook  (interface only)
//
// Implement and register with MemoryTracker::AddLeakDetectionHook() to receive
// callbacks when a leak report is generated (e.g. at MemoryManager::Shutdown()
// or on an explicit call to MemoryTracker::ReportLeaks()).
//
// The hooks are called in registration order.
//
// Lifetime contract:
//   The hook object must outlive the MemoryTracker (or be removed before
//   destruction).  The tracker holds raw, non-owning pointers.
// =============================================================================

class HYDRA_API ILeakDetectionHook
{
public:
    virtual ~ILeakDetectionHook() = default;

    // -------------------------------------------------------------------------
    // Callbacks
    // -------------------------------------------------------------------------

    /// Called before the first leak record is reported.
    /// @param leakCount  Total number of live allocations detected.
    virtual void OnLeakReportBegin(usize leakCount) = 0;

    /// Called once for each live (unfree'd) allocation.
    /// Called between OnLeakReportBegin() and OnLeakReportEnd().
    virtual void OnLeakDetected(const AllocationRecord& record) = 0;

    /// Called after all leak records have been reported.
    virtual void OnLeakReportEnd() = 0;
};

} // namespace Hydra
