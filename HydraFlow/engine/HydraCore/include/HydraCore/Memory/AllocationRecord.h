#pragma once

#include <HydraCore/Common/Platform.h>
#include <HydraCore/Common/Types.h>

#include <chrono>

namespace Hydra {

// =============================================================================
// AllocationRecord
//
// Immutable snapshot of one tracked allocation.  Stored inside MemoryTracker
// and passed to IMemoryProfilingHook callbacks.
//
// Source-location fields (file, line, function) are populated by the
// HYDRA_ALLOC / HYDRA_NEW macros via __FILE__ / __LINE__ / __func__.
// They are null / 0 when the allocation bypasses those macros.
// =============================================================================

struct HYDRA_API AllocationRecord
{
    void*       address       = nullptr;   ///< Address returned by the allocator
    usize       size          = 0;         ///< Requested allocation size in bytes
    usize       alignment     = 0;         ///< Requested alignment

    const char* file          = nullptr;   ///< Source file (__FILE__)
    i32         line          = 0;         ///< Source line (__LINE__)
    const char* function      = nullptr;   ///< Source function (__func__)

    /// Monotonically-increasing sequence number (1-based) within this tracker.
    u64         sequenceId    = 0;

    /// Wall-clock time when the allocation was recorded (nanoseconds since epoch).
    u64         timestampNs   = 0;

    /// Name of the allocator that produced this allocation, if known.
    String      allocatorName;

    // -------------------------------------------------------------------------
    // Helpers
    // -------------------------------------------------------------------------

    [[nodiscard]] static u64 NowNs() noexcept
    {
        using Clock = std::chrono::steady_clock;
        return static_cast<u64>(
            std::chrono::duration_cast<std::chrono::nanoseconds>(
                Clock::now().time_since_epoch()).count());
    }
};

} // namespace Hydra
