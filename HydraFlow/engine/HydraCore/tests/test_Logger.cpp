#include <gtest/gtest.h>
#include <HydraCore/Logging/Logger.h>
#include <HydraCore/Logging/LoggerFactory.h>
#include <HydraCore/Logging/NullSink.h>
#include <HydraCore/Logging/LoggingMacros.h>

using namespace Hydra;

// Module 1 compatibility test suite.
// Now that Module 2 has replaced the static Logger with LoggerFactory +
// instance-based Logger, these tests exercise the same contract through the
// new API so existing expectations still hold.

class LoggerTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        LoggerFactory::Shutdown(); // always start clean

        LoggerConfig cfg;
        cfg.name          = "TestLogger";
        cfg.enableConsole = false; // suppress output during tests
        cfg.enableFile    = false;
        cfg.level         = LogLevel::Trace;
        LoggerFactory::Initialize(cfg);
    }

    void TearDown() override
    {
        LoggerFactory::Shutdown();
    }
};

TEST_F(LoggerTest, IsInitializedAfterInit)
{
    EXPECT_TRUE(LoggerFactory::IsInitialized());
    EXPECT_NE(LoggerFactory::GetDefault(), nullptr);
}

TEST_F(LoggerTest, IsNotInitializedAfterShutdown)
{
    LoggerFactory::Shutdown();
    EXPECT_FALSE(LoggerFactory::IsInitialized());
    EXPECT_EQ(LoggerFactory::GetDefault(), nullptr);
}

TEST_F(LoggerTest, SetAndGetLevel)
{
    auto logger = LoggerFactory::GetDefault();
    ASSERT_NE(logger, nullptr);

    logger->SetLevel(LogLevel::Warn);
    EXPECT_EQ(logger->GetLevel(), LogLevel::Warn);

    logger->SetLevel(LogLevel::Debug);
    EXPECT_EQ(logger->GetLevel(), LogLevel::Debug);
}

TEST_F(LoggerTest, DoesNotCrashOnDoubleInitialize)
{
    // Second Initialize() call must be a safe no-op.
    LoggerConfig cfg;
    cfg.enableConsole = false;
    EXPECT_NO_THROW(LoggerFactory::Initialize(cfg));
    EXPECT_TRUE(LoggerFactory::IsInitialized());
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
