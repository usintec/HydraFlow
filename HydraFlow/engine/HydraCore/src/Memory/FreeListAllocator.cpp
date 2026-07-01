#include <HydraCore/Memory/FreeListAllocator.h>

#include <algorithm>
#include <cstring>
#include <new>

namespace Hydra {

// =============================================================================
// Construction
// =============================================================================

FreeListAllocator::FreeListAllocator(usize capacity, StringView name)
    : m_Buffer  (new byte[capacity])
    , m_Capacity(capacity)
    , m_Owning  (true)
    , m_Name    (name)
{
    HYDRA_ASSERT(capacity >= k_MinBlockSize);
    InitFreeList();
}

FreeListAllocator::FreeListAllocator(void* buffer, usize capacity, StringView name)
    : m_Buffer  (static_cast<byte*>(buffer))
    , m_Capacity(capacity)
    , m_Owning  (false)
    , m_Name    (name)
{
    HYDRA_ASSERT(capacity >= k_MinBlockSize);
    InitFreeList();
}

FreeListAllocator::~FreeListAllocator()
{
    if (m_Owning)
        delete[] m_Buffer;
}

void FreeListAllocator::InitFreeList()
{
    m_FreeList   = new (m_Buffer) FreeNode{ m_Capacity, nullptr };
    m_UsedBytes  = 0;
    m_AllocCount = 0;
}

// =============================================================================
// Allocate
//
// Block layout (allocated):
//
//   [block start]
//   [gap bytes  ← adjustment]
//   [AllocHeader (kHdrSize bytes)]   <- at userPtr - kHdrSize
//   [user data]                      <- userPtr  (returned)
//   [end of allocated region]
//
// The AllocHeader is ALWAYS placed directly before the user pointer.
// We compute userPtr as AlignUp(blockStart + kHdrSize, effectiveAlignment),
// where effectiveAlignment = max(alignment, alignof(AllocHeader)).
// This guarantees (userPtr - kHdrSize) is properly aligned for AllocHeader.
//
// Recovery in Deallocate:
//   AllocHeader* hdr = (AllocHeader*)(ptr - kHdrSize)
//   blockStart       = ptr - kHdrSize - hdr->adjustment
// =============================================================================

void* FreeListAllocator::Allocate(usize size, usize alignment)
{
    if (size == 0 || alignment == 0) return nullptr;
    HYDRA_ASSERT(Memory::IsPowerOfTwo(alignment));

    constexpr usize kHdrSize  = sizeof(AllocHeader);
    const     usize kEffAlign = std::max(alignment, static_cast<usize>(alignof(AllocHeader)));

    FreeNode* prev = nullptr;
    FreeNode* cur  = m_FreeList;

    while (cur) {
        const usize blockAddr   = reinterpret_cast<usize>(cur);
        // User ptr: aligned to kEffAlign, at least kHdrSize past block start.
        const usize userAddr    = Memory::AlignUp(blockAddr + kHdrSize, kEffAlign);
        const usize adjustment  = userAddr - kHdrSize - blockAddr;   // gap before header
        const usize totalNeeded = adjustment + kHdrSize + size;       // from block start

        if (cur->totalSize >= totalNeeded) {
            const usize remainder = cur->totalSize - totalNeeded;
            const usize blockUsed = (remainder >= k_MinBlockSize) ? totalNeeded : cur->totalSize;

            // Remove cur from free list (or replace with split node).
            if (remainder >= k_MinBlockSize) {
                auto* newFree = new (reinterpret_cast<byte*>(cur) + totalNeeded)
                                    FreeNode{ remainder, cur->next };
                if (prev) prev->next = newFree;
                else      m_FreeList = newFree;
            } else {
                if (prev) prev->next = cur->next;
                else      m_FreeList = cur->next;
            }

            // Write AllocHeader directly before user ptr.
            auto* hdr       = reinterpret_cast<AllocHeader*>(userAddr - kHdrSize);
            hdr->totalSize  = blockUsed;
            hdr->adjustment = static_cast<usize>(adjustment);

            m_UsedBytes  += blockUsed;
            ++m_AllocCount;

            return reinterpret_cast<void*>(userAddr);
        }

        prev = cur;
        cur  = cur->next;
    }

    return nullptr;   // OOM
}

// =============================================================================
// Deallocate
// =============================================================================

void FreeListAllocator::Deallocate(void* ptr, usize /*size*/)
{
    if (!ptr) return;

    constexpr usize kHdrSize = sizeof(AllocHeader);

    // Header is always at ptr - kHdrSize.
    auto* hdr = reinterpret_cast<AllocHeader*>(
                    reinterpret_cast<byte*>(ptr) - kHdrSize);

    // Recover block start via the stored adjustment.
    byte* blockStart = reinterpret_cast<byte*>(ptr) - kHdrSize - hdr->adjustment;

    HYDRA_ASSERT(blockStart >= m_Buffer
              && blockStart < m_Buffer + m_Capacity
              && "Deallocate called with pointer outside this allocator");

    const usize blockSize = hdr->totalSize;
    m_UsedBytes -= blockSize;
    HYDRA_ASSERT(m_AllocCount > 0);
    --m_AllocCount;

    auto* freeNode = new (blockStart) FreeNode{ blockSize, nullptr };
    InsertFreeNode(freeNode);
}

// =============================================================================
// Reset
// =============================================================================

void FreeListAllocator::Reset()
{
    InitFreeList();
}

// =============================================================================
// InsertFreeNode — sorted by address, then coalesce adjacent blocks
// =============================================================================

void FreeListAllocator::InsertFreeNode(FreeNode* node)
{
    FreeNode* prev = nullptr;
    FreeNode* cur  = m_FreeList;

    while (cur && cur < node) {
        prev = cur;
        cur  = cur->next;
    }

    node->next = cur;
    if (prev) prev->next = node;
    else      m_FreeList = node;

    // Coalesce with successor.
    if (node->next) {
        byte* nodeEnd = reinterpret_cast<byte*>(node) + node->totalSize;
        if (nodeEnd == reinterpret_cast<byte*>(node->next)) {
            node->totalSize += node->next->totalSize;
            node->next       = node->next->next;
        }
    }

    // Coalesce with predecessor.
    if (prev) {
        byte* prevEnd = reinterpret_cast<byte*>(prev) + prev->totalSize;
        if (prevEnd == reinterpret_cast<byte*>(node)) {
            prev->totalSize += node->totalSize;
            prev->next       = node->next;
        }
    }
}

} // namespace Hydra
