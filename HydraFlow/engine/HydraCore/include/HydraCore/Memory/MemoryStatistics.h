#pragma once

#include <HydraCore/Common/Platform.h>
#include <HydraCore/Common/Types.h>

namespace Hydra {

// =============================================================================
// MemoryStatistics
//
// Counters maintained by MemoryTracker.  All values refer to bytes unless
// the field name says "Count".
// =============================================================================

struct HYDRA_API MemoryStatistics
{
    usize totalAllocated      = 0;   ///< Cumulative bytes allocated since tracker start
    usize totalFreed          = 0;   ///< Cumulative bytes freed since tracker start
    usize currentUsage        = 0;   ///< totalAllocated - totalFreed (live bytes)
    usize peakUsage           = 0;   ///< Maximum currentUsage ever observed
    usize allocationCount     = 0;   ///< Total number of Allocate() calls recorded
    usize deallocationCount   = 0;   ///< Total number of Deallocate() calls recorded
    usize activeAllocations   = 0;   ///< Live (not yet freed) allocation count

    // -------------------------------------------------------------------------
    // Convenience
    // -------------------------------------------------------------------------

    /// True when there are allocations that have not been freed.
    [[nodiscard]] bool HasLeaks() const noexcept { return activeAllocations > 0; }

    /// Reset all counters to zero.
    void Reset() noexcept
    {
        totalAllocated    = 0;
        totalFreed        = 0;
        currentUsage      = 0;
        peakUsage         = 0;
        allocationCount   = 0;
        deallocationCount = 0;
        activeAllocations = 0;
    }

    // -------------------------------------------------------------------------
    // Derived helpers (named to avoid ambiguity)
    // -------------------------------------------------------------------------

    /// Bytes freed / bytes allocated as a ratio (0 when nothing allocated).
    [[nodiscard]] f64 FreeRatio() const noexcept
    {
        if (totalAllocated == 0) return 0.0;
        return static_cast<f64>(totalFreed) / static_cast<f64>(totalAllocated);
    }
};

} // namespace Hydra
