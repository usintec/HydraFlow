#include <HydraCore/FileSystem/Path.h>

#include <filesystem>
#include <ostream>
#include <stdexcept>

namespace Hydra {

// -----------------------------------------------------------------------
// Construction
// -----------------------------------------------------------------------

Path::Path(StringView utf8Path)
    // std::filesystem::path on Linux treats std::string as UTF-8 natively
    // because the locale is UTF-8. On Windows one would use u8string; since
    // the engine currently targets Linux we can pass the std::string directly.
    : m_Path(std::string(utf8Path))
{
}

Path Path::FromNativePath(std::filesystem::path nativePath)
{
    Path p;
    p.m_Path = std::move(nativePath);
    return p;
}

// -----------------------------------------------------------------------
// Path composition
// -----------------------------------------------------------------------

Path Path::Append(StringView component) const
{
    // std::filesystem::path::operator/ handles trailing-separator edge cases
    // and absolute-component replacement exactly as POSIX specifies.
    return FromNativePath(m_Path / std::string(component));
}

Path Path::operator/(StringView component) const  { return Append(component); }
Path Path::operator/(const Path& other) const      { return FromNativePath(m_Path / other.m_Path); }

// -----------------------------------------------------------------------
// Path decomposition
// -----------------------------------------------------------------------

Path Path::GetParent() const { return FromNativePath(m_Path.parent_path()); }

String Path::GetFilename()  const { return m_Path.filename().string(); }
String Path::GetStem()      const { return m_Path.stem().string(); }
String Path::GetExtension() const { return m_Path.extension().string(); }

// -----------------------------------------------------------------------
// Path queries
// -----------------------------------------------------------------------

bool Path::IsEmpty()    const noexcept { return m_Path.empty(); }
bool Path::IsAbsolute() const noexcept { return m_Path.is_absolute(); }
bool Path::IsRelative() const noexcept { return m_Path.is_relative(); }

bool Path::Exists()      const { return std::filesystem::exists(m_Path); }
bool Path::IsFile()      const { return std::filesystem::is_regular_file(m_Path); }
bool Path::IsDirectory() const { return std::filesystem::is_directory(m_Path); }

// -----------------------------------------------------------------------
// String conversion
// -----------------------------------------------------------------------

String Path::ToString() const
{
    // Always return with forward-slash separators so the string is
    // consistent across platforms and safe for cross-platform storage.
    String result = m_Path.generic_string();
    return result;
}

String Path::ToNative() const
{
    // On Linux this is identical to ToString() since '/' is the native
    // separator. On Windows this would use backslashes. The standard
    // library handles the difference for us via .string().
    return m_Path.string();
}

// -----------------------------------------------------------------------
// Normalisation
// -----------------------------------------------------------------------

Path Path::Normalize() const
{
    // lexically_normal resolves '.' and '..' without touching the disk
    // (no filesystem calls, safe for non-existent paths).
    return FromNativePath(m_Path.lexically_normal());
}

Path Path::ToAbsolute() const
{
    // weakly_canonical makes the path absolute and resolves '.'/'..' but
    // does NOT require every component to exist (unlike canonical()).
    std::error_code ec;
    auto abs = std::filesystem::weakly_canonical(m_Path, ec);
    if (ec)
    {
        // Fallback: simple absolute_path if weakly_canonical fails.
        return FromNativePath(std::filesystem::absolute(m_Path));
    }
    return FromNativePath(std::move(abs));
}

// -----------------------------------------------------------------------
// Static helpers
// -----------------------------------------------------------------------

Path Path::GetCurrentDirectory()
{
    return FromNativePath(std::filesystem::current_path());
}

Path Path::GetTempDirectory()
{
    return FromNativePath(std::filesystem::temp_directory_path());
}

// -----------------------------------------------------------------------
// Comparison
// -----------------------------------------------------------------------

bool Path::operator==(const Path& other) const noexcept { return m_Path == other.m_Path; }
bool Path::operator!=(const Path& other) const noexcept { return m_Path != other.m_Path; }
bool Path::operator< (const Path& other) const noexcept { return m_Path <  other.m_Path; }

// -----------------------------------------------------------------------
// Stream output
// -----------------------------------------------------------------------

std::ostream& operator<<(std::ostream& os, const Path& path)
{
    return os << path.ToString();
}

} // namespace Hydra
