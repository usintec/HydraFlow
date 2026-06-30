#include <gtest/gtest.h>
#include <HydraCore/Config/ConfigManager.h>

#include <fstream>
#include <filesystem>

using namespace Hydra;

class ConfigManagerTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        m_TmpYaml = std::filesystem::temp_directory_path() / "hydra_test_config.yaml";
        m_TmpJson = std::filesystem::temp_directory_path() / "hydra_test_config.json";

        {
            std::ofstream f(m_TmpYaml);
            f << "app:\n"
              << "  name: TestApp\n"
              << "  version: 1.2.3\n"
              << "window:\n"
              << "  width: 1920\n"
              << "  height: 1080\n"
              << "  fullscreen: true\n";
        }

        {
            std::ofstream f(m_TmpJson);
            f << R"({ "app": { "name": "JsonApp", "count": 42 } })";
        }
    }

    void TearDown() override
    {
        std::filesystem::remove(m_TmpYaml);
        std::filesystem::remove(m_TmpJson);
    }

    std::filesystem::path m_TmpYaml;
    std::filesystem::path m_TmpJson;
};

TEST_F(ConfigManagerTest, LoadYamlFile)
{
    ConfigManager cfg;
    EXPECT_TRUE(cfg.LoadFile(m_TmpYaml));
    EXPECT_TRUE(cfg.IsLoaded());
}

TEST_F(ConfigManagerTest, GetStringFromYaml)
{
    ConfigManager cfg;
    cfg.LoadFile(m_TmpYaml);
    auto name = cfg.Get<std::string>("app.name");
    ASSERT_TRUE(name.has_value());
    EXPECT_EQ(*name, "TestApp");
}

TEST_F(ConfigManagerTest, GetIntFromYaml)
{
    ConfigManager cfg;
    cfg.LoadFile(m_TmpYaml);
    auto width = cfg.Get<i64>("window.width");
    ASSERT_TRUE(width.has_value());
    EXPECT_EQ(*width, 1920);
}

TEST_F(ConfigManagerTest, GetBoolFromYaml)
{
    ConfigManager cfg;
    cfg.LoadFile(m_TmpYaml);
    auto fs = cfg.Get<bool>("window.fullscreen");
    ASSERT_TRUE(fs.has_value());
    EXPECT_TRUE(*fs);
}

TEST_F(ConfigManagerTest, GetOrDefaultReturnsFallback)
{
    ConfigManager cfg;
    cfg.LoadFile(m_TmpYaml);
    auto val = cfg.GetOrDefault<std::string>("does.not.exist", "fallback");
    EXPECT_EQ(val, "fallback");
}

TEST_F(ConfigManagerTest, LoadJsonFile)
{
    ConfigManager cfg;
    EXPECT_TRUE(cfg.LoadFile(m_TmpJson));
    auto name = cfg.Get<std::string>("app.name");
    ASSERT_TRUE(name.has_value());
    EXPECT_EQ(*name, "JsonApp");
}

TEST_F(ConfigManagerTest, MissingKeyReturnsNullopt)
{
    ConfigManager cfg;
    cfg.LoadFile(m_TmpYaml);
    EXPECT_FALSE(cfg.Get<std::string>("non.existent.key").has_value());
}

TEST_F(ConfigManagerTest, LoadNonExistentFileReturnsFalse)
{
    ConfigManager cfg;
    EXPECT_FALSE(cfg.LoadFile("/tmp/this_file_does_not_exist_hydra.yaml"));
    EXPECT_FALSE(cfg.IsLoaded());
}
