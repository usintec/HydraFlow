#pragma once

#include <HydraCore/Memory/MemoryManager.h>

#include <type_traits>

namespace Hydra {

// =============================================================================
// Memory Macros
//
// Convenience macros that route allocations through MemoryManager with full
// source-location capture (__FILE__, __LINE__, __func__).
//
// HYDRA_ALLOC(size)
//   Raw byte allocation.  Returns void*.
//
// HYDRA_ALLOC_ALIGNED(size, alignment)
//   Raw byte allocation with explicit alignment.  Returns void*.
//
// HYDRA_FREE(ptr)
//   Free a pointer obtained from HYDRA_ALLOC / HYDRA_ALLOC_ALIGNED.
//   Safe to call with nullptr.
//
// HYDRA_NEW(Type, ...)
//   Allocate sizeof(Type) bytes aligned to alignof(Type), then placement-new.
//   Returns Type*.
//
// HYDRA_DELETE(ptr)
//   Call ~Type() then HYDRA_FREE.  Sets the pointer to nullptr.
//   Type is deduced automatically from the pointer's static type.
// =============================================================================

#define HYDRA_ALLOC(size) \
    ::Hydra::MemoryManager::Allocate( \
        (size), alignof(std::max_align_t), __FILE__, __LINE__, __func__)

#define HYDRA_ALLOC_ALIGNED(size, alignment) \
    ::Hydra::MemoryManager::Allocate( \
        (size), (alignment), __FILE__, __LINE__, __func__)

#define HYDRA_FREE(ptr) \
    ::Hydra::MemoryManager::Deallocate(static_cast<void*>(ptr))

// HYDRA_NEW: placement-new into manager-allocated memory.
// Usage: auto* obj = HYDRA_NEW(MyClass, ctorArg1, ctorArg2);
#define HYDRA_NEW(Type, ...) \
    (new (::Hydra::MemoryManager::Allocate( \
            sizeof(Type), alignof(Type), __FILE__, __LINE__, __func__)) \
        Type(__VA_ARGS__))

// HYDRA_DELETE: destruct then free.  Nulls the pointer.
// Usage: HYDRA_DELETE(obj);
#define HYDRA_DELETE(ptr) \
    do { \
        if ((ptr) != nullptr) { \
            using _HydraDeleteType = std::remove_pointer_t<decltype(ptr)>; \
            (ptr)->~_HydraDeleteType(); \
            ::Hydra::MemoryManager::Deallocate(static_cast<void*>(ptr), \
                                               sizeof(_HydraDeleteType)); \
            (ptr) = nullptr; \
        } \
    } while (false)

} // namespace Hydra
