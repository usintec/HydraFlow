#pragma once

#include <HydraCore/Common/Platform.h>
#include <HydraCore/Logging/ILogSink.h>

#include <filesystem>
#include <fstream>

namespace Hydra {

// =============================================================================
// FileSink
//
// A thread-safe log sink that writes formatted messages to a file on disk.
//
// Log rotation:
//   When the active log file exceeds m_MaxBytes, the sink performs a
//   size-based rotation before the next write:
//
//     hydra.log   → hydra.log.1
//     hydra.log.1 → hydra.log.2
//     ...
//     hydra.log.N → deleted   (where N = m_MaxFiles)
//
//   After rotation a fresh hydra.log is opened.  The rotation threshold and
//   the number of backup files kept are both configurable.
//
// Truncation:
//   The active log file is truncated (overwritten) on construction by default.
//   Pass truncate=false to append to an existing file.
//
// Thread safety:
//   All writes are serialised via the inherited m_Mutex; the sink is safe for
//   concurrent use from multiple threads.
//
// Usage:
//   auto sink = std::make_shared<FileSink>("logs/hydra.log");
//   sink->SetMaxBytes(10 * 1024 * 1024); // 10 MB per file
//   sink->SetMaxFiles(5);                // keep 5 backups
// =============================================================================

class HYDRA_API FileSink final : public ILogSink
{
public:
    /// Construct and open the log file.
    /// @param path       Path to the active log file.
    ///                   Parent directories are created if they do not exist.
    /// @param truncate   If true (default), the file is cleared on open.
    explicit FileSink(std::filesystem::path path, bool truncate = true);

    ~FileSink() override;

    // -------------------------------------------------------------------------
    // Rotation configuration
    // -------------------------------------------------------------------------

    /// Maximum size in bytes before rotation is triggered.
    /// Set to 0 to disable size-based rotation.
    /// Default: 10 MiB.
    void SetMaxBytes(usize maxBytes) noexcept;

    [[nodiscard]] usize GetMaxBytes() const noexcept;

    /// Number of rotated backup files to keep (hydra.log.1 … hydra.log.N).
    /// Older backups beyond this limit are deleted.
    /// Default: 5.
    void SetMaxFiles(u32 maxFiles) noexcept;

    [[nodiscard]] u32 GetMaxFiles() const noexcept;

    // -------------------------------------------------------------------------
    // Queries
    // -------------------------------------------------------------------------

    [[nodiscard]] const std::filesystem::path& GetPath() const noexcept;

    /// Returns the current size of the active log file in bytes.
    [[nodiscard]] usize GetCurrentSize() const noexcept;

    [[nodiscard]] bool IsOpen() const noexcept;

    // -------------------------------------------------------------------------
    // ILogSink
    // -------------------------------------------------------------------------

    /// Writes the formatted record, rotating the file first if needed.
    /// Thread-safe via inherited mutex.
    void Sink(const LogMessage& msg) override;

    /// Flushes the underlying file stream immediately.
    void Flush() override;

private:
    /// Opens (or re-opens) m_Path with the requested mode.
    bool OpenFile(bool truncate);

    /// Performs size-based rotation: shift .1→.2, …, rotate active → .1.
    /// Assumes m_Mutex is already held by the caller.
    void Rotate();

    std::filesystem::path m_Path;          ///< Path to the active log file
    std::ofstream         m_Stream;        ///< Active output stream
    usize                 m_CurrentSize;   ///< Bytes written since last open/rotate
    usize                 m_MaxBytes;      ///< Rotation threshold (0 = disabled)
    u32                   m_MaxFiles;      ///< Max number of backup files to retain
};

} // namespace Hydra
