#include <HydraCore/FileSystem/TextWriter.h>

#include <stdexcept>
#include <utility>

namespace Hydra {

// -----------------------------------------------------------------------
// Construction
// -----------------------------------------------------------------------

TextWriter::TextWriter(const Path& path, OpenMode mode)
{
    // Decide whether to append or truncate based on the caller's OpenMode.
    std::ios::openmode flags = std::ios::out;
    if (HasOpenMode(mode, OpenMode::Append))
    {
        flags |= std::ios::app;
    }
    else
    {
        flags |= std::ios::trunc;
    }

    m_OwnedStream = std::make_unique<std::ofstream>(path.Native(), flags);
    if (!m_OwnedStream->is_open())
    {
        throw std::runtime_error("TextWriter: cannot open '" + path.ToString() + "'");
    }
    m_Stream = m_OwnedStream.get();
}

TextWriter::TextWriter(std::ostream& stream)
    : m_ExternalStream(&stream)
    , m_Stream(&stream)
{
}

TextWriter::~TextWriter()
{
    if (m_OwnedStream && m_OwnedStream->is_open())
    {
        m_OwnedStream->flush();
    }
}

TextWriter::TextWriter(TextWriter&& other) noexcept
    : m_ExternalStream(other.m_ExternalStream)
    , m_OwnedStream(std::move(other.m_OwnedStream))
{
    m_Stream = m_OwnedStream ? static_cast<std::ostream*>(m_OwnedStream.get())
                             : m_ExternalStream;
    other.m_Stream = nullptr;
    other.m_ExternalStream = nullptr;
}

TextWriter& TextWriter::operator=(TextWriter&& other) noexcept
{
    if (this != &other)
    {
        m_ExternalStream = other.m_ExternalStream;
        m_OwnedStream    = std::move(other.m_OwnedStream);
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

bool TextWriter::IsOpen() const noexcept
{
    if (!m_Stream) return false;
    if (m_OwnedStream) return m_OwnedStream->is_open();
    return m_Stream->good();
}

void TextWriter::Flush()
{
    if (m_Stream) m_Stream->flush();
}

void TextWriter::Close()
{
    if (m_OwnedStream)
    {
        m_OwnedStream->flush();
        m_OwnedStream->close();
    }
}

// -----------------------------------------------------------------------
// Writing
// -----------------------------------------------------------------------

void TextWriter::Write(StringView text)
{
    if (!m_Stream || text.empty()) return;
    m_Stream->write(text.data(), static_cast<std::streamsize>(text.size()));
    if (!m_Stream->good())
    {
        throw std::runtime_error("TextWriter::Write: write error");
    }
}

void TextWriter::WriteLine(StringView text)
{
    Write(text);
    // Always use '\n'; on Windows the C++ runtime can translate this to
    // '\r\n' if the stream was opened in text mode, but we opened in
    // binary-compatible mode so '\n' is written as-is (cross-platform safe).
    const char nl = '\n';
    m_Stream->put(nl);
}

// -----------------------------------------------------------------------
// Positioning
// -----------------------------------------------------------------------

i64 TextWriter::Tell() const
{
    if (!m_Stream) throw std::runtime_error("TextWriter::Tell: stream not open");
    return static_cast<i64>(m_Stream->tellp());
}

void TextWriter::Seek(i64 offset, SeekOrigin origin)
{
    if (!m_Stream) throw std::runtime_error("TextWriter::Seek: stream not open");
    std::ios::seekdir dir;
    switch (origin)
    {
        case SeekOrigin::Begin:   dir = std::ios::beg; break;
        case SeekOrigin::Current: dir = std::ios::cur; break;
        case SeekOrigin::End:     dir = std::ios::end; break;
        default:                  dir = std::ios::beg; break;
    }
    m_Stream->seekp(offset, dir);
}

} // namespace Hydra
