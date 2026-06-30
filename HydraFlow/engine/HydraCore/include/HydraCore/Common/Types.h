#pragma once

#include <cstdint>
#include <cstddef>
#include <string>
#include <string_view>
#include <memory>
#include <optional>
#include <functional>
#include <vector>
#include <unordered_map>
#include <map>

namespace Hydra {

// ==============================================================================
// Primitive Types
// ==============================================================================

using i8  = int8_t;
using i16 = int16_t;
using i32 = int32_t;
using i64 = int64_t;

using u8  = uint8_t;
using u16 = uint16_t;
using u32 = uint32_t;
using u64 = uint64_t;

using f32 = float;
using f64 = double;

using usize = std::size_t;
using isize = std::ptrdiff_t;

using byte = std::byte;

// ==============================================================================
// String Types
// ==============================================================================

using String     = std::string;
using StringView = std::string_view;

// ==============================================================================
// Smart Pointer Aliases
// ==============================================================================

template<typename T>
using UniquePtr = std::unique_ptr<T>;

template<typename T>
using SharedPtr = std::shared_ptr<T>;

template<typename T>
using WeakPtr = std::weak_ptr<T>;

template<typename T, typename... Args>
[[nodiscard]] UniquePtr<T> MakeUnique(Args&&... args)
{
    return std::make_unique<T>(std::forward<Args>(args)...);
}

template<typename T, typename... Args>
[[nodiscard]] SharedPtr<T> MakeShared(Args&&... args)
{
    return std::make_shared<T>(std::forward<Args>(args)...);
}

// ==============================================================================
// Optional
// ==============================================================================

template<typename T>
using Optional = std::optional<T>;

// ==============================================================================
// Function Wrapper
// ==============================================================================

template<typename Signature>
using Function = std::function<Signature>;

// ==============================================================================
// Collection Aliases
// ==============================================================================

template<typename T>
using Vector = std::vector<T>;

template<typename K, typename V>
using HashMap = std::unordered_map<K, V>;

template<typename K, typename V>
using Map = std::map<K, V>;

} // namespace Hydra
