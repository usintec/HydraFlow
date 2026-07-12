// =============================================================================
// test_FileSystem_Module8.cpp
//
// Unit tests for Module 8 — File System.
//
// Tests use /tmp/ for all temporary files so they work in any environment
// without needing a project-relative writable directory. Each test that
// creates files cleans up after itself via SetUp/TearDown helpers or at the
// end of the test body, keeping the test runner idempotent.
//
// Test groups:
//   PathTests         — construction, decomposition, composition, queries
//   FileTests         — bulk read/write/delete/copy, File::Exists/Size
//   DirectoryTests    — Create/Delete/List/ListFiles/ListSubdirs/Recursive
//   BinaryReaderWriter— round-trip for every primitive type, byte order
//   TextReaderWriter  — ReadLine/ReadAll/Write/WriteLine/Append, BOM strip
//   VirtualFileSystem — NativeFileSystem via IVirtualFileSystem interface
// =============================================================================

#include <gtest/gtest.h>

#include <HydraCore/FileSystem/FileSystemTypes.h>
#include <HydraCore/FileSystem/Path.h>
#include <HydraCore/FileSystem/File.h>
#include <HydraCore/FileSystem/Directory.h>
#include <HydraCore/FileSystem/BinaryReader.h>
#include <HydraCore/FileSystem/BinaryWriter.h>
#include <HydraCore/FileSystem/TextReader.h>
#include <HydraCore/FileSystem/TextWriter.h>
#include <HydraCore/FileSystem/IVirtualFileSystem.h>
#include <HydraCore/FileSystem/NativeFileSystem.h>

#include <sstream>
#include <string>
#include <filesystem>

using namespace Hydra;

// =============================================================================
// Helper: produce a unique path in /tmp/ for each test to avoid collisions
// between parallel test runs.
// =============================================================================
static Path TmpPath(const char* name)
{
    return Path(std::string("/tmp/hydra_fs_test_") + name);
}

// =============================================================================
// PathTests — construction, decomposition, composition, normalisation
// =============================================================================

TEST(PathTests, DefaultConstructedIsEmpty)
{
    Path p;
    EXPECT_TRUE(p.IsEmpty());
}

TEST(PathTests, ConstructFromStringView)
{
    Path p("/usr/local/bin");
    EXPECT_FALSE(p.IsEmpty());
    EXPECT_EQ(p.ToString(), "/usr/local/bin");
}

TEST(PathTests, IsAbsoluteAndRelative)
{
    Path abs("/etc/hosts");
    Path rel("data/config.txt");

    EXPECT_TRUE(abs.IsAbsolute());
    EXPECT_FALSE(abs.IsRelative());

    EXPECT_TRUE(rel.IsRelative());
    EXPECT_FALSE(rel.IsAbsolute());
}

TEST(PathTests, GetFilenameGetStemGetExtension)
{
    Path p("/data/textures/rock.png");

    EXPECT_EQ(p.GetFilename(),  "rock.png");
    EXPECT_EQ(p.GetStem(),      "rock");
    EXPECT_EQ(p.GetExtension(), ".png");
}

TEST(PathTests, GetParent)
{
    Path p("/data/textures/rock.png");
    EXPECT_EQ(p.GetParent().ToString(), "/data/textures");
}

TEST(PathTests, AppendOperator)
{
    Path root("/data");
    Path full = root / "textures" / "rock.png";
    EXPECT_EQ(full.ToString(), "/data/textures/rock.png");
}

TEST(PathTests, AppendMethodAndOperatorEquivalent)
{
    Path base("/base");
    EXPECT_EQ(base.Append("sub").ToString(), base.operator/("sub").ToString());
}

TEST(PathTests, Normalize)
{
    // Redundant separators and dot-components are resolved lexically.
    Path p("/data/../data/./textures//rock.png");
    Path n = p.Normalize();
    EXPECT_EQ(n.ToString(), "/data/textures/rock.png");
}

TEST(PathTests, EqualityAndLessThan)
{
    Path a("/foo/bar");
    Path b("/foo/bar");
    Path c("/foo/baz");

    EXPECT_EQ(a, b);
    EXPECT_NE(a, c);
    EXPECT_LT(a, c);
}

TEST(PathTests, GetCurrentDirectoryReturnsNonEmpty)
{
    Path cwd = Path::GetCurrentDirectory();
    EXPECT_FALSE(cwd.IsEmpty());
    EXPECT_TRUE(cwd.IsAbsolute());
}

TEST(PathTests, GetTempDirectoryReturnsNonEmpty)
{
    Path tmp = Path::GetTempDirectory();
    EXPECT_FALSE(tmp.IsEmpty());
    EXPECT_TRUE(tmp.Exists());
}

TEST(PathTests, ExistsIsFalseForNonExistentPath)
{
    Path p("/this_does_not_exist_hydra_test_xyz");
    EXPECT_FALSE(p.Exists());
    EXPECT_FALSE(p.IsFile());
    EXPECT_FALSE(p.IsDirectory());
}

// =============================================================================
// FileTests — static File utilities
// =============================================================================

TEST(FileTests, WriteAllTextAndReadAllText)
{
    const Path path = TmpPath("write_read_text.txt");
    const std::string content = "Hello, HydraFlow!\nLine 2\nLine 3";

    File::WriteAllText(path, content);
    ASSERT_TRUE(File::Exists(path));

    const std::string read = File::ReadAllText(path);
    EXPECT_EQ(read, content);

    File::Delete(path);
    EXPECT_FALSE(File::Exists(path));
}

TEST(FileTests, WriteAllBytesAndReadAllBytes)
{
    const Path path = TmpPath("write_read_bytes.bin");
    const Vector<u8> data = { 0x01, 0x02, 0x03, 0xFE, 0xFF };

    File::WriteAllBytes(path, data);
    ASSERT_TRUE(File::Exists(path));

    const Vector<u8> read = File::ReadAllBytes(path);
    EXPECT_EQ(read, data);

    File::Delete(path);
}

TEST(FileTests, WriteAllLinesAndReadAllLines)
{
    const Path path = TmpPath("write_read_lines.txt");
    const Vector<String> lines = { "alpha", "beta", "gamma" };

    File::WriteAllLines(path, lines);
    const Vector<String> read = File::ReadAllLines(path);

    EXPECT_EQ(read, lines);

    File::Delete(path);
}

TEST(FileTests, AppendText)
{
    const Path path = TmpPath("append_text.txt");
    File::WriteAllText(path, "first\n");
    File::AppendText(path, "second\n");
    File::AppendText(path, "third\n");

    const String content = File::ReadAllText(path);
    EXPECT_EQ(content, "first\nsecond\nthird\n");

    File::Delete(path);
}

TEST(FileTests, GetSize)
{
    const Path path = TmpPath("size_test.txt");
    const String content = "1234567890"; // 10 bytes
    File::WriteAllText(path, content);

    EXPECT_EQ(File::GetSize(path), static_cast<u64>(content.size()));

    File::Delete(path);
}

TEST(FileTests, CopyFile)
{
    const Path src = TmpPath("copy_src.txt");
    const Path dst = TmpPath("copy_dst.txt");
    File::WriteAllText(src, "copy test content");

    File::Copy(src, dst);
    ASSERT_TRUE(File::Exists(dst));
    EXPECT_EQ(File::ReadAllText(dst), File::ReadAllText(src));

    File::Delete(src);
    File::Delete(dst);
}

TEST(FileTests, MoveFile)
{
    const Path src = TmpPath("move_src.txt");
    const Path dst = TmpPath("move_dst.txt");
    File::WriteAllText(src, "move test");

    File::Move(src, dst);
    EXPECT_FALSE(File::Exists(src));
    ASSERT_TRUE(File::Exists(dst));
    EXPECT_EQ(File::ReadAllText(dst), "move test");

    File::Delete(dst);
}

TEST(FileTests, DeleteReturnsFalseWhenNotExists)
{
    const Path missing = TmpPath("does_not_exist_xyz.txt");
    EXPECT_FALSE(File::Delete(missing));
}

TEST(FileTests, TouchCreatesFile)
{
    const Path path = TmpPath("touch_test.txt");
    File::Delete(path); // ensure clean state

    File::Touch(path);
    EXPECT_TRUE(File::Exists(path));
    EXPECT_EQ(File::GetSize(path), 0u);

    File::Delete(path);
}

TEST(FileTests, GetLastModifiedReturnsSomething)
{
    const Path path = TmpPath("mtime_test.txt");
    File::WriteAllText(path, "mtime");

    auto t = File::GetLastModified(path);
    EXPECT_TRUE(t.has_value());

    File::Delete(path);
}

// =============================================================================
// DirectoryTests — static Directory utilities
// =============================================================================

class DirectoryTests : public ::testing::Test
{
protected:
    // Each test gets its own unique root directory under /tmp/
    Path m_Root;

    void SetUp() override
    {
        m_Root = TmpPath("dir_test_root");
        // Clean up any leftover from a previous failed run.
        if (Directory::Exists(m_Root))
        {
            Directory::DeleteAll(m_Root);
        }
    }

    void TearDown() override
    {
        if (Directory::Exists(m_Root))
        {
            Directory::DeleteAll(m_Root);
        }
    }
};

TEST_F(DirectoryTests, CreateAndExistsAndDelete)
{
    EXPECT_FALSE(Directory::Exists(m_Root));
    const bool created = Directory::Create(m_Root);
    EXPECT_TRUE(created);
    EXPECT_TRUE(Directory::Exists(m_Root));

    const bool deleted = Directory::Delete(m_Root);
    EXPECT_TRUE(deleted);
    EXPECT_FALSE(Directory::Exists(m_Root));
}

TEST_F(DirectoryTests, CreateAllMakesIntermediateDirs)
{
    const Path deep = m_Root / "a" / "b" / "c";
    Directory::CreateAll(deep);
    EXPECT_TRUE(Directory::Exists(deep));
}

TEST_F(DirectoryTests, CreateReturnsFalseIfAlreadyExists)
{
    Directory::Create(m_Root);
    // A second Create() on an existing dir should return false (not throw).
    const bool created = Directory::Create(m_Root);
    EXPECT_FALSE(created);
}

TEST_F(DirectoryTests, ListFilesAndSubdirectories)
{
    Directory::Create(m_Root);

    // Create a subdirectory and two files inside the root.
    Directory::Create(m_Root / "subdir");
    File::WriteAllText(m_Root / "file1.txt", "a");
    File::WriteAllText(m_Root / "file2.txt", "b");

    const Vector<Path> files = Directory::ListFiles(m_Root);
    EXPECT_EQ(files.size(), 2u);

    const Vector<Path> subdirs = Directory::ListSubdirectories(m_Root);
    EXPECT_EQ(subdirs.size(), 1u);

    const Vector<Path> all = Directory::List(m_Root);
    EXPECT_EQ(all.size(), 3u); // 2 files + 1 subdir
}

TEST_F(DirectoryTests, ListFilesRecursive)
{
    Directory::CreateAll(m_Root / "sub");
    File::WriteAllText(m_Root / "top.txt",     "1");
    File::WriteAllText(m_Root / "sub" / "deep.txt", "2");

    const Vector<Path> all = Directory::ListFilesRecursive(m_Root);
    EXPECT_EQ(all.size(), 2u);
}

TEST_F(DirectoryTests, DeleteAllReturnsCount)
{
    Directory::CreateAll(m_Root / "sub");
    File::WriteAllText(m_Root / "a.txt",       "1");
    File::WriteAllText(m_Root / "sub" / "b.txt", "2");

    // remove_all counts both the dir entries and the root itself.
    const usize count = Directory::DeleteAll(m_Root);
    EXPECT_GE(count, 3u); // root + sub + 2 files = 4 items
    EXPECT_FALSE(Directory::Exists(m_Root));
}

TEST_F(DirectoryTests, GetCurrentDirectory)
{
    const Path cwd = Directory::GetCurrentDirectory();
    EXPECT_FALSE(cwd.IsEmpty());
    EXPECT_TRUE(cwd.IsAbsolute());
}

// =============================================================================
// BinaryReaderWriter — round-trip every primitive type, both byte orders
// =============================================================================

class BinaryRoundTripTest : public ::testing::Test
{
protected:
    Path m_Path;

    void SetUp() override
    {
        m_Path = TmpPath("binary_roundtrip.bin");
    }

    void TearDown() override
    {
        File::Delete(m_Path);
    }
};

TEST_F(BinaryRoundTripTest, PrimitivesLittleEndian)
{
    {
        BinaryWriter w(m_Path, ByteOrder::LittleEndian);
        w.WriteU8 (0xAB);
        w.WriteU16(0xBEEF);
        w.WriteU32(0xDEADBEEF);
        w.WriteU64(0x0102030405060708ULL);
        w.WriteI8 (-1);
        w.WriteI16(-1000);
        w.WriteI32(-1000000);
        w.WriteI64(-1000000000LL);
        w.WriteF32(3.14f);
        w.WriteF64(2.718281828);
        w.WriteBool(true);
        w.WriteBool(false);
    }

    {
        BinaryReader r(m_Path, ByteOrder::LittleEndian);
        EXPECT_EQ(r.ReadU8(),  static_cast<u8>(0xAB));
        EXPECT_EQ(r.ReadU16(), static_cast<u16>(0xBEEF));
        EXPECT_EQ(r.ReadU32(), static_cast<u32>(0xDEADBEEF));
        EXPECT_EQ(r.ReadU64(), static_cast<u64>(0x0102030405060708ULL));
        EXPECT_EQ(r.ReadI8(),  static_cast<i8>(-1));
        EXPECT_EQ(r.ReadI16(), static_cast<i16>(-1000));
        EXPECT_EQ(r.ReadI32(), static_cast<i32>(-1000000));
        EXPECT_EQ(r.ReadI64(), static_cast<i64>(-1000000000LL));
        EXPECT_FLOAT_EQ (r.ReadF32(), 3.14f);
        EXPECT_DOUBLE_EQ(r.ReadF64(), 2.718281828);
        EXPECT_TRUE (r.ReadBool());
        EXPECT_FALSE(r.ReadBool());
    }
}

TEST_F(BinaryRoundTripTest, PrimitivesBigEndian)
{
    // Same values written and read in big-endian order — the round-trip
    // result should be identical to the little-endian test above, because
    // both writer and reader agree on the byte order.
    {
        BinaryWriter w(m_Path, ByteOrder::BigEndian);
        w.WriteU16(0x1234);
        w.WriteU32(0xCAFEBABE);
    }

    {
        BinaryReader r(m_Path, ByteOrder::BigEndian);
        EXPECT_EQ(r.ReadU16(), static_cast<u16>(0x1234));
        EXPECT_EQ(r.ReadU32(), static_cast<u32>(0xCAFEBABE));
    }
}

TEST_F(BinaryRoundTripTest, ByteSwapConsistency)
{
    // Write as little-endian, read back as big-endian — the raw bytes
    // will be in opposite order, so the values should differ (unless
    // the test system is big-endian, but the Replit sandbox is x64 LE).
    const u32 written = 0x12345678;
    {
        BinaryWriter w(m_Path, ByteOrder::LittleEndian);
        w.WriteU32(written);
    }
    {
        BinaryReader r(m_Path, ByteOrder::BigEndian);
        const u32 read = r.ReadU32();
        // On a little-endian host: written LE → bytes {78 56 34 12},
        // read as BE → 0x78563412, which differs from the original.
        EXPECT_NE(read, written);
        EXPECT_EQ(read, static_cast<u32>(0x78563412));
    }
}

TEST_F(BinaryRoundTripTest, StringRoundTrip)
{
    const String msg = "Hello, 世界!"; // UTF-8 multi-byte characters
    {
        BinaryWriter w(m_Path);
        w.WriteString(msg);
    }
    {
        BinaryReader r(m_Path);
        EXPECT_EQ(r.ReadString(), msg);
    }
}

TEST_F(BinaryRoundTripTest, FixedStringRoundTrip)
{
    {
        BinaryWriter w(m_Path);
        w.WriteFixedString("MAGIC", 8); // 5 bytes of text + 3 bytes of null padding
    }
    {
        BinaryReader r(m_Path);
        const String s = r.ReadFixedString(5); // only the 5 text bytes
        EXPECT_EQ(s, "MAGIC");
    }
}

TEST_F(BinaryRoundTripTest, BytesRoundTrip)
{
    const Vector<u8> data = { 10, 20, 30, 40, 50 };
    {
        BinaryWriter w(m_Path);
        w.WriteBytes(data);
    }
    {
        BinaryReader r(m_Path);
        const Vector<u8> read = r.ReadBytes(data.size());
        EXPECT_EQ(read, data);
    }
}

TEST_F(BinaryRoundTripTest, SeekAndTell)
{
    {
        BinaryWriter w(m_Path);
        w.WriteU32(111);
        w.WriteU32(222);
        w.WriteU32(333);
    }
    {
        BinaryReader r(m_Path);
        // Skip the first u32 (4 bytes) and read only the second.
        r.Seek(4, SeekOrigin::Begin);
        EXPECT_EQ(r.Tell(), 4);
        EXPECT_EQ(r.ReadU32(), static_cast<u32>(222));
    }
}

// Test using a stringstream so no file I/O is needed — good for unit isolation.
TEST(BinaryStreamConstructorTest, WorksWithExternalStringStream)
{
    std::ostringstream oss;
    {
        BinaryWriter w(oss);
        w.WriteU32(0xDEAD);
        w.WriteU32(0xBEEF);
    }

    std::istringstream iss(oss.str());
    BinaryReader r(iss);
    EXPECT_EQ(r.ReadU32(), static_cast<u32>(0xDEAD));
    EXPECT_EQ(r.ReadU32(), static_cast<u32>(0xBEEF));
}

// =============================================================================
// TextReaderWriter — ReadLine/ReadAll, UTF-8, BOM, CRLF, Append
// =============================================================================

class TextRoundTripTest : public ::testing::Test
{
protected:
    Path m_Path;

    void SetUp() override { m_Path = TmpPath("text_roundtrip.txt"); }
    void TearDown() override { File::Delete(m_Path); }
};

TEST_F(TextRoundTripTest, WriteAndReadAllLines)
{
    {
        TextWriter w(m_Path);
        w.WriteLine("Line 1");
        w.WriteLine("Line 2");
        w.WriteLine("Line 3");
    }
    {
        TextReader r(m_Path);
        const Vector<String> lines = r.ReadAllLines();
        ASSERT_EQ(lines.size(), 3u);
        EXPECT_EQ(lines[0], "Line 1");
        EXPECT_EQ(lines[1], "Line 2");
        EXPECT_EQ(lines[2], "Line 3");
    }
}

TEST_F(TextRoundTripTest, ReadAll)
{
    const String content = "hello world\nfoo bar\n";
    {
        TextWriter w(m_Path);
        w.Write(content);
    }
    {
        TextReader r(m_Path);
        EXPECT_EQ(r.ReadAll(), content);
    }
}

TEST_F(TextRoundTripTest, AppendMode)
{
    {
        TextWriter w(m_Path);
        w.WriteLine("first");
    }
    {
        TextWriter w(m_Path, OpenMode::Append);
        w.WriteLine("second");
    }
    {
        TextReader r(m_Path);
        const Vector<String> lines = r.ReadAllLines();
        ASSERT_EQ(lines.size(), 2u);
        EXPECT_EQ(lines[0], "first");
        EXPECT_EQ(lines[1], "second");
    }
}

TEST_F(TextRoundTripTest, ReadLineSingleLine)
{
    {
        TextWriter w(m_Path);
        w.Write("no newline at end");
    }
    {
        TextReader r(m_Path);
        auto line = r.ReadLine();
        ASSERT_TRUE(line.has_value());
        EXPECT_EQ(*line, "no newline at end");

        // Second ReadLine() should return nullopt (EOF).
        auto eof = r.ReadLine();
        EXPECT_FALSE(eof.has_value());
    }
}

TEST_F(TextRoundTripTest, ReadLineStripsCarriageReturn)
{
    // Write a file with Windows-style CRLF endings manually.
    {
        BinaryWriter w(m_Path);
        const std::string crlf = "line1\r\nline2\r\n";
        w.WriteBytes(reinterpret_cast<const u8*>(crlf.data()), crlf.size());
    }
    {
        TextReader r(m_Path);
        auto l1 = r.ReadLine(); ASSERT_TRUE(l1.has_value()); EXPECT_EQ(*l1, "line1");
        auto l2 = r.ReadLine(); ASSERT_TRUE(l2.has_value()); EXPECT_EQ(*l2, "line2");
    }
}

TEST_F(TextRoundTripTest, BomIsStripped)
{
    // Write the UTF-8 BOM followed by some text.
    {
        BinaryWriter w(m_Path);
        const u8 bom[] = { 0xEF, 0xBB, 0xBF };
        w.WriteBytes(bom, 3);
        const char text[] = "BOM test";
        w.WriteBytes(reinterpret_cast<const u8*>(text), sizeof(text) - 1);
    }
    {
        TextReader r(m_Path);
        const String content = r.ReadAll();
        // The BOM should NOT appear in the returned string.
        EXPECT_EQ(content, "BOM test");
        EXPECT_EQ(content[0], 'B'); // not 0xEF
    }
}

TEST_F(TextRoundTripTest, Utf8MultiByteCharacters)
{
    // "Hydra" in Japanese kanji (and a snowflake) — all valid UTF-8.
    const String japanese = "ハイドラ ❄";
    {
        TextWriter w(m_Path);
        w.WriteLine(japanese);
    }
    {
        TextReader r(m_Path);
        auto line = r.ReadLine();
        ASSERT_TRUE(line.has_value());
        EXPECT_EQ(*line, japanese);
    }
}

TEST_F(TextRoundTripTest, SeekAndTell)
{
    {
        TextWriter w(m_Path);
        w.Write("ABCDEFGH");
    }
    {
        TextReader r(m_Path);
        r.Seek(4, SeekOrigin::Begin);
        EXPECT_EQ(r.Tell(), 4);
        const String rest = r.ReadAll();
        EXPECT_EQ(rest, "EFGH");
    }
}

// Test with std::stringstream to avoid file I/O.
TEST(TextStreamConstructorTest, WorksWithExternalStringStream)
{
    std::istringstream iss("alpha\nbeta\ngamma\n");
    TextReader r(iss);
    auto l1 = r.ReadLine(); ASSERT_TRUE(l1.has_value()); EXPECT_EQ(*l1, "alpha");
    auto l2 = r.ReadLine(); ASSERT_TRUE(l2.has_value()); EXPECT_EQ(*l2, "beta");
    auto l3 = r.ReadLine(); ASSERT_TRUE(l3.has_value()); EXPECT_EQ(*l3, "gamma");
    EXPECT_FALSE(r.ReadLine().has_value());
}

TEST(TextWriterStreamTest, WorksWithExternalOStringStream)
{
    std::ostringstream oss;
    {
        TextWriter w(oss);
        w.WriteLine("hello");
        w.Write("world");
    }
    EXPECT_EQ(oss.str(), "hello\nworld");
}

// =============================================================================
// VirtualFileSystem — NativeFileSystem through the IVirtualFileSystem interface
// =============================================================================

class VirtualFSTest : public ::testing::Test
{
protected:
    Path         m_Root;
    NativeFileSystem m_VFS;

    void SetUp() override
    {
        m_Root = TmpPath("vfs_test");
        if (Directory::Exists(m_Root)) Directory::DeleteAll(m_Root);
        Directory::Create(m_Root);
    }

    void TearDown() override
    {
        if (Directory::Exists(m_Root)) Directory::DeleteAll(m_Root);
    }
};

TEST_F(VirtualFSTest, ExistsReturnsFalseForMissingFile)
{
    EXPECT_FALSE(m_VFS.Exists(m_Root / "missing.txt"));
}

TEST_F(VirtualFSTest, OpenWriteThenOpenRead)
{
    const Path file = m_Root / "vfs_data.bin";

    {
        auto writer = m_VFS.OpenWrite(file);
        ASSERT_NE(writer, nullptr);
        writer->WriteU32(0xABCD1234);
    }

    {
        auto reader = m_VFS.OpenRead(file);
        ASSERT_NE(reader, nullptr);
        EXPECT_EQ(reader->ReadU32(), static_cast<u32>(0xABCD1234));
    }
}

TEST_F(VirtualFSTest, OpenTextWriteThenOpenTextRead)
{
    const Path file = m_Root / "vfs_text.txt";

    {
        auto writer = m_VFS.OpenTextWrite(file);
        ASSERT_NE(writer, nullptr);
        writer->WriteLine("VFS text line");
    }

    {
        auto reader = m_VFS.OpenTextRead(file);
        ASSERT_NE(reader, nullptr);
        auto line = reader->ReadLine();
        ASSERT_TRUE(line.has_value());
        EXPECT_EQ(*line, "VFS text line");
    }
}

TEST_F(VirtualFSTest, ListFilesReturnsCorrectCount)
{
    File::WriteAllText(m_Root / "a.txt", "a");
    File::WriteAllText(m_Root / "b.txt", "b");

    const Vector<Path> files = m_VFS.ListFiles(m_Root);
    EXPECT_EQ(files.size(), 2u);
}

TEST_F(VirtualFSTest, CreateDirectoryAndDelete)
{
    const Path sub = m_Root / "newdir";
    EXPECT_FALSE(m_VFS.IsDirectory(sub));

    const bool created = m_VFS.CreateDirectory(sub);
    EXPECT_TRUE(created);
    EXPECT_TRUE(m_VFS.IsDirectory(sub));

    m_VFS.Delete(sub);
    EXPECT_FALSE(m_VFS.IsDirectory(sub));
}

TEST_F(VirtualFSTest, OpenReadReturnsNullptrForMissingFile)
{
    auto reader = m_VFS.OpenRead(m_Root / "not_there.bin");
    EXPECT_EQ(reader, nullptr);
}

TEST_F(VirtualFSTest, GetFileSizeMatchesFileSize)
{
    const Path file = m_Root / "sized.txt";
    const String content = "12345";
    File::WriteAllText(file, content);

    EXPECT_EQ(m_VFS.GetFileSize(file), static_cast<u64>(content.size()));
}

// =============================================================================
// FileSystem global accessor
// =============================================================================

TEST(FileSystemGlobalTest, GetReturnsNonNullReference)
{
    // Just calling Get() should not crash and return a valid reference.
    IVirtualFileSystem& vfs = FileSystem::Get();
    // The default is NativeFileSystem, so /tmp must exist.
    EXPECT_TRUE(vfs.IsDirectory(Path("/tmp")));
}

TEST(FileSystemGlobalTest, SetAndRestore)
{
    // Install a custom (second) NativeFileSystem instance.
    FileSystem::Set(MakeUnique<NativeFileSystem>());
    EXPECT_TRUE(FileSystem::Get().IsDirectory(Path("/tmp")));

    // Passing nullptr should restore the default NativeFileSystem.
    FileSystem::Set(nullptr);
    EXPECT_TRUE(FileSystem::Get().IsDirectory(Path("/tmp")));
}
