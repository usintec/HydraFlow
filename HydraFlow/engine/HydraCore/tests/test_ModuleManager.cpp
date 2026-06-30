#include <gtest/gtest.h>
#include <HydraCore/Application/ModuleManager.h>
#include <HydraCore/Application/EngineContext.h>
#include <HydraCore/Logging/LoggerFactory.h>

using namespace Hydra;

// =============================================================================
// Test helper module — records which lifecycle hooks were called
// =============================================================================

struct CallLog
{
    bool registered   = false;
    bool initialized  = false;
    int  updateCount  = 0;
    int  lateCount    = 0;
    bool shutdown     = false;
    bool unregistered = false;
};

class TestModule : public IHydraModule
{
public:
    explicit TestModule(CallLog& log, bool failInit = false)
        : m_Log(log), m_FailInit(failInit) {}

    StringView GetName()    const noexcept override { return "TestModule"; }
    StringView GetVersion() const noexcept override { return "1.0.0"; }

    void OnRegister(EngineContext&)        override { m_Log.registered  = true; }
    bool OnInitialize(EngineContext&)      override { m_Log.initialized = true; return !m_FailInit; }
    void OnUpdate(EngineContext&, f64)     override { ++m_Log.updateCount; }
    void OnLateUpdate(EngineContext&, f64) override { ++m_Log.lateCount; }
    void OnShutdown(EngineContext&)        override { m_Log.shutdown     = true; }
    void OnUnregister(EngineContext&)      override { m_Log.unregistered = true; }

private:
    CallLog& m_Log;
    bool     m_FailInit;
};

// =============================================================================
// Fixture — ensures LoggerFactory is up so ModuleManager can log freely
// =============================================================================

class ModuleManagerTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        LoggerFactory::Shutdown();
        LoggerConfig cfg;
        cfg.name          = "Hydra";
        cfg.enableConsole = false; // silence during tests
        LoggerFactory::Initialize(cfg);
    }

    void TearDown() override
    {
        LoggerFactory::Shutdown();
    }

    EngineContext ctx;
};

// =============================================================================
// Tests
// =============================================================================

TEST_F(ModuleManagerTest, RegisterAndCountModules)
{
    ModuleManager mgr;
    CallLog log;
    mgr.Register(MakeUnique<TestModule>(log));
    EXPECT_EQ(mgr.Count(), 1u);
    EXPECT_FALSE(mgr.IsEmpty());
}

TEST_F(ModuleManagerTest, InitializeAllCallsLifecycle)
{
    ModuleManager mgr;
    CallLog log;
    mgr.Register(MakeUnique<TestModule>(log));
    EXPECT_TRUE(mgr.InitializeAll(ctx));
    EXPECT_TRUE(log.registered);
    EXPECT_TRUE(log.initialized);
}

TEST_F(ModuleManagerTest, UpdateAllCallsOnUpdate)
{
    ModuleManager mgr;
    CallLog log;
    mgr.Register(MakeUnique<TestModule>(log));
    mgr.InitializeAll(ctx);
    mgr.UpdateAll(ctx, 0.016);
    mgr.UpdateAll(ctx, 0.016);
    EXPECT_EQ(log.updateCount, 2);
}

TEST_F(ModuleManagerTest, LateUpdateAllCallsOnLateUpdate)
{
    ModuleManager mgr;
    CallLog log;
    mgr.Register(MakeUnique<TestModule>(log));
    mgr.InitializeAll(ctx);
    mgr.LateUpdateAll(ctx, 0.016);
    EXPECT_EQ(log.lateCount, 1);
}

TEST_F(ModuleManagerTest, ShutdownAllCallsShutdownAndUnregister)
{
    ModuleManager mgr;
    CallLog log;
    mgr.Register(MakeUnique<TestModule>(log));
    mgr.InitializeAll(ctx);
    mgr.ShutdownAll(ctx);
    EXPECT_TRUE(log.shutdown);
    EXPECT_TRUE(log.unregistered);
    EXPECT_EQ(mgr.Count(), 0u);
}

TEST_F(ModuleManagerTest, FailingModuleAbortsInitialize)
{
    ModuleManager mgr;
    CallLog log;
    mgr.Register(MakeUnique<TestModule>(log, /*failInit=*/true));
    EXPECT_FALSE(mgr.InitializeAll(ctx));
}

TEST_F(ModuleManagerTest, FindModuleByName)
{
    ModuleManager mgr;
    CallLog log;
    mgr.Register(MakeUnique<TestModule>(log));
    EXPECT_NE(mgr.Find("TestModule"), nullptr);
    EXPECT_EQ(mgr.Find("NonExistent"), nullptr);
}
