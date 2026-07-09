#include <HydraCore/Memory/ArenaAllocator.h>

#include <new>

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

    AddPage(newPageSize);

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
        ::operator delete[](page.data, std::align_val_t(kPageAlignment));
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

void ArenaAllocator::AddPage(usize capacity)
{
    const usize pageCapacity = (capacity > 0) ? capacity : m_PageSize;

    Page page;
    // Pages must start at an address aligned to at least kPageAlignment so
    // that any in-range alignment request (<= kPageAlignment) can be
    // satisfied purely by AlignUp(page.offset, alignment) from offset 0.
    page.data     = static_cast<byte*>(::operator new[](pageCapacity, std::align_val_t(kPageAlignment)));
    page.offset   = 0;
    page.capacity = pageCapacity;
    m_Pages.push_back(std::move(page));
}

} // namespace Hydra
