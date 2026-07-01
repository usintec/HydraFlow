#pragma once

#include <HydraCore/Common/Platform.h>
#include <HydraCore/Common/Types.h>
#include <HydraCore/Memory/IAllocator.h>
#include <HydraCore/Memory/Alignment.h>

namespace Hydra {

// =============================================================================
// LinearAllocator  (aka "Bump-pointer allocator")
//
// The simplest possible allocator: a pointer that advances on every Allocate().
// Individual deallocations are impossible — the only way to reclaim memory is
// to call Reset(), which rewinds the pointer to the start.
//
// Best for: per-frame scratch memory, transient data that lives for one scope.
//
// Properties:
//   – O(1) allocation
//   – O(1) reset (all at once)
//   – Zero fragmentation
//   – Individual Deallocate() is a no-op
// =============================================================================

class HYDRA_API LinearAllocator final : public IAllocator
{
public:
    // -------------------------------------------------------------------------
    // Construction
    // -------------------------------------------------------------------------

    /// Allocate a new internal buffer of `capacity` bytes.
    explicit LinearAllocator(usize capacity,
                             StringView name = "LinearAllocator");

    /// Use a caller-supplied buffer (non-owning).
    /// The buffer must remain valid for the lifetime of this allocator.
    LinearAllocator(void* buffer, usize capacity,
                    StringView name = "LinearAllocator");

    ~LinearAllocator() override;

    // Non-copyable (owns or borrows a buffer)
    LinearAllocator(const LinearAllocator&)            = delete;
    LinearAllocator& operator=(const LinearAllocator&) = delete;
    LinearAllocator(LinearAllocator&&)                 = delete;
    LinearAllocator& operator=(LinearAllocator&&)      = delete;

    // -------------------------------------------------------------------------
    // IAllocator
    // -------------------------------------------------------------------------

    /// Returns a pointer to the next aligned position in the buffer.
    /// Returns nullptr when the buffer is exhausted.
    [[nodiscard]] void* Allocate(usize size,
                                 usize alignment = alignof(std::max_align_t)) override;

    /// No-op.  Individual allocations cannot be freed from a LinearAllocator.
    void Deallocate(void* ptr, usize size = 0) override;

    /// Rewind the bump pointer to the beginning of the buffer.
    void Reset() override;

    [[nodiscard]] StringView GetName()            const noexcept override { return m_Name;       }
    [[nodiscard]] usize      GetUsedBytes()        const noexcept override { return m_Offset;     }
    [[nodiscard]] usize      GetCapacityBytes()    const noexcept override { return m_Capacity;   }
    [[nodiscard]] usize      GetAllocationCount()  const noexcept override { return m_AllocCount; }

    // -------------------------------------------------------------------------
    // Extra diagnostics
    // -------------------------------------------------------------------------

    [[nodiscard]] usize GetFreeBytes() const noexcept { return m_Capacity - m_Offset; }

private:
    byte*  m_Buffer     = nullptr;
    usize  m_Capacity   = 0;
    usize  m_Offset     = 0;
    usize  m_AllocCount = 0;
    bool   m_Owning     = false;   ///< true if we own the buffer (delete[] on dtor)
    String m_Name;
};

} // namespace Hydra
