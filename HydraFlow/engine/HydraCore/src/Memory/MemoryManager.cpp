#include <HydraCore/Memory/MemoryManager.h>

#include <new>
#include <algorithm>
#include <cstdlib>

namespace Hydra {

// =============================================================================
// Static member definitions
// =============================================================================

bool                                  MemoryManager::s_Initialized = false;
MemoryManagerSettings                 MemoryManager::s_Settings;
MemoryTracker                         MemoryManager::s_Tracker;
HashMap<String, UniquePtr<IAllocator>> MemoryManager::s_Allocators;
std::mutex                            MemoryManager::s_Mutex;

// =============================================================================
// Lifecycle
// =============================================================================

void MemoryManager::Initialize(MemoryManagerSettings settings)
{
    std::lock_guard lock(s_Mutex);
    if (s_Initialized) return;

    s_Settings    = settings;
    s_Initialized = true;

    s_Tracker.SetEnabled(settings.enableTracking);
}

void MemoryManager::Shutdown()
{
    std::lock_guard lock(s_Mutex);
    if (!s_Initialized) return;

    if (s_Settings.reportLeaksOnShutdown && s_Tracker.HasLeaks())
        s_Tracker.DumpLeaks();

    s_Tracker.ResetStatistics();
    s_Allocators.clear();
    s_Initialized = false;
}

bool MemoryManager::IsInitialized() noexcept
{
    return s_Initialized;
}

// =============================================================================
// Tracked system allocation
// =============================================================================

void* MemoryManager::Allocate(usize       size,
                              usize       alignment,
                              const char* file,
                              i32         line,
                              const char* function,
                              StringView  allocatorName)
{
    if (size == 0) return nullptr;

    // Guarantee alignment is at least the default.
    if (alignment < alignof(std::max_align_t))
        alignment = alignof(std::max_align_t);

    void* ptr = nullptr;

    // Use aligned operator new (C++17).
    if (alignment <= alignof(std::max_align_t)) {
        ptr = ::operator new(size, std::nothrow);
    } else {
        ptr = ::operator new(size, std::align_val_t{ alignment }, std::nothrow);
    }

    if (!ptr) return nullptr;

    // Record in tracker.
    if (s_Initialized && s_Settings.enableTracking)
        s_Tracker.Track(ptr, size, alignment, file, line, function, allocatorName);

    return ptr;
}

void MemoryManager::Deallocate(void* ptr, usize /*size*/)
{
    if (!ptr) return;

    // Untrack BEFORE freeing.
    if (s_Initialized && s_Settings.enableTracking)
        s_Tracker.Untrack(ptr);

    ::operator delete(ptr);
}

// =============================================================================
// Named allocator registry
// =============================================================================

void MemoryManager::RegisterAllocator(String name, UniquePtr<IAllocator> allocator)
{
    std::lock_guard lock(s_Mutex);
    s_Allocators[std::move(name)] = std::move(allocator);
}

IAllocator* MemoryManager::GetAllocator(StringView name)
{
    std::lock_guard lock(s_Mutex);
    auto it = s_Allocators.find(String(name));
    if (it == s_Allocators.end()) return nullptr;
    return it->second.get();
}

// =============================================================================
// Tracker
// =============================================================================

MemoryTracker& MemoryManager::GetTracker()
{
    return s_Tracker;
}

bool MemoryManager::HasLeaks()
{
    return s_Tracker.HasLeaks();
}

void MemoryManager::DumpLeaks()
{
    s_Tracker.DumpLeaks();
}

} // namespace Hydra
