#include <gtest/gtest.h>
#include <HydraCore/Logging/Logger.h>

using namespace Hydra;

class LoggerTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        LoggerConfig cfg;
        cfg.loggerName    = "TestLogger";
        cfg.enableConsole = false; // suppress output during tests
        cfg.enableFile    = false;
        cfg.level         = LogLevel::Trace;
        Logger::Initialize(cfg);
    }

    void TearDown() override
    {
        Logger::Shutdown();
    }
};

TEST_F(LoggerTest, IsInitializedAfterInit)
{
    EXPECT_TRUE(Logger::IsInitialized());
}

TEST_F(LoggerTest, IsNotInitializedAfterShutdown)
{
    Logger::Shutdown();
    EXPECT_FALSE(Logger::IsInitialized());
}

TEST_F(LoggerTest, SetAndGetLevel)
{
    Logger::SetLevel(LogLevel::Warn);
    EXPECT_EQ(Logger::GetLevel(), LogLevel::Warn);

    Logger::SetLevel(LogLevel::Debug);
    EXPECT_EQ(Logger::GetLevel(), LogLevel::Debug);
}

TEST_F(LoggerTest, DoesNotCrashOnDoubleInitialize)
{
    LoggerConfig cfg;
    cfg.enableConsole = false;
    EXPECT_NO_THROW(Logger::Initialize(cfg));
    EXPECT_TRUE(Logger::IsInitialized());
}

TEST_F(LoggerTest, MacrosDoNotThrow)
{
    EXPECT_NO_THROW({
        HYDRA_LOG_TRACE("trace {}", 1);
        HYDRA_LOG_DEBUG("debug {}", 2);
        HYDRA_LOG_INFO("info  {}", 3);
        HYDRA_LOG_WARN("warn  {}", 4);
        HYDRA_LOG_ERROR("error {}", 5);
    });
}
