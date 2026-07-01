#include <HydraCore/Memory/ArenaAllocator.h>

namespace Hydra {

// =============================================================================
// Construction / destruction
// =============================================================================

ArenaAllocator::ArenaAllocator(usize pageSize, StringView name)
    : m_PageSize(pageSize)
    , m_Name    (name)
{
    HYDRA_ASSERT(pageSize > 0 && "Page size must be > 0");
    // Allocate the first page eagerly so the arena is immediately usable.
    AddPage();
}

ArenaAllocator::~ArenaAllocator()
{
    ReleasePages();
}

// =============================================================================
// IAllocator
// =============================================================================

void* ArenaAllocator::Allocate(usize size, usize alignment)
{
    if (size == 0 || alignment == 0) return nullptr;
    HYDRA_ASSERT(Memory::IsPowerOfTwo(alignment));

    // Try to satisfy from the current (last) page.
    auto tryAllocFromPage = [&](Page& page) -> void* {
        const usize aligned = Memory::AlignUp(page.offset, alignment);
        if (aligned + size > page.capacity)
            return nullptr;
        void* ptr   = page.data + aligned;
        page.offset = aligned + size;
        m_TotalUsed += size;
        ++m_AllocCount;
        return ptr;
    };

    {
        void* ptr = tryAllocFromPage(CurrentPage());
        if (ptr) return ptr;
    }

    // Current page is full — grow.
    const usize needed = Memory::AlignUp(size, alignment) + alignment;
    const usize newPageSize = (needed > m_PageSize) ? needed : m_PageSize;

    AddPage();
    m_Pages.back().capacity = newPageSize;   // override capacity for oversized allocs

    void* ptr = tryAllocFromPage(m_Pages.back());
    HYDRA_ASSERT(ptr && "Oversized allocation still failed after new page");
    return ptr;
}

void ArenaAllocator::Deallocate(void* /*ptr*/, usize /*size*/)
{
    // Intentional no-op.
}

void ArenaAllocator::Reset()
{
    for (auto& page : m_Pages)
        page.offset = 0;
    m_TotalUsed  = 0;
    m_AllocCount = 0;
}

usize ArenaAllocator::GetCapacityBytes() const noexcept
{
    usize total = 0;
    for (const auto& p : m_Pages)
        total += p.capacity;
    return total;
}

// =============================================================================
// Arena-specific
// =============================================================================

void ArenaAllocator::ReleasePages()
{
    for (auto& page : m_Pages)
        delete[] page.data;
    m_Pages.clear();
    m_TotalUsed  = 0;
    m_AllocCount = 0;
}

// =============================================================================
// Private
// =============================================================================

ArenaAllocator::Page& ArenaAllocator::CurrentPage()
{
    HYDRA_ASSERT(!m_Pages.empty());
    return m_Pages.back();
}

void ArenaAllocator::AddPage()
{
    Page page;
    page.data     = new byte[m_PageSize];
    page.offset   = 0;
    page.capacity = m_PageSize;
    m_Pages.push_back(std::move(page));
}

} // namespace Hydra
