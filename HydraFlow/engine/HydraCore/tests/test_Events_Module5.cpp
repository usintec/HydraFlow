#include <gtest/gtest.h>

#include <HydraCore/Events/Event.h>
#include <HydraCore/Events/EventListener.h>
#include <HydraCore/Events/Subscription.h>
#include <HydraCore/Events/EventDispatcher.h>
#include <HydraCore/Events/EventQueue.h>
#include <HydraCore/Events/EventBus.h>

#include <vector>
#include <string>
#include <thread>

using namespace Hydra;

// =============================================================================
// Test event types
// =============================================================================

class TestEventA : public EventType<TestEventA>
{
public:
    explicit TestEventA(i32 v = 0) : value(v) {}
    i32 value;
};

class TestEventB : public EventType<TestEventB>
{
public:
    explicit TestEventB(String s = "") : text(std::move(s)) {}
    String text;
};

// =============================================================================
// Event
// =============================================================================

TEST(EventTest, DefaultsUnhandledAndNonStopping)
{
    TestEventA e(5);
    EXPECT_FALSE(e.IsHandled());
    EXPECT_FALSE(e.StopsPropagationWhenHandled());
}

TEST(EventTest, SetHandledToggles)
{
    TestEventA e;
    e.SetHandled(true);
    EXPECT_TRUE(e.IsHandled());
    e.SetHandled(false);
    EXPECT_FALSE(e.IsHandled());
}

TEST(EventTest, StaticTypeIsStableAndDistinctPerType)
{
    EXPECT_EQ(TestEventA::StaticType(), TestEventA::StaticType());
    EXPECT_NE(TestEventA::StaticType(), TestEventB::StaticType());

    TestEventA a;
    TestEventB b;
    EXPECT_EQ(a.GetType(), TestEventA::StaticType());
    EXPECT_EQ(b.GetType(), TestEventB::StaticType());
    EXPECT_NE(a.GetType(), b.GetType());
}

TEST(EventTest, TimestampIsSet)
{
    TestEventA e;
    EXPECT_NE(e.GetTimestamp().time_since_epoch().count(), 0);
}

// =============================================================================
// Subscription
// =============================================================================

TEST(SubscriptionTest, DefaultConstructedIsInvalid)
{
    Subscription s;
    EXPECT_FALSE(s.IsValid());
}

TEST(SubscriptionTest, ActiveSubscriptionCallsUnsubscribeOnDestroy)
{
    bool called = false;
    {
        Subscription s([&called]() { called = true; });
        EXPECT_TRUE(s.IsValid());
    }
    EXPECT_TRUE(called);
}

TEST(SubscriptionTest, ResetInvokesUnsubscribeOnce)
{
    int count = 0;
    Subscription s([&count]() { ++count; });
    s.Reset();
    s.Reset();
    EXPECT_EQ(count, 1);
    EXPECT_FALSE(s.IsValid());
}

TEST(SubscriptionTest, ReleaseDetachesWithoutUnsubscribing)
{
    bool called = false;
    {
        Subscription s([&called]() { called = true; });
        s.Release();
        EXPECT_FALSE(s.IsValid());
    }
    EXPECT_FALSE(called);
}

TEST(SubscriptionTest, MoveConstructTransfersOwnership)
{
    int count = 0;
    Subscription s1([&count]() { ++count; });
    Subscription s2(std::move(s1));
    EXPECT_FALSE(s1.IsValid());
    EXPECT_TRUE(s2.IsValid());
    s2.Reset();
    EXPECT_EQ(count, 1);
}

TEST(SubscriptionTest, MoveAssignUnsubscribesPreviousTarget)
{
    int countA = 0, countB = 0;
    Subscription s1([&countA]() { ++countA; });
    Subscription s2([&countB]() { ++countB; });
    s1 = std::move(s2);
    EXPECT_EQ(countA, 1);
    EXPECT_EQ(countB, 0);
    s1.Reset();
    EXPECT_EQ(countB, 1);
}

// =============================================================================
// EventDispatcher — subscription & synchronous dispatch
// =============================================================================

class EventDispatcherTest : public ::testing::Test
{
protected:
    EventDispatcher dispatcher;
};

TEST_F(EventDispatcherTest, DispatchWithNoListenersReturnsZero)
{
    TestEventA e;
    EXPECT_EQ(dispatcher.Dispatch(e), 0u);
}

TEST_F(EventDispatcherTest, SingleListenerIsInvoked)
{
    int received = -1;
    auto sub = dispatcher.Subscribe<TestEventA>([&received](TestEventA& e) { received = e.value; });

    TestEventA e(42);
    usize invoked = dispatcher.Dispatch(e);

    EXPECT_EQ(invoked, 1u);
    EXPECT_EQ(received, 42);
}

TEST_F(EventDispatcherTest, MultipleListenersAllInvoked)
{
    int count = 0;
    auto s1 = dispatcher.Subscribe<TestEventA>([&count](TestEventA&) { ++count; });
    auto s2 = dispatcher.Subscribe<TestEventA>([&count](TestEventA&) { ++count; });
    auto s3 = dispatcher.Subscribe<TestEventA>([&count](TestEventA&) { ++count; });

    TestEventA e;
    EXPECT_EQ(dispatcher.Dispatch(e), 3u);
    EXPECT_EQ(count, 3);
}

TEST_F(EventDispatcherTest, ListenerOnlyReceivesItsOwnEventType)
{
    bool aCalled = false, bCalled = false;
    auto s1 = dispatcher.Subscribe<TestEventA>([&aCalled](TestEventA&) { aCalled = true; });
    auto s2 = dispatcher.Subscribe<TestEventB>([&bCalled](TestEventB&) { bCalled = true; });

    TestEventA e;
    dispatcher.Dispatch(e);

    EXPECT_TRUE(aCalled);
    EXPECT_FALSE(bCalled);
}

TEST_F(EventDispatcherTest, UnsubscribeViaSubscriptionDestructionStopsDelivery)
{
    int count = 0;
    {
        auto sub = dispatcher.Subscribe<TestEventA>([&count](TestEventA&) { ++count; });
        TestEventA e;
        dispatcher.Dispatch(e);
    }
    EXPECT_EQ(count, 1);

    TestEventA e2;
    dispatcher.Dispatch(e2);
    EXPECT_EQ(count, 1);
}

TEST_F(EventDispatcherTest, PriorityOrderingHighestFirst)
{
    std::vector<int> order;
    auto low  = dispatcher.Subscribe<TestEventA>([&order](TestEventA&) { order.push_back(1); },
                                                  static_cast<i32>(EventPriority::Low));
    auto high = dispatcher.Subscribe<TestEventA>([&order](TestEventA&) { order.push_back(2); },
                                                  static_cast<i32>(EventPriority::High));
    auto norm = dispatcher.Subscribe<TestEventA>([&order](TestEventA&) { order.push_back(3); },
                                                  static_cast<i32>(EventPriority::Normal));

    TestEventA e;
    dispatcher.Dispatch(e);

    ASSERT_EQ(order.size(), 3u);
    EXPECT_EQ(order[0], 2); // High
    EXPECT_EQ(order[1], 3); // Normal
    EXPECT_EQ(order[2], 1); // Low
}

TEST_F(EventDispatcherTest, EqualPriorityPreservesInsertionOrder)
{
    std::vector<int> order;
    auto s1 = dispatcher.Subscribe<TestEventA>([&order](TestEventA&) { order.push_back(1); });
    auto s2 = dispatcher.Subscribe<TestEventA>([&order](TestEventA&) { order.push_back(2); });
    auto s3 = dispatcher.Subscribe<TestEventA>([&order](TestEventA&) { order.push_back(3); });

    TestEventA e;
    dispatcher.Dispatch(e);

    EXPECT_EQ(order, (std::vector<int>{1, 2, 3}));
}

TEST_F(EventDispatcherTest, StopPropagationWhenHandledSkipsLowerPriorityListeners)
{
    std::vector<int> order;
    auto high = dispatcher.Subscribe<TestEventA>(
        [&order](TestEventA& e) { order.push_back(1); e.SetHandled(true); },
        static_cast<i32>(EventPriority::High));
    auto low = dispatcher.Subscribe<TestEventA>([&order](TestEventA&) { order.push_back(2); },
                                                 static_cast<i32>(EventPriority::Low));

    TestEventA e;
    e.SetStopPropagationWhenHandled(true);
    usize invoked = dispatcher.Dispatch(e);

    EXPECT_EQ(invoked, 1u);
    EXPECT_EQ(order, (std::vector<int>{1}));
}

TEST_F(EventDispatcherTest, WithoutStopPropagationAllListenersRunEvenIfHandled)
{
    std::vector<int> order;
    auto high = dispatcher.Subscribe<TestEventA>(
        [&order](TestEventA& e) { order.push_back(1); e.SetHandled(true); },
        static_cast<i32>(EventPriority::High));
    auto low = dispatcher.Subscribe<TestEventA>([&order](TestEventA&) { order.push_back(2); },
                                                 static_cast<i32>(EventPriority::Low));

    TestEventA e; // StopPropagationWhenHandled defaults to false
    usize invoked = dispatcher.Dispatch(e);

    EXPECT_EQ(invoked, 2u);
    EXPECT_EQ(order, (std::vector<int>{1, 2}));
}

TEST_F(EventDispatcherTest, GetListenerCountReflectsSubscriptions)
{
    EXPECT_EQ(dispatcher.GetListenerCount(TestEventA::StaticType()), 0u);
    auto s1 = dispatcher.Subscribe<TestEventA>([](TestEventA&) {});
    auto s2 = dispatcher.Subscribe<TestEventA>([](TestEventA&) {});
    EXPECT_EQ(dispatcher.GetListenerCount(TestEventA::StaticType()), 2u);
}

TEST_F(EventDispatcherTest, GetTotalListenerCountSumsAcrossTypes)
{
    auto s1 = dispatcher.Subscribe<TestEventA>([](TestEventA&) {});
    auto s2 = dispatcher.Subscribe<TestEventB>([](TestEventB&) {});
    EXPECT_EQ(dispatcher.GetTotalListenerCount(), 2u);
}

TEST_F(EventDispatcherTest, UnsubscribeAllRemovesEveryListenerForType)
{
    auto s1 = dispatcher.Subscribe<TestEventA>([](TestEventA&) {});
    auto s2 = dispatcher.Subscribe<TestEventA>([](TestEventA&) {});
    dispatcher.UnsubscribeAll(TestEventA::StaticType());
    EXPECT_EQ(dispatcher.GetListenerCount(TestEventA::StaticType()), 0u);
}

TEST_F(EventDispatcherTest, ClearRemovesAllListenersAcrossTypes)
{
    auto s1 = dispatcher.Subscribe<TestEventA>([](TestEventA&) {});
    auto s2 = dispatcher.Subscribe<TestEventB>([](TestEventB&) {});
    dispatcher.Clear();
    EXPECT_EQ(dispatcher.GetTotalListenerCount(), 0u);
}

TEST_F(EventDispatcherTest, ListenerCanUnsubscribeItselfDuringDispatchWithoutDeadlock)
{
    int count = 0;
    Subscription selfSub;
    selfSub = dispatcher.Subscribe<TestEventA>([&](TestEventA&)
    {
        ++count;
        selfSub.Reset();
    });

    TestEventA e1;
    dispatcher.Dispatch(e1);
    EXPECT_EQ(count, 1);

    TestEventA e2;
    dispatcher.Dispatch(e2);
    EXPECT_EQ(count, 1); // no longer subscribed
}

// =============================================================================
// EventQueue — asynchronous delivery
// =============================================================================

class EventQueueTest : public ::testing::Test
{
protected:
    EventDispatcher dispatcher;
    EventQueue      queue;
};

TEST_F(EventQueueTest, StartsEmpty)
{
    EXPECT_EQ(queue.GetPendingCount(), 0u);
}

TEST_F(EventQueueTest, EnqueueIncreasesPendingCount)
{
    queue.Enqueue(MakeUnique<TestEventA>(1));
    EXPECT_EQ(queue.GetPendingCount(), 1u);
}

TEST_F(EventQueueTest, EmplaceConstructsAndEnqueues)
{
    queue.Emplace<TestEventA>(7);
    EXPECT_EQ(queue.GetPendingCount(), 1u);
}

TEST_F(EventQueueTest, EnqueueNullptrIsNoOp)
{
    queue.Enqueue(nullptr);
    EXPECT_EQ(queue.GetPendingCount(), 0u);
}

TEST_F(EventQueueTest, ProcessAllDispatchesAndDrains)
{
    int total = 0;
    auto sub = dispatcher.Subscribe<TestEventA>([&total](TestEventA& e) { total += e.value; });

    queue.Emplace<TestEventA>(3);
    queue.Emplace<TestEventA>(4);
    queue.Emplace<TestEventA>(5);

    usize processed = queue.ProcessAll(dispatcher);

    EXPECT_EQ(processed, 3u);
    EXPECT_EQ(total, 12);
    EXPECT_EQ(queue.GetPendingCount(), 0u);
}

TEST_F(EventQueueTest, ProcessAllPreservesFifoOrder)
{
    std::vector<int> order;
    auto sub = dispatcher.Subscribe<TestEventA>([&order](TestEventA& e) { order.push_back(e.value); });

    queue.Emplace<TestEventA>(1);
    queue.Emplace<TestEventA>(2);
    queue.Emplace<TestEventA>(3);
    queue.ProcessAll(dispatcher);

    EXPECT_EQ(order, (std::vector<int>{1, 2, 3}));
}

TEST_F(EventQueueTest, ProcessUpToLimitsCount)
{
    int total = 0;
    auto sub = dispatcher.Subscribe<TestEventA>([&total](TestEventA&) { ++total; });

    queue.Emplace<TestEventA>(1);
    queue.Emplace<TestEventA>(2);
    queue.Emplace<TestEventA>(3);

    usize processed = queue.ProcessUpTo(dispatcher, 2);

    EXPECT_EQ(processed, 2u);
    EXPECT_EQ(total, 2);
    EXPECT_EQ(queue.GetPendingCount(), 1u);
}

TEST_F(EventQueueTest, EventsEnqueuedDuringProcessAreDeferred)
{
    int count = 0;
    auto sub = dispatcher.Subscribe<TestEventA>([&](TestEventA&)
    {
        ++count;
        if (count == 1)
        {
            queue.Emplace<TestEventA>(99); // enqueued mid-processing
        }
    });

    queue.Emplace<TestEventA>(1);
    usize processed = queue.ProcessAll(dispatcher);

    EXPECT_EQ(processed, 1u);
    EXPECT_EQ(queue.GetPendingCount(), 1u); // deferred to next call

    usize processed2 = queue.ProcessAll(dispatcher);
    EXPECT_EQ(processed2, 1u);
    EXPECT_EQ(count, 2);
}

TEST_F(EventQueueTest, ClearDiscardsPendingWithoutDispatch)
{
    bool called = false;
    auto sub = dispatcher.Subscribe<TestEventA>([&called](TestEventA&) { called = true; });

    queue.Emplace<TestEventA>(1);
    queue.Clear();

    EXPECT_EQ(queue.GetPendingCount(), 0u);
    queue.ProcessAll(dispatcher);
    EXPECT_FALSE(called);
}

TEST_F(EventQueueTest, ConcurrentEnqueueIsThreadSafe)
{
    constexpr int kThreads       = 4;
    constexpr int kPerThread     = 200;
    std::vector<std::thread> workers;

    for (int t = 0; t < kThreads; ++t)
    {
        workers.emplace_back([this]()
        {
            for (int i = 0; i < kPerThread; ++i)
            {
                queue.Emplace<TestEventA>(i);
            }
        });
    }
    for (auto& w : workers) w.join();

    EXPECT_EQ(queue.GetPendingCount(), static_cast<usize>(kThreads * kPerThread));
}

// =============================================================================
// EventBus — combined façade
// =============================================================================

class EventBusTest : public ::testing::Test
{
protected:
    EventBus bus;
};

TEST_F(EventBusTest, PublishDispatchesSynchronously)
{
    int received = -1;
    auto sub = bus.Subscribe<TestEventA>([&received](TestEventA& e) { received = e.value; });

    TestEventA e(10);
    usize invoked = bus.Publish(e);

    EXPECT_EQ(invoked, 1u);
    EXPECT_EQ(received, 10);
}

TEST_F(EventBusTest, EmitConstructsAndDispatchesSynchronously)
{
    String received;
    auto sub = bus.Subscribe<TestEventB>([&received](TestEventB& e) { received = e.text; });

    usize invoked = bus.Emit<TestEventB>("hello");

    EXPECT_EQ(invoked, 1u);
    EXPECT_EQ(received, "hello");
}

TEST_F(EventBusTest, PublishAsyncDoesNotDispatchUntilProcessQueue)
{
    bool called = false;
    auto sub = bus.Subscribe<TestEventA>([&called](TestEventA&) { called = true; });

    bus.PublishAsync(MakeUnique<TestEventA>(1));
    EXPECT_FALSE(called);
    EXPECT_EQ(bus.GetPendingCount(), 1u);

    usize processed = bus.ProcessQueue();
    EXPECT_EQ(processed, 1u);
    EXPECT_TRUE(called);
}

TEST_F(EventBusTest, EmitAsyncQueuesConstructedEvent)
{
    int received = -1;
    auto sub = bus.Subscribe<TestEventA>([&received](TestEventA& e) { received = e.value; });

    bus.EmitAsync<TestEventA>(77);
    EXPECT_EQ(bus.GetPendingCount(), 1u);

    bus.ProcessQueue();
    EXPECT_EQ(received, 77);
}

TEST_F(EventBusTest, ProcessQueueRespectsMaxEvents)
{
    int count = 0;
    auto sub = bus.Subscribe<TestEventA>([&count](TestEventA&) { ++count; });

    bus.EmitAsync<TestEventA>(1);
    bus.EmitAsync<TestEventA>(2);
    bus.EmitAsync<TestEventA>(3);

    usize processed = bus.ProcessQueue(1);
    EXPECT_EQ(processed, 1u);
    EXPECT_EQ(bus.GetPendingCount(), 2u);
}

TEST_F(EventBusTest, GetListenerCountReflectsSubscribe)
{
    EXPECT_EQ(bus.GetListenerCount(TestEventA::StaticType()), 0u);
    auto s1 = bus.Subscribe<TestEventA>([](TestEventA&) {});
    auto s2 = bus.Subscribe<TestEventA>([](TestEventA&) {});
    EXPECT_EQ(bus.GetListenerCount(TestEventA::StaticType()), 2u);
}

TEST_F(EventBusTest, UnsubscribeAllRemovesListenersForType)
{
    auto s1 = bus.Subscribe<TestEventA>([](TestEventA&) {});
    bus.UnsubscribeAll(TestEventA::StaticType());
    EXPECT_EQ(bus.GetListenerCount(TestEventA::StaticType()), 0u);
}

TEST_F(EventBusTest, ClearListenersRemovesEverything)
{
    auto s1 = bus.Subscribe<TestEventA>([](TestEventA&) {});
    auto s2 = bus.Subscribe<TestEventB>([](TestEventB&) {});
    bus.ClearListeners();
    EXPECT_EQ(bus.GetListenerCount(TestEventA::StaticType()), 0u);
    EXPECT_EQ(bus.GetListenerCount(TestEventB::StaticType()), 0u);
}

TEST_F(EventBusTest, ClearQueueDiscardsPendingAsyncEvents)
{
    bool called = false;
    auto sub = bus.Subscribe<TestEventA>([&called](TestEventA&) { called = true; });

    bus.EmitAsync<TestEventA>(1);
    bus.ClearQueue();
    EXPECT_EQ(bus.GetPendingCount(), 0u);

    bus.ProcessQueue();
    EXPECT_FALSE(called);
}

TEST_F(EventBusTest, SyncAndAsyncListenersShareSameDispatcher)
{
    int syncCount = 0, asyncCount = 0;
    auto s1 = bus.Subscribe<TestEventA>([&syncCount](TestEventA&) { ++syncCount; },
                                         static_cast<i32>(EventPriority::High));
    auto s2 = bus.Subscribe<TestEventA>([&asyncCount](TestEventA&) { ++asyncCount; },
                                         static_cast<i32>(EventPriority::Low));

    TestEventA e;
    bus.Publish(e);
    EXPECT_EQ(syncCount, 1);
    EXPECT_EQ(asyncCount, 1);

    bus.EmitAsync<TestEventA>(1);
    bus.ProcessQueue();
    EXPECT_EQ(syncCount, 2);
    EXPECT_EQ(asyncCount, 2);
}
