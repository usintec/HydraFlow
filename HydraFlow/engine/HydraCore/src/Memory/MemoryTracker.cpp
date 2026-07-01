#include <HydraCore/Memory/MemoryTracker.h>

#include <algorithm>
#include <cstdio>
#include <format>

namespace Hydra {

// =============================================================================
// Recording
// =============================================================================

void MemoryTracker::Track(void*       address,
                          usize       size,
                          usize       alignment,
                          const char* file,
                          i32         line,
                          const char* function,
                          StringView  allocatorName)
{
    if (!address) return;

    std::lock_guard lock(m_Mutex);
    if (!m_Enabled) return;

    AllocationRecord rec;
    rec.address       = address;
    rec.size          = size;
    rec.alignment     = alignment;
    rec.file          = file;
    rec.line          = line;
    rec.function      = function;
    rec.sequenceId    = m_NextSeqId++;
    rec.timestampNs   = AllocationRecord::NowNs();
    rec.allocatorName = String(allocatorName);

    m_Records[address] = rec;

    // Update statistics
    m_Stats.totalAllocated    += size;
    m_Stats.currentUsage      += size;
    m_Stats.allocationCount   += 1;
    m_Stats.activeAllocations += 1;
    if (m_Stats.currentUsage > m_Stats.peakUsage)
        m_Stats.peakUsage = m_Stats.currentUsage;

    // Notify profiling hooks
    for (auto* hook : m_ProfilingHooks)
        hook->OnAllocate(rec);
}

void MemoryTracker::Untrack(void* address)
{
    if (!address) return;

    std::lock_guard lock(m_Mutex);
    if (!m_Enabled) return;

    auto it = m_Records.find(address);
    if (it == m_Records.end()) return;   // not tracked (may be a system alloc)

    const AllocationRecord& rec = it->second;
    const usize size = rec.size;
    const String allocName = rec.allocatorName;

    // Update statistics
    m_Stats.totalFreed        += size;
    m_Stats.currentUsage      -= size;
    m_Stats.deallocationCount += 1;
    m_Stats.activeAllocations -= 1;

    // Notify profiling hooks BEFORE erasing the record
    for (auto* hook : m_ProfilingHooks)
        hook->OnDeallocate(address, size, allocName);

    m_Records.erase(it);
}

// =============================================================================
// Statistics
// =============================================================================

const MemoryStatistics& MemoryTracker::GetStatistics() const noexcept
{
    return m_Stats;
}

Vector<AllocationRecord> MemoryTracker::GetActiveAllocations() const
{
    std::lock_guard lock(m_Mutex);
    Vector<AllocationRecord> result;
    result.reserve(m_Records.size());
    for (const auto& [ptr, rec] : m_Records)
        result.push_back(rec);
    return result;
}

bool MemoryTracker::HasLeaks() const noexcept
{
    return m_Stats.HasLeaks();
}

usize MemoryTracker::ActiveAllocCount() const noexcept
{
    return m_Stats.activeAllocations;
}

void MemoryTracker::ResetStatistics()
{
    std::lock_guard lock(m_Mutex);
    m_Stats.Reset();
    m_Records.clear();
    m_NextSeqId = 1;
}

// =============================================================================
// Leak reporting
// =============================================================================

void MemoryTracker::ReportLeaks()
{
    std::lock_guard lock(m_Mutex);
    if (m_Records.empty()) return;

    const usize count = m_Records.size();

    // Collect and sort by sequence id for deterministic output.
    Vector<const AllocationRecord*> sorted;
    sorted.reserve(count);
    for (const auto& [_, rec] : m_Records)
        sorted.push_back(&rec);
    std::sort(sorted.begin(), sorted.end(),
              [](const AllocationRecord* a, const AllocationRecord* b) {
                  return a->sequenceId < b->sequenceId;
              });

    for (auto* hook : m_LeakHooks)
        hook->OnLeakReportBegin(count);

    for (const auto* rec : sorted)
        for (auto* hook : m_LeakHooks)
            hook->OnLeakDetected(*rec);

    for (auto* hook : m_LeakHooks)
        hook->OnLeakReportEnd();
}

void MemoryTracker::DumpLeaks() const
{
    std::lock_guard lock(m_Mutex);
    if (m_Records.empty()) return;

    fprintf(stderr, "[MemoryTracker] %zu leak(s) detected:\n", m_Records.size());

    for (const auto& [ptr, rec] : m_Records) {
        if (rec.file)
            fprintf(stderr, "  #%llu  %p  %zu bytes  (%s:%d  %s)  [%s]\n",
                    (unsigned long long)rec.sequenceId,
                    rec.address,
                    rec.size,
                    rec.file, rec.line, rec.function ? rec.function : "?",
                    rec.allocatorName.c_str());
        else
            fprintf(stderr, "  #%llu  %p  %zu bytes  [%s]\n",
                    (unsigned long long)rec.sequenceId,
                    rec.address,
                    rec.size,
                    rec.allocatorName.c_str());
    }
}

// =============================================================================
// Hook management
// =============================================================================

void MemoryTracker::AddProfilingHook(IMemoryProfilingHook* hook)
{
    if (!hook) return;
    std::lock_guard lock(m_Mutex);
    if (std::find(m_ProfilingHooks.begin(), m_ProfilingHooks.end(), hook) == m_ProfilingHooks.end())
        m_ProfilingHooks.push_back(hook);
}

void MemoryTracker::RemoveProfilingHook(IMemoryProfilingHook* hook)
{
    std::lock_guard lock(m_Mutex);
    auto it = std::find(m_ProfilingHooks.begin(), m_ProfilingHooks.end(), hook);
    if (it != m_ProfilingHooks.end())
        m_ProfilingHooks.erase(it);
}

void MemoryTracker::AddLeakDetectionHook(ILeakDetectionHook* hook)
{
    if (!hook) return;
    std::lock_guard lock(m_Mutex);
    if (std::find(m_LeakHooks.begin(), m_LeakHooks.end(), hook) == m_LeakHooks.end())
        m_LeakHooks.push_back(hook);
}

void MemoryTracker::RemoveLeakDetectionHook(ILeakDetectionHook* hook)
{
    std::lock_guard lock(m_Mutex);
    auto it = std::find(m_LeakHooks.begin(), m_LeakHooks.end(), hook);
    if (it != m_LeakHooks.end())
        m_LeakHooks.erase(it);
}

// =============================================================================
// Enable / disable
// =============================================================================

void MemoryTracker::SetEnabled(bool enabled) noexcept
{
    m_Enabled = enabled;
}

bool MemoryTracker::IsEnabled() const noexcept
{
    return m_Enabled;
}

} // namespace Hydra
