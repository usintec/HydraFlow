#pragma once

namespace Hydra {

/// Base class that disables copy construction and copy assignment.
/// Move is still permitted unless also deriving from NonMovable.
class NonCopyable
{
protected:
    NonCopyable()  = default;
    ~NonCopyable() = default;

    NonCopyable(const NonCopyable&)            = delete;
    NonCopyable& operator=(const NonCopyable&) = delete;

    NonCopyable(NonCopyable&&)            = default;
    NonCopyable& operator=(NonCopyable&&) = default;
};

/// Base class that disables both copy and move semantics.
class NonCopyableNonMovable
{
protected:
    NonCopyableNonMovable()  = default;
    ~NonCopyableNonMovable() = default;

    NonCopyableNonMovable(const NonCopyableNonMovable&)            = delete;
    NonCopyableNonMovable& operator=(const NonCopyableNonMovable&) = delete;

    NonCopyableNonMovable(NonCopyableNonMovable&&)            = delete;
    NonCopyableNonMovable& operator=(NonCopyableNonMovable&&) = delete;
};

} // namespace Hydra
