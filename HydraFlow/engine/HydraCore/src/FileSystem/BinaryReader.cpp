#include <HydraCore/FileSystem/BinaryReader.h>

#include <bit>
#include <cstring>
#include <stdexcept>
#include <utility>

namespace Hydra {

// -----------------------------------------------------------------------
// Detect host endianness at startup
// -----------------------------------------------------------------------

// C++20 provides std::endian to check host byte order at compile time.
bool BinaryReader::s_HostIsBigEndian = (std::endian::native == std::endian::big);

// -----------------------------------------------------------------------
// Construction
// -----------------------------------------------------------------------

BinaryReader::BinaryReader(const Path& path, ByteOrder byteOrder)
    : m_OwnedStream(std::make_unique<std::ifstream>(path.Native(), std::ios::in | std::ios::binary))
    , m_ByteOrder(byteOrder)
{
    if (!m_OwnedStream->is_open())
    {
        throw std::runtime_error("BinaryReader: cannot open '" + path.ToString() + "'");
    }
    m_Stream = m_OwnedStream.get();
}

BinaryReader::BinaryReader(std::istream& stream, ByteOrder byteOrder)
    : m_ExternalStream(&stream)
    , m_Stream(&stream)
    , m_ByteOrder(byteOrder)
{
}

BinaryReader::~BinaryReader() = default;

BinaryReader::BinaryReader(BinaryReader&& other) noexcept
    : m_ExternalStream(other.m_ExternalStream)
    , m_OwnedStream(std::move(other.m_OwnedStream))
    , m_ByteOrder(other.m_ByteOrder)
{
    // After the move, re-point m_Stream at whatever is now active.
    m_Stream = m_OwnedStream ? static_cast<std::istream*>(m_OwnedStream.get())
                             : m_ExternalStream;
    other.m_Stream = nullptr;
    other.m_ExternalStream = nullptr;
}

BinaryReader& BinaryReader::operator=(BinaryReader&& other) noexcept
{
    if (this != &other)
    {
        m_ExternalStream = other.m_ExternalStream;
        m_OwnedStream    = std::move(other.m_OwnedStream);
        m_ByteOrder      = other.m_ByteOrder;
        m_Stream = m_OwnedStream ? static_cast<std::istream*>(m_OwnedStream.get())
                                 : m_ExternalStream;
        other.m_Stream = nullptr;
        other.m_ExternalStream = nullptr;
    }
    return *this;
}

// -----------------------------------------------------------------------
// State
// -----------------------------------------------------------------------

bool BinaryReader::IsOpen() const noexcept
{
    if (!m_Stream) return false;
    if (m_OwnedStream) return m_OwnedStream->is_open();
    return m_Stream->good();
}

bool BinaryReader::IsEof() const noexcept
{
    return !m_Stream || m_Stream->eof();
}

void BinaryReader::Close()
{
    if (m_OwnedStream)
    {
        m_OwnedStream->close();
    }
}

// -----------------------------------------------------------------------
// Positioning
// -----------------------------------------------------------------------

i64 BinaryReader::Tell() const
{
    if (!m_Stream)
    {
        throw std::runtime_error("BinaryReader::Tell: stream not open");
    }
    return static_cast<i64>(m_Stream->tellg());
}

void BinaryReader::Seek(i64 offset, SeekOrigin origin)
{
    if (!m_Stream)
    {
        throw std::runtime_error("BinaryReader::Seek: stream not open");
    }

    std::ios::seekdir dir;
    switch (origin)
    {
        case SeekOrigin::Begin:   dir = std::ios::beg; break;
        case SeekOrigin::Current: dir = std::ios::cur; break;
        case SeekOrigin::End:     dir = std::ios::end; break;
        default:                  dir = std::ios::beg; break;
    }

    m_Stream->seekg(offset, dir);
    if (!m_Stream->good())
    {
        throw std::runtime_error("BinaryReader::Seek: seek failed");
    }
}

// -----------------------------------------------------------------------
// Internal I/O helpers
// -----------------------------------------------------------------------

void BinaryReader::MaybeByteSwap(void* data, usize size) const noexcept
{
    if (size <= 1 || m_ByteOrder == ByteOrder::Native)
    {
        return; // No swap needed.
    }

    // Determine whether the file's declared byte order matches the host.
    const bool fileIsBig = (m_ByteOrder == ByteOrder::BigEndian);
    if (fileIsBig == s_HostIsBigEndian)
    {
        return; // File and host agree — nothing to swap.
    }

    // Reverse the bytes in place.
    auto* bytes = static_cast<u8*>(data);
    for (usize i = 0, j = size - 1; i < j; ++i, --j)
    {
        std::swap(bytes[i], bytes[j]);
    }
}

void BinaryReader::ReadRaw(void* buffer, usize size)
{
    if (!m_Stream || !m_Stream->read(static_cast<char*>(buffer), static_cast<std::streamsize>(size)))
    {
        const usize bytesRead = static_cast<usize>(m_Stream ? m_Stream->gcount() : 0);
        throw std::runtime_error(
            "BinaryReader: unexpected end of stream (expected " + std::to_string(size)
            + " bytes, got " + std::to_string(bytesRead) + ")");
    }
}

// -----------------------------------------------------------------------
// Typed reads — each reads the raw bytes then maybe byte-swaps.
// -----------------------------------------------------------------------

u8 BinaryReader::ReadU8()
{
    u8 v;
    ReadRaw(&v, sizeof(v));
    return v;
}

u16 BinaryReader::ReadU16()
{
    u16 v;
    ReadRaw(&v, sizeof(v));
    MaybeByteSwap(&v, sizeof(v));
    return v;
}

u32 BinaryReader::ReadU32()
{
    u32 v;
    ReadRaw(&v, sizeof(v));
    MaybeByteSwap(&v, sizeof(v));
    return v;
}

u64 BinaryReader::ReadU64()
{
    u64 v;
    ReadRaw(&v, sizeof(v));
    MaybeByteSwap(&v, sizeof(v));
    return v;
}

i8  BinaryReader::ReadI8()  { return static_cast<i8>(ReadU8());  }
i16 BinaryReader::ReadI16() { return static_cast<i16>(ReadU16()); }
i32 BinaryReader::ReadI32() { return static_cast<i32>(ReadU32()); }
i64 BinaryReader::ReadI64() { return static_cast<i64>(ReadU64()); }

f32 BinaryReader::ReadF32()
{
    // Read the raw bits as a u32 (same size), byte-swap if needed, then
    // reinterpret as float. Using memcpy avoids undefined behaviour from
    // type-punning through a union or pointer cast.
    u32 bits;
    ReadRaw(&bits, sizeof(bits));
    MaybeByteSwap(&bits, sizeof(bits));
    f32 v;
    std::memcpy(&v, &bits, sizeof(v));
    return v;
}

f64 BinaryReader::ReadF64()
{
    u64 bits;
    ReadRaw(&bits, sizeof(bits));
    MaybeByteSwap(&bits, sizeof(bits));
    f64 v;
    std::memcpy(&v, &bits, sizeof(v));
    return v;
}

bool BinaryReader::ReadBool()
{
    return ReadU8() != 0;
}

Vector<u8> BinaryReader::ReadBytes(usize count)
{
    Vector<u8> buffer(count);
    ReadRaw(buffer.data(), count);
    return buffer;
}

void BinaryReader::ReadBytes(u8* buffer, usize count)
{
    ReadRaw(buffer, count);
}

String BinaryReader::ReadString()
{
    // Length-prefixed format: u32 byte count, then that many UTF-8 bytes.
    const u32 length = ReadU32();
    return ReadFixedString(static_cast<usize>(length));
}

String BinaryReader::ReadFixedString(usize byteCount)
{
    String result(byteCount, '\0');
    ReadRaw(result.data(), byteCount);
    return result;
}

} // namespace Hydra
