#include <HydraCore/FileSystem/File.h>

#include <filesystem>
#include <fstream>
#include <sstream>
#include <stdexcept>

namespace Hydra::File {

// -----------------------------------------------------------------------
// Queries
// -----------------------------------------------------------------------

bool Exists(const Path& path)
{
    return std::filesystem::is_regular_file(path.Native());
}

u64 GetSize(const Path& path)
{
    std::error_code ec;
    const auto size = std::filesystem::file_size(path.Native(), ec);
    return ec ? 0u : static_cast<u64>(size);
}

Optional<std::filesystem::file_time_type> GetLastModified(const Path& path)
{
    std::error_code ec;
    auto t = std::filesystem::last_write_time(path.Native(), ec);
    if (ec)
    {
        return std::nullopt;
    }
    return t;
}

// -----------------------------------------------------------------------
// Helpers shared by several functions below
// -----------------------------------------------------------------------

namespace {

/// Opens a file for reading, throwing std::runtime_error on failure so
/// callers don't have to repeat the same error message everywhere.
std::ifstream OpenForRead(const Path& path)
{
    std::ifstream stream(path.Native(), std::ios::in | std::ios::binary);
    if (!stream.is_open())
    {
        throw std::runtime_error("File::OpenForRead: cannot open '" + path.ToString() + "'");
    }
    return stream;
}

std::ofstream OpenForWrite(const Path& path, std::ios::openmode extraFlags = {})
{
    std::ofstream stream(path.Native(), std::ios::out | std::ios::binary | extraFlags);
    if (!stream.is_open())
    {
        throw std::runtime_error("File::OpenForWrite: cannot open '" + path.ToString() + "'");
    }
    return stream;
}

} // anonymous namespace

// -----------------------------------------------------------------------
// Bulk I/O
// -----------------------------------------------------------------------

Vector<u8> ReadAllBytes(const Path& path)
{
    auto stream = OpenForRead(path);

    // Seek to end to find size, then back to beginning.
    stream.seekg(0, std::ios::end);
    const std::streamsize size = stream.tellg();
    stream.seekg(0, std::ios::beg);

    Vector<u8> buffer(static_cast<usize>(size));
    stream.read(reinterpret_cast<char*>(buffer.data()), size);

    if (!stream && size > 0)
    {
        throw std::runtime_error("File::ReadAllBytes: read error on '" + path.ToString() + "'");
    }
    return buffer;
}

String ReadAllText(const Path& path)
{
    auto stream = OpenForRead(path);
    std::ostringstream oss;
    oss << stream.rdbuf();
    return oss.str();
}

Vector<String> ReadAllLines(const Path& path)
{
    // Open in text mode so the stream handles newline translation.
    std::ifstream stream(path.Native(), std::ios::in);
    if (!stream.is_open())
    {
        throw std::runtime_error("File::ReadAllLines: cannot open '" + path.ToString() + "'");
    }

    Vector<String> lines;
    String line;
    while (std::getline(stream, line))
    {
        // Strip Windows-style '\r' that may be left after getline strips '\n'.
        if (!line.empty() && line.back() == '\r')
        {
            line.pop_back();
        }
        lines.push_back(std::move(line));
    }
    return lines;
}

void WriteAllBytes(const Path& path, const Vector<u8>& data)
{
    auto stream = OpenForWrite(path, std::ios::trunc);
    if (!data.empty())
    {
        stream.write(reinterpret_cast<const char*>(data.data()),
                     static_cast<std::streamsize>(data.size()));
    }
    if (!stream)
    {
        throw std::runtime_error("File::WriteAllBytes: write error on '" + path.ToString() + "'");
    }
}

void WriteAllText(const Path& path, StringView text)
{
    auto stream = OpenForWrite(path, std::ios::trunc);
    if (!text.empty())
    {
        stream.write(text.data(), static_cast<std::streamsize>(text.size()));
    }
    if (!stream)
    {
        throw std::runtime_error("File::WriteAllText: write error on '" + path.ToString() + "'");
    }
}

void WriteAllLines(const Path& path, const Vector<String>& lines)
{
    auto stream = OpenForWrite(path, std::ios::trunc);
    for (const String& line : lines)
    {
        stream.write(line.data(), static_cast<std::streamsize>(line.size()));
        stream.put('\n');
    }
    if (!stream)
    {
        throw std::runtime_error("File::WriteAllLines: write error on '" + path.ToString() + "'");
    }
}

void AppendText(const Path& path, StringView text)
{
    std::ofstream stream(path.Native(), std::ios::out | std::ios::binary | std::ios::app);
    if (!stream.is_open())
    {
        throw std::runtime_error("File::AppendText: cannot open '" + path.ToString() + "'");
    }
    if (!text.empty())
    {
        stream.write(text.data(), static_cast<std::streamsize>(text.size()));
    }
}

// -----------------------------------------------------------------------
// File manipulation
// -----------------------------------------------------------------------

void Copy(const Path& source, const Path& destination, bool overwrite)
{
    const auto opts = overwrite
        ? std::filesystem::copy_options::overwrite_existing
        : std::filesystem::copy_options::none;

    std::error_code ec;
    std::filesystem::copy_file(source.Native(), destination.Native(), opts, ec);
    if (ec)
    {
        throw std::runtime_error("File::Copy: '" + source.ToString() + "' -> '"
                                 + destination.ToString() + "': " + ec.message());
    }
}

void Move(const Path& source, const Path& destination)
{
    std::error_code ec;
    std::filesystem::rename(source.Native(), destination.Native(), ec);
    if (ec)
    {
        throw std::runtime_error("File::Move: '" + source.ToString() + "' -> '"
                                 + destination.ToString() + "': " + ec.message());
    }
}

bool Delete(const Path& path)
{
    std::error_code ec;
    return std::filesystem::remove(path.Native(), ec);
}

void Touch(const Path& path, bool createParents)
{
    if (createParents)
    {
        const auto parent = path.GetParent();
        if (!parent.IsEmpty())
        {
            std::error_code ec;
            std::filesystem::create_directories(parent.Native(), ec);
            if (ec)
            {
                throw std::runtime_error("File::Touch: cannot create parents for '"
                                         + path.ToString() + "': " + ec.message());
            }
        }
    }

    if (!Exists(path))
    {
        // Creating an empty file — open-and-immediately-close is the simplest
        // way without needing platform-specific calls.
        std::ofstream stream(path.Native(), std::ios::out | std::ios::app);
        if (!stream.is_open())
        {
            throw std::runtime_error("File::Touch: cannot create '" + path.ToString() + "'");
        }
    }
    // If the file already exists, Touch() leaves its contents unchanged
    // and just succeeds (matching the behaviour of the Unix `touch` command
    // when -t is not specified — normally it would update mtime, but updating
    // timestamps requires platform-specific calls we avoid here for portability).
}

} // namespace Hydra::File
