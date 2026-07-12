#pragma once

// =============================================================================
// File.h
//
// Static utility functions for working with individual files on the native
// file system: existence/size queries, bulk read/write convenience functions,
// copy/move/delete, and last-modification time. None of these require an
// open file handle — they operate directly on paths. For streaming access
// (reading line by line, writing incrementally) use TextReader/TextWriter or
// BinaryReader/BinaryWriter instead.
// =============================================================================

#include <HydraCore/Common/Platform.h>
#include <HydraCore/Common/Types.h>
#include <HydraCore/FileSystem/Path.h>

#include <chrono>
#include <optional>

namespace Hydra {

namespace File {

// -----------------------------------------------------------------------
// Queries
// -----------------------------------------------------------------------

/// Returns true if `path` exists and is a regular file (not a directory
/// or special device).
[[nodiscard]] HYDRA_API bool Exists(const Path& path);

/// Returns the file size in bytes. Returns 0 if the file doesn't exist
/// or can't be stat-d; use Exists() first if you need to distinguish
/// "file is genuinely 0 bytes" from "file not found".
[[nodiscard]] HYDRA_API u64 GetSize(const Path& path);

/// Returns the time point of the file's last modification, or nullopt if
/// the file doesn't exist or the time can't be read.
[[nodiscard]] HYDRA_API Optional<std::filesystem::file_time_type> GetLastModified(const Path& path);

// -----------------------------------------------------------------------
// Bulk I/O convenience
// -----------------------------------------------------------------------

/// Reads the entire contents of `path` into memory and returns them as a
/// byte vector. Throws std::runtime_error if the file doesn't exist or
/// can't be opened.
[[nodiscard]] HYDRA_API Vector<u8> ReadAllBytes(const Path& path);

/// Reads the entire contents of `path` as UTF-8 text. Throws
/// std::runtime_error if the file doesn't exist or can't be opened.
[[nodiscard]] HYDRA_API String ReadAllText(const Path& path);

/// Reads `path` line by line, returning each line (without the trailing
/// newline) as a separate string. Throws std::runtime_error on I/O error.
[[nodiscard]] HYDRA_API Vector<String> ReadAllLines(const Path& path);

/// Writes `data` to `path`, creating the file if it doesn't exist and
/// truncating any existing content. Throws std::runtime_error on failure.
HYDRA_API void WriteAllBytes(const Path& path, const Vector<u8>& data);

/// Writes `text` (UTF-8) to `path`, creating or truncating as above.
/// Does NOT append a trailing newline — pass one explicitly if needed.
HYDRA_API void WriteAllText(const Path& path, StringView text);

/// Writes each string in `lines` followed by a newline character ('\n')
/// to `path`, creating or truncating as above.
HYDRA_API void WriteAllLines(const Path& path, const Vector<String>& lines);

/// Appends `text` (UTF-8) to the end of an existing file, or creates it
/// if it doesn't exist. Throws std::runtime_error on failure.
HYDRA_API void AppendText(const Path& path, StringView text);

// -----------------------------------------------------------------------
// File manipulation
// -----------------------------------------------------------------------

/// Copies `source` to `destination`. If `overwrite` is false and
/// `destination` already exists, throws std::runtime_error.
HYDRA_API void Copy(const Path& source, const Path& destination, bool overwrite = true);

/// Moves/renames `source` to `destination`. Throws std::runtime_error on
/// failure. This is atomic on most file systems if both paths are on the
/// same device.
HYDRA_API void Move(const Path& source, const Path& destination);

/// Deletes the file at `path`. Does nothing and returns false if the file
/// doesn't exist. Returns true if successfully deleted, false otherwise.
HYDRA_API bool Delete(const Path& path);

/// Creates an empty file at `path` if one doesn't already exist (equivalent
/// to Unix `touch`). Creates parent directories if `createParents` is true.
/// Throws std::runtime_error on failure.
HYDRA_API void Touch(const Path& path, bool createParents = false);

} // namespace File

} // namespace Hydra
