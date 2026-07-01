#pragma once

#include <HydraCore/Common/Platform.h>
#include <HydraCore/Common/NonCopyable.h>
#include <HydraCore/Common/Types.h>
#include <HydraCore/Memory/AllocationRecord.h>
#include <HydraCore/Memory/MemoryStatistics.h>
#include <HydraCore/Memory/IMemoryProfilingHook.h>
#include <HydraCore/Memory/ILeakDetectionHook.h>

#include <mutex>

namespace Hydra {

// =============================================================================
// MemoryTracker
//
// Tracks every Allocate() / Deallocate() call, maintains MemoryStatistics,
// and notifies registered profiling and leak-detection hooks.
//
// Thread-safety:
//   All public methods are thread-safe (protected by an internal mutex).
//   Hook callbacks are invoked while the mutex is held — hooks must not call
//   back into MemoryTracker to avoid deadlock.
//
// Enabling / disabling:
//   When disabled (SetEnabled(false)), Track() and Untrack() become no-ops.
//   Statistics continue to accumulate; hooks are not called.
// =============================================================================

class HYDRA_API MemoryTracker final : private NonCopyable
{
public:
    MemoryTracker()  = default;
    ~MemoryTracker() = default;

    // -------------------------------------------------------------------------
    // Recording
    // -------------------------------------------------------------------------

    /// Record an allocation.  Called by MemoryManager::Allocate() and the
    /// HYDRA_ALLOC family of macros.
    void Track(void*       address,
               usize       size,
               usize       alignment,
               const char* file,
               i32         line,
               const char* function,
               StringView  allocatorName = {});

    /// Remove the record for a previously tracked allocation.
    /// Called by MemoryManager::Deallocate() and HYDRA_FREE.
    void Untrack(void* address);

    // -------------------------------------------------------------------------
    // Statistics
    // -------------------------------------------------------------------------

    [[nodiscard]] const MemoryStatistics& GetStatistics() const noexcept;

    /// Snapshot of all live (not yet freed) AllocationRecords.
    [[nodiscard]] Vector<AllocationRecord> GetActiveAllocations() const;

    [[nodiscard]] bool  HasLeaks()          const noexcept;
    [[nodiscard]] usize ActiveAllocCount()  const noexcept;

    /// Reset statistics counters and clear all recorded allocations.
    void ResetStatistics();

    // -------------------------------------------------------------------------
    // Leak reporting
    // -------------------------------------------------------------------------

    /// Call all registered ILeakDetectionHook implementations for every live
    /// allocation.  No-op when there are no leaks.
    void ReportLeaks();

    /// Write a human-readable leak summary to stderr (independent of hooks).
    void DumpLeaks() const;

    // -------------------------------------------------------------------------
    // Hook management
    // -------------------------------------------------------------------------

    void AddProfilingHook(IMemoryProfilingHook*   hook);
    void RemoveProfilingHook(IMemoryProfilingHook* hook);

    void AddLeakDetectionHook(ILeakDetectionHook*   hook);
    void RemoveLeakDetectionHook(ILeakDetectionHook* hook);

    // -------------------------------------------------------------------------
    // Enable / disable
    // -------------------------------------------------------------------------

    void SetEnabled(bool enabled) noexcept;
    [[nodiscard]] bool IsEnabled() const noexcept;

private:
    mutable std::mutex                       m_Mutex;
    HashMap<void*, AllocationRecord>         m_Records;
    MemoryStatistics                         m_Stats;
    Vector<IMemoryProfilingHook*>            m_ProfilingHooks;
    Vector<ILeakDetectionHook*>              m_LeakHooks;
    bool                                     m_Enabled  = true;
    u64                                      m_NextSeqId = 1;
};

} // namespace Hydra
