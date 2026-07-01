#include <HydraCore/Memory/LinearAllocator.h>

#include <cstring>

namespace Hydra {

// =============================================================================
// Construction
// =============================================================================

LinearAllocator::LinearAllocator(usize capacity, StringView name)
    : m_Buffer   (new byte[capacity])
    , m_Capacity (capacity)
    , m_Owning   (true)
    , m_Name     (name)
{}

LinearAllocator::LinearAllocator(void* buffer, usize capacity, StringView name)
    : m_Buffer   (static_cast<byte*>(buffer))
    , m_Capacity (capacity)
    , m_Owning   (false)
    , m_Name     (name)
{}

LinearAllocator::~LinearAllocator()
{
    if (m_Owning)
        delete[] m_Buffer;
}

// =============================================================================
// IAllocator
// =============================================================================

void* LinearAllocator::Allocate(usize size, usize alignment)
{
    if (size == 0 || alignment == 0) return nullptr;
    HYDRA_ASSERT(Memory::IsPowerOfTwo(alignment));

    const usize aligned = Memory::AlignUp(m_Offset, alignment);
    if (aligned + size > m_Capacity)
        return nullptr;

    void* ptr   = m_Buffer + aligned;
    m_Offset    = aligned + size;
    ++m_AllocCount;
    return ptr;
}

void LinearAllocator::Deallocate(void* /*ptr*/, usize /*size*/)
{
    // Intentional no-op: individual frees are not supported.
}

void LinearAllocator::Reset()
{
    m_Offset    = 0;
    m_AllocCount = 0;
}

} // namespace Hydra
