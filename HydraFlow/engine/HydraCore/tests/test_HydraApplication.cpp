#include <gtest/gtest.h>
#include <HydraCore/Application/HydraApplication.h>
#include <HydraCore/Application/IHydraModule.h>

using namespace Hydra;

// --------------------------------------------------------------------------
// A self-terminating application that runs for exactly N frames
// --------------------------------------------------------------------------

class CountingModule : public IHydraModule
{
public:
    explicit CountingModule(int maxFrames)
        : m_MaxFrames(maxFrames) {}

    StringView GetName() const noexcept override { return "CountingModule"; }

    void OnUpdate(EngineContext&, f64) override
    {
        ++m_FrameCount;
        if (m_FrameCount >= m_MaxFrames)
            m_App->RequestShutdown();
    }

    void SetApp(HydraApplication* app) { m_App = app; }

    [[nodiscard]] int GetFrameCount() const { return m_FrameCount; }

private:
    HydraApplication* m_App      = nullptr;
    int               m_MaxFrames = 1;
    int               m_FrameCount = 0;
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
    HydraApplication app(MakeTestSettings());

    auto& module = app.GetModules().Register<CountingModule>(3);
    module.SetApp(&app);

    int result = app.Run();
    EXPECT_EQ(result, EXIT_SUCCESS);
    EXPECT_EQ(module.GetFrameCount(), 3);
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
