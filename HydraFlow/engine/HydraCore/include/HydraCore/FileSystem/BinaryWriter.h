#pragma once

// =============================================================================
// BinaryWriter.h
//
// Writes typed binary data to a file or any std::ostream, with explicit
// control over byte order (endianness). Automatically byte-swaps multi-byte
// values when the desired file byte order differs from the host CPU's native
// order, so call sites stay endianness-agnostic.
//
// Construction:
//   BinaryWriter writer(Path("data.bin"));                    // creates/truncates
//   BinaryWriter writer(someOstream, ByteOrder::BigEndian);   // wraps a stream
//
// Writing primitives:
//   writer.WriteU32(0xDEADBEEF);
//   writer.WriteF32(3.14f);
//   writer.WriteBool(true);
//   writer.WriteString("hello");  // writes u32 length, then UTF-8 bytes
//
// BinaryWriter is non-copyable (exclusive ownership of the stream) but movable.
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

class HYDRA_API BinaryWriter final : private NonCopyable
{
public:
    // -----------------------------------------------------------------------
    // Construction / destruction
    // -----------------------------------------------------------------------

    /// Creates or truncates the file at `path` and opens it in binary write
    /// mode. Throws std::runtime_error if the file can't be opened.
    explicit BinaryWriter(const Path& path, ByteOrder byteOrder = ByteOrder::LittleEndian);

    /// Wraps an existing output stream without taking ownership — the stream
    /// must outlive this writer. Useful for unit-testing with std::ostringstream.
    explicit BinaryWriter(std::ostream& stream, ByteOrder byteOrder = ByteOrder::LittleEndian);

    ~BinaryWriter();
    BinaryWriter(BinaryWriter&&) noexcept;
    BinaryWriter& operator=(BinaryWriter&&) noexcept;

    // -----------------------------------------------------------------------
    // State
    // -----------------------------------------------------------------------

    [[nodiscard]] bool IsOpen() const noexcept;

    /// Flushes internal buffers to the OS. Call before reading back the file
    /// from a different handle.
    void Flush();

    /// Flushes and closes the stream. Safe to call more than once.
    void Close();

    // -----------------------------------------------------------------------
    // Positioning
    // -----------------------------------------------------------------------

    [[nodiscard]] i64 Tell() const;
    void Seek(i64 offset, SeekOrigin origin = SeekOrigin::Begin);

    // -----------------------------------------------------------------------
    // Unsigned integers
    // -----------------------------------------------------------------------

    void WriteU8 (u8  value);
    void WriteU16(u16 value);
    void WriteU32(u32 value);
    void WriteU64(u64 value);

    // -----------------------------------------------------------------------
    // Signed integers
    // -----------------------------------------------------------------------

    void WriteI8 (i8  value);
    void WriteI16(i16 value);
    void WriteI32(i32 value);
    void WriteI64(i64 value);

    // -----------------------------------------------------------------------
    // Floating-point
    // -----------------------------------------------------------------------

    void WriteF32(f32 value);
    void WriteF64(f64 value);

    // -----------------------------------------------------------------------
    // Boolean (stored as a single byte: 0 = false, 1 = true)
    // -----------------------------------------------------------------------

    void WriteBool(bool value);

    // -----------------------------------------------------------------------
    // Bytes / strings
    // -----------------------------------------------------------------------

    /// Writes exactly `count` bytes from `data`.
    void WriteBytes(const u8* data, usize count);

    /// Convenience: writes all bytes in a vector.
    void WriteBytes(const Vector<u8>& data);

    /// Writes a length-prefixed UTF-8 string: a u32 byte-count followed by
    /// that many bytes of UTF-8 text (no null terminator). This mirrors what
    /// BinaryReader::ReadString() expects.
    void WriteString(StringView text);

    /// Writes exactly `byteCount` bytes of `text`, padded with '\0' if
    /// `text` is shorter, truncated if longer. Useful for fixed-size
    /// string fields in binary file formats.
    void WriteFixedString(StringView text, usize byteCount);

private:
    void WriteRaw(const void* data, usize size);
    void MaybeByteSwap(void* data, usize size) const noexcept;
    static bool s_HostIsBigEndian;

    std::ostream*              m_ExternalStream = nullptr;
    std::unique_ptr<std::ofstream> m_OwnedStream;
    std::ostream*              m_Stream = nullptr;
    ByteOrder                  m_ByteOrder;
};

} // namespace Hydra
