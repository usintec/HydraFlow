#include <HydraCore/FileSystem/NativeFileSystem.h>
#include <HydraCore/FileSystem/BinaryReader.h>
#include <HydraCore/FileSystem/BinaryWriter.h>
#include <HydraCore/FileSystem/TextReader.h>
#include <HydraCore/FileSystem/TextWriter.h>
#include <HydraCore/FileSystem/File.h>
#include <HydraCore/FileSystem/Directory.h>

#include <filesystem>
#include <memory>

namespace Hydra {

// -----------------------------------------------------------------------
// NativeFileSystem — delegates to std::filesystem and our own utilities
// -----------------------------------------------------------------------

bool NativeFileSystem::Exists(const Path& path) const
{
    return std::filesystem::exists(path.Native());
}

bool NativeFileSystem::IsDirectory(const Path& path) const
{
    return std::filesystem::is_directory(path.Native());
}

u64 NativeFileSystem::GetFileSize(const Path& path) const
{
    return File::GetSize(path);
}

UniquePtr<BinaryReader> NativeFileSystem::OpenRead(const Path& path) const
{
    if (!std::filesystem::is_regular_file(path.Native()))
    {
        return nullptr;
    }
    try
    {
        return MakeUnique<BinaryReader>(path);
    }
    catch (...)
    {
        return nullptr;
    }
}

UniquePtr<BinaryWriter> NativeFileSystem::OpenWrite(const Path& path)
{
    try
    {
        return MakeUnique<BinaryWriter>(path);
    }
    catch (...)
    {
        return nullptr;
    }
}

UniquePtr<TextReader> NativeFileSystem::OpenTextRead(const Path& path) const
{
    if (!std::filesystem::is_regular_file(path.Native()))
    {
        return nullptr;
    }
    try
    {
        return MakeUnique<TextReader>(path);
    }
    catch (...)
    {
        return nullptr;
    }
}

UniquePtr<TextWriter> NativeFileSystem::OpenTextWrite(const Path& path)
{
    try
    {
        return MakeUnique<TextWriter>(path);
    }
    catch (...)
    {
        return nullptr;
    }
}

Vector<Path> NativeFileSystem::ListFiles(const Path& path) const
{
    if (!std::filesystem::is_directory(path.Native()))
    {
        return {};
    }
    // List both files and subdirectories for maximum usefulness (callers
    // wanting only files can filter using Path::IsFile()).
    return Directory::List(path);
}

bool NativeFileSystem::CreateDirectory(const Path& path)
{
    std::error_code ec;
    std::filesystem::create_directories(path.Native(), ec);
    return !ec;
}

bool NativeFileSystem::Delete(const Path& path)
{
    std::error_code ec;
    return std::filesystem::remove(path.Native(), ec) && !ec;
}

// -----------------------------------------------------------------------
// FileSystem global accessor — defined here so NativeFileSystem.cpp is the
// only translation unit that needs to know about the concrete default type.
// -----------------------------------------------------------------------

namespace FileSystem {

static UniquePtr<IVirtualFileSystem> s_Instance;

IVirtualFileSystem& Get()
{
    if (!s_Instance)
    {
        // Lazy initialisation: create the default NativeFileSystem on first
        // access so callers never need to call an explicit Init().
        s_Instance = MakeUnique<NativeFileSystem>();
    }
    return *s_Instance;
}

void Set(UniquePtr<IVirtualFileSystem> vfs)
{
    if (vfs)
    {
        s_Instance = std::move(vfs);
    }
    else
    {
        // Passing nullptr restores the default NativeFileSystem (next Get()
        // call will create a fresh one).
        s_Instance.reset();
    }
}

} // namespace FileSystem

} // namespace Hydra
