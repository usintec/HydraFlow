#pragma once

// =============================================================================
// TextReader.h
//
// Reads UTF-8 text from a file or any std::istream. Provides both whole-file
// bulk operations (ReadAll, ReadAllLines) and incremental per-line / per-char
// reading (ReadLine, ReadChar, Peek).
//
// BOM (Byte-Order Mark) handling: if the file starts with the UTF-8 BOM
// (0xEF 0xBB 0xBF), it is silently consumed and not included in any returned
// string. Files without a BOM are read as-is (assumed UTF-8).
//
// Newline handling: ReadLine() strips both '\n' (Unix) and '\r\n' (Windows)
// line endings, so callers receive clean lines regardless of the source
// platform.
//
// Construction:
//   TextReader reader(Path("notes.txt"));
//   TextReader reader(someIstream);          // wraps without taking ownership
//
// TextReader is non-copyable but movable.
// =============================================================================

#include <HydraCore/Common/Platform.h>
#include <HydraCore/Common/Types.h>
#include <HydraCore/Common/NonCopyable.h>
#include <HydraCore/FileSystem/FileSystemTypes.h>
#include <HydraCore/FileSystem/Path.h>

#include <fstream>
#include <memory>
#include <optional>

namespace Hydra {

class HYDRA_API TextReader final : private NonCopyable
{
public:
    // -----------------------------------------------------------------------
    // Construction / destruction
    // -----------------------------------------------------------------------

    /// Opens `path` for reading as UTF-8 text. Throws std::runtime_error if
    /// the file doesn't exist or can't be opened.
    explicit TextReader(const Path& path);

    /// Wraps an existing input stream (non-owning). The stream must outlive
    /// this TextReader. Useful for testing with std::istringstream.
    explicit TextReader(std::istream& stream);

    ~TextReader();
    TextReader(TextReader&&) noexcept;
    TextReader& operator=(TextReader&&) noexcept;

    // -----------------------------------------------------------------------
    // State
    // -----------------------------------------------------------------------

    [[nodiscard]] bool IsOpen() const noexcept;
    [[nodiscard]] bool IsEof()  const noexcept;
    void Close();

    // -----------------------------------------------------------------------
    // Line-by-line reading
    // -----------------------------------------------------------------------

    /// Reads the next line (up to '\n' or EOF), stripping any trailing '\r'.
    /// Returns nullopt when the end of the stream has been reached.
    [[nodiscard]] Optional<String> ReadLine();

    /// Reads every remaining line and returns them in order. The reader is
    /// positioned at EOF after this call.
    [[nodiscard]] Vector<String> ReadAllLines();

    // -----------------------------------------------------------------------
    // Character reading
    // -----------------------------------------------------------------------

    /// Reads and returns the next Unicode code-unit (byte). Returns -1 at EOF.
    /// Note: for multi-byte UTF-8 sequences, each byte is returned individually.
    [[nodiscard]] int ReadChar();

    /// Returns the next byte without consuming it, or -1 at EOF.
    [[nodiscard]] int Peek() const;

    // -----------------------------------------------------------------------
    // Bulk reading
    // -----------------------------------------------------------------------

    /// Reads the entire remaining content of the stream as a UTF-8 String.
    [[nodiscard]] String ReadAll();

    // -----------------------------------------------------------------------
    // Positioning
    // -----------------------------------------------------------------------

    [[nodiscard]] i64 Tell() const;
    void Seek(i64 offset, SeekOrigin origin = SeekOrigin::Begin);

private:
    void SkipBom();

    std::istream*             m_ExternalStream = nullptr;
    std::unique_ptr<std::ifstream> m_OwnedStream;
    std::istream*             m_Stream = nullptr;
};

} // namespace Hydra
