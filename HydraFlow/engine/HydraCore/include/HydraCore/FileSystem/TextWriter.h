#pragma once

// =============================================================================
// TextWriter.h
//
// Writes UTF-8 text to a file or any std::ostream. Provides both convenience
// methods (Write, WriteLine, WriteFormat) and basic stream operations (Flush,
// Close, Seek).
//
// By default a new file is created (or the existing file is truncated). Pass
// OpenMode::Append to the path constructor to instead append to an existing
// file without truncating it.
//
// Construction:
//   TextWriter writer(Path("log.txt"));                         // create/truncate
//   TextWriter writer(Path("log.txt"), OpenMode::Append);       // append mode
//   TextWriter writer(someOstream);                             // wrap a stream
//
// TextWriter is non-copyable but movable.
// =============================================================================

#include <HydraCore/Common/Platform.h>
#include <HydraCore/Common/Types.h>
#include <HydraCore/Common/NonCopyable.h>
#include <HydraCore/FileSystem/FileSystemTypes.h>
#include <HydraCore/FileSystem/Path.h>

#include <fstream>
#include <memory>

namespace Hydra {

class HYDRA_API TextWriter final : private NonCopyable
{
public:
    // -----------------------------------------------------------------------
    // Construction / destruction
    // -----------------------------------------------------------------------

    /// Creates or truncates `path` for text writing. Pass OpenMode::Append
    /// to append instead. Throws std::runtime_error on failure.
    explicit TextWriter(const Path& path, OpenMode mode = OpenMode::Write);

    /// Wraps an existing output stream (non-owning). The stream must outlive
    /// this TextWriter.
    explicit TextWriter(std::ostream& stream);

    ~TextWriter();
    TextWriter(TextWriter&&) noexcept;
    TextWriter& operator=(TextWriter&&) noexcept;

    // -----------------------------------------------------------------------
    // State
    // -----------------------------------------------------------------------

    [[nodiscard]] bool IsOpen() const noexcept;
    void Flush();
    void Close();

    // -----------------------------------------------------------------------
    // Writing
    // -----------------------------------------------------------------------

    /// Writes `text` as-is (no trailing newline added).
    void Write(StringView text);

    /// Writes `text` followed by a '\n' character.
    void WriteLine(StringView text = {});

    /// printf-style formatted output. Example:
    ///   writer.WriteFormat("x={} y={}\n", 3, 4.5f);
    /// Note: uses std::snprintf internally; unlimited-length strings are
    /// handled automatically via an internal growth loop.
    template<typename... Args>
    void WriteFormat(const char* fmt, Args&&... args)
    {
        // First try with a modest stack buffer; grow if it doesn't fit.
        char stackBuf[512];
        int needed = std::snprintf(stackBuf, sizeof(stackBuf), fmt, std::forward<Args>(args)...);
        if (needed < 0)
        {
            return; // encoding error — silently skip
        }
        if (static_cast<usize>(needed) < sizeof(stackBuf))
        {
            Write(StringView(stackBuf, static_cast<usize>(needed)));
        }
        else
        {
            // Didn't fit — allocate exactly the right size and retry.
            String heap(static_cast<usize>(needed + 1), '\0');
            std::snprintf(heap.data(), heap.size(), fmt, args...);
            heap.resize(static_cast<usize>(needed));
            Write(heap);
        }
    }

    // -----------------------------------------------------------------------
    // Positioning
    // -----------------------------------------------------------------------

    [[nodiscard]] i64 Tell() const;
    void Seek(i64 offset, SeekOrigin origin = SeekOrigin::Begin);

private:
    std::ostream*              m_ExternalStream = nullptr;
    std::unique_ptr<std::ofstream> m_OwnedStream;
    std::ostream*              m_Stream = nullptr;
};

} // namespace Hydra
