#include <gtest/gtest.h>

#include <HydraCore/Configuration/ConfigValue.h>
#include <HydraCore/Configuration/ConfigNode.h>
#include <HydraCore/Configuration/Configuration.h>
#include <HydraCore/Configuration/ConfigSchema.h>
#include <HydraCore/Configuration/EnvironmentSettings.h>
#include <HydraCore/Configuration/IHotReloadListener.h>
#include <HydraCore/Configuration/ConfigurationManager.h>

#include <fstream>
#include <filesystem>
#include <cstdlib>

using namespace Hydra;

// =============================================================================
// Helpers
// =============================================================================

namespace {

std::filesystem::path TmpPath(const char* name)
{
    return std::filesystem::temp_directory_path() / name;
}

void WriteFile(const std::filesystem::path& p, const char* content)
{
    std::ofstream f(p);
    f << content;
}

void RemoveFile(const std::filesystem::path& p)
{
    std::filesystem::remove(p);
}

} // anonymous namespace

// =============================================================================
// ConfigValueTest  (18 tests)
// =============================================================================

class ConfigValueTest : public ::testing::Test {};

TEST_F(ConfigValueTest, DefaultIsNull)
{
    ConfigValue v;
    EXPECT_TRUE(v.IsNull());
    EXPECT_EQ(v.GetType(), ConfigValue::Type::Null);
}

TEST_F(ConfigValueTest, BoolConstruction)
{
    ConfigValue v{ true };
    EXPECT_TRUE(v.IsBool());
    EXPECT_EQ(v.AsBool(), Optional<bool>{ true });
}

TEST_F(ConfigValueTest, IntConstruction)
{
    ConfigValue v{ i64{ 42 } };
    EXPECT_TRUE(v.IsInteger());
    EXPECT_EQ(v.AsInt(), Optional<i64>{ 42 });
}

TEST_F(ConfigValueTest, I32Construction)
{
    ConfigValue v{ i32{ -7 } };
    EXPECT_TRUE(v.IsInteger());
    EXPECT_EQ(*v.AsInt(), -7);
}

TEST_F(ConfigValueTest, FloatConstruction)
{
    ConfigValue v{ 3.14 };
    EXPECT_TRUE(v.IsFloat());
    ASSERT_TRUE(v.AsFloat().has_value());
    EXPECT_NEAR(*v.AsFloat(), 3.14, 1e-9);
}

TEST_F(ConfigValueTest, StringConstruction)
{
    ConfigValue v{ StringView("hello") };
    EXPECT_TRUE(v.IsString());
    EXPECT_EQ(v.AsString(), Optional<String>{ "hello" });
}

TEST_F(ConfigValueTest, CStringConstruction)
{
    ConfigValue v{ "world" };
    EXPECT_EQ(*v.AsString(), "world");
}

TEST_F(ConfigValueTest, NullAsOtherTypeReturnsNullopt)
{
    ConfigValue v;
    EXPECT_FALSE(v.AsBool().has_value());
    EXPECT_FALSE(v.AsInt().has_value());
    EXPECT_FALSE(v.AsFloat().has_value());
    EXPECT_FALSE(v.AsString().has_value());
}

TEST_F(ConfigValueTest, AsOrReturnsFallback)
{
    ConfigValue v;
    EXPECT_EQ(v.AsOr<i64>(99), 99);
    EXPECT_EQ(v.AsOr<String>("fallback"), "fallback");
}

TEST_F(ConfigValueTest, ArrayConstruction)
{
    ConfigValue arr{ ConfigValue{ i64{1} }, ConfigValue{ i64{2} }, ConfigValue{ i64{3} } };
    EXPECT_TRUE(arr.IsArray());
    EXPECT_EQ(arr.Size(), 3u);
    EXPECT_EQ(*arr.At(0).AsInt(), 1);
    EXPECT_EQ(*arr.At(2).AsInt(), 3);
}

TEST_F(ConfigValueTest, ArrayOutOfRangeIsNull)
{
    ConfigValue arr = ConfigValue::MakeArray();
    arr.Append(ConfigValue{ i64{1} });
    EXPECT_TRUE(arr.At(99).IsNull());
}

TEST_F(ConfigValueTest, ObjectConstruction)
{
    ConfigValue obj = ConfigValue::MakeObject();
    obj.Set("name", ConfigValue{ "Alice" });
    obj.Set("age",  ConfigValue{ i64{30} });

    EXPECT_TRUE(obj.IsObject());
    EXPECT_TRUE(obj.HasKey("name"));
    EXPECT_TRUE(obj.HasKey("age"));
    EXPECT_FALSE(obj.HasKey("missing"));
    EXPECT_EQ(*obj.Get("name").AsString(), "Alice");
    EXPECT_EQ(*obj.Get("age").AsInt(), 30);
}

TEST_F(ConfigValueTest, ObjectMissingKeyIsNull)
{
    ConfigValue obj = ConfigValue::MakeObject();
    EXPECT_TRUE(obj.Get("missing").IsNull());
}

TEST_F(ConfigValueTest, IsNumberCoversIntAndFloat)
{
    EXPECT_TRUE(ConfigValue{ i64{1} }.IsNumber());
    EXPECT_TRUE(ConfigValue{ 1.0   }.IsNumber());
    EXPECT_FALSE(ConfigValue{ "x"  }.IsNumber());
}

TEST_F(ConfigValueTest, Equality)
{
    EXPECT_EQ(ConfigValue{ i64{5} }, ConfigValue{ i64{5} });
    EXPECT_NE(ConfigValue{ i64{5} }, ConfigValue{ i64{6} });
    EXPECT_EQ(ConfigValue{},        ConfigValue{});
}

TEST_F(ConfigValueTest, JsonRoundTrip)
{
    ConfigValue orig{ "roundtrip" };
    const auto json  = orig.ToJson();
    const auto back  = ConfigValue::FromJson(json);
    EXPECT_EQ(orig, back);
}

TEST_F(ConfigValueTest, NestedObjectRoundTrip)
{
    ConfigValue outer = ConfigValue::MakeObject();
    ConfigValue inner = ConfigValue::MakeObject();
    inner.Set("x", ConfigValue{ i64{1} });
    outer.Set("inner", inner);

    const auto json = outer.ToJson();
    const auto back = ConfigValue::FromJson(json);
    EXPECT_EQ(outer, back);
}

TEST_F(ConfigValueTest, ToStringCompact)
{
    ConfigValue v{ i64{42} };
    const String s = v.ToString();
    EXPECT_EQ(s, "42");
}

// =============================================================================
// ConfigNodeTest  (8 tests)
// =============================================================================

class ConfigNodeTest : public ::testing::Test {};

TEST_F(ConfigNodeTest, DefaultIsInvalid)
{
    ConfigNode node;
    EXPECT_FALSE(node.IsValid());
    EXPECT_FALSE(static_cast<bool>(node));
}

TEST_F(ConfigNodeTest, ValidNodeFromConfigValue)
{
    ConfigValue v{ i64{7} };
    ConfigNode node{ v, "root" };
    EXPECT_TRUE(node.IsValid());
    EXPECT_EQ(*node.AsInt(), 7);
    EXPECT_EQ(node.GetPath(), "root");
}

TEST_F(ConfigNodeTest, ChildNavigation)
{
    ConfigValue obj = ConfigValue::MakeObject();
    obj.Set("key", ConfigValue{ "value" });

    ConfigNode root{ obj, "" };
    ConfigNode child = root.Child("key");
    EXPECT_TRUE(child.IsValid());
    EXPECT_EQ(*child.AsString(), "value");
    EXPECT_EQ(child.GetPath(), "key");
}

TEST_F(ConfigNodeTest, MissingChildIsInvalid)
{
    ConfigValue obj = ConfigValue::MakeObject();
    ConfigNode root{ obj, "" };
    EXPECT_FALSE(root.Child("missing").IsValid());
}

TEST_F(ConfigNodeTest, NavigateDotPath)
{
    ConfigValue inner = ConfigValue::MakeObject();
    inner.Set("z", ConfigValue{ i64{99} });

    ConfigValue outer = ConfigValue::MakeObject();
    outer.Set("a", inner);

    ConfigNode root{ outer, "" };
    ConfigNode found = root.Navigate("a.z");
    EXPECT_TRUE(found.IsValid());
    EXPECT_EQ(*found.AsInt(), 99);
}

TEST_F(ConfigNodeTest, NavigateMissingSegmentIsInvalid)
{
    ConfigValue obj = ConfigValue::MakeObject();
    ConfigNode root{ obj, "" };
    EXPECT_FALSE(root.Navigate("a.b.c").IsValid());
}

TEST_F(ConfigNodeTest, ArrayIndexNavigation)
{
    ConfigValue arr = ConfigValue::MakeArray();
    arr.Append(ConfigValue{ "first" });
    arr.Append(ConfigValue{ "second" });

    ConfigNode root{ arr, "arr" };
    EXPECT_TRUE(root.At(0).IsValid());
    EXPECT_EQ(*root.At(0).AsString(), "first");
    EXPECT_FALSE(root.At(99).IsValid());
}

TEST_F(ConfigNodeTest, OperatorBracketIsChildAlias)
{
    ConfigValue obj = ConfigValue::MakeObject();
    obj.Set("k", ConfigValue{ true });

    ConfigNode root{ obj, "" };
    EXPECT_EQ(root["k"].AsBool(), Optional<bool>{ true });
}

// =============================================================================
// ConfigurationTest  (12 tests)
// =============================================================================

class ConfigurationTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        m_Yaml = TmpPath("hydra_cfg_test.yaml");
        m_Json = TmpPath("hydra_cfg_test.json");

        WriteFile(m_Yaml,
            "app:\n"
            "  name: TestApp\n"
            "  version: 2.0.0\n"
            "server:\n"
            "  port: 8080\n"
            "  debug: true\n");

        WriteFile(m_Json,
            R"({ "db": { "host": "localhost", "port": 5432 } })");
    }

    void TearDown() override
    {
        RemoveFile(m_Yaml);
        RemoveFile(m_Json);
    }

    std::filesystem::path m_Yaml;
    std::filesystem::path m_Json;
};

TEST_F(ConfigurationTest, DefaultIsInvalid)
{
    Configuration cfg;
    EXPECT_FALSE(cfg.IsValid());
}

TEST_F(ConfigurationTest, LoadYaml)
{
    auto cfg = LoadConfigurationFromFile(m_Yaml);
    EXPECT_TRUE(cfg.IsValid());
}

TEST_F(ConfigurationTest, LoadJson)
{
    auto cfg = LoadConfigurationFromFile(m_Json);
    EXPECT_TRUE(cfg.IsValid());
}

TEST_F(ConfigurationTest, GetStringValue)
{
    auto cfg = LoadConfigurationFromFile(m_Yaml);
    auto name = cfg.Get<String>("app.name");
    ASSERT_TRUE(name.has_value());
    EXPECT_EQ(*name, "TestApp");
}

TEST_F(ConfigurationTest, GetIntValue)
{
    auto cfg = LoadConfigurationFromFile(m_Yaml);
    auto port = cfg.Get<i64>("server.port");
    ASSERT_TRUE(port.has_value());
    EXPECT_EQ(*port, 8080);
}

TEST_F(ConfigurationTest, GetBoolValue)
{
    auto cfg = LoadConfigurationFromFile(m_Yaml);
    auto dbg = cfg.Get<bool>("server.debug");
    ASSERT_TRUE(dbg.has_value());
    EXPECT_TRUE(*dbg);
}

TEST_F(ConfigurationTest, GetOrDefaultFallback)
{
    auto cfg = LoadConfigurationFromFile(m_Yaml);
    auto val = cfg.GetOrDefault<String>("does.not.exist", "fallback");
    EXPECT_EQ(val, "fallback");
}

TEST_F(ConfigurationTest, HasExistingKey)
{
    auto cfg = LoadConfigurationFromFile(m_Yaml);
    EXPECT_TRUE(cfg.Has("app.name"));
    EXPECT_FALSE(cfg.Has("app.missing"));
}

TEST_F(ConfigurationTest, GetNodeNavigation)
{
    auto cfg  = LoadConfigurationFromFile(m_Yaml);
    auto node = cfg.Get("server.port");
    EXPECT_TRUE(node.IsValid());
    EXPECT_EQ(*node.AsInt(), 8080);
}

TEST_F(ConfigurationTest, DumpProducesJson)
{
    auto cfg  = LoadConfigurationFromFile(m_Yaml);
    auto dump = cfg.Dump(0);
    EXPECT_FALSE(dump.empty());
    EXPECT_NE(dump.find("TestApp"), std::string::npos);
}

TEST_F(ConfigurationTest, MergeOverWinsOnConflict)
{
    auto base = LoadConfigurationFromFile(m_Yaml);
    ASSERT_TRUE(base.IsValid());

    // Create an overlay that overrides app.name
    auto overlay = TmpPath("hydra_cfg_overlay.yaml");
    WriteFile(overlay, "app:\n  name: OverriddenApp\n  extra: yes\n");

    auto over = LoadConfigurationFromFile(overlay);
    ASSERT_TRUE(over.IsValid());

    base.MergeOver(over);

    EXPECT_EQ(*base.Get<String>("app.name"),    "OverriddenApp"); // overridden
    EXPECT_EQ(*base.Get<String>("app.version"), "2.0.0");         // preserved
    EXPECT_EQ(*base.Get<bool>("app.extra"),     true);             // added

    RemoveFile(overlay);
}

TEST_F(ConfigurationTest, LoadMissingFileReturnsInvalid)
{
    auto cfg = LoadConfigurationFromFile("/tmp/no_such_file_hydra_m3.yaml");
    EXPECT_FALSE(cfg.IsValid());
}

// =============================================================================
// ConfigSchemaTest  (10 tests)
// =============================================================================

class ConfigSchemaTest : public ::testing::Test
{
protected:
    Configuration MakeConfig(const char* yaml)
    {
        auto p = TmpPath("hydra_schema_test.yaml");
        WriteFile(p, yaml);
        auto cfg = LoadConfigurationFromFile(p);
        RemoveFile(p);
        return cfg;
    }
};

TEST_F(ConfigSchemaTest, RequireKeyPassesWhenPresent)
{
    auto cfg = MakeConfig("foo: bar\n");
    ConfigSchema schema;
    schema.RequireKey("foo");
    EXPECT_TRUE(schema.Validate(cfg).IsValid());
}

TEST_F(ConfigSchemaTest, RequireKeyFailsWhenAbsent)
{
    auto cfg = MakeConfig("foo: bar\n");
    ConfigSchema schema;
    schema.RequireKey("missing");
    EXPECT_FALSE(schema.Validate(cfg).IsValid());
}

TEST_F(ConfigSchemaTest, RequireTypePassesOnMatch)
{
    auto cfg = MakeConfig("count: 5\n");
    ConfigSchema schema;
    schema.RequireType("count", ConfigValue::Type::Integer);
    EXPECT_TRUE(schema.Validate(cfg).IsValid());
}

TEST_F(ConfigSchemaTest, RequireTypeFailsOnMismatch)
{
    auto cfg = MakeConfig("count: hello\n");
    ConfigSchema schema;
    schema.RequireType("count", ConfigValue::Type::Integer);
    EXPECT_FALSE(schema.Validate(cfg).IsValid());
}

TEST_F(ConfigSchemaTest, RequireRangePassesInBounds)
{
    auto cfg = MakeConfig("level: 5\n");
    ConfigSchema schema;
    schema.RequireRange("level", Optional<f64>{ 1.0 }, Optional<f64>{ 10.0 });
    EXPECT_TRUE(schema.Validate(cfg).IsValid());
}

TEST_F(ConfigSchemaTest, RequireRangeFailsBelowMin)
{
    auto cfg = MakeConfig("level: 0\n");
    ConfigSchema schema;
    schema.RequireRange("level", Optional<f64>{ 1.0 }, std::nullopt);
    EXPECT_FALSE(schema.Validate(cfg).IsValid());
}

TEST_F(ConfigSchemaTest, RequireRangeFailsAboveMax)
{
    auto cfg = MakeConfig("level: 100\n");
    ConfigSchema schema;
    schema.RequireRange("level", std::nullopt, Optional<f64>{ 10.0 });
    EXPECT_FALSE(schema.Validate(cfg).IsValid());
}

TEST_F(ConfigSchemaTest, AllowValuesPassesForValidValue)
{
    auto cfg = MakeConfig("env: production\n");
    ConfigSchema schema;
    schema.AllowValues("env", { "development", "staging", "production" });
    EXPECT_TRUE(schema.Validate(cfg).IsValid());
}

TEST_F(ConfigSchemaTest, AllowValuesFailsForInvalidValue)
{
    auto cfg = MakeConfig("env: unknown\n");
    ConfigSchema schema;
    schema.AllowValues("env", { "development", "staging", "production" });
    EXPECT_FALSE(schema.Validate(cfg).IsValid());
}

TEST_F(ConfigSchemaTest, MultipleErrorsAccumulate)
{
    auto cfg = MakeConfig("foo: bar\n");
    ConfigSchema schema;
    schema.RequireKey("missing1");
    schema.RequireKey("missing2");
    schema.RequireKey("missing3");
    auto result = schema.Validate(cfg);
    EXPECT_FALSE(result.IsValid());
    EXPECT_GE(result.ErrorCount(), 3u);
}

// =============================================================================
// EnvironmentSettingsTest  (8 tests)
// =============================================================================

class EnvironmentSettingsTest : public ::testing::Test {};

TEST_F(EnvironmentSettingsTest, DefaultIsDevelopment)
{
    EnvironmentSettings env;
    EXPECT_TRUE(env.IsDevelopment());
    EXPECT_EQ(env.GetEnvironmentName(), "development");
}

TEST_F(EnvironmentSettingsTest, SetProduction)
{
    EnvironmentSettings env;
    env.Set(Environment::Production);
    EXPECT_TRUE(env.IsProduction());
    EXPECT_FALSE(env.IsDevelopment());
    EXPECT_EQ(env.GetEnvironmentName(), "production");
}

TEST_F(EnvironmentSettingsTest, SetTesting)
{
    EnvironmentSettings env;
    env.Set(Environment::Testing);
    EXPECT_TRUE(env.IsTesting());
    EXPECT_EQ(env.GetEnvironmentName(), "testing");
}

TEST_F(EnvironmentSettingsTest, SetStaging)
{
    EnvironmentSettings env;
    env.Set(Environment::Staging);
    EXPECT_TRUE(env.IsStaging());
    EXPECT_EQ(env.GetEnvironmentName(), "staging");
}

TEST_F(EnvironmentSettingsTest, SetCustom)
{
    EnvironmentSettings env;
    env.SetCustom("my-custom-env");
    EXPECT_TRUE(env.IsCustom());
    EXPECT_EQ(env.GetEnvironmentName(), "my-custom-env");
}

TEST_F(EnvironmentSettingsTest, EqualityAndInequality)
{
    EnvironmentSettings a, b;
    EXPECT_EQ(a, b);
    b.Set(Environment::Production);
    EXPECT_NE(a, b);
}

TEST_F(EnvironmentSettingsTest, FromEnvVarUnsetReturnsDevelopment)
{
    // Ensure HYDRA_ENV is not set
#if defined(HYDRA_PLATFORM_WINDOWS)
    _putenv_s("HYDRA_ENV", "");
#else
    unsetenv("HYDRA_ENV");
#endif
    auto env = EnvironmentSettings::FromEnvironmentVariable();
    EXPECT_TRUE(env.IsDevelopment());
}

TEST_F(EnvironmentSettingsTest, FromEnvVarProduction)
{
#if defined(HYDRA_PLATFORM_WINDOWS)
    _putenv_s("HYDRA_ENV", "production");
#else
    setenv("HYDRA_ENV", "production", 1);
#endif
    auto env = EnvironmentSettings::FromEnvironmentVariable();
    EXPECT_TRUE(env.IsProduction());
#if defined(HYDRA_PLATFORM_WINDOWS)
    _putenv_s("HYDRA_ENV", "");
#else
    unsetenv("HYDRA_ENV");
#endif
}

// =============================================================================
// ConfigurationManagerTest  (13 tests)
// =============================================================================

class ConfigurationManagerTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        m_BaseYaml = TmpPath("hydra_mgr_base.yaml");
        m_DevYaml  = TmpPath("hydra_mgr_base.development.yaml");
        m_ProdYaml = TmpPath("hydra_mgr_base.production.yaml");

        WriteFile(m_BaseYaml,
            "app:\n"
            "  name: MgrApp\n"
            "  workers: 2\n"
            "db:\n"
            "  host: localhost\n");

        WriteFile(m_DevYaml,
            "app:\n"
            "  workers: 1\n"
            "debug: true\n");

        WriteFile(m_ProdYaml,
            "app:\n"
            "  workers: 16\n"
            "db:\n"
            "  host: prod-db.internal\n");
    }

    void TearDown() override
    {
        RemoveFile(m_BaseYaml);
        RemoveFile(m_DevYaml);
        RemoveFile(m_ProdYaml);
    }

    std::filesystem::path m_BaseYaml;
    std::filesystem::path m_DevYaml;
    std::filesystem::path m_ProdYaml;
    std::filesystem::path TmpDir() { return std::filesystem::temp_directory_path(); }
};

TEST_F(ConfigurationManagerTest, NotLoadedByDefault)
{
    ConfigurationManager mgr;
    EXPECT_FALSE(mgr.IsLoaded());
}

TEST_F(ConfigurationManagerTest, LoadFileSucceeds)
{
    ConfigurationManager mgr;
    EXPECT_TRUE(mgr.LoadFile(m_BaseYaml));
    EXPECT_TRUE(mgr.IsLoaded());
}

TEST_F(ConfigurationManagerTest, LoadMissingFileReturnsFalse)
{
    ConfigurationManager mgr;
    EXPECT_FALSE(mgr.LoadFile("/tmp/no_such_file_m3.yaml"));
    EXPECT_FALSE(mgr.IsLoaded());
}

TEST_F(ConfigurationManagerTest, GetReturnsLoadedValue)
{
    ConfigurationManager mgr;
    mgr.LoadFile(m_BaseYaml);
    EXPECT_EQ(*mgr.Get<String>("app.name"), "MgrApp");
    EXPECT_EQ(*mgr.Get<i64>("app.workers"), 2);
}

TEST_F(ConfigurationManagerTest, GetOrDefaultReturnsFallback)
{
    ConfigurationManager mgr;
    mgr.LoadFile(m_BaseYaml);
    EXPECT_EQ(mgr.GetOrDefault<String>("missing.key", "fallback"), "fallback");
}

TEST_F(ConfigurationManagerTest, HasReturnsCorrectly)
{
    ConfigurationManager mgr;
    mgr.LoadFile(m_BaseYaml);
    EXPECT_TRUE(mgr.Has("app.name"));
    EXPECT_FALSE(mgr.Has("nonexistent"));
}

TEST_F(ConfigurationManagerTest, SetDefaultsLowPriority)
{
    // Defaults: workers=99, extra=hello
    ConfigValue defRoot = ConfigValue::MakeObject();
    ConfigValue defApp  = ConfigValue::MakeObject();
    defApp.Set("workers", ConfigValue{ i64{99} });
    defApp.Set("extra",   ConfigValue{ "hello" });
    defRoot.Set("app", defApp);

    ConfigurationManager mgr;
    mgr.SetDefaults(Configuration{ defRoot });
    mgr.LoadFile(m_BaseYaml);   // workers=2 overrides default 99

    EXPECT_EQ(*mgr.Get<i64>("app.workers"),   2);       // file wins
    EXPECT_EQ(*mgr.Get<String>("app.extra"),  "hello"); // default preserved
}

TEST_F(ConfigurationManagerTest, EnvironmentOverlayDevMerges)
{
    EnvironmentSettings env{ Environment::Development };
    ConfigurationManager mgr{ env };
    mgr.LoadFile(m_BaseYaml);
    mgr.LoadEnvironmentOverlay(TmpDir(), "hydra_mgr_base");

    // Dev overlay sets workers=1
    EXPECT_EQ(*mgr.Get<i64>("app.workers"), 1);
    // Base value preserved
    EXPECT_EQ(*mgr.Get<String>("app.name"), "MgrApp");
    // Dev overlay adds debug=true
    EXPECT_EQ(*mgr.Get<bool>("debug"), true);
}

TEST_F(ConfigurationManagerTest, EnvironmentOverlayProdMerges)
{
    EnvironmentSettings env{ Environment::Production };
    ConfigurationManager mgr{ env };
    mgr.LoadFile(m_BaseYaml);
    mgr.LoadEnvironmentOverlay(TmpDir(), "hydra_mgr_base");

    EXPECT_EQ(*mgr.Get<i64>("app.workers"),    16);
    EXPECT_EQ(*mgr.Get<String>("db.host"),     "prod-db.internal");
}

TEST_F(ConfigurationManagerTest, SchemaValidationPasses)
{
    ConfigurationManager mgr;
    mgr.LoadFile(m_BaseYaml);
    mgr.SetSchema(ConfigSchema{}
        .RequireKey("app.name")
        .RequireType("app.workers", ConfigValue::Type::Integer));
    EXPECT_TRUE(mgr.IsValid());
    EXPECT_TRUE(mgr.Validate().IsValid());
}

TEST_F(ConfigurationManagerTest, SchemaValidationFails)
{
    ConfigurationManager mgr;
    mgr.LoadFile(m_BaseYaml);
    mgr.SetSchema(ConfigSchema{}.RequireKey("does.not.exist"));
    EXPECT_FALSE(mgr.IsValid());
}

TEST_F(ConfigurationManagerTest, ReloadUpdatesConfig)
{
    ConfigurationManager mgr;
    mgr.LoadFile(m_BaseYaml);
    EXPECT_EQ(*mgr.Get<i64>("app.workers"), 2);

    // Mutate the file on disk
    WriteFile(m_BaseYaml,
        "app:\n"
        "  name: MgrApp\n"
        "  workers: 8\n"
        "db:\n"
        "  host: localhost\n");

    EXPECT_TRUE(mgr.Reload());
    EXPECT_EQ(*mgr.Get<i64>("app.workers"), 8);
}

TEST_F(ConfigurationManagerTest, DumpProducesNonEmptyString)
{
    ConfigurationManager mgr;
    mgr.LoadFile(m_BaseYaml);
    EXPECT_FALSE(mgr.Dump().empty());
}

// =============================================================================
// HotReloadListenerTest  (4 tests)
// =============================================================================

class MockHotReloadListener final : public IHotReloadListener
{
public:
    bool  reloaded          = false;
    bool  acceptReload      = true;
    bool  failedCalled      = false;
    bool  rejectedCalled    = false;
    int   callCount         = 0;

    bool OnConfigReloaded(StringView, const Configuration&) override
    {
        reloaded = true;
        ++callCount;
        return acceptReload;
    }
    void OnConfigReloadFailed(StringView, const ValidationResult&) override
    {
        failedCalled = true;
    }
    void OnConfigReloadRejected(StringView) override
    {
        rejectedCalled = true;
    }
};

class HotReloadListenerTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        m_File = TmpPath("hydra_reload_test.yaml");
        WriteFile(m_File, "value: 1\n");
    }
    void TearDown() override { RemoveFile(m_File); }

    std::filesystem::path m_File;
};

TEST_F(HotReloadListenerTest, ListenerCalledOnReload)
{
    ConfigurationManager mgr;
    mgr.LoadFile(m_File);

    MockHotReloadListener listener;
    mgr.AddReloadListener(&listener);

    EXPECT_TRUE(mgr.Reload());
    EXPECT_TRUE(listener.reloaded);
    EXPECT_EQ(listener.callCount, 1);
}

TEST_F(HotReloadListenerTest, ListenerNotCalledAfterRemoval)
{
    ConfigurationManager mgr;
    mgr.LoadFile(m_File);

    MockHotReloadListener listener;
    mgr.AddReloadListener(&listener);
    mgr.RemoveReloadListener(&listener);

    EXPECT_TRUE(mgr.Reload());
    EXPECT_FALSE(listener.reloaded);
}

TEST_F(HotReloadListenerTest, RejectingListenerKeepsOldConfig)
{
    ConfigurationManager mgr;
    mgr.LoadFile(m_File);

    MockHotReloadListener listener;
    listener.acceptReload = false;    // will reject
    mgr.AddReloadListener(&listener);

    // Mutate the file so there is a change to detect
    WriteFile(m_File, "value: 999\n");

    bool ok = mgr.Reload();

    EXPECT_FALSE(ok);
    EXPECT_TRUE(listener.rejectedCalled);
    // Old value still in effect
    EXPECT_EQ(*mgr.Get<i64>("value"), 1);
}

TEST_F(HotReloadListenerTest, SchemaFailureCallsOnReloadFailed)
{
    ConfigurationManager mgr;
    mgr.LoadFile(m_File);
    mgr.SetSchema(ConfigSchema{}.RequireKey("required_but_missing"));

    MockHotReloadListener listener;
    mgr.AddReloadListener(&listener);

    bool ok = mgr.Reload();

    EXPECT_FALSE(ok);
    EXPECT_TRUE(listener.failedCalled);
    EXPECT_FALSE(listener.reloaded);
}
