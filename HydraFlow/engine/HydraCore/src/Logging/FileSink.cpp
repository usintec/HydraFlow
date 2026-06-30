#include <HydraCore/Logging/FileSink.h>
#include <HydraCore/Logging/LogLevel.h>

#include <cstdio>
#include <format>
#include <stdexcept>

namespace Hydra {

// Default rotation thresholds — conservative values appropriate for most
// applications; can be overridden per-sink via SetMaxBytes / SetMaxFiles.
static constexpr usize kDefaultMaxBytes = 10 * 1024 * 1024; // 10 MiB
static constexpr u32   kDefaultMaxFiles = 5;

// =============================================================================
// Construction / Destruction
// =============================================================================

FileSink::FileSink(std::filesystem::path path, bool truncate)
    : m_Path(std::move(path))
    , m_CurrentSize(0)
    , m_MaxBytes(kDefaultMaxBytes)
    , m_MaxFiles(kDefaultMaxFiles)
{
    // Ensure the parent directory exists so we do not fail on first write
    if (m_Path.has_parent_path())
        std::filesystem::create_directories(m_Path.parent_path());

    if (!OpenFile(truncate))
    {
        // Report construction failure — the sink will silently discard messages
        // when m_Stream is not open, but we warn the caller here.
        fprintf(stderr, "[FileSink] Failed to open log file: %s\n",
                m_Path.c_str());
    }
}

FileSink::~FileSink()
{
    // Ensure any buffered writes reach disk before the stream is destroyed.
    if (m_Stream.is_open())
    {
        m_Stream.flush();
        m_Stream.close();
    }
}

// =============================================================================
// Configuration
// =============================================================================

void FileSink::SetMaxBytes(usize maxBytes) noexcept
{
    std::lock_guard lock(m_Mutex);
    m_MaxBytes = maxBytes;
}

usize FileSink::GetMaxBytes() const noexcept
{
    return m_MaxBytes;
}

void FileSink::SetMaxFiles(u32 maxFiles) noexcept
{
    std::lock_guard lock(m_Mutex);
    m_MaxFiles = maxFiles;
}

u32 FileSink::GetMaxFiles() const noexcept
{
    return m_MaxFiles;
}

const std::filesystem::path& FileSink::GetPath() const noexcept
{
    return m_Path;
}

usize FileSink::GetCurrentSize() const noexcept
{
    std::lock_guard lock(m_Mutex);
    return m_CurrentSize;
}

bool FileSink::IsOpen() const noexcept
{
    std::lock_guard lock(m_Mutex);
    return m_Stream.is_open();
}

// =============================================================================
// ILogSink::Sink
// =============================================================================

void FileSink::Sink(const LogMessage& msg)
{
    // Format outside the lock — formatting is pure / thread-safe.
    String formatted = FormatMessage(msg);
    formatted += '\n';

    std::lock_guard lock(m_Mutex);

    if (!m_Stream.is_open()) return;

    // Rotate before the write if the file has grown past the threshold.
    // Skip rotation when m_MaxBytes == 0 (rotation disabled).
    if (m_MaxBytes > 0 && m_CurrentSize + formatted.size() > m_MaxBytes)
    {
        Rotate();
    }

    m_Stream.write(formatted.data(), static_cast<std::streamsize>(formatted.size()));
    m_CurrentSize += formatted.size();

    // Always flush on Error/Fatal to preserve diagnostic data even if the
    // process terminates abnormally shortly after.
    if (msg.level >= LogLevel::Error)
    {
        m_Stream.flush();
    }
}

// =============================================================================
// ILogSink::Flush
// =============================================================================

void FileSink::Flush()
{
    std::lock_guard lock(m_Mutex);
    if (m_Stream.is_open())
        m_Stream.flush();
}

// =============================================================================
// Private helpers
// =============================================================================

bool FileSink::OpenFile(bool truncate)
{
    // Close any previously open stream before re-opening.
    if (m_Stream.is_open())
        m_Stream.close();

    auto mode = std::ios::out | std::ios::binary;
    if (!truncate)
        mode |= std::ios::app;

    m_Stream.open(m_Path, mode);
    m_CurrentSize = 0;

    // If we are appending, initialise m_CurrentSize to the current file size
    // so rotation triggers correctly even if we are picking up from a prior run.
    if (!truncate && m_Stream.is_open())
    {
        std::error_code ec;
        auto sz = std::filesystem::file_size(m_Path, ec);
        if (!ec) m_CurrentSize = sz;
    }

    return m_Stream.is_open();
}

void FileSink::Rotate()
{
    // Close the active file before renaming.
    m_Stream.flush();
    m_Stream.close();

    // Shift existing backups: .N → delete, .N-1 → .N, …, .1 → .2
    // We walk from the oldest to the newest to avoid clobbering files.
    for (u32 i = m_MaxFiles; i >= 1; --i)
    {
        auto older = m_Path;
        older += std::format(".{}", i);

        if (i == m_MaxFiles)
        {
            // The oldest allowed backup gets deleted.
            std::error_code ec;
            std::filesystem::remove(older, ec); // ignore errors
        }
        else
        {
            auto newer = m_Path;
            newer += std::format(".{}", i + 1);

            std::error_code ec;
            if (std::filesystem::exists(older, ec))
                std::filesystem::rename(older, newer, ec); // ignore errors
        }
    }

    // Rename the active file to .1
    {
        auto backup = m_Path;
        backup += ".1";
        std::error_code ec;
        std::filesystem::rename(m_Path, backup, ec);
    }

    // Open a fresh log file (always truncate after rotation).
    OpenFile(/*truncate=*/true);
}

} // namespace Hydra
