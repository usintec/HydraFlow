#include <HydraCore/FileSystem/BinaryWriter.h>

#include <bit>
#include <cstring>
#include <stdexcept>
#include <utility>

namespace Hydra {

// -----------------------------------------------------------------------
// Detect host endianness at startup (mirrors BinaryReader.cpp)
// -----------------------------------------------------------------------

bool BinaryWriter::s_HostIsBigEndian = (std::endian::native == std::endian::big);

// -----------------------------------------------------------------------
// Construction
// -----------------------------------------------------------------------

BinaryWriter::BinaryWriter(const Path& path, ByteOrder byteOrder)
    : m_OwnedStream(std::make_unique<std::ofstream>(
          path.Native(), std::ios::out | std::ios::binary | std::ios::trunc))
    , m_ByteOrder(byteOrder)
{
    if (!m_OwnedStream->is_open())
    {
        throw std::runtime_error("BinaryWriter: cannot open '" + path.ToString() + "'");
    }
    m_Stream = m_OwnedStream.get();
}

BinaryWriter::BinaryWriter(std::ostream& stream, ByteOrder byteOrder)
    : m_ExternalStream(&stream)
    , m_Stream(&stream)
    , m_ByteOrder(byteOrder)
{
}

BinaryWriter::~BinaryWriter()
{
    if (m_OwnedStream && m_OwnedStream->is_open())
    {
        m_OwnedStream->flush();
    }
}

BinaryWriter::BinaryWriter(BinaryWriter&& other) noexcept
    : m_ExternalStream(other.m_ExternalStream)
    , m_OwnedStream(std::move(other.m_OwnedStream))
    , m_ByteOrder(other.m_ByteOrder)
{
    m_Stream = m_OwnedStream ? static_cast<std::ostream*>(m_OwnedStream.get())
                             : m_ExternalStream;
    other.m_Stream = nullptr;
    other.m_ExternalStream = nullptr;
}

BinaryWriter& BinaryWriter::operator=(BinaryWriter&& other) noexcept
{
    if (this != &other)
    {
        m_ExternalStream = other.m_ExternalStream;
        m_OwnedStream    = std::move(other.m_OwnedStream);
        m_ByteOrder      = other.m_ByteOrder;
        m_Stream = m_OwnedStream ? static_cast<std::ostream*>(m_OwnedStream.get())
                                 : m_ExternalStream;
        other.m_Stream = nullptr;
        other.m_ExternalStream = nullptr;
    }
    return *this;
}

// -----------------------------------------------------------------------
// State
// -----------------------------------------------------------------------

bool BinaryWriter::IsOpen() const noexcept
{
    if (!m_Stream) return false;
    if (m_OwnedStream) return m_OwnedStream->is_open();
    return m_Stream->good();
}

void BinaryWriter::Flush()
{
    if (m_Stream) m_Stream->flush();
}

void BinaryWriter::Close()
{
    if (m_OwnedStream)
    {
        m_OwnedStream->flush();
        m_OwnedStream->close();
    }
}

// -----------------------------------------------------------------------
// Positioning
// -----------------------------------------------------------------------

i64 BinaryWriter::Tell() const
{
    if (!m_Stream) throw std::runtime_error("BinaryWriter::Tell: stream not open");
    return static_cast<i64>(m_Stream->tellp());
}

void BinaryWriter::Seek(i64 offset, SeekOrigin origin)
{
    if (!m_Stream) throw std::runtime_error("BinaryWriter::Seek: stream not open");
    std::ios::seekdir dir;
    switch (origin)
    {
        case SeekOrigin::Begin:   dir = std::ios::beg; break;
        case SeekOrigin::Current: dir = std::ios::cur; break;
        case SeekOrigin::End:     dir = std::ios::end; break;
        default:                  dir = std::ios::beg; break;
    }
    m_Stream->seekp(offset, dir);
    if (!m_Stream->good()) throw std::runtime_error("BinaryWriter::Seek: seek failed");
}

// -----------------------------------------------------------------------
// Internal helpers
// -----------------------------------------------------------------------

void BinaryWriter::MaybeByteSwap(void* data, usize size) const noexcept
{
    if (size <= 1 || m_ByteOrder == ByteOrder::Native) return;

    const bool fileIsBig = (m_ByteOrder == ByteOrder::BigEndian);
    if (fileIsBig == s_HostIsBigEndian) return;

    auto* bytes = static_cast<u8*>(data);
    for (usize i = 0, j = size - 1; i < j; ++i, --j)
    {
        std::swap(bytes[i], bytes[j]);
    }
}

void BinaryWriter::WriteRaw(const void* data, usize size)
{
    if (!m_Stream || !m_Stream->write(static_cast<const char*>(data),
                                       static_cast<std::streamsize>(size)))
    {
        throw std::runtime_error("BinaryWriter: write error");
    }
}

// -----------------------------------------------------------------------
// Typed writes — copy value, maybe byte-swap the copy, then write raw.
// The copy ensures we never modify the caller's variable.
// -----------------------------------------------------------------------

void BinaryWriter::WriteU8(u8 value)
{
    WriteRaw(&value, sizeof(value));
}

void BinaryWriter::WriteU16(u16 value)
{
    MaybeByteSwap(&value, sizeof(value));
    WriteRaw(&value, sizeof(value));
}

void BinaryWriter::WriteU32(u32 value)
{
    MaybeByteSwap(&value, sizeof(value));
    WriteRaw(&value, sizeof(value));
}

void BinaryWriter::WriteU64(u64 value)
{
    MaybeByteSwap(&value, sizeof(value));
    WriteRaw(&value, sizeof(value));
}

void BinaryWriter::WriteI8 (i8  value) { WriteU8 (static_cast<u8> (value)); }
void BinaryWriter::WriteI16(i16 value) { WriteU16(static_cast<u16>(value)); }
void BinaryWriter::WriteI32(i32 value) { WriteU32(static_cast<u32>(value)); }
void BinaryWriter::WriteI64(i64 value) { WriteU64(static_cast<u64>(value)); }

void BinaryWriter::WriteF32(f32 value)
{
    // Copy float bits into a u32 via memcpy to avoid UB, byte-swap the u32,
    // then write the (possibly swapped) bits.
    u32 bits;
    std::memcpy(&bits, &value, sizeof(bits));
    MaybeByteSwap(&bits, sizeof(bits));
    WriteRaw(&bits, sizeof(bits));
}

void BinaryWriter::WriteF64(f64 value)
{
    u64 bits;
    std::memcpy(&bits, &value, sizeof(bits));
    MaybeByteSwap(&bits, sizeof(bits));
    WriteRaw(&bits, sizeof(bits));
}

void BinaryWriter::WriteBool(bool value)
{
    WriteU8(value ? 1u : 0u);
}

void BinaryWriter::WriteBytes(const u8* data, usize count)
{
    WriteRaw(data, count);
}

void BinaryWriter::WriteBytes(const Vector<u8>& data)
{
    if (!data.empty())
    {
        WriteRaw(data.data(), data.size());
    }
}

void BinaryWriter::WriteString(StringView text)
{
    // Write a u32 length prefix followed by the UTF-8 bytes (no null terminator).
    const u32 length = static_cast<u32>(text.size());
    WriteU32(length);
    if (length > 0)
    {
        WriteRaw(text.data(), length);
    }
}

void BinaryWriter::WriteFixedString(StringView text, usize byteCount)
{
    // Write min(text.size(), byteCount) bytes of text, then pad to byteCount
    // with null bytes.
    const usize toWrite = std::min(text.size(), byteCount);
    if (toWrite > 0)
    {
        WriteRaw(text.data(), toWrite);
    }
    const usize padding = byteCount - toWrite;
    for (usize i = 0; i < padding; ++i)
    {
        const char zero = '\0';
        WriteRaw(&zero, 1);
    }
}

} // namespace Hydra
