#include <HydraCore/Memory/StackAllocator.h>

#include <algorithm>
#include <cstring>

namespace Hydra {

// =============================================================================
// Construction
// =============================================================================

StackAllocator::StackAllocator(usize capacity, StringView name)
    : m_Buffer  (new byte[capacity])
    , m_Capacity(capacity)
    , m_Owning  (true)
    , m_Name    (name)
{}

StackAllocator::StackAllocator(void* buffer, usize capacity, StringView name)
    : m_Buffer  (static_cast<byte*>(buffer))
    , m_Capacity(capacity)
    , m_Owning  (false)
    , m_Name    (name)
{}

StackAllocator::~StackAllocator()
{
    if (m_Owning)
        delete[] m_Buffer;
}

// =============================================================================
// IAllocator
// =============================================================================
//
// Block layout:
//
//   m_Buffer + m_Offset (before this alloc)
//     [gap bytes]
//     [Header]          <- at userStart - sizeof(Header)
//     [user data]       <- userStart  (returned ptr)
//
// We place the Header DIRECTLY before the user pointer (no gap between
// them).  To guarantee the Header is properly aligned we use:
//
//   effectiveAlign = max(alignment, alignof(Header))
//   userStart      = AlignUp(m_Offset + sizeof(Header), effectiveAlign)
//
// Then Header is at (userStart - sizeof(Header)), which is
// effectiveAlign-aligned (since effectiveAlign is a multiple of
// alignof(Header) and sizeof(Header) is a multiple of alignof(Header)).
//
// Recovery in Deallocate:
//   Header* hdr = (Header*)(userPtr - sizeof(Header));
//   prevOffset  = hdr->prevOffset;

void* StackAllocator::Allocate(usize size, usize alignment)
{
    if (size == 0 || alignment == 0) return nullptr;
    HYDRA_ASSERT(Memory::IsPowerOfTwo(alignment));

    constexpr usize kHdrSize  = sizeof(Header);
    const     usize kEffAlign = std::max(alignment, alignof(Header));

    // userStart is aligned to kEffAlign AND is at least kHdrSize bytes past m_Offset.
    const usize userStart = Memory::AlignUp(m_Offset + kHdrSize, kEffAlign);
    const usize newOffset = userStart + size;

    if (newOffset > m_Capacity) return nullptr;

    auto* hdr       = reinterpret_cast<Header*>(m_Buffer + userStart - kHdrSize);
    hdr->prevOffset = m_Offset;
    hdr->adjustment = userStart - kHdrSize - m_Offset;   // informational only

    m_Offset = newOffset;
    ++m_AllocCount;

    return m_Buffer + userStart;
}

void StackAllocator::Deallocate(void* ptr, usize /*size*/)
{
    if (!ptr) return;

    constexpr usize kHdrSize = sizeof(Header);
    const usize userStart    = static_cast<usize>(static_cast<byte*>(ptr) - m_Buffer);

    HYDRA_ASSERT(userStart >= kHdrSize     && "Invalid pointer: below buffer start");
    HYDRA_ASSERT(userStart <= m_Offset     && "Invalid pointer: above current top");

    auto* hdr = reinterpret_cast<Header*>(m_Buffer + userStart - kHdrSize);

    HYDRA_ASSERT(hdr->prevOffset <= m_Offset && "Stack corruption detected");

    m_Offset = hdr->prevOffset;
    HYDRA_ASSERT(m_AllocCount > 0);
    --m_AllocCount;
}

void StackAllocator::Reset()
{
    m_Offset     = 0;
    m_AllocCount = 0;
}

// =============================================================================
// Markers
// =============================================================================

StackMarker StackAllocator::GetMarker() const noexcept
{
    return { m_Offset, m_AllocCount };
}

void StackAllocator::FreeToMarker(StackMarker marker)
{
    HYDRA_ASSERT(marker.offset <= m_Offset  && "Marker is ahead of current top");
    m_Offset     = marker.offset;
    m_AllocCount = marker.allocCount;
}

} // namespace Hydra
