#include <gtest/gtest.h>
#include <HydraCore/Application/EngineContext.h>
#include <HydraCore/Config/ConfigManager.h>

using namespace Hydra;

struct FakeService
{
    int value = 42;
};

struct AnotherService
{
    std::string label = "hello";
};

TEST(EngineContextTest, RegisterAndRetrieve)
{
    EngineContext ctx;
    FakeService   svc;

    ctx.RegisterService<FakeService>(&svc);
    ASSERT_TRUE(ctx.HasService<FakeService>());

    FakeService* retrieved = ctx.GetService<FakeService>();
    ASSERT_NE(retrieved, nullptr);
    EXPECT_EQ(retrieved->value, 42);
}

TEST(EngineContextTest, GetServiceReturnsNullForMissing)
{
    EngineContext ctx;
    EXPECT_EQ(ctx.GetService<FakeService>(), nullptr);
    EXPECT_FALSE(ctx.HasService<FakeService>());
}

TEST(EngineContextTest, UnregisterRemovesService)
{
    EngineContext ctx;
    FakeService   svc;
    ctx.RegisterService<FakeService>(&svc);
    ctx.UnregisterService<FakeService>();
    EXPECT_FALSE(ctx.HasService<FakeService>());
    EXPECT_EQ(ctx.GetService<FakeService>(), nullptr);
}

TEST(EngineContextTest, MultipleServicesCoexist)
{
    EngineContext  ctx;
    FakeService    a;
    AnotherService b;

    ctx.RegisterService<FakeService>(&a);
    ctx.RegisterService<AnotherService>(&b);

    EXPECT_NE(ctx.GetService<FakeService>(),    nullptr);
    EXPECT_NE(ctx.GetService<AnotherService>(), nullptr);
}

TEST(EngineContextTest, OverwriteServiceUpdatesPointer)
{
    EngineContext ctx;
    FakeService   a{10};
    FakeService   b{99};

    ctx.RegisterService<FakeService>(&a);
    ctx.RegisterService<FakeService>(&b); // overwrite

    EXPECT_EQ(ctx.GetService<FakeService>()->value, 99);
}

TEST(EngineContextTest, RequireServiceThrowsIfMissing)
{
    EngineContext ctx;
    EXPECT_DEATH(ctx.RequireService<FakeService>(), ".*");
}
