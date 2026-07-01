#pragma once

#include <HydraCore/Common/Platform.h>
#include <HydraCore/Common/Types.h>

#include <new>   // std::align_val_t, std::bad_alloc

namespace Hydra {

// =============================================================================
// IAllocator
//
// Abstract interface for all HydraCore custom allocators.
//
// Contract:
//   – Allocate() returns a non-null pointer or nullptr on OOM.
//   – The returned pointer is aligned to at least `alignment` bytes.
//   – Alignment must be a power of two.
//   – Deallocate() accepts a pointer previously returned by Allocate() on the
//     same allocator.  size must match the size passed to Allocate() (or 0 if
//     unknown — individual implementations document whether 0 is acceptable).
//   – Deallocate(nullptr, *) is always a no-op.
//   – Reset() reclaims all memory owned by this allocator in one shot.
//     Whether it calls destructors is implementation-defined.
// =============================================================================

class HYDRA_API IAllocator
{
public:
    virtual ~IAllocator() = default;

    // -------------------------------------------------------------------------
    // Core interface
    // -------------------------------------------------------------------------

    /// Allocate `size` bytes with at least `alignment`-byte alignment.
    /// Returns nullptr when the allocator is out of memory.
    [[nodiscard]] virtual void* Allocate(
        usize size,
        usize alignment = alignof(std::max_align_t)) = 0;

    /// Deallocate a pointer previously returned by Allocate().
    /// `size` must match the original allocation size (or 0 if not tracked).
    virtual void Deallocate(void* ptr, usize size = 0) = 0;

    /// Release all memory held by this allocator.
    /// After Reset() the allocator behaves as if freshly constructed.
    virtual void Reset() = 0;

    // -------------------------------------------------------------------------
    // Diagnostics
    // -------------------------------------------------------------------------

    /// Unique human-readable name (for logging and profiling tools).
    [[nodiscard]] virtual StringView GetName() const noexcept = 0;

    /// Bytes currently in use (allocated but not yet freed).
    [[nodiscard]] virtual usize GetUsedBytes()      const noexcept = 0;

    /// Total capacity of this allocator's backing buffer.
    [[nodiscard]] virtual usize GetCapacityBytes()  const noexcept = 0;

    /// Number of live (outstanding) allocations.
    [[nodiscard]] virtual usize GetAllocationCount() const noexcept = 0;

    // -------------------------------------------------------------------------
    // Convenience helpers (not virtual — implemented in terms of the above)
    // -------------------------------------------------------------------------

    /// Allocate storage for a single T and placement-new it with the given args.
    /// Caller is responsible for calling the destructor and Deallocate().
    template<typename T, typename... Args>
    [[nodiscard]] T* New(Args&&... args)
    {
        void* mem = Allocate(sizeof(T), alignof(T));
        if (!mem) return nullptr;
        return new (mem) T(std::forward<Args>(args)...);
    }

    /// Call T's destructor then Deallocate.
    template<typename T>
    void Delete(T* ptr)
    {
        if (!ptr) return;
        ptr->~T();
        Deallocate(ptr, sizeof(T));
    }
};

} // namespace Hydra
