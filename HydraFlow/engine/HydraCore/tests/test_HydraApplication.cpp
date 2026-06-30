#include <gtest/gtest.h>
#include <HydraCore/Application/HydraApplication.h>
#include <HydraCore/Application/IHydraModule.h>

using namespace Hydra;

// --------------------------------------------------------------------------
// A self-terminating application that runs for exactly N frames.
//
// IMPORTANT: The module is owned (and destroyed) by ModuleManager before
// app.Run() returns.  To read the final frame count after Run(), the module
// writes into an external int* supplied at construction rather than into a
// member that would become a dangling reference.
// --------------------------------------------------------------------------

class CountingModule : public IHydraModule
{
public:
    /// @param maxFrames  Stop after this many updates.
    /// @param outCount   Written with the final frame count on shutdown.
    CountingModule(int maxFrames, int& outCount)
        : m_MaxFrames(maxFrames), m_OutCount(outCount) {}

    StringView GetName()    const noexcept override { return "CountingModule"; }
    StringView GetVersion() const noexcept override { return "1.0.0"; }

    void OnUpdate(EngineContext&, f64) override
    {
        ++m_FrameCount;
        if (m_FrameCount >= m_MaxFrames)
            m_App->RequestShutdown();
    }

    // Called by ShutdownSequence before the object is destroyed — safe to
    // write to the external counter here because the referent outlives the module.
    void OnShutdown(EngineContext&) override
    {
        m_OutCount = m_FrameCount;
    }

    void SetApp(HydraApplication* app) { m_App = app; }

private:
    HydraApplication* m_App       = nullptr;
    int               m_MaxFrames = 1;
    int               m_FrameCount = 0;
    int&              m_OutCount;   ///< Caller-owned storage for the result
};

class HydraApplicationTest : public ::testing::Test
{
protected:
    ApplicationSettings MakeTestSettings()
    {
        auto s = ApplicationSettings::MakeDefault();
        s.logging.enableConsole  = false;
        s.configFilePath         = ""; // skip config file lookup
        s.updateLoop.targetFrameRate = 10000.0; // fast
        return s;
    }
};

TEST_F(HydraApplicationTest, RunsAndExitsCleanly)
{
    // outFrameCount outlives the app — CountingModule writes into it on shutdown
    // so we can read the final count after Run() (and after the module is destroyed).
    int outFrameCount = 0;

    HydraApplication app(MakeTestSettings());

    auto& module = app.GetModules().Register<CountingModule>(3, outFrameCount);
    module.SetApp(&app);

    int result = app.Run();
    EXPECT_EQ(result, EXIT_SUCCESS);
    EXPECT_EQ(outFrameCount, 3);
}

TEST_F(HydraApplicationTest, GetSettingsReturnsCorrectName)
{
    auto s = MakeTestSettings();
    s.appName = "MyTestApp";
    HydraApplication app(s);
    EXPECT_EQ(app.GetSettings().appName, "MyTestApp");
}

TEST_F(HydraApplicationTest, IsRunningFalseBeforeRun)
{
    HydraApplication app(MakeTestSettings());
    EXPECT_FALSE(app.IsRunning());
}
