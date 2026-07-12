#pragma once

// =============================================================================
// Directory.h
//
// Static utility functions for working with directories on the native file
// system: creation, deletion, listing, and navigation. Like File.h, these are
// stateless free functions operating on paths — no open handles involved.
// =============================================================================

#include <HydraCore/Common/Platform.h>
#include <HydraCore/Common/Types.h>
#include <HydraCore/FileSystem/Path.h>

namespace Hydra {

namespace Directory {

// -----------------------------------------------------------------------
// Queries
// -----------------------------------------------------------------------

/// Returns true if `path` exists and is a directory.
[[nodiscard]] HYDRA_API bool Exists(const Path& path);

// -----------------------------------------------------------------------
// Creation
// -----------------------------------------------------------------------

/// Creates the directory at `path`. Returns true if the directory was
/// newly created, false if it already existed. Throws std::runtime_error
/// if the parent directory does not exist or a non-directory file already
/// occupies `path`.
HYDRA_API bool Create(const Path& path);

/// Like Create() but also creates any missing intermediate (parent)
/// directories. Equivalent to `mkdir -p`. Returns true if any new
/// directories were created, false if `path` already existed in full.
/// Throws std::runtime_error only on genuine errors (permission denied,
/// not-a-directory conflicts, etc.).
HYDRA_API bool CreateAll(const Path& path);

// -----------------------------------------------------------------------
// Deletion
// -----------------------------------------------------------------------

/// Deletes the directory at `path` if it is empty. Returns true on
/// success, false if the directory was not empty or didn't exist.
HYDRA_API bool Delete(const Path& path);

/// Recursively deletes `path` and everything inside it (files,
/// subdirectories, and their contents). Returns the number of items
/// deleted. Throws std::runtime_error if the operation fails partway
/// through. Use with care — there is no undo.
HYDRA_API usize DeleteAll(const Path& path);

// -----------------------------------------------------------------------
// Listing
// -----------------------------------------------------------------------

/// Returns the immediate children of `path` (both files and
/// subdirectories, but not '.' or '..'). Throws std::runtime_error if
/// `path` doesn't exist or is not a directory.
[[nodiscard]] HYDRA_API Vector<Path> List(const Path& path);

/// Returns only the regular files (not subdirectories) that are immediate
/// children of `path`. Throws std::runtime_error on error.
[[nodiscard]] HYDRA_API Vector<Path> ListFiles(const Path& path);

/// Returns only the immediate subdirectories of `path` (not files).
/// Throws std::runtime_error on error.
[[nodiscard]] HYDRA_API Vector<Path> ListSubdirectories(const Path& path);

/// Recursively returns every file (not directory) reachable under `root`,
/// at any depth. Result is in undefined order. Throws std::runtime_error
/// on error.
[[nodiscard]] HYDRA_API Vector<Path> ListFilesRecursive(const Path& root);

// -----------------------------------------------------------------------
// Current directory
// -----------------------------------------------------------------------

/// Returns the process's current working directory.
[[nodiscard]] HYDRA_API Path GetCurrentDirectory();

/// Sets the process's current working directory to `path`. Throws
/// std::runtime_error if the path doesn't exist or isn't a directory.
HYDRA_API void SetCurrentDirectory(const Path& path);

} // namespace Directory

} // namespace Hydra
