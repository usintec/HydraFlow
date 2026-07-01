#pragma once

#include <HydraCore/Common/Platform.h>
#include <HydraCore/Common/Types.h>
#include <HydraCore/Memory/IAllocator.h>
#include <HydraCore/Memory/Alignment.h>

namespace Hydra {

// =============================================================================
// StackAllocator  (aka "Rollback allocator")
//
// A LinearAllocator extended with LIFO (last-in, first-out) deallocation and
// marker-based rollback.  A small Header is stored before each allocation to
// record the previous stack position, enabling Deallocate() to unwind.
//
// Deallocate(ptr) MUST be called in reverse allocation order.
// Violation is detected in Debug builds (HYDRA_ASSERT).
//
// Best for: hierarchical temp data where scope exit pops the whole sub-tree.
//
// Properties:
//   – O(1) allocation, O(1) LIFO deallocation
//   – GetMarker() / FreeToMarker() for bulk rollback
//   – Individual out-of-order Deallocate is undefined behaviour in Release.
// =============================================================================

/// Opaque marker: captures the allocator's current stack position.
struct StackMarker
{
    usize offset     = 0;
    usize allocCount = 0;
};

class HYDRA_API StackAllocator final : public IAllocator
{
public:
    // -------------------------------------------------------------------------
    // Construction
    // -------------------------------------------------------------------------

    explicit StackAllocator(usize capacity,
                            StringView name = "StackAllocator");

    StackAllocator(void* buffer, usize capacity,
                   StringView name = "StackAllocator");

    ~StackAllocator() override;

    StackAllocator(const StackAllocator&)            = delete;
    StackAllocator& operator=(const StackAllocator&) = delete;
    StackAllocator(StackAllocator&&)                 = delete;
    StackAllocator& operator=(StackAllocator&&)      = delete;

    // -------------------------------------------------------------------------
    // IAllocator
    // -------------------------------------------------------------------------

    /// Allocate on top of the stack with an embedded header recording the
    /// previous stack position.
    [[nodiscard]] void* Allocate(usize size,
                                 usize alignment = alignof(std::max_align_t)) override;

    /// Unwind to the position recorded in this allocation's header.
    /// ptr must be the MOST RECENTLY allocated pointer still live on the stack.
    void Deallocate(void* ptr, usize size = 0) override;

    /// Rewind to the empty state.
    void Reset() override;

    [[nodiscard]] StringView GetName()            const noexcept override { return m_Name;       }
    [[nodiscard]] usize      GetUsedBytes()        const noexcept override { return m_Offset;     }
    [[nodiscard]] usize      GetCapacityBytes()    const noexcept override { return m_Capacity;   }
    [[nodiscard]] usize      GetAllocationCount()  const noexcept override { return m_AllocCount; }

    // -------------------------------------------------------------------------
    // Marker-based rollback
    // -------------------------------------------------------------------------

    /// Capture the current top-of-stack position.
    [[nodiscard]] StackMarker GetMarker() const noexcept;

    /// Rewind to a previously captured marker in one step.
    /// All allocations made after the marker was captured are implicitly freed.
    void FreeToMarker(StackMarker marker);

    [[nodiscard]] usize GetFreeBytes() const noexcept { return m_Capacity - m_Offset; }

private:
    // Stored immediately before each user allocation (at a naturally-aligned address).
    struct Header
    {
        usize prevOffset;   ///< stack offset before this allocation (enables rollback)
        usize adjustment;   ///< bytes between end-of-Header and start of user data
    };

    byte*  m_Buffer     = nullptr;
    usize  m_Capacity   = 0;
    usize  m_Offset     = 0;
    usize  m_AllocCount = 0;
    bool   m_Owning     = false;
    String m_Name;
};

} // namespace Hydra
