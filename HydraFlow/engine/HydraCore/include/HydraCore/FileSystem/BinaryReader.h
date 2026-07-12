#pragma once

// =============================================================================
// BinaryReader.h
//
// Reads typed binary data from a file or any std::istream, with explicit
// control over byte order (endianness). Automatically byte-swaps multi-byte
// values when the file's declared byte order differs from the host CPU's
// native order, so call sites never have to think about endianness.
//
// Construction:
//   BinaryReader reader(Path("data.bin"));                    // opens a file
//   BinaryReader reader(someIstream, ByteOrder::BigEndian);   // wraps a stream
//
// Reading primitives:
//   u32 magic   = reader.ReadU32();
//   f32 value   = reader.ReadF32();
//   bool flag   = reader.ReadBool();
//   String name = reader.ReadString();  // reads a u32 length prefix then that many bytes
//
// BinaryReader is non-copyable (exclusive ownership of the underlying
// stream) but movable.
// =============================================================================

#include <HydraCore/Common/Platform.h>
#include <HydraCore/Common/Types.h>
#include <HydraCore/Common/NonCopyable.h>
#include <HydraCore/FileSystem/FileSystemTypes.h>
#include <HydraCore/FileSystem/Path.h>

#include <fstream>
#include <iosfwd>
#include <memory>

namespace Hydra {

class HYDRA_API BinaryReader final : private NonCopyable
{
public:
    // -----------------------------------------------------------------------
    // Construction / destruction
    // -----------------------------------------------------------------------

    /// Opens the file at `path` in binary mode. Throws std::runtime_error
    /// if the file can't be opened.
    explicit BinaryReader(const Path& path, ByteOrder byteOrder = ByteOrder::LittleEndian);

    /// Wraps an existing stream without taking ownership — the stream must
    /// outlive this reader. Useful for unit-testing with std::istringstream.
    explicit BinaryReader(std::istream& stream, ByteOrder byteOrder = ByteOrder::LittleEndian);

    ~BinaryReader();
    BinaryReader(BinaryReader&&) noexcept;
    BinaryReader& operator=(BinaryReader&&) noexcept;

    // -----------------------------------------------------------------------
    // State
    // -----------------------------------------------------------------------

    /// True while the underlying stream is open and no unrecoverable error has occurred.
    [[nodiscard]] bool IsOpen() const noexcept;

    /// True once the end of the stream has been reached.
    [[nodiscard]] bool IsEof() const noexcept;

    /// Closes the stream. Safe to call more than once.
    void Close();

    // -----------------------------------------------------------------------
    // Positioning
    // -----------------------------------------------------------------------

    /// Returns the current read position (byte offset from the beginning).
    [[nodiscard]] i64 Tell() const;

    /// Moves the read position. Throws std::runtime_error if the stream
    /// is not seekable (e.g. a non-seekable istream reference).
    void Seek(i64 offset, SeekOrigin origin = SeekOrigin::Begin);

    // -----------------------------------------------------------------------
    // Unsigned integers
    // -----------------------------------------------------------------------

    [[nodiscard]] u8  ReadU8();
    [[nodiscard]] u16 ReadU16();
    [[nodiscard]] u32 ReadU32();
    [[nodiscard]] u64 ReadU64();

    // -----------------------------------------------------------------------
    // Signed integers
    // -----------------------------------------------------------------------

    [[nodiscard]] i8  ReadI8();
    [[nodiscard]] i16 ReadI16();
    [[nodiscard]] i32 ReadI32();
    [[nodiscard]] i64 ReadI64();

    // -----------------------------------------------------------------------
    // Floating-point
    // -----------------------------------------------------------------------

    [[nodiscard]] f32 ReadF32();
    [[nodiscard]] f64 ReadF64();

    // -----------------------------------------------------------------------
    // Boolean (stored as a single byte: 0 = false, non-zero = true)
    // -----------------------------------------------------------------------

    [[nodiscard]] bool ReadBool();

    // -----------------------------------------------------------------------
    // Bytes / strings
    // -----------------------------------------------------------------------

    /// Reads exactly `count` bytes into a new vector. Throws
    /// std::runtime_error if fewer bytes are available.
    [[nodiscard]] Vector<u8> ReadBytes(usize count);

    /// Reads `count` bytes into `buffer` (which must be at least `count`
    /// bytes large). Throws std::runtime_error if fewer bytes are available.
    void ReadBytes(u8* buffer, usize count);

    /// Reads a length-prefixed UTF-8 string: first reads a u32 byte-count,
    /// then reads that many bytes and returns them as a String. This
    /// mirrors what BinaryWriter::WriteString() writes.
    [[nodiscard]] String ReadString();

    /// Reads exactly `byteCount` bytes as a String (no length prefix).
    /// Useful when the length is known from the file format header.
    [[nodiscard]] String ReadFixedString(usize byteCount);

private:
    // Reads `size` raw bytes from the stream, performing byte-swapping
    // if m_ByteOrder differs from the host. Used by all typed Read methods.
    void ReadRaw(void* buffer, usize size);

    // Byte-swaps a value in place if the file's declared byte order differs
    // from the host machine's byte order.
    void MaybeByteSwap(void* data, usize size) const noexcept;

    // Whether the host is big-endian (computed once at construction).
    static bool s_HostIsBigEndian;

    std::istream*             m_ExternalStream = nullptr; ///< Non-owning; set by the stream-ref constructor.
    std::unique_ptr<std::ifstream> m_OwnedStream;         ///< Owning; set by the path constructor.
    std::istream*             m_Stream = nullptr;          ///< Always points at whichever of the above is active.
    ByteOrder                 m_ByteOrder;
};

} // namespace Hydra
