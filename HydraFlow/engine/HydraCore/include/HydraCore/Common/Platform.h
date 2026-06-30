#pragma once

// ==============================================================================
// Platform Detection
// ==============================================================================

#if defined(_WIN32) || defined(_WIN64)
    #define HYDRA_PLATFORM_WINDOWS 1
    #if defined(_WIN64)
        #define HYDRA_PLATFORM_64BIT 1
    #else
        #define HYDRA_PLATFORM_32BIT 1
    #endif
#elif defined(__linux__)
    #define HYDRA_PLATFORM_LINUX 1
    #define HYDRA_PLATFORM_64BIT 1
#elif defined(__APPLE__)
    #define HYDRA_PLATFORM_MACOS 1
    #define HYDRA_PLATFORM_64BIT 1
#else
    #error "Unsupported platform"
#endif

// ==============================================================================
// Compiler Detection
// ==============================================================================

#if defined(_MSC_VER)
    #define HYDRA_COMPILER_MSVC 1
    #define HYDRA_COMPILER_VERSION _MSC_VER
#elif defined(__clang__)
    #define HYDRA_COMPILER_CLANG 1
    #define HYDRA_COMPILER_VERSION (__clang_major__ * 10000 + __clang_minor__ * 100 + __clang_patchlevel__)
#elif defined(__GNUC__)
    #define HYDRA_COMPILER_GCC 1
    #define HYDRA_COMPILER_VERSION (__GNUC__ * 10000 + __GNUC_MINOR__ * 100 + __GNUC_PATCHLEVEL__)
#else
    #error "Unsupported compiler"
#endif

// ==============================================================================
// DLL / Visibility
// ==============================================================================

#if defined(HYDRA_PLATFORM_WINDOWS)
    #if defined(HYDRA_SHARED)
        #if defined(HYDRACORE_EXPORTS)
            #define HYDRA_API __declspec(dllexport)
        #else
            #define HYDRA_API __declspec(dllimport)
        #endif
    #else
        #define HYDRA_API
    #endif
    #define HYDRA_FORCE_INLINE __forceinline
    #define HYDRA_NO_INLINE    __declspec(noinline)
#else
    #if defined(HYDRA_SHARED)
        #define HYDRA_API __attribute__((visibility("default")))
    #else
        #define HYDRA_API
    #endif
    #define HYDRA_FORCE_INLINE __attribute__((always_inline)) inline
    #define HYDRA_NO_INLINE    __attribute__((noinline))
#endif

// ==============================================================================
// Utility Macros
// ==============================================================================

#define HYDRA_STRINGIFY_IMPL(x) #x
#define HYDRA_STRINGIFY(x)      HYDRA_STRINGIFY_IMPL(x)

#define HYDRA_CONCAT_IMPL(a, b) a##b
#define HYDRA_CONCAT(a, b)      HYDRA_CONCAT_IMPL(a, b)

#define HYDRA_UNUSED(x) (void)(x)

// Prevents copy and move
#define HYDRA_NON_COPYABLE(ClassName)           \
    ClassName(const ClassName&)            = delete; \
    ClassName& operator=(const ClassName&) = delete;

#define HYDRA_NON_MOVABLE(ClassName)            \
    ClassName(ClassName&&)            = delete; \
    ClassName& operator=(ClassName&&) = delete;

#define HYDRA_NON_COPYABLE_NON_MOVABLE(ClassName) \
    HYDRA_NON_COPYABLE(ClassName)                 \
    HYDRA_NON_MOVABLE(ClassName)

// ==============================================================================
// Assert
// ==============================================================================

#include <cassert>
#define HYDRA_ASSERT(expr)               assert(expr)
#define HYDRA_ASSERT_MSG(expr, msg)      assert((expr) && (msg))
#define HYDRA_STATIC_ASSERT(expr, msg)   static_assert(expr, msg)
