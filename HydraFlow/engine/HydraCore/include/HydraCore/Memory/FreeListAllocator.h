#pragma once

#include <HydraCore/Common/Platform.h>
#include <HydraCore/Common/Types.h>
#include <HydraCore/Memory/IAllocator.h>
#include <HydraCore/Memory/Alignment.h>

namespace Hydra {

// =============================================================================
// FreeListAllocator
//
// General-purpose variable-size allocator over a fixed backing buffer.
// Uses a first-fit free list sorted by address; adjacent free blocks are
// coalesced on every Deallocate() call.
//
// Block layout (when allocated):
//   [gap bytes for header alignment][AllocHeader][user data ← returned ptr]
//
// Block layout (when free):
//   [FreeNode: totalSize + next ptr][...]
//
// On Deallocate(ptr):
//   The AllocHeader stored at (ptr - sizeof(AllocHeader)) records the block
//   start offset, allowing the FreeNode to be reconstructed.
//
// Best for: general-purpose allocations with variable lifetimes where a pool
// would be impractical.  Slower than pool/linear due to free-list traversal.
//
// Properties:
//   – O(n) allocation (first-fit search), O(n) deallocation (in-order insert)
//   – Coalesces adjacent free blocks on every deallocation
//   – Alignment overhead: sizeof(AllocHeader) = 16 bytes per allocation
// =============================================================================

class HYDRA_API FreeListAllocator final : public IAllocator
{
public:
    // -------------------------------------------------------------------------
    // Construction
    // -------------------------------------------------------------------------

    explicit FreeListAllocator(usize      capacity,
                               StringView name = "FreeListAllocator");

    FreeListAllocator(void*      buffer,
                      usize      capacity,
                      StringView name = "FreeListAllocator");

    ~FreeListAllocator() override;

    FreeListAllocator(const FreeListAllocator&)            = delete;
    FreeListAllocator& operator=(const FreeListAllocator&) = delete;
    FreeListAllocator(FreeListAllocator&&)                 = delete;
    FreeListAllocator& operator=(FreeListAllocator&&)      = delete;

    // -------------------------------------------------------------------------
    // IAllocator
    // -------------------------------------------------------------------------

    [[nodiscard]] void* Allocate(usize size,
                                 usize alignment = alignof(std::max_align_t)) override;

    void Deallocate(void* ptr, usize size = 0) override;

    void Reset() override;

    [[nodiscard]] StringView GetName()            const noexcept override { return m_Name;       }
    [[nodiscard]] usize      GetUsedBytes()        const noexcept override { return m_UsedBytes;  }
    [[nodiscard]] usize      GetCapacityBytes()    const noexcept override { return m_Capacity;   }
    [[nodiscard]] usize      GetAllocationCount()  const noexcept override { return m_AllocCount; }

    [[nodiscard]] usize GetFreeBytes() const noexcept { return m_Capacity - m_UsedBytes; }

private:
    // Stored at the START of every free block.
    struct FreeNode
    {
        usize     totalSize;   ///< total bytes from this node to end of block
        FreeNode* next;
    };

    // Stored immediately BEFORE the user pointer in every allocated block.
    // Allows Deallocate() to locate the block start without external bookkeeping.
    struct AllocHeader
    {
        usize totalSize;   ///< total block size from FreeNode / block start
        usize adjustment;  ///< bytes from block start to this AllocHeader
    };

    static constexpr usize k_MinBlockSize = sizeof(FreeNode);

    void  InitFreeList();
    void  InsertFreeNode(FreeNode* node);   ///< sorted insert + coalesce

    byte*     m_Buffer     = nullptr;
    usize     m_Capacity   = 0;
    FreeNode* m_FreeList   = nullptr;
    usize     m_UsedBytes  = 0;
    usize     m_AllocCount = 0;
    bool      m_Owning     = false;
    String    m_Name;
};

} // namespace Hydra
