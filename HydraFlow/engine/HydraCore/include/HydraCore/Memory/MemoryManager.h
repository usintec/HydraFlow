#pragma once

#include <HydraCore/Common/Platform.h>
#include <HydraCore/Common/NonCopyable.h>
#include <HydraCore/Common/Types.h>
#include <HydraCore/Memory/IAllocator.h>
#include <HydraCore/Memory/MemoryTracker.h>

#include <mutex>

namespace Hydra {

// =============================================================================
// MemoryManagerSettings
// =============================================================================

struct MemoryManagerSettings
{
    bool enableTracking  = true;   ///< Enable MemoryTracker (slight overhead)
    bool reportLeaksOnShutdown = true;  ///< Call DumpLeaks() during Shutdown()
};

// =============================================================================
// MemoryManager
//
// Global façade for the HydraCore Memory module.
//
// Responsibilities:
//   1. Owns the global MemoryTracker instance.
//   2. Provides a registry of named custom allocators.
//   3. Exposes Allocate() / Deallocate() for use by the HYDRA_ALLOC macros;
//      these go through the system allocator AND the tracker.
//
// All methods are thread-safe.
//
// Note: the HYDRA_ALLOC / HYDRA_FREE macros are defined in MemoryMacros.h and
// wrap MemoryManager::Allocate() / Deallocate() with source-location capture.
// =============================================================================

class HYDRA_API MemoryManager final : private NonCopyable
{
public:
    // -------------------------------------------------------------------------
    // Lifecycle
    // -------------------------------------------------------------------------

    static void Initialize(MemoryManagerSettings settings = {});
    static void Shutdown();
    static bool IsInitialized() noexcept;

    // -------------------------------------------------------------------------
    // Tracked system allocation
    //
    // Backed by aligned operator new / operator delete.
    // Goes through the global MemoryTracker when tracking is enabled.
    // -------------------------------------------------------------------------

    [[nodiscard]] static void* Allocate(usize       size,
                                        usize       alignment,
                                        const char* file,
                                        i32         line,
                                        const char* function,
                                        StringView  allocatorName = "System");

    static void Deallocate(void* ptr, usize size = 0);

    // -------------------------------------------------------------------------
    // Named allocator registry
    // -------------------------------------------------------------------------

    /// Register a named allocator.  Replaces any existing entry with the
    /// same name.  The MemoryManager takes ownership.
    static void RegisterAllocator(String name, UniquePtr<IAllocator> allocator);

    /// Retrieve a previously registered allocator by name.
    /// Returns nullptr when the name is not found.
    [[nodiscard]] static IAllocator* GetAllocator(StringView name);

    // -------------------------------------------------------------------------
    // Tracker access
    // -------------------------------------------------------------------------

    [[nodiscard]] static MemoryTracker& GetTracker();

    // -------------------------------------------------------------------------
    // Leak reporting convenience
    // -------------------------------------------------------------------------

    [[nodiscard]] static bool HasLeaks();
    static void               DumpLeaks();

private:
    MemoryManager() = default;

    static bool                            s_Initialized;
    static MemoryManagerSettings           s_Settings;
    static MemoryTracker                   s_Tracker;
    static HashMap<String, UniquePtr<IAllocator>> s_Allocators;
    static std::mutex                      s_Mutex;
};

} // namespace Hydra
