#pragma once

// =============================================================================
// IVirtualFileSystem.h
//
// Pure abstract interface for pluggable file-system backends. The default
// implementation (NativeFileSystem) delegates directly to the OS via
// std::filesystem. Alternative implementations might:
//   • Read from a ZIP/PAK archive (game asset bundles)
//   • Layer an in-memory overlay on top of the native FS (save-game patching)
//   • Redirect paths to a remote blob store (cloud asset streaming)
//   • Present a read-only snapshot for deterministic testing
//
// Usage pattern:
//   FileSystem::Set(MakeUnique<MyCustomVFS>());
//   auto reader = FileSystem::Get().OpenRead("data/config.bin");
//
// The interface returns UniquePtr<T> for each opened reader/writer so that
// the caller owns the resource and lifetimes are explicit.
// =============================================================================

#include <HydraCore/Common/Types.h>
#include <HydraCore/FileSystem/Path.h>

namespace Hydra {

// Forward declarations to avoid circular header dependencies.
class BinaryReader;
class BinaryWriter;
class TextReader;
class TextWriter;

class IVirtualFileSystem
{
public:
    virtual ~IVirtualFileSystem() = default;

    // -----------------------------------------------------------------------
    // Queries
    // -----------------------------------------------------------------------

    /// Returns true if `path` names a file that can be opened for reading.
    [[nodiscard]] virtual bool Exists(const Path& path) const = 0;

    /// Returns true if `path` names a directory that can be listed.
    [[nodiscard]] virtual bool IsDirectory(const Path& path) const = 0;

    /// Returns the size of the file at `path` in bytes, or 0 if unknown.
    [[nodiscard]] virtual u64 GetFileSize(const Path& path) const = 0;

    // -----------------------------------------------------------------------
    // File opening
    // -----------------------------------------------------------------------

    /// Opens `path` for binary reading. Returns nullptr if the file can't
    /// be opened rather than throwing, so callers can handle missing files
    /// gracefully.
    [[nodiscard]] virtual UniquePtr<BinaryReader> OpenRead(const Path& path) const = 0;

    /// Creates or truncates `path` for binary writing. Returns nullptr on
    /// failure (e.g. permission denied, read-only VFS).
    [[nodiscard]] virtual UniquePtr<BinaryWriter> OpenWrite(const Path& path) = 0;

    /// Opens `path` for UTF-8 text reading. Returns nullptr if the file
    /// can't be opened.
    [[nodiscard]] virtual UniquePtr<TextReader> OpenTextRead(const Path& path) const = 0;

    /// Creates or truncates `path` for UTF-8 text writing. Returns nullptr
    /// on failure.
    [[nodiscard]] virtual UniquePtr<TextWriter> OpenTextWrite(const Path& path) = 0;

    // -----------------------------------------------------------------------
    // Directory operations
    // -----------------------------------------------------------------------

    /// Lists the immediate children of `path` (files and subdirectories).
    /// Returns an empty vector if `path` doesn't exist or is not a directory.
    [[nodiscard]] virtual Vector<Path> ListFiles(const Path& path) const = 0;

    /// Creates the directory `path` and any missing parents. Returns true
    /// if the directory now exists (whether newly created or pre-existing).
    virtual bool CreateDirectory(const Path& path) = 0;

    /// Deletes the file or empty directory at `path`. Returns true on
    /// success or if the path didn't exist; false on error.
    virtual bool Delete(const Path& path) = 0;
};

// =============================================================================
// FileSystem — global VFS accessor
//
// Holds the currently active IVirtualFileSystem implementation. Defaults to
// NativeFileSystem on first access. Call Set() to install a different backend
// (e.g. in unit tests, or when the engine mounts a PAK archive).
//
// Thread-safety: Get() returns a reference, so concurrent reads are safe.
// Set() is NOT thread-safe — call it before spawning threads that use the VFS.
// =============================================================================
namespace FileSystem {

/// Returns the currently active VFS (creates and installs a NativeFileSystem
/// on first call if none has been set).
HYDRA_API IVirtualFileSystem& Get();

/// Replaces the active VFS with `vfs`. Ownership is transferred to the
/// FileSystem module. Pass nullptr to restore the default NativeFileSystem.
HYDRA_API void Set(UniquePtr<IVirtualFileSystem> vfs);

} // namespace FileSystem

} // namespace Hydra
