#include <HydraCore/FileSystem/Directory.h>

#include <filesystem>
#include <stdexcept>

namespace Hydra::Directory {

// -----------------------------------------------------------------------
// Queries
// -----------------------------------------------------------------------

bool Exists(const Path& path)
{
    return std::filesystem::is_directory(path.Native());
}

// -----------------------------------------------------------------------
// Creation
// -----------------------------------------------------------------------

bool Create(const Path& path)
{
    std::error_code ec;
    const bool created = std::filesystem::create_directory(path.Native(), ec);
    if (ec)
    {
        throw std::runtime_error("Directory::Create: '" + path.ToString() + "': " + ec.message());
    }
    return created;
}

bool CreateAll(const Path& path)
{
    std::error_code ec;
    const bool created = std::filesystem::create_directories(path.Native(), ec);
    if (ec)
    {
        throw std::runtime_error("Directory::CreateAll: '" + path.ToString() + "': " + ec.message());
    }
    return created;
}

// -----------------------------------------------------------------------
// Deletion
// -----------------------------------------------------------------------

bool Delete(const Path& path)
{
    // std::filesystem::remove() only removes empty directories; it returns
    // false (not an error) if the path didn't exist.
    std::error_code ec;
    const bool removed = std::filesystem::remove(path.Native(), ec);
    if (ec)
    {
        // "not empty" is not an error code here; it just means remove()
        // couldn't do the job, so we return false rather than throwing.
        return false;
    }
    return removed;
}

usize DeleteAll(const Path& path)
{
    std::error_code ec;
    const auto count = std::filesystem::remove_all(path.Native(), ec);
    if (ec)
    {
        throw std::runtime_error("Directory::DeleteAll: '" + path.ToString() + "': " + ec.message());
    }
    return static_cast<usize>(count);
}

// -----------------------------------------------------------------------
// Listing helpers
// -----------------------------------------------------------------------

namespace {

/// Fills `out` with immediate children of `dirPath`, optionally filtering to
/// only files or only directories depending on the two bool parameters.
void CollectEntries(const Path& dirPath, Vector<Path>& out,
                    bool includeFiles, bool includeDirs)
{
    if (!std::filesystem::is_directory(dirPath.Native()))
    {
        throw std::runtime_error("Directory::List: '" + dirPath.ToString() + "' is not a directory");
    }

    for (const auto& entry : std::filesystem::directory_iterator(dirPath.Native()))
    {
        const bool isFile = entry.is_regular_file();
        const bool isDir  = entry.is_directory();

        if ((isFile && includeFiles) || (isDir && includeDirs))
        {
            out.emplace_back(Path::FromNativePath(entry.path()));
        }
    }
}

} // anonymous namespace

// -----------------------------------------------------------------------
// Listing
// -----------------------------------------------------------------------

Vector<Path> List(const Path& path)
{
    Vector<Path> results;
    CollectEntries(path, results, /*files=*/true, /*dirs=*/true);
    return results;
}

Vector<Path> ListFiles(const Path& path)
{
    Vector<Path> results;
    CollectEntries(path, results, /*files=*/true, /*dirs=*/false);
    return results;
}

Vector<Path> ListSubdirectories(const Path& path)
{
    Vector<Path> results;
    CollectEntries(path, results, /*files=*/false, /*dirs=*/true);
    return results;
}

Vector<Path> ListFilesRecursive(const Path& root)
{
    if (!std::filesystem::is_directory(root.Native()))
    {
        throw std::runtime_error("Directory::ListFilesRecursive: '" + root.ToString() + "' is not a directory");
    }

    Vector<Path> results;
    for (const auto& entry : std::filesystem::recursive_directory_iterator(root.Native()))
    {
        if (entry.is_regular_file())
        {
            results.emplace_back(Path::FromNativePath(entry.path()));
        }
    }
    return results;
}

// -----------------------------------------------------------------------
// Current directory
// -----------------------------------------------------------------------

Path GetCurrentDirectory()
{
    return Path::FromNativePath(std::filesystem::current_path());
}

void SetCurrentDirectory(const Path& path)
{
    std::error_code ec;
    std::filesystem::current_path(path.Native(), ec);
    if (ec)
    {
        throw std::runtime_error("Directory::SetCurrentDirectory: '" + path.ToString() + "': " + ec.message());
    }
}

} // namespace Hydra::Directory
