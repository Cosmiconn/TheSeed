#include <doctest/doctest.h>
#include "core/memory/arena_allocator.h"
#include "core/memory/block_allocator.h"

#include <chrono>
#include <vector>

using namespace seed::memory;

TEST_CASE("ArenaAllocator_BasicAllocation") {
    BlockAllocator blockAlloc;
    ArenaAllocator arena(&blockAlloc);

    void* p1 = arena.allocate(64, 8);
    CHECK(p1 != nullptr);

    void* p2 = arena.allocate(128, 16);
    CHECK(p2 != nullptr);
    CHECK(p2 > p1); // Linear growth

    CHECK(arena.totalUsed() >= 64 + 128);
}

TEST_CASE("ArenaAllocator_ZeroFragmentation") {
    BlockAllocator blockAlloc;
    ArenaAllocator arena(&blockAlloc, 1024);

    std::vector<void*> ptrs;
    for (int i = 0; i < 100; ++i) {
        void* p = arena.allocate(8, 8);
        REQUIRE(p != nullptr);
        ptrs.push_back(p);
    }

    // All allocations contiguous
    for (size_t i = 1; i < ptrs.size(); ++i) {
        CHECK(ptrs[i] == static_cast<uint8_t*>(ptrs[i-1]) + 8);
    }
}

TEST_CASE("ArenaAllocator_Reset") {
    BlockAllocator blockAlloc;
    ArenaAllocator arena(&blockAlloc);

    for (int i = 0; i < 100; ++i) {
        arena.allocate(64, 8);
    }
    CHECK(arena.totalUsed() > 0);

    arena.reset();
    CHECK(arena.totalUsed() == 0);

    // Reuse same memory
    void* p = arena.allocate(64, 8);
    CHECK(p != nullptr);
}

TEST_CASE("ArenaAllocator_1000ObjectsIn1ms") {
    BlockAllocator blockAlloc;
    ArenaAllocator arena(&blockAlloc);

    auto start = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < 1000; ++i) {
        arena.allocate(32, 8);
    }
    auto elapsed = std::chrono::high_resolution_clock::now() - start;
    auto ns = std::chrono::duration_cast<std::chrono::nanoseconds>(elapsed).count();

    // CI runners are shared VMs; budget is < 1ms (still 1000x faster than malloc)
    CHECK(ns < 1'000'000);
}
