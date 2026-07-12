#pragma once

// =============================================================================
// Path.h
//
// A cross-platform file-system path that always stores and exposes its
// content as UTF-8 (via Hydra::String = std::string). Internally it wraps
// std::filesystem::path, so all the heavy lifting of case normalisation,
// path-component parsing, and platform-separator differences is delegated
// to the C++ standard library.
//
// Why a wrapper rather than typedef?
//   • Keeps the Hydra API surface in terms of Hydra types (String/StringView)
//     rather than C++ standard-library types — callers never need to think
//     about wchar_t on Windows.
//   • Centralises UTF-8 ↔ native-string conversion in one class so the rest
//     of the codebase never has to do that conversion by hand.
//   • Allows operator/ chaining that returns a Hydra::Path instead of a
//     std::filesystem::path, keeping code using the result consistent.
//
// All string-returning methods (GetFilename, GetExtension, etc.) return
// UTF-8 Hydra::String values. ToString() always uses forward slashes '/'
// as the separator, making paths safe to store in config files or log
// messages on all platforms. ToNative() returns the path using the
// platform's preferred separator if you need to pass it to a native API.
// =============================================================================

#include <HydraCore/Common/Platform.h>
#include <HydraCore/Common/Types.h>

#include <filesystem>
#include <iosfwd>

namespace Hydra {

class HYDRA_API Path
{
public:
    // -----------------------------------------------------------------------
    // Construction
    // -----------------------------------------------------------------------

    /// Constructs an empty (relative, zero-component) path.
    Path() = default;

    /// Constructs from a UTF-8 string. Works on all platforms including
    /// Windows where std::filesystem::path internally converts to wchar_t.
    Path(StringView utf8Path);

    /// Creates a Path from a std::filesystem::path (e.g. returned by
    /// directory-iteration APIs). Named factory to avoid ambiguity with the
    /// StringView constructor when the compiler sees a string literal.
    [[nodiscard]] static Path FromNativePath(std::filesystem::path nativePath);

    // -----------------------------------------------------------------------
    // Path composition
    // -----------------------------------------------------------------------

    /// Returns a new path that is this path with `component` appended using
    /// the platform's path separator. Equivalent to Python's os.path.join()
    /// or std::filesystem::path::operator/=.
    [[nodiscard]] Path Append(StringView component) const;

    /// Operator alias for Append() — allows natural chaining:
    ///   Path root("data"); Path full = root / "textures" / "rock.png";
    [[nodiscard]] Path operator/(StringView component) const;
    [[nodiscard]] Path operator/(const Path& other) const;

    // -----------------------------------------------------------------------
    // Path decomposition
    // -----------------------------------------------------------------------

    /// The parent directory. Returns "." for a single-component relative
    /// path with no explicit parent.
    [[nodiscard]] Path GetParent() const;

    /// The filename (last component), including any extension.
    ///   "data/textures/rock.png" → "rock.png"
    [[nodiscard]] String GetFilename() const;

    /// The filename *without* its extension (the "stem").
    ///   "rock.png" → "rock"
    [[nodiscard]] String GetStem() const;

    /// The extension including the leading dot, or empty string if there is none.
    ///   "rock.png" → ".png"
    [[nodiscard]] String GetExtension() const;

    // -----------------------------------------------------------------------
    // Path queries
    // -----------------------------------------------------------------------

    [[nodiscard]] bool IsEmpty()    const noexcept;
    [[nodiscard]] bool IsAbsolute() const noexcept;
    [[nodiscard]] bool IsRelative() const noexcept;

    /// True if the path exists on the native file system (either as a file
    /// or a directory).
    [[nodiscard]] bool Exists() const;

    /// True if the path exists and refers to a regular file.
    [[nodiscard]] bool IsFile() const;

    /// True if the path exists and refers to a directory.
    [[nodiscard]] bool IsDirectory() const;

    // -----------------------------------------------------------------------
    // String conversion
    // -----------------------------------------------------------------------

    /// Returns the path as a UTF-8 string with '/' separators on all
    /// platforms — safe for cross-platform storage, logging, hashing, etc.
    [[nodiscard]] String ToString() const;

    /// Returns the path with the platform's native separator (backslash on
    /// Windows, forward slash everywhere else). Use this when passing the
    /// path to OS APIs that don't accept forward slashes on Windows.
    [[nodiscard]] String ToNative() const;

    /// Implicit conversion to String — equivalent to ToString(), provided
    /// as a convenience so a Path can be passed to any API that accepts a
    /// String without an explicit call.
    explicit operator String() const { return ToString(); }

    // -----------------------------------------------------------------------
    // Normalisation
    // -----------------------------------------------------------------------

    /// Returns a lexically normalised copy: resolves '.' and '..' components,
    /// removes redundant separators, etc. Does NOT require the path to exist
    /// on disk (use std::filesystem::canonical() for that, which does require
    /// existence and resolves symlinks).
    [[nodiscard]] Path Normalize() const;

    /// Returns an absolute path relative to the current working directory,
    /// but without resolving symlinks (see std::filesystem::weakly_canonical).
    [[nodiscard]] Path ToAbsolute() const;

    // -----------------------------------------------------------------------
    // Static helpers
    // -----------------------------------------------------------------------

    /// The current working directory.
    [[nodiscard]] static Path GetCurrentDirectory();

    /// The OS-provided temporary directory (e.g. /tmp on Linux, %TEMP% on Windows).
    [[nodiscard]] static Path GetTempDirectory();

    // -----------------------------------------------------------------------
    // Comparison / hashing
    // -----------------------------------------------------------------------

    bool operator==(const Path& other) const noexcept;
    bool operator!=(const Path& other) const noexcept;
    bool operator< (const Path& other) const noexcept;

    // -----------------------------------------------------------------------
    // Access to the underlying std::filesystem::path
    // -----------------------------------------------------------------------

    /// Direct access for callers that need to interact with std::filesystem
    /// APIs not wrapped here (e.g. directory iterators).
    [[nodiscard]] const std::filesystem::path& Native() const noexcept { return m_Path; }

private:
    std::filesystem::path m_Path;
};

/// Allows `std::cout << path` or similar — outputs the UTF-8 string form.
HYDRA_API std::ostream& operator<<(std::ostream& os, const Path& path);

} // namespace Hydra

// -----------------------------------------------------------------------
// std::hash specialisation so Path can be used as a HashMap key.
// -----------------------------------------------------------------------
namespace std {
template<>
struct hash<Hydra::Path>
{
    std::size_t operator()(const Hydra::Path& p) const noexcept
    {
        return std::filesystem::hash_value(p.Native());
    }
};
} // namespace std
