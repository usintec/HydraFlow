#pragma once

#include <HydraCore/Common/Platform.h>
#include <HydraCore/Common/Types.h>
#include <HydraCore/Memory/IAllocator.h>
#include <HydraCore/Memory/Alignment.h>

namespace Hydra {

// =============================================================================
// ArenaAllocator
//
// A paged bump-pointer allocator that grows by allocating new OS pages when
// the current page is full.  Like LinearAllocator but unbounded (until the
// process runs out of address space / physical memory).
//
// Pages stay alive until ReleasePages() or destruction.  Reset() rewinds all
// page offsets without releasing the OS allocations — subsequent allocations
// reuse existing pages, so no additional OS calls are required after the warm-
// up phase.
//
// Best for: subsystems that need variable-length scratch memory across many
// frames but want to avoid per-frame OS allocations after the first frame.
//
// Properties:
//   – O(1) allocation (amortised — O(1) page growth is rare)
//   – Reset() rewinds without freeing pages (hot-path friendly)
//   – Deallocate() is a no-op
//   – Thread-unsafe (like all HydraCore allocators — use external locking)
// =============================================================================

class HYDRA_API ArenaAllocator final : public IAllocator
{
public:
    // -------------------------------------------------------------------------
    // Construction
    // -------------------------------------------------------------------------

    explicit ArenaAllocator(usize      pageSize = 64 * 1024,   // 64 KiB
                            StringView name     = "ArenaAllocator");

    ~ArenaAllocator() override;

    ArenaAllocator(const ArenaAllocator&)            = delete;
    ArenaAllocator& operator=(const ArenaAllocator&) = delete;
    ArenaAllocator(ArenaAllocator&&)                 = delete;
    ArenaAllocator& operator=(ArenaAllocator&&)      = delete;

    // -------------------------------------------------------------------------
    // IAllocator
    // -------------------------------------------------------------------------

    /// Allocate from the current page; grow by one page if needed.
    [[nodiscard]] void* Allocate(usize size,
                                 usize alignment = alignof(std::max_align_t)) override;

    /// No-op.  Individual deallocations are not supported.
    void Deallocate(void* ptr, usize size = 0) override;

    /// Rewind all page offsets to zero.  Pages are NOT released to the OS.
    void Reset() override;

    [[nodiscard]] StringView GetName()            const noexcept override { return m_Name;       }
    [[nodiscard]] usize      GetUsedBytes()        const noexcept override { return m_TotalUsed;  }
    [[nodiscard]] usize      GetCapacityBytes()    const noexcept override;
    [[nodiscard]] usize      GetAllocationCount()  const noexcept override { return m_AllocCount; }

    // -------------------------------------------------------------------------
    // Arena-specific
    // -------------------------------------------------------------------------

    /// Free all pages back to the OS.  After this call the arena is empty.
    void ReleasePages();

    [[nodiscard]] usize GetPageSize()  const noexcept { return m_PageSize;          }
    [[nodiscard]] usize GetPageCount() const noexcept { return m_Pages.size();      }

private:
    struct Page
    {
        byte* data     = nullptr;
        usize offset   = 0;
        usize capacity = 0;
    };

    Page& CurrentPage();
    void  AddPage();

    Vector<Page> m_Pages;
    usize        m_PageSize  = 0;
    usize        m_TotalUsed = 0;
    usize        m_AllocCount = 0;
    String       m_Name;
};

} // namespace Hydra
