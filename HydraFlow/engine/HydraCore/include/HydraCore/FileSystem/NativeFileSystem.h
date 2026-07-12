#pragma once

// =============================================================================
// NativeFileSystem.h
//
// The default IVirtualFileSystem implementation: delegates every operation
// directly to the OS via std::filesystem and standard C++ file streams.
// This is what you get automatically when nothing else has been installed
// via FileSystem::Set().
//
// There is normally no reason to use NativeFileSystem directly in application
// code — go through the IVirtualFileSystem interface (and the FileSystem::Get()
// accessor) instead, so you can swap in a different backend for testing or
// asset-bundling without changing call sites.
// =============================================================================

#include <HydraCore/Common/Platform.h>
#include <HydraCore/FileSystem/IVirtualFileSystem.h>

namespace Hydra {

class HYDRA_API NativeFileSystem final : public IVirtualFileSystem
{
public:
    NativeFileSystem()  = default;
    ~NativeFileSystem() = default;

    [[nodiscard]] bool   Exists(const Path& path) const override;
    [[nodiscard]] bool   IsDirectory(const Path& path) const override;
    [[nodiscard]] u64    GetFileSize(const Path& path) const override;

    [[nodiscard]] UniquePtr<BinaryReader> OpenRead    (const Path& path) const override;
    [[nodiscard]] UniquePtr<BinaryWriter> OpenWrite   (const Path& path)       override;
    [[nodiscard]] UniquePtr<TextReader>   OpenTextRead (const Path& path) const override;
    [[nodiscard]] UniquePtr<TextWriter>   OpenTextWrite(const Path& path)       override;

    [[nodiscard]] Vector<Path> ListFiles       (const Path& path) const override;
    bool                       CreateDirectory (const Path& path)       override;
    bool                       Delete          (const Path& path)       override;
};

} // namespace Hydra
