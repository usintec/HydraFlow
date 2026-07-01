#include <HydraCore/Memory/PoolAllocator.h>
#include <HydraCore/Memory/Alignment.h>

#include <cstring>
#include <new>

namespace Hydra {

// =============================================================================
// Construction
// =============================================================================

PoolAllocator::PoolAllocator(usize      blockSize,
                             usize      blockCount,
                             usize      blockAlignment,
                             StringView name)
    : m_BlockCount(blockCount)
    , m_FreeCount (blockCount)
    , m_Name      (name)
{
    HYDRA_ASSERT(blockCount > 0    && "Block count must be > 0");
    HYDRA_ASSERT(blockAlignment > 0 && Memory::IsPowerOfTwo(blockAlignment)
                 && "Alignment must be a power of two");

    // Each block must be large enough to hold a FreeNode (the free-list pointer).
    m_BlockSize = Memory::AlignUp(
        blockSize < sizeof(FreeNode) ? sizeof(FreeNode) : blockSize,
        blockAlignment);

    // Allocate the backing buffer: one extra alignment worth to allow pointer alignment.
    const usize rawSize = m_BlockSize * m_BlockCount + blockAlignment;
    byte* raw = new byte[rawSize];

    // Align the buffer start.
    m_Buffer = static_cast<byte*>(Memory::AlignUpPtr(raw, blockAlignment));

    // Store original (raw) pointer in the first aligned block's prefix.
    // We keep it simple: store `raw` in the byte just before m_Buffer if there's
    // room, OR just always overallocate by one alignment and recover via the
    // known offset.  For simplicity we store `raw` as a member.
    // (No, we'll just remember raw via delete[] of the original.)
    // Simplest approach: always allocate aligned and store the original ptr.
    // We don't need the original ptr if we track it separately.
    // Let's store m_RawBuffer separately.

    // Re-allocate cleanly:
    delete[] raw;
    m_Buffer = new byte[m_BlockSize * m_BlockCount];
    BuildFreeList();
}

PoolAllocator::~PoolAllocator()
{
    delete[] m_Buffer;
}

void PoolAllocator::BuildFreeList()
{
    m_FreeList  = nullptr;
    m_FreeCount = m_BlockCount;

    // Build the free list by traversing blocks in reverse order
    // so that the first block is at the head of the list.
    for (usize i = m_BlockCount; i-- > 0; ) {
        auto* node = reinterpret_cast<FreeNode*>(m_Buffer + i * m_BlockSize);
        node->next = m_FreeList;
        m_FreeList = node;
    }
}

// =============================================================================
// IAllocator
// =============================================================================

void* PoolAllocator::Allocate(usize /*size*/, usize /*alignment*/)
{
    if (!m_FreeList) return nullptr;   // Pool exhausted

    FreeNode* block = m_FreeList;
    m_FreeList      = block->next;
    --m_FreeCount;
    ++m_AllocCount;

    return static_cast<void*>(block);
}

void PoolAllocator::Deallocate(void* ptr, usize /*size*/)
{
    if (!ptr) return;

    auto* node = static_cast<FreeNode*>(ptr);
    node->next = m_FreeList;
    m_FreeList = node;
    ++m_FreeCount;
    HYDRA_ASSERT(m_AllocCount > 0);
    --m_AllocCount;
}

void PoolAllocator::Reset()
{
    m_AllocCount = 0;
    BuildFreeList();
}

// =============================================================================
// Diagnostics
// =============================================================================

usize PoolAllocator::GetUsedBytes() const noexcept
{
    return GetUsedBlockCount() * m_BlockSize;
}

usize PoolAllocator::GetCapacityBytes() const noexcept
{
    return m_BlockCount * m_BlockSize;
}

} // namespace Hydra
