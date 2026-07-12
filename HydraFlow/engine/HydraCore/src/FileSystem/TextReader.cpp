#include <HydraCore/FileSystem/TextReader.h>

#include <sstream>
#include <stdexcept>
#include <utility>

namespace Hydra {

// -----------------------------------------------------------------------
// Construction
// -----------------------------------------------------------------------

TextReader::TextReader(const Path& path)
    : m_OwnedStream(std::make_unique<std::ifstream>(path.Native(), std::ios::in))
{
    if (!m_OwnedStream->is_open())
    {
        throw std::runtime_error("TextReader: cannot open '" + path.ToString() + "'");
    }
    m_Stream = m_OwnedStream.get();
    SkipBom();
}

TextReader::TextReader(std::istream& stream)
    : m_ExternalStream(&stream)
    , m_Stream(&stream)
{
    SkipBom();
}

TextReader::~TextReader() = default;

TextReader::TextReader(TextReader&& other) noexcept
    : m_ExternalStream(other.m_ExternalStream)
    , m_OwnedStream(std::move(other.m_OwnedStream))
{
    m_Stream = m_OwnedStream ? static_cast<std::istream*>(m_OwnedStream.get())
                             : m_ExternalStream;
    other.m_Stream = nullptr;
    other.m_ExternalStream = nullptr;
}

TextReader& TextReader::operator=(TextReader&& other) noexcept
{
    if (this != &other)
    {
        m_ExternalStream = other.m_ExternalStream;
        m_OwnedStream    = std::move(other.m_OwnedStream);
        m_Stream = m_OwnedStream ? static_cast<std::istream*>(m_OwnedStream.get())
                                 : m_ExternalStream;
        other.m_Stream = nullptr;
        other.m_ExternalStream = nullptr;
    }
    return *this;
}

// -----------------------------------------------------------------------
// BOM detection
// -----------------------------------------------------------------------

void TextReader::SkipBom()
{
    // The UTF-8 BOM is the three-byte sequence 0xEF 0xBB 0xBF.
    // We peek at the first three bytes; if they match, consume them.
    // If they don't match we put the bytes back via seekg (safe here
    // because std::ifstream opened in text mode is seekable from the start).
    if (!m_Stream || !m_Stream->good())
    {
        return;
    }

    const int b0 = m_Stream->get();
    if (b0 != 0xEF) { if (b0 != EOF) m_Stream->putback(static_cast<char>(b0)); return; }

    const int b1 = m_Stream->get();
    if (b1 != 0xBB)
    {
        // Not a BOM — put both bytes back.
        if (b1 != EOF) m_Stream->putback(static_cast<char>(b1));
        m_Stream->putback(static_cast<char>(b0));
        return;
    }

    const int b2 = m_Stream->get();
    if (b2 != 0xBF)
    {
        if (b2 != EOF) m_Stream->putback(static_cast<char>(b2));
        m_Stream->putback(static_cast<char>(b1));
        m_Stream->putback(static_cast<char>(b0));
        return;
    }

    // All three BOM bytes found — they are now consumed and won't appear
    // in any subsequent read.
}

// -----------------------------------------------------------------------
// State
// -----------------------------------------------------------------------

bool TextReader::IsOpen() const noexcept
{
    if (!m_Stream) return false;
    if (m_OwnedStream) return m_OwnedStream->is_open();
    return m_Stream->good();
}

bool TextReader::IsEof() const noexcept
{
    return !m_Stream || m_Stream->eof();
}

void TextReader::Close()
{
    if (m_OwnedStream) m_OwnedStream->close();
}

// -----------------------------------------------------------------------
// Line-by-line reading
// -----------------------------------------------------------------------

Optional<String> TextReader::ReadLine()
{
    if (!m_Stream || m_Stream->eof())
    {
        return std::nullopt;
    }

    String line;
    if (!std::getline(*m_Stream, line))
    {
        return std::nullopt;
    }

    // Strip Windows-style carriage return that std::getline leaves before '\n'.
    if (!line.empty() && line.back() == '\r')
    {
        line.pop_back();
    }

    return line;
}

Vector<String> TextReader::ReadAllLines()
{
    Vector<String> lines;
    while (Optional<String> line = ReadLine())
    {
        lines.push_back(std::move(*line));
    }
    return lines;
}

// -----------------------------------------------------------------------
// Character reading
// -----------------------------------------------------------------------

int TextReader::ReadChar()
{
    if (!m_Stream) return -1;
    return m_Stream->get();
}

int TextReader::Peek() const
{
    if (!m_Stream) return -1;
    return m_Stream->peek();
}

// -----------------------------------------------------------------------
// Bulk reading
// -----------------------------------------------------------------------

String TextReader::ReadAll()
{
    if (!m_Stream) return {};
    std::ostringstream oss;
    oss << m_Stream->rdbuf();
    return oss.str();
}

// -----------------------------------------------------------------------
// Positioning
// -----------------------------------------------------------------------

i64 TextReader::Tell() const
{
    if (!m_Stream) throw std::runtime_error("TextReader::Tell: stream not open");
    return static_cast<i64>(m_Stream->tellg());
}

void TextReader::Seek(i64 offset, SeekOrigin origin)
{
    if (!m_Stream) throw std::runtime_error("TextReader::Seek: stream not open");
    std::ios::seekdir dir;
    switch (origin)
    {
        case SeekOrigin::Begin:   dir = std::ios::beg; break;
        case SeekOrigin::Current: dir = std::ios::cur; break;
        case SeekOrigin::End:     dir = std::ios::end; break;
        default:                  dir = std::ios::beg; break;
    }
    m_Stream->seekg(offset, dir);
}

} // namespace Hydra
