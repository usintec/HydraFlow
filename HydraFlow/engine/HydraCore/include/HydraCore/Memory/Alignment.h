#pragma once

#include <HydraCore/Common/Types.h>

#include <cstdint>

namespace Hydra {

// =============================================================================
// Alignment utilities
//
// All functions are constexpr where possible so they can be used in static
// assertions and compile-time array sizing.
// =============================================================================

namespace Memory {

/// Returns true when value is an exact power of two (alignment requirement).
[[nodiscard]] constexpr bool IsPowerOfTwo(usize value) noexcept
{
    return value != 0 && (value & (value - 1)) == 0;
}

/// Round value UP to the next multiple of alignment.
/// alignment must be a power of two.
[[nodiscard]] constexpr usize AlignUp(usize value, usize alignment) noexcept
{
    return (value + alignment - 1) & ~(alignment - 1);
}

/// Round value DOWN to the previous multiple of alignment.
/// alignment must be a power of two.
[[nodiscard]] constexpr usize AlignDown(usize value, usize alignment) noexcept
{
    return value & ~(alignment - 1);
}

/// True when value is already a multiple of alignment.
[[nodiscard]] constexpr bool IsAligned(usize value, usize alignment) noexcept
{
    return (value & (alignment - 1)) == 0;
}

/// Return the number of bytes needed to align `value` up to `alignment`.
[[nodiscard]] constexpr usize AlignmentPadding(usize value, usize alignment) noexcept
{
    const usize mod = value & (alignment - 1);
    return mod == 0 ? 0 : alignment - mod;
}

/// Round a pointer UP to the next address that satisfies `alignment`.
/// alignment must be a power of two.
[[nodiscard]] inline void* AlignUpPtr(void* ptr, usize alignment) noexcept
{
    return reinterpret_cast<void*>(
        AlignUp(reinterpret_cast<usize>(ptr), alignment));
}

[[nodiscard]] inline const void* AlignUpPtr(const void* ptr, usize alignment) noexcept
{
    return reinterpret_cast<const void*>(
        AlignUp(reinterpret_cast<usize>(ptr), alignment));
}

/// Round a pointer DOWN to the previous address satisfying `alignment`.
[[nodiscard]] inline void* AlignDownPtr(void* ptr, usize alignment) noexcept
{
    return reinterpret_cast<void*>(
        AlignDown(reinterpret_cast<usize>(ptr), alignment));
}

/// Returns the adjustment in bytes needed to align `ptr` to `alignment`.
[[nodiscard]] inline usize AlignmentAdjustment(const void* ptr, usize alignment) noexcept
{
    return AlignmentPadding(reinterpret_cast<usize>(ptr), alignment);
}

} // namespace Memory
} // namespace Hydra
