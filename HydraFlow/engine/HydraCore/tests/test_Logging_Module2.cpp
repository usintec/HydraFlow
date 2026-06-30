#include <gtest/gtest.h>

#include <HydraCore/Logging/LogLevel.h>
#include <HydraCore/Logging/LogMessage.h>
#include <HydraCore/Logging/Formatter.h>
#include <HydraCore/Logging/ILogSink.h>
#include <HydraCore/Logging/NullSink.h>
#include <HydraCore/Logging/ConsoleSink.h>
#include <HydraCore/Logging/FileSink.h>
#include <HydraCore/Logging/Logger.h>
#include <HydraCore/Logging/LoggerFactory.h>
#include <HydraCore/Logging/LoggingMacros.h>

#include <cstring>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <thread>
#include <vector>
#include <atomic>

using namespace Hydra;

// =============================================================================
// Helpers
// =============================================================================

/// A sink that captures every formatted message into a string vector.
/// Used by tests that need to inspect what was logged.
class CaptureSink final : public ILogSink
{
public:
    void Sink(const LogMessage& msg) override
    {
        std::lock_guard lock(m_Mutex);
        m_Lines.push_back(FormatMessage(msg));
        m_Messages.push_back(msg);
    }

    void Flush() override {}

    [[nodiscard]] std::vector<std::string> Lines() const
    {
        std::lock_guard lock(m_Mutex);
        return m_Lines;
    }

    [[nodiscard]] std::vector<LogMessage> Messages() const
    {
        std::lock_guard lock(m_Mutex);
        return m_Messages;
    }

    [[nodiscard]] usize Count() const
    {
        std::lock_guard lock(m_Mutex);
        return m_Lines.size();
    }

    void Clear()
    {
        std::lock_guard lock(m_Mutex);
        m_Lines.clear();
        m_Messages.clear();
    }

private:
    mutable std::mutex      m_Mutex;
    std::vector<std::string> m_Lines;
    std::vector<LogMessage>  m_Messages;
};

// =============================================================================
// LogLevel Tests
// =============================================================================

TEST(LogLevelTest, ToStringRoundTrip)
{
    EXPECT_STREQ(LogLevelToString(LogLevel::Trace),    "TRACE");
    EXPECT_STREQ(LogLevelToString(LogLevel::Debug),    "DEBUG");
    EXPECT_STREQ(LogLevelToString(LogLevel::Info),     "INFO");
    EXPECT_STREQ(LogLevelToString(LogLevel::Warn),     "WARN");
    EXPECT_STREQ(LogLevelToString(LogLevel::Error),    "ERROR");
    EXPECT_STREQ(LogLevelToString(LogLevel::Fatal),    "FATAL");
}

TEST(LogLevelTest, PaddedStringIsAlways5Chars)
{
    // All padded strings must be exactly 5 characters for aligned columns.
    const auto levels = { LogLevel::Trace, LogLevel::Debug, LogLevel::Info,
                          LogLevel::Warn,  LogLevel::Error, LogLevel::Fatal };
    for (auto lvl : levels)
    {
        EXPECT_EQ(std::strlen(LogLevelToStringPadded(lvl)), 5u)
            << "Failed for level " << LogLevelToString(lvl);
    }
}

TEST(LogLevelTest, FromStringCaseInsensitive)
{
    EXPECT_EQ(LogLevelFromString("trace"),   LogLevel::Trace);
    EXPECT_EQ(LogLevelFromString("DEBUG"),   LogLevel::Debug);
    EXPECT_EQ(LogLevelFromString("Info"),    LogLevel::Info);
    EXPECT_EQ(LogLevelFromString("warning"), LogLevel::Warn);
    EXPECT_EQ(LogLevelFromString("ERROR"),   LogLevel::Error);
    EXPECT_EQ(LogLevelFromString("critical"),LogLevel::Fatal);
    EXPECT_EQ(LogLevelFromString("FATAL"),   LogLevel::Fatal);
    EXPECT_EQ(LogLevelFromString("unknown"), LogLevel::Info);  // default fallback
}

TEST(LogLevelTest, AnsiColorForLevelReturnsNonNull)
{
    EXPECT_NE(AnsiColor::ForLevel(LogLevel::Trace), nullptr);
    EXPECT_NE(AnsiColor::ForLevel(LogLevel::Info),  nullptr);
    EXPECT_NE(AnsiColor::ForLevel(LogLevel::Fatal), nullptr);
}

// =============================================================================
// LogMessage Tests
// =============================================================================

TEST(LogMessageTest, MakePopulatesAllFields)
{
    SourceLocation loc{"file.cpp", 42, "myFunc"};
    auto msg = LogMessage::Make("TestLogger", LogLevel::Warn, "hello world", loc);

    EXPECT_EQ(msg.loggerName, "TestLogger");
    EXPECT_EQ(msg.level,      LogLevel::Warn);
    EXPECT_EQ(msg.message,    "hello world");
    EXPECT_STREQ(msg.source.file, "file.cpp");
    EXPECT_EQ(msg.source.line, 42);
    EXPECT_STREQ(msg.source.function, "myFunc");
    // Timestamp must be in the past (or right now), not the epoch.
    EXPECT_GT(msg.timestamp.time_since_epoch().count(), 0);
}

TEST(LogMessageTest, SourceLocationIsValid)
{
    SourceLocation valid{"a.cpp", 1, "f"};
    EXPECT_TRUE(valid.IsValid());

    SourceLocation empty{};
    EXPECT_FALSE(empty.IsValid());
}

// =============================================================================
// PatternFormatter Tests
// =============================================================================

class FormatterTest : public ::testing::Test
{
protected:
    LogMessage MakeMsg(LogLevel lvl = LogLevel::Info,
                       const std::string& text = "hello")
    {
        return LogMessage::Make("TestLogger", lvl, text,
                                SourceLocation{"src/main.cpp", 10, "main"});
    }
};

TEST_F(FormatterTest, DefaultPatternContainsMessage)
{
    PatternFormatter fmt;
    auto result = fmt.Format(MakeMsg());
    EXPECT_NE(result.find("hello"), std::string::npos);
}

TEST_F(FormatterTest, LevelTokenReplaced)
{
    PatternFormatter fmt("{level}");
    auto result = fmt.Format(MakeMsg(LogLevel::Warn));
    EXPECT_NE(result.find("WARN"), std::string::npos);
}

TEST_F(FormatterTest, NameTokenReplaced)
{
    PatternFormatter fmt("{name}");
    EXPECT_EQ(fmt.Format(MakeMsg()), "TestLogger");
}

TEST_F(FormatterTest, FileTokenIsBasenameOnly)
{
    PatternFormatter fmt("{file}");
    auto result = fmt.Format(MakeMsg());
    // Should be "main.cpp", not the full "src/main.cpp"
    EXPECT_EQ(result, "main.cpp");
}

TEST_F(FormatterTest, FilepathTokenIsFullPath)
{
    PatternFormatter fmt("{filepath}");
    EXPECT_EQ(fmt.Format(MakeMsg()), "src/main.cpp");
}

TEST_F(FormatterTest, LineTokenIsNumber)
{
    PatternFormatter fmt("{line}");
    EXPECT_EQ(fmt.Format(MakeMsg()), "10");
}

TEST_F(FormatterTest, MessageTokenReplaced)
{
    PatternFormatter fmt("{message}");
    EXPECT_EQ(fmt.Format(MakeMsg(LogLevel::Info, "payload")), "payload");
}

TEST_F(FormatterTest, UnknownTokenPassedThrough)
{
    PatternFormatter fmt("{bogus}");
    auto result = fmt.Format(MakeMsg());
    EXPECT_EQ(result, "{bogus}");
}

TEST_F(FormatterTest, ColoredLevelContainsAnsiCodes)
{
    PatternFormatter fmt("{LEVEL}");
    fmt.SetColorEnabled(true);
    auto result = fmt.Format(MakeMsg(LogLevel::Error));
    // Should start with \033[ (ESC = 0x1B = 27)
    EXPECT_EQ(result[0], '\033');
}

TEST_F(FormatterTest, JsonFormatterProducesValidJson)
{
    JsonFormatter fmt;
    auto result = fmt.Format(MakeMsg(LogLevel::Debug, "json test"));
    EXPECT_NE(result.find("{"), std::string::npos);
    EXPECT_NE(result.find("}"), std::string::npos);
    EXPECT_NE(result.find("\"level\""), std::string::npos);
    EXPECT_NE(result.find("json test"), std::string::npos);
}

// =============================================================================
// Logger Instance Tests
// =============================================================================

class LoggerInstanceTest : public ::testing::Test
{
protected:
    std::shared_ptr<Logger>      logger;
    std::shared_ptr<CaptureSink> capture;

    void SetUp() override
    {
        logger  = std::make_shared<Logger>("UnitTest", LogLevel::Trace);
        capture = std::make_shared<CaptureSink>();
        // Use a simple pattern so tests can check substrings reliably
        capture->SetFormatter(std::make_shared<PatternFormatter>("{level}|{message}"));
        logger->AddSink(capture);
    }
};

TEST_F(LoggerInstanceTest, GetNameReturnsConstructedName)
{
    EXPECT_EQ(logger->GetName(), "UnitTest");
}

TEST_F(LoggerInstanceTest, SetAndGetLevel)
{
    logger->SetLevel(LogLevel::Warn);
    EXPECT_EQ(logger->GetLevel(), LogLevel::Warn);
}

TEST_F(LoggerInstanceTest, ShouldLogRespectsLevel)
{
    logger->SetLevel(LogLevel::Warn);
    EXPECT_FALSE(logger->ShouldLog(LogLevel::Debug));
    EXPECT_FALSE(logger->ShouldLog(LogLevel::Info));
    EXPECT_TRUE(logger->ShouldLog(LogLevel::Warn));
    EXPECT_TRUE(logger->ShouldLog(LogLevel::Error));
    EXPECT_TRUE(logger->ShouldLog(LogLevel::Fatal));
}

TEST_F(LoggerInstanceTest, ShouldLogReturnsFalseWhenOff)
{
    logger->SetLevel(LogLevel::Off);
    EXPECT_FALSE(logger->ShouldLog(LogLevel::Fatal));
}

TEST_F(LoggerInstanceTest, InfoMessageReachesSink)
{
    logger->Info("hello {}", 42);
    ASSERT_EQ(capture->Count(), 1u);
    EXPECT_NE(capture->Lines()[0].find("hello 42"), std::string::npos);
}

TEST_F(LoggerInstanceTest, MessageBelowLevelIsDropped)
{
    logger->SetLevel(LogLevel::Error);
    logger->Info("should be dropped");
    EXPECT_EQ(capture->Count(), 0u);
}

TEST_F(LoggerInstanceTest, MessageAboveSinkLevelIsDropped)
{
    // Set sink to only accept Fatal; logger accepts everything
    capture->SetLevel(LogLevel::Fatal);
    logger->Info("sink should drop this");
    EXPECT_EQ(capture->Count(), 0u);
    logger->Fatal("sink should pass this");
    EXPECT_EQ(capture->Count(), 1u);
}

TEST_F(LoggerInstanceTest, AllConvenienceLevelMethodsWork)
{
    logger->Trace("t");
    logger->Debug("d");
    logger->Info("i");
    logger->Warn("w");
    logger->Error("e");
    logger->Fatal("f");
    EXPECT_EQ(capture->Count(), 6u);
}

TEST_F(LoggerInstanceTest, RemoveSinkStopsMessages)
{
    logger->RemoveSink(capture);
    logger->Info("should not appear");
    EXPECT_EQ(capture->Count(), 0u);
}

TEST_F(LoggerInstanceTest, ClearSinksStopsMessages)
{
    logger->ClearSinks();
    logger->Info("should not appear");
    EXPECT_EQ(capture->Count(), 0u);
}

TEST_F(LoggerInstanceTest, DispatchPopulatesLoggerName)
{
    logger->Info("name test");
    ASSERT_EQ(capture->Count(), 1u);
    EXPECT_EQ(capture->Messages()[0].loggerName, "UnitTest");
}

TEST_F(LoggerInstanceTest, DispatchPopulatesTimestamp)
{
    logger->Info("ts test");
    ASSERT_EQ(capture->Count(), 1u);
    auto& ts = capture->Messages()[0].timestamp;
    EXPECT_GT(ts.time_since_epoch().count(), 0);
}

TEST_F(LoggerInstanceTest, ThreadSafetyNoCrash)
{
    // Write from 8 threads simultaneously — should not crash or corrupt state.
    constexpr int kThreads = 8;
    constexpr int kMessages = 100;

    std::vector<std::thread> threads;
    threads.reserve(kThreads);
    for (int t = 0; t < kThreads; ++t)
    {
        threads.emplace_back([&, t] {
            for (int i = 0; i < kMessages; ++i)
                logger->Info("thread {} message {}", t, i);
        });
    }
    for (auto& th : threads) th.join();

    EXPECT_EQ(capture->Count(), static_cast<usize>(kThreads * kMessages));
}

// =============================================================================
// NullSink Tests
// =============================================================================

TEST(NullSinkTest, DiscardsAllMessages)
{
    auto logger = std::make_shared<Logger>("Null", LogLevel::Trace);
    logger->AddSink(std::make_shared<NullSink>());
    // Should not crash and should produce no output
    EXPECT_NO_THROW({
        logger->Info("discarded");
        logger->Fatal("also discarded");
    });
}

// =============================================================================
// FileSink Tests
// =============================================================================

class FileSinkTest : public ::testing::Test
{
protected:
    std::filesystem::path m_Dir;
    std::filesystem::path m_File;

    void SetUp() override
    {
        m_Dir  = std::filesystem::temp_directory_path() / "hydra_filesink_test";
        m_File = m_Dir / "test.log";
        std::filesystem::create_directories(m_Dir);
    }

    void TearDown() override
    {
        std::error_code ec;
        std::filesystem::remove_all(m_Dir, ec);
    }

    std::string ReadFile(const std::filesystem::path& path)
    {
        std::ifstream f(path);
        return { std::istreambuf_iterator<char>(f),
                 std::istreambuf_iterator<char>() };
    }
};

TEST_F(FileSinkTest, CreatesFileOnOpen)
{
    {
        FileSink sink(m_File);
        EXPECT_TRUE(sink.IsOpen());
        EXPECT_TRUE(std::filesystem::exists(m_File));
    }
}

TEST_F(FileSinkTest, WritesMessageToFile)
{
    auto logger = std::make_shared<Logger>("File", LogLevel::Trace);
    auto sink   = std::make_shared<FileSink>(m_File);
    sink->SetFormatter(std::make_shared<PatternFormatter>("{message}"));
    logger->AddSink(sink);

    logger->Info("file content test");
    logger->Flush();

    auto content = ReadFile(m_File);
    EXPECT_NE(content.find("file content test"), std::string::npos);
}

TEST_F(FileSinkTest, RotatesWhenSizeExceedsThreshold)
{
    auto sink = std::make_shared<FileSink>(m_File);
    sink->SetMaxBytes(50);   // very small threshold for testing
    sink->SetMaxFiles(3);

    auto logger = std::make_shared<Logger>("Rotate", LogLevel::Trace);
    logger->AddSink(sink);

    // Write enough to trigger multiple rotations
    for (int i = 0; i < 20; ++i)
        logger->Info("rotation test line {:03d}", i);
    logger->Flush();

    // At least one backup file should exist after rotation
    auto backup = m_File;
    backup += ".1";
    EXPECT_TRUE(std::filesystem::exists(backup))
        << "Expected " << backup << " to exist after rotation";
}

TEST_F(FileSinkTest, TruncatesFileOnReopen)
{
    // First open: write some content
    {
        FileSink sink(m_File, /*truncate=*/true);
        auto logger = std::make_shared<Logger>("T", LogLevel::Trace);
        sink.SetFormatter(std::make_shared<PatternFormatter>("{message}"));
        logger->AddSink(std::make_shared<FileSink>(m_File, true));
        logger->Info("first run");
        logger->Flush();
    }

    // Second open: truncate = true should clear the file
    {
        FileSink sink(m_File, /*truncate=*/true);
        EXPECT_EQ(sink.GetCurrentSize(), 0u);
    }
}

// =============================================================================
// LoggerFactory Tests
// =============================================================================

class LoggerFactoryTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        // Always start from a clean state
        LoggerFactory::Shutdown();
    }

    void TearDown() override
    {
        LoggerFactory::Shutdown();
    }
};

TEST_F(LoggerFactoryTest, NotInitializedBeforeInit)
{
    EXPECT_FALSE(LoggerFactory::IsInitialized());
}

TEST_F(LoggerFactoryTest, InitializedAfterInit)
{
    LoggerFactory::Initialize();
    EXPECT_TRUE(LoggerFactory::IsInitialized());
}

TEST_F(LoggerFactoryTest, GetDefaultReturnsLogger)
{
    LoggerFactory::Initialize();
    auto logger = LoggerFactory::GetDefault();
    ASSERT_NE(logger, nullptr);
    EXPECT_EQ(logger->GetName(), "Hydra");
}

TEST_F(LoggerFactoryTest, GetReturnsNullForUnknownName)
{
    LoggerFactory::Initialize();
    EXPECT_EQ(LoggerFactory::Get("NonExistent"), nullptr);
}

TEST_F(LoggerFactoryTest, CreateAndRetrieveNamedLogger)
{
    LoggerFactory::Initialize();
    LoggerConfig cfg;
    cfg.name          = "Physics";
    cfg.enableConsole = false;

    auto created   = LoggerFactory::Create(cfg);
    auto retrieved = LoggerFactory::Get("Physics");

    ASSERT_NE(created, nullptr);
    ASSERT_NE(retrieved, nullptr);
    EXPECT_EQ(created.get(), retrieved.get()); // same object
}

TEST_F(LoggerFactoryTest, DuplicateCreateReturnsSameLogger)
{
    LoggerFactory::Initialize();
    LoggerConfig cfg;
    cfg.name          = "Dup";
    cfg.enableConsole = false;

    auto first  = LoggerFactory::Create(cfg);
    auto second = LoggerFactory::Create(cfg);
    EXPECT_EQ(first.get(), second.get());
}

TEST_F(LoggerFactoryTest, CreateConsoleLogger)
{
    LoggerFactory::Initialize();
    auto logger = LoggerFactory::CreateConsoleLogger("Console");
    ASSERT_NE(logger, nullptr);
    EXPECT_EQ(logger->GetName(), "Console");
    EXPECT_FALSE(logger->GetSinks().empty());
}

TEST_F(LoggerFactoryTest, ShutdownClearsRegistry)
{
    LoggerFactory::Initialize();
    LoggerFactory::Shutdown();
    EXPECT_FALSE(LoggerFactory::IsInitialized());
    EXPECT_EQ(LoggerFactory::GetDefault(), nullptr);
}

TEST_F(LoggerFactoryTest, SetLevelAllUpdatesEveryLogger)
{
    LoggerFactory::Initialize();
    LoggerConfig cfg;
    cfg.name          = "A";
    cfg.enableConsole = false;
    LoggerFactory::Create(cfg);
    cfg.name = "B";
    LoggerFactory::Create(cfg);

    LoggerFactory::SetLevelAll(LogLevel::Fatal);

    EXPECT_EQ(LoggerFactory::Get("A")->GetLevel(), LogLevel::Fatal);
    EXPECT_EQ(LoggerFactory::Get("B")->GetLevel(), LogLevel::Fatal);
}

// =============================================================================
// Macro Tests
// =============================================================================

class MacroTest : public ::testing::Test
{
protected:
    std::shared_ptr<CaptureSink> capture;

    void SetUp() override
    {
        LoggerFactory::Shutdown();

        // Build a logger with a capture sink as the default
        auto logger = std::make_shared<Logger>("Hydra", LogLevel::Trace);
        capture     = std::make_shared<CaptureSink>();
        capture->SetFormatter(std::make_shared<PatternFormatter>("{level}|{message}"));
        logger->AddSink(capture);

        // Manually register as the default (bypassing factory for simplicity)
        LoggerFactory::Initialize();
        LoggerFactory::GetDefault()->AddSink(capture);
    }

    void TearDown() override
    {
        LoggerFactory::Shutdown();
    }
};

TEST_F(MacroTest, HydraInfoMacroLogs)
{
    const usize before = capture->Count();
    HYDRA_INFO("info macro {}", 1);
    EXPECT_GT(capture->Count(), before);
}

TEST_F(MacroTest, HydraWarningMacroLogs)
{
    const usize before = capture->Count();
    HYDRA_WARNING("warning macro");
    EXPECT_GT(capture->Count(), before);
}

TEST_F(MacroTest, HydraErrorMacroLogs)
{
    const usize before = capture->Count();
    HYDRA_ERROR("error macro");
    EXPECT_GT(capture->Count(), before);
}

TEST_F(MacroTest, HydraFatalMacroLogs)
{
    const usize before = capture->Count();
    HYDRA_FATAL("fatal macro");
    EXPECT_GT(capture->Count(), before);
}

TEST_F(MacroTest, MacroCapturesSourceLocation)
{
    HYDRA_INFO("location test");
    // Find the message in the default logger's last output
    auto msgs = capture->Messages();
    bool found = false;
    for (auto& m : msgs)
    {
        if (m.message.find("location test") != std::string::npos)
        {
            EXPECT_TRUE(m.source.IsValid());
            EXPECT_GT(m.source.line, 0);
            found = true;
            break;
        }
    }
    EXPECT_TRUE(found);
}

TEST_F(MacroTest, MacroDropsMessageWhenLoggerIsNull)
{
    // After shutdown the factory returns nullptr — macros must not crash.
    LoggerFactory::Shutdown();
    EXPECT_NO_THROW(HYDRA_INFO("no logger"));
    EXPECT_NO_THROW(HYDRA_FATAL("no logger fatal"));
}
