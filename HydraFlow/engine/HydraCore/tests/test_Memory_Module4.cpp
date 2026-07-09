#include <gtest/gtest.h>

#include <HydraCore/Memory/Alignment.h>
#include <HydraCore/Memory/LinearAllocator.h>
#include <HydraCore/Memory/StackAllocator.h>
#include <HydraCore/Memory/PoolAllocator.h>
#include <HydraCore/Memory/FreeListAllocator.h>
#include <HydraCore/Memory/ArenaAllocator.h>
#include <HydraCore/Memory/MemoryTracker.h>
#include <HydraCore/Memory/MemoryStatistics.h>
#include <HydraCore/Memory/MemoryManager.h>
#include <HydraCore/Memory/MemoryMacros.h>
#include <HydraCore/Memory/IMemoryProfilingHook.h>
#include <HydraCore/Memory/ILeakDetectionHook.h>

#include <cstring>
#include <thread>
#include <vector>

using namespace Hydra;
using namespace Hydra::Memory;

// =============================================================================
// AlignmentTest  (6 tests)
// =============================================================================

TEST(AlignmentTest, IsPowerOfTwo)
{
    EXPECT_TRUE(IsPowerOfTwo(1));
    EXPECT_TRUE(IsPowerOfTwo(2));
    EXPECT_TRUE(IsPowerOfTwo(4));
    EXPECT_TRUE(IsPowerOfTwo(64));
    EXPECT_FALSE(IsPowerOfTwo(0));
    EXPECT_FALSE(IsPowerOfTwo(3));
    EXPECT_FALSE(IsPowerOfTwo(7));
}

TEST(AlignmentTest, AlignUp)
{
    EXPECT_EQ(AlignUp(0u,  4u), 0u);
    EXPECT_EQ(AlignUp(1u,  4u), 4u);
    EXPECT_EQ(AlignUp(4u,  4u), 4u);
    EXPECT_EQ(AlignUp(5u,  4u), 8u);
    EXPECT_EQ(AlignUp(7u,  8u), 8u);
    EXPECT_EQ(AlignUp(8u,  8u), 8u);
    EXPECT_EQ(AlignUp(9u,  8u), 16u);
    EXPECT_EQ(AlignUp(15u, 16u),16u);
}

TEST(AlignmentTest, AlignDown)
{
    EXPECT_EQ(AlignDown(0u,  4u), 0u);
    EXPECT_EQ(AlignDown(7u,  4u), 4u);
    EXPECT_EQ(AlignDown(8u,  4u), 8u);
    EXPECT_EQ(AlignDown(9u,  8u), 8u);
    EXPECT_EQ(AlignDown(16u, 8u), 16u);
}

TEST(AlignmentTest, IsAligned)
{
    EXPECT_TRUE (IsAligned(0u,  8u));
    EXPECT_TRUE (IsAligned(8u,  8u));
    EXPECT_FALSE(IsAligned(7u,  8u));
    EXPECT_TRUE (IsAligned(16u, 16u));
    EXPECT_FALSE(IsAligned(17u, 16u));
}

TEST(AlignmentTest, AlignmentPadding)
{
    EXPECT_EQ(AlignmentPadding(0u,  8u), 0u);
    EXPECT_EQ(AlignmentPadding(1u,  8u), 7u);
    EXPECT_EQ(AlignmentPadding(8u,  8u), 0u);
    EXPECT_EQ(AlignmentPadding(9u,  8u), 7u);
    EXPECT_EQ(AlignmentPadding(13u, 4u), 3u);
}

TEST(AlignmentTest, AlignUpPtrRoundTrip)
{
    alignas(16) char buf[64];
    void* p  = buf + 1;               // deliberately misaligned
    void* aligned = AlignUpPtr(p, 16);
    EXPECT_EQ(reinterpret_cast<usize>(aligned) % 16, 0u);
    EXPECT_GE(aligned, p);
}

// =============================================================================
// LinearAllocatorTest  (8 tests)
// =============================================================================

class LinearAllocatorTest : public ::testing::Test
{
protected:
    static constexpr usize kCapacity = 1024;
    LinearAllocator alloc{ kCapacity, "TestLinear" };
};

TEST_F(LinearAllocatorTest, InitialState)
{
    EXPECT_EQ(alloc.GetUsedBytes(),     0u);
    EXPECT_EQ(alloc.GetCapacityBytes(), kCapacity);
    EXPECT_EQ(alloc.GetAllocationCount(), 0u);
    EXPECT_EQ(alloc.GetFreeBytes(),     kCapacity);
    EXPECT_EQ(alloc.GetName(),          "TestLinear");
}

TEST_F(LinearAllocatorTest, AllocateReturnsNonNull)
{
    void* p = alloc.Allocate(64, 8);
    EXPECT_NE(p, nullptr);
    EXPECT_EQ(alloc.GetAllocationCount(), 1u);
}

TEST_F(LinearAllocatorTest, AllocatedPtrIsAligned)
{
    void* p4  = alloc.Allocate(4, 4);
    void* p16 = alloc.Allocate(1, 16);
    EXPECT_EQ(reinterpret_cast<usize>(p4)  % 4,  0u);
    EXPECT_EQ(reinterpret_cast<usize>(p16) % 16, 0u);
}

TEST_F(LinearAllocatorTest, SequentialAllocationsDoNotOverlap)
{
    void* a = alloc.Allocate(100, 1);
    void* b = alloc.Allocate(100, 1);
    EXPECT_NE(a, nullptr);
    EXPECT_NE(b, nullptr);
    EXPECT_GE(static_cast<byte*>(b), static_cast<byte*>(a) + 100);
}

TEST_F(LinearAllocatorTest, ReturnsNullWhenExhausted)
{
    void* p = alloc.Allocate(kCapacity + 1, 1);
    EXPECT_EQ(p, nullptr);
}

TEST_F(LinearAllocatorTest, DeallocateIsNoOp)
{
    void* p = alloc.Allocate(64, 8);
    usize used = alloc.GetUsedBytes();
    alloc.Deallocate(p);
    EXPECT_EQ(alloc.GetUsedBytes(), used);   // unchanged
}

TEST_F(LinearAllocatorTest, ResetReclaims)
{
    (void)alloc.Allocate(512, 8);
    alloc.Reset();
    EXPECT_EQ(alloc.GetUsedBytes(),      0u);
    EXPECT_EQ(alloc.GetAllocationCount(), 0u);
    void* p = alloc.Allocate(512, 8);
    EXPECT_NE(p, nullptr);
}

TEST_F(LinearAllocatorTest, WritabilityOfAllocatedMemory)
{
    void* p = alloc.Allocate(128, 8);
    ASSERT_NE(p, nullptr);
    std::memset(p, 0xAB, 128);
    EXPECT_EQ(static_cast<unsigned char*>(p)[0],   0xAB);
    EXPECT_EQ(static_cast<unsigned char*>(p)[127], 0xAB);
}

// =============================================================================
// StackAllocatorTest  (8 tests)
// =============================================================================

class StackAllocatorTest : public ::testing::Test
{
protected:
    static constexpr usize kCapacity = 2048;
    StackAllocator alloc{ kCapacity, "TestStack" };
};

TEST_F(StackAllocatorTest, InitialState)
{
    EXPECT_EQ(alloc.GetUsedBytes(),       0u);
    EXPECT_EQ(alloc.GetCapacityBytes(),   kCapacity);
    EXPECT_EQ(alloc.GetAllocationCount(), 0u);
}

TEST_F(StackAllocatorTest, AllocateAndDeallocateLIFO)
{
    void* a = alloc.Allocate(64, 8);
    void* b = alloc.Allocate(64, 8);
    EXPECT_NE(a, nullptr);
    EXPECT_NE(b, nullptr);

    alloc.Deallocate(b);
    EXPECT_EQ(alloc.GetAllocationCount(), 1u);
    alloc.Deallocate(a);
    EXPECT_EQ(alloc.GetAllocationCount(), 0u);
    EXPECT_EQ(alloc.GetUsedBytes(), 0u);
}

TEST_F(StackAllocatorTest, AllocatedPtrsAreAligned)
{
    void* p1 = alloc.Allocate(1,  16);
    void* p2 = alloc.Allocate(1,  32);
    void* p3 = alloc.Allocate(1,   4);
    EXPECT_EQ(reinterpret_cast<usize>(p1) % 16, 0u);
    EXPECT_EQ(reinterpret_cast<usize>(p2) % 32, 0u);
    EXPECT_EQ(reinterpret_cast<usize>(p3) %  4, 0u);
}

TEST_F(StackAllocatorTest, MarkerRollback)
{
    void* a = alloc.Allocate(64, 8);
    StackMarker m = alloc.GetMarker();
    (void)alloc.Allocate(64, 8);
    (void)alloc.Allocate(64, 8);
    alloc.FreeToMarker(m);
    EXPECT_EQ(alloc.GetAllocationCount(), 1u);
    // Can re-allocate from the rolled-back position
    void* b = alloc.Allocate(64, 8);
    EXPECT_NE(b, nullptr);
    (void)a;
}

TEST_F(StackAllocatorTest, ResetClearsAll)
{
    (void)alloc.Allocate(128, 8);
    (void)alloc.Allocate(128, 8);
    alloc.Reset();
    EXPECT_EQ(alloc.GetUsedBytes(),       0u);
    EXPECT_EQ(alloc.GetAllocationCount(), 0u);
}

TEST_F(StackAllocatorTest, OOMReturnsNull)
{
    EXPECT_EQ(alloc.Allocate(kCapacity + 1, 1), nullptr);
}

TEST_F(StackAllocatorTest, WritabilityAfterDeallocAndRealloc)
{
    void* p = alloc.Allocate(64, 8);
    std::memset(p, 0x55, 64);
    alloc.Deallocate(p);
    void* q = alloc.Allocate(64, 8);
    std::memset(q, 0xAA, 64);
    EXPECT_EQ(static_cast<unsigned char*>(q)[0], 0xAA);
}

TEST_F(StackAllocatorTest, FreeByteDecreaseAfterDealloc)
{
    usize before = alloc.GetFreeBytes();
    void* p = alloc.Allocate(128, 8);
    EXPECT_LT(alloc.GetFreeBytes(), before);
    alloc.Deallocate(p);
    EXPECT_EQ(alloc.GetFreeBytes(), before);
}

// =============================================================================
// PoolAllocatorTest  (8 tests)
// =============================================================================

class PoolAllocatorTest : public ::testing::Test
{
protected:
    // Pool of 16 blocks, each 32 bytes, aligned to 8
    PoolAllocator alloc{ 32, 16, 8, "TestPool" };
};

TEST_F(PoolAllocatorTest, InitialState)
{
    EXPECT_EQ(alloc.GetBlockSize(),      32u);
    EXPECT_EQ(alloc.GetBlockCount(),     16u);
    EXPECT_EQ(alloc.GetFreeBlockCount(), 16u);
    EXPECT_EQ(alloc.GetUsedBlockCount(), 0u);
    EXPECT_EQ(alloc.GetAllocationCount(), 0u);
}

TEST_F(PoolAllocatorTest, AllocateReducesFreeCount)
{
    void* p = alloc.Allocate();
    EXPECT_NE(p, nullptr);
    EXPECT_EQ(alloc.GetFreeBlockCount(), 15u);
    EXPECT_EQ(alloc.GetUsedBlockCount(), 1u);
}

TEST_F(PoolAllocatorTest, DeallocateRestoresFreeCount)
{
    void* p = alloc.Allocate();
    alloc.Deallocate(p);
    EXPECT_EQ(alloc.GetFreeBlockCount(), 16u);
    EXPECT_EQ(alloc.GetUsedBlockCount(), 0u);
}

TEST_F(PoolAllocatorTest, ExhaustionReturnsNull)
{
    void* blocks[16];
    for (auto& b : blocks) b = alloc.Allocate();
    EXPECT_EQ(alloc.Allocate(), nullptr);
    for (auto* b : blocks) alloc.Deallocate(b);
}

TEST_F(PoolAllocatorTest, ResetRestoresAllBlocks)
{
    for (int i = 0; i < 8; ++i) alloc.Allocate();
    alloc.Reset();
    EXPECT_EQ(alloc.GetFreeBlockCount(), 16u);
    EXPECT_EQ(alloc.GetAllocationCount(), 0u);
}

TEST_F(PoolAllocatorTest, WritabilityOfBlock)
{
    void* p = alloc.Allocate();
    ASSERT_NE(p, nullptr);
    std::memset(p, 0xCC, 32);
    EXPECT_EQ(static_cast<unsigned char*>(p)[0],  0xCC);
    EXPECT_EQ(static_cast<unsigned char*>(p)[31], 0xCC);
    alloc.Deallocate(p);
}

TEST_F(PoolAllocatorTest, ReuseAfterFree)
{
    void* a = alloc.Allocate();
    alloc.Deallocate(a);
    void* b = alloc.Allocate();
    EXPECT_NE(b, nullptr);
    alloc.Deallocate(b);
}

TEST_F(PoolAllocatorTest, CapacityAndUsed)
{
    (void)alloc.Allocate();
    (void)alloc.Allocate();
    EXPECT_EQ(alloc.GetCapacityBytes(), 32u * 16u);
    EXPECT_EQ(alloc.GetUsedBytes(),     32u * 2u);
}

// =============================================================================
// FreeListAllocatorTest  (10 tests)
// =============================================================================

class FreeListAllocatorTest : public ::testing::Test
{
protected:
    static constexpr usize kCapacity = 4096;
    FreeListAllocator alloc{ kCapacity, "TestFreeList" };
};

TEST_F(FreeListAllocatorTest, InitialState)
{
    EXPECT_EQ(alloc.GetUsedBytes(),       0u);
    EXPECT_EQ(alloc.GetCapacityBytes(),   kCapacity);
    EXPECT_EQ(alloc.GetAllocationCount(), 0u);
}

TEST_F(FreeListAllocatorTest, AllocateReturnsNonNull)
{
    void* p = alloc.Allocate(64, 8);
    EXPECT_NE(p, nullptr);
    EXPECT_EQ(alloc.GetAllocationCount(), 1u);
}

TEST_F(FreeListAllocatorTest, AllocatedPtrsAreAligned)
{
    void* p8  = alloc.Allocate(1,  8);
    void* p16 = alloc.Allocate(1, 16);
    void* p32 = alloc.Allocate(1, 32);
    EXPECT_EQ(reinterpret_cast<usize>(p8)  %  8, 0u);
    EXPECT_EQ(reinterpret_cast<usize>(p16) % 16, 0u);
    EXPECT_EQ(reinterpret_cast<usize>(p32) % 32, 0u);
}

TEST_F(FreeListAllocatorTest, DeallocateAndReallocate)
{
    void* p = alloc.Allocate(128, 8);
    ASSERT_NE(p, nullptr);
    alloc.Deallocate(p);
    EXPECT_EQ(alloc.GetAllocationCount(), 0u);

    void* q = alloc.Allocate(128, 8);
    EXPECT_NE(q, nullptr);
    alloc.Deallocate(q);
}

TEST_F(FreeListAllocatorTest, MultipleAllocFreeRecyclesMemory)
{
    void* a = alloc.Allocate(64, 8);
    void* b = alloc.Allocate(64, 8);
    void* c = alloc.Allocate(64, 8);
    EXPECT_NE(a, nullptr);
    EXPECT_NE(b, nullptr);
    EXPECT_NE(c, nullptr);
    alloc.Deallocate(b);
    alloc.Deallocate(a);
    alloc.Deallocate(c);
    EXPECT_EQ(alloc.GetAllocationCount(), 0u);
}

TEST_F(FreeListAllocatorTest, CoalescesAfterFree)
{
    void* a = alloc.Allocate(256, 8);
    void* b = alloc.Allocate(256, 8);
    alloc.Deallocate(a);
    alloc.Deallocate(b);
    // After coalescing, we should be able to allocate a large block
    void* big = alloc.Allocate(512, 8);
    EXPECT_NE(big, nullptr);
    alloc.Deallocate(big);
}

TEST_F(FreeListAllocatorTest, ReturnsNullWhenExhausted)
{
    void* big = alloc.Allocate(kCapacity, 1);
    EXPECT_EQ(big, nullptr);   // too big even for the full capacity (header overhead)
}

TEST_F(FreeListAllocatorTest, WritabilityAndNonOverlap)
{
    void* a = alloc.Allocate(64, 8);
    void* b = alloc.Allocate(64, 8);
    ASSERT_NE(a, nullptr);
    ASSERT_NE(b, nullptr);
    std::memset(a, 0xAA, 64);
    std::memset(b, 0xBB, 64);
    EXPECT_EQ(static_cast<unsigned char*>(a)[0], 0xAA);
    EXPECT_EQ(static_cast<unsigned char*>(b)[0], 0xBB);
    alloc.Deallocate(a);
    alloc.Deallocate(b);
}

TEST_F(FreeListAllocatorTest, ResetReturnsFullCapacity)
{
    (void)alloc.Allocate(128, 8);
    (void)alloc.Allocate(128, 8);
    alloc.Reset();
    EXPECT_EQ(alloc.GetUsedBytes(),       0u);
    EXPECT_EQ(alloc.GetAllocationCount(), 0u);
    void* big = alloc.Allocate(1024, 8);
    EXPECT_NE(big, nullptr);
    alloc.Deallocate(big);
}

TEST_F(FreeListAllocatorTest, GetFreeBytes)
{
    const usize initFree = alloc.GetFreeBytes();
    void* p = alloc.Allocate(64, 8);
    EXPECT_LT(alloc.GetFreeBytes(), initFree);
    alloc.Deallocate(p);
}

// =============================================================================
// ArenaAllocatorTest  (8 tests)
// =============================================================================

class ArenaAllocatorTest : public ::testing::Test
{
protected:
    static constexpr usize kPageSize = 1024;
    ArenaAllocator alloc{ kPageSize, "TestArena" };
};

TEST_F(ArenaAllocatorTest, InitialState)
{
    EXPECT_EQ(alloc.GetPageSize(),        kPageSize);
    EXPECT_GE(alloc.GetPageCount(),       1u);
    EXPECT_EQ(alloc.GetAllocationCount(), 0u);
    EXPECT_GE(alloc.GetCapacityBytes(),   kPageSize);
}

TEST_F(ArenaAllocatorTest, AllocateReturnsNonNull)
{
    void* p = alloc.Allocate(64, 8);
    EXPECT_NE(p, nullptr);
    EXPECT_EQ(alloc.GetAllocationCount(), 1u);
}

TEST_F(ArenaAllocatorTest, AllocatedPtrIsAligned)
{
    void* p = alloc.Allocate(1, 32);
    EXPECT_EQ(reinterpret_cast<usize>(p) % 32, 0u);
}

TEST_F(ArenaAllocatorTest, GrowsAcrossPages)
{
    // Allocate enough to exhaust the first page and trigger a new page.
    usize count = 0;
    for (usize i = 0; i < kPageSize / 16 + 4; ++i) {
        void* p = alloc.Allocate(16, 8);
        if (p) ++count;
    }
    EXPECT_GT(alloc.GetPageCount(), 1u);
    EXPECT_GT(count, 0u);
}

TEST_F(ArenaAllocatorTest, DeallocateIsNoOp)
{
    void* p = alloc.Allocate(64, 8);
    usize used = alloc.GetUsedBytes();
    alloc.Deallocate(p);
    EXPECT_EQ(alloc.GetUsedBytes(), used);
}

TEST_F(ArenaAllocatorTest, ResetReusesPagesWithoutOSAlloc)
{
    for (usize i = 0; i < kPageSize / 8 + 4; ++i)
        (void)alloc.Allocate(8, 8);
    usize pageCntBefore = alloc.GetPageCount();
    alloc.Reset();
    EXPECT_EQ(alloc.GetUsedBytes(),       0u);
    EXPECT_EQ(alloc.GetAllocationCount(), 0u);
    EXPECT_EQ(alloc.GetPageCount(),       pageCntBefore);   // pages NOT released
}

TEST_F(ArenaAllocatorTest, ReleasePagesFreesMemory)
{
    (void)alloc.Allocate(512, 8);
    alloc.ReleasePages();
    EXPECT_EQ(alloc.GetPageCount(), 0u);
    EXPECT_EQ(alloc.GetUsedBytes(), 0u);
}

TEST_F(ArenaAllocatorTest, WritabilityOfAllocatedMemory)
{
    void* p = alloc.Allocate(64, 8);
    ASSERT_NE(p, nullptr);
    std::memset(p, 0x77, 64);
    EXPECT_EQ(static_cast<unsigned char*>(p)[0], 0x77);
}

// =============================================================================
// MemoryTrackerTest  (10 tests)
// =============================================================================

class MemoryTrackerTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        tracker.ResetStatistics();
    }

    MemoryTracker tracker;
};

TEST_F(MemoryTrackerTest, TrackIncreasesActiveCount)
{
    char buf[64];
    tracker.Track(buf, 64, 8, __FILE__, __LINE__, __func__, "test");
    EXPECT_EQ(tracker.ActiveAllocCount(), 1u);
    EXPECT_EQ(tracker.GetStatistics().totalAllocated, 64u);
    tracker.Untrack(buf);
}

TEST_F(MemoryTrackerTest, UntrackDecreases)
{
    char buf[64];
    tracker.Track(buf, 64, 8, nullptr, 0, nullptr, "test");
    tracker.Untrack(buf);
    EXPECT_EQ(tracker.ActiveAllocCount(), 0u);
    EXPECT_FALSE(tracker.HasLeaks());
}

TEST_F(MemoryTrackerTest, HasLeaksWhenAllocNotFreed)
{
    char buf[32];
    tracker.Track(buf, 32, 8, nullptr, 0, nullptr, "test");
    EXPECT_TRUE(tracker.HasLeaks());
    tracker.Untrack(buf);
}

TEST_F(MemoryTrackerTest, PeakUsageTracked)
{
    char a[100], b[200];
    tracker.Track(a, 100, 8, nullptr, 0, nullptr, {});
    tracker.Track(b, 200, 8, nullptr, 0, nullptr, {});
    EXPECT_EQ(tracker.GetStatistics().peakUsage, 300u);
    tracker.Untrack(a);
    tracker.Untrack(b);
    EXPECT_EQ(tracker.GetStatistics().peakUsage, 300u);   // peak stays
}

TEST_F(MemoryTrackerTest, StatisticsAfterResetAreZero)
{
    char buf[64];
    tracker.Track(buf, 64, 8, nullptr, 0, nullptr, {});
    tracker.Untrack(buf);
    tracker.ResetStatistics();
    const auto& s = tracker.GetStatistics();
    EXPECT_EQ(s.totalAllocated, 0u);
    EXPECT_EQ(s.currentUsage,   0u);
    EXPECT_EQ(s.peakUsage,      0u);
}

TEST_F(MemoryTrackerTest, GetActiveAllocations)
{
    char a[32], b[64];
    tracker.Track(a, 32, 8, nullptr, 0, nullptr, "A");
    tracker.Track(b, 64, 8, nullptr, 0, nullptr, "B");
    auto recs = tracker.GetActiveAllocations();
    EXPECT_EQ(recs.size(), 2u);
    tracker.Untrack(a);
    tracker.Untrack(b);
}

TEST_F(MemoryTrackerTest, DisabledTrackerDoesNotRecord)
{
    tracker.SetEnabled(false);
    char buf[64];
    tracker.Track(buf, 64, 8, nullptr, 0, nullptr, {});
    EXPECT_EQ(tracker.ActiveAllocCount(), 0u);
    tracker.SetEnabled(true);
}

TEST_F(MemoryTrackerTest, ProfilingHookCalledOnAllocate)
{
    struct MockHook : IMemoryProfilingHook {
        int allocCalls = 0;
        int deallocCalls = 0;
        void OnAllocate(const AllocationRecord&) override { ++allocCalls; }
        void OnDeallocate(void*, usize, StringView) override { ++deallocCalls; }
    } hook;

    tracker.AddProfilingHook(&hook);
    char buf[32];
    tracker.Track(buf, 32, 8, nullptr, 0, nullptr, {});
    EXPECT_EQ(hook.allocCalls, 1);
    tracker.Untrack(buf);
    EXPECT_EQ(hook.deallocCalls, 1);
    tracker.RemoveProfilingHook(&hook);
}

TEST_F(MemoryTrackerTest, LeakHookCalledOnReportLeaks)
{
    struct MockLeak : ILeakDetectionHook {
        usize beginCount = 0;
        usize leakCount  = 0;
        bool  endCalled  = false;
        void OnLeakReportBegin(usize count) override { beginCount = count; }
        void OnLeakDetected(const AllocationRecord&) override { ++leakCount; }
        void OnLeakReportEnd() override { endCalled = true; }
    } hook;

    tracker.AddLeakDetectionHook(&hook);

    char a[32], b[64];
    tracker.Track(a, 32, 8, nullptr, 0, nullptr, {});
    tracker.Track(b, 64, 8, nullptr, 0, nullptr, {});

    tracker.ReportLeaks();

    EXPECT_EQ(hook.beginCount, 2u);
    EXPECT_EQ(hook.leakCount,  2u);
    EXPECT_TRUE(hook.endCalled);

    tracker.Untrack(a);
    tracker.Untrack(b);
    tracker.RemoveLeakDetectionHook(&hook);
}

TEST_F(MemoryTrackerTest, SequenceIdIncrements)
{
    char a[8], b[8], c[8];
    tracker.Track(a, 8, 8, nullptr, 0, nullptr, {});
    tracker.Track(b, 8, 8, nullptr, 0, nullptr, {});
    tracker.Track(c, 8, 8, nullptr, 0, nullptr, {});
    auto recs = tracker.GetActiveAllocations();
    // All sequence IDs must be distinct (order may vary due to HashMap)
    EXPECT_EQ(recs.size(), 3u);
    u64 ids[3] = { recs[0].sequenceId, recs[1].sequenceId, recs[2].sequenceId };
    EXPECT_NE(ids[0], ids[1]);
    EXPECT_NE(ids[1], ids[2]);
    EXPECT_NE(ids[0], ids[2]);
    tracker.Untrack(a);
    tracker.Untrack(b);
    tracker.Untrack(c);
}

// =============================================================================
// MemoryManagerTest  (8 tests)
// =============================================================================

class MemoryManagerTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        MemoryManager::Initialize({ .enableTracking = true, .reportLeaksOnShutdown = false });
    }
    void TearDown() override
    {
        MemoryManager::Shutdown();
    }
};

TEST_F(MemoryManagerTest, IsInitializedAfterInit)
{
    EXPECT_TRUE(MemoryManager::IsInitialized());
}

TEST_F(MemoryManagerTest, IsNotInitializedAfterShutdown)
{
    MemoryManager::Shutdown();
    EXPECT_FALSE(MemoryManager::IsInitialized());
    // Re-initialize so TearDown doesn't double-shutdown
    MemoryManager::Initialize({ .enableTracking = true, .reportLeaksOnShutdown = false });
}

TEST_F(MemoryManagerTest, AllocateReturnsNonNull)
{
    void* p = MemoryManager::Allocate(64, 8, __FILE__, __LINE__, __func__);
    EXPECT_NE(p, nullptr);
    MemoryManager::Deallocate(p);
}

TEST_F(MemoryManagerTest, TrackerRecordsAllocAndDealloc)
{
    void* p = MemoryManager::Allocate(128, 8, __FILE__, __LINE__, __func__);
    EXPECT_GE(MemoryManager::GetTracker().ActiveAllocCount(), 1u);
    MemoryManager::Deallocate(p);
    EXPECT_EQ(MemoryManager::HasLeaks(), false);
}

TEST_F(MemoryManagerTest, LeakDetectedWhenNotFreed)
{
    void* p = MemoryManager::Allocate(32, 8, __FILE__, __LINE__, __func__);
    EXPECT_TRUE(MemoryManager::HasLeaks());
    MemoryManager::Deallocate(p);
}

TEST_F(MemoryManagerTest, RegisterAndGetAllocator)
{
    MemoryManager::RegisterAllocator("myLinear",
        MakeUnique<LinearAllocator>(4096u, StringView("myLinear")));

    IAllocator* alloc = MemoryManager::GetAllocator("myLinear");
    EXPECT_NE(alloc, nullptr);
    EXPECT_EQ(alloc->GetName(), "myLinear");
}

TEST_F(MemoryManagerTest, GetUnknownAllocatorReturnsNull)
{
    EXPECT_EQ(MemoryManager::GetAllocator("does_not_exist"), nullptr);
}

TEST_F(MemoryManagerTest, HydraAllocMacro)
{
    void* p = HYDRA_ALLOC(256);
    EXPECT_NE(p, nullptr);
    HYDRA_FREE(p);
    EXPECT_FALSE(MemoryManager::HasLeaks());
}
