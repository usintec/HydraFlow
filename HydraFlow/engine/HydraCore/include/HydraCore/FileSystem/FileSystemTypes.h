#pragma once

// =============================================================================
// FileSystemTypes.h
//
// Small shared enumerations and constants used across the whole File System
// module. Kept in one place so every other FileSystem/ header can include
// just this file rather than each other.
// =============================================================================

#include <HydraCore/Common/Types.h>

namespace Hydra {

// -----------------------------------------------------------------------
// OpenMode
//
// Controls how a file is opened by BinaryReader/Writer, TextReader/Writer,
// or a VirtualFileSystem. Multiple flags may be OR-ed together where the
// combination makes sense (e.g. Write | Append).
// -----------------------------------------------------------------------
enum class OpenMode : u32
{
    Read     = 1 << 0,  ///< Open for reading; file must exist.
    Write    = 1 << 1,  ///< Open for writing; creates if absent, truncates by default.
    Append   = 1 << 2,  ///< All writes go to end of file (implies Write).
    Truncate = 1 << 3,  ///< Truncate to zero length on open (default for plain Write).
    Create   = 1 << 4,  ///< Create the file if it doesn't exist (implied by Write).
    Binary   = 1 << 5,  ///< Open in binary mode (no newline translation). Default for BinaryReader/Writer.
    Text     = 1 << 6,  ///< Open in text mode (newline translation on Windows). Default for TextReader/Writer.
};

/// Bitwise OR for combining OpenMode flags.
inline OpenMode operator|(OpenMode a, OpenMode b)
{
    return static_cast<OpenMode>(static_cast<u32>(a) | static_cast<u32>(b));
}

/// Test whether a specific flag is set in a combined OpenMode value.
inline bool HasOpenMode(OpenMode combined, OpenMode flag)
{
    return (static_cast<u32>(combined) & static_cast<u32>(flag)) != 0;
}

// -----------------------------------------------------------------------
// SeekOrigin
//
// The reference point used by stream Seek() calls, mirroring the POSIX
// whence parameter of fseek().
// -----------------------------------------------------------------------
enum class SeekOrigin : u8
{
    Begin,   ///< Seek from the start of the stream (offset >= 0).
    Current, ///< Seek from the current position (offset may be negative).
    End      ///< Seek from the end of the stream (offset <= 0).
};

// -----------------------------------------------------------------------
// ByteOrder
//
// The endianness used when BinaryReader/BinaryWriter serialises multi-byte
// integers and floating-point values. The file format's required byte order
// is specified at reader/writer construction time; the reader/writer
// transparently byte-swaps if the host machine's native order differs.
//
// LittleEndian is the default for newly written files (it's the native
// order on x86/x64 and most modern ARM platforms), which means no swapping
// is needed on the most common hardware.
// -----------------------------------------------------------------------
enum class ByteOrder : u8
{
    LittleEndian, ///< Least-significant byte first (x86/x64 native).
    BigEndian,    ///< Most-significant byte first (network byte order).
    Native        ///< No byte-swapping; use whatever the host CPU uses.
};

} // namespace Hydra
