#pragma once

#include <HydraCore/Common/Platform.h>
#include <HydraCore/Common/Types.h>
#include <HydraCore/Memory/IAllocator.h>

namespace Hydra {

// =============================================================================
// PoolAllocator
//
// Fixed-size block allocator backed by a singly-linked free list.
// All blocks have the same size so allocation and deallocation are both O(1)
// with no fragmentation.
//
// Best for: large numbers of same-typed objects (particles, messages, handles).
//
// Properties:
//   – O(1) allocation and deallocation
//   – Zero fragmentation (all blocks are equal)
//   – blockSize must be >= sizeof(void*) (free list pointer is stored in the
//     unused block memory)
//   – All blocks are aligned to `blockAlignment`
// =============================================================================

class HYDRA_API PoolAllocator final : public IAllocator
{
public:
    // -------------------------------------------------------------------------
    // Construction
    // -------------------------------------------------------------------------

    /// Create a pool of `blockCount` blocks each of `blockSize` bytes,
    /// aligned to `blockAlignment`.
    PoolAllocator(usize      blockSize,
                  usize      blockCount,
                  usize      blockAlignment = alignof(std::max_align_t),
                  StringView name           = "PoolAllocator");

    ~PoolAllocator() override;

    PoolAllocator(const PoolAllocator&)            = delete;
    PoolAllocator& operator=(const PoolAllocator&) = delete;
    PoolAllocator(PoolAllocator&&)                 = delete;
    PoolAllocator& operator=(PoolAllocator&&)      = delete;

    // -------------------------------------------------------------------------
    // IAllocator
    // -------------------------------------------------------------------------

    /// Pull one block from the free list.  `size` and `alignment` are ignored
    /// (the pool always returns a block of the fixed size and alignment).
    /// Returns nullptr when the pool is exhausted.
    [[nodiscard]] void* Allocate(usize size      = 0,
                                 usize alignment = 0) override;

    /// Return a block to the free list.
    void Deallocate(void* ptr, usize size = 0) override;

    /// Reset the pool to its initial (all-free) state.
    void Reset() override;

    [[nodiscard]] StringView GetName()            const noexcept override { return m_Name;       }
    [[nodiscard]] usize      GetUsedBytes()        const noexcept override;
    [[nodiscard]] usize      GetCapacityBytes()    const noexcept override;
    [[nodiscard]] usize      GetAllocationCount()  const noexcept override { return m_AllocCount; }

    // -------------------------------------------------------------------------
    // Pool-specific diagnostics
    // -------------------------------------------------------------------------

    [[nodiscard]] usize GetBlockSize()      const noexcept { return m_BlockSize;  }
    [[nodiscard]] usize GetBlockCount()     const noexcept { return m_BlockCount; }
    [[nodiscard]] usize GetFreeBlockCount() const noexcept { return m_FreeCount;  }
    [[nodiscard]] usize GetUsedBlockCount() const noexcept { return m_BlockCount - m_FreeCount; }

private:
    /// Free blocks are stored as a singly-linked list using the block memory itself.
    struct FreeNode
    {
        FreeNode* next = nullptr;
    };

    void BuildFreeList();  ///< (Re)initialise the free list — called by ctor and Reset().

    byte*     m_Buffer     = nullptr;
    FreeNode* m_FreeList   = nullptr;
    usize     m_BlockSize  = 0;
    usize     m_BlockCount = 0;
    usize     m_FreeCount  = 0;
    usize     m_AllocCount = 0;
    String    m_Name;
};

} // namespace Hydra
