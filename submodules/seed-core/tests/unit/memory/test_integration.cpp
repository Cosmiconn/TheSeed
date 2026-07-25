#include <doctest/doctest.h>
#include "core/memory/memory_system.h"
#include "core/memory/pool_allocator.h"
#include "core/memory/arena_allocator.h"
#include "core/memory/stack_allocator.h"

#include <chrono>
#include <thread>
#include <vector>
#include <random>

using namespace seed::memory;

// ---------------------------------------------------------------------------
// Integration: All allocators work together
// ---------------------------------------------------------------------------
TEST_CASE("Integration_AllAllocatorsTogether") {
    BlockAllocator blockAlloc;
    MemoryTracker tracker;
    ArenaAllocator arena(&blockAlloc);
    StackAllocator stack(&blockAlloc, 4096);

    PoolAllocator<uint64_t> entityPool(&blockAlloc);

    for (int frame = 0; frame < 100; ++frame) {
        void* temp1 = arena.allocate(256, 16);
        void* temp2 = arena.allocate(512, 32);
        REQUIRE(temp1 != nullptr);
        REQUIRE(temp2 != nullptr);

        auto* e1 = entityPool.construct(static_cast<uint64_t>(frame));
        auto* e2 = entityPool.construct(static_cast<uint64_t>(frame + 1));
        REQUIRE(e1 != nullptr);
        REQUIRE(e2 != nullptr);

        auto marker = stack.getMarker();
        void* scope = stack.allocate(128, 8);
        REQUIRE(scope != nullptr);
        stack.freeToMarker(marker);

        tracker.trackAllocation("frame", 256 + 512);
        tracker.trackDeallocation("frame", 256 + 512);

        arena.reset();
        entityPool.destroy(e1);
        entityPool.destroy(e2);
    }

    CHECK(arena.totalUsed() == 0);
    CHECK(entityPool.numAllocated() == 0);
    CHECK(stack.totalUsed() == 0);
}

// ---------------------------------------------------------------------------
// Stresstest: 100k entities allocate/deallocate
// ---------------------------------------------------------------------------
TEST_CASE("Integration_100kEntities_Stress") {
    BlockAllocator blockAlloc;
    PoolAllocator<uint64_t> pool(&blockAlloc);

    constexpr size_t N = 100'000;
    std::vector<uint64_t*> entities;
    entities.reserve(N);

    auto start = std::chrono::high_resolution_clock::now();

    for (size_t i = 0; i < N; ++i) {
        entities.push_back(pool.construct(i));
    }
    CHECK(pool.numAllocated() == N);

    for (auto* e : entities) {
        pool.destroy(e);
    }

    auto elapsed = std::chrono::high_resolution_clock::now() - start;
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(elapsed).count();

    CHECK(pool.numAllocated() == 0);
    CHECK(ms < 1000);
}

// ---------------------------------------------------------------------------
// Multi-threaded stresstest (restored with fixed PoolAllocator)
// ---------------------------------------------------------------------------
TEST_CASE("Integration_MultiThreadStress") {
    BlockAllocator blockAlloc;
    PoolAllocator<uint64_t> pool(&blockAlloc);

    constexpr size_t THREADS = 8;
    constexpr size_t OPS = 50'000;

    std::vector<std::thread> threads;
    for (size_t t = 0; t < THREADS; ++t) {
        threads.emplace_back([&]() {
            std::mt19937 rng(static_cast<unsigned>(std::hash<std::thread::id>{}(
                std::this_thread::get_id())));
            std::uniform_int_distribution<int> dist(0, 9);

            std::vector<uint64_t*> local;
            local.reserve(100);

            for (size_t i = 0; i < OPS; ++i) {
                if (local.empty() || dist(rng) < 6) {
                    auto* p = pool.construct(i);
                    if (p) local.push_back(p);
                } else {
                    size_t idx = static_cast<size_t>(dist(rng)) % local.size();
                    pool.destroy(local[idx]);
                    local[idx] = local.back();
                    local.pop_back();
                }
            }

            for (auto* p : local) {
                pool.destroy(p);
            }
        });
    }

    for (auto& t : threads) t.join();
    CHECK(pool.numAllocated() == 0);
}

// ---------------------------------------------------------------------------
// Memory integrity: write patterns, verify no corruption
// ---------------------------------------------------------------------------
TEST_CASE("Integration_MemoryIntegrity") {
    BlockAllocator blockAlloc;
    PoolAllocator<uint64_t> pool(&blockAlloc);
    ArenaAllocator arena(&blockAlloc);

    std::vector<uint64_t*> ptrs;
    for (int i = 0; i < 1000; ++i) {
        auto* p = pool.construct(static_cast<uint64_t>(i));
        if (!p) {
            FAIL("Pool allocation failed");
            return;
        }
        *p = 0xDEADBEEFCAFEBABEULL;
        ptrs.push_back(p);
    }
    for (auto* p : ptrs) {
        CHECK(*p == 0xDEADBEEFCAFEBABEULL);
        pool.destroy(p);
    }

    struct TestStruct {
        uint32_t a;
        uint64_t b;
        float c;
    };

    for (int round = 0; round < 10; ++round) {
        auto* t1 = static_cast<TestStruct*>(arena.allocate(sizeof(TestStruct), alignof(TestStruct)));
        if (!t1) {
            FAIL("Arena allocation failed");
            return;
        }
        t1->a = 0x12345678;
        t1->b = 0xABCDEF0123456789ULL;
        t1->c = 3.14159f;

        CHECK(t1->a == 0x12345678);
        CHECK(t1->b == 0xABCDEF0123456789ULL);
        CHECK(t1->c == 3.14159f);

        arena.reset();
    }
}

// ---------------------------------------------------------------------------
// Budget enforcement across categories
// ---------------------------------------------------------------------------
TEST_CASE("Integration_BudgetEnforcement") {
    MemoryTracker tracker;

    tracker.setBudget("ecs", 10'000);
    tracker.setBudget("render", 5'000);
    tracker.setBudget("audio", 1'000);

    int alarms = 0;
    tracker.setAlarmCallback([&](const std::string&, size_t, size_t) {
        ++alarms;
    });

    tracker.trackAllocation("ecs", 5'000);
    tracker.trackAllocation("render", 3'000);
    tracker.trackAllocation("audio", 500);
    tracker.trackAllocation("ecs", 6'000);
    tracker.trackAllocation("render", 3'000);
    tracker.trackAllocation("audio", 600);

    CHECK(alarms == 3);
    CHECK(tracker.checkBudget("ecs"));
    CHECK(tracker.checkBudget("render"));
    CHECK(tracker.checkBudget("audio"));
}
