#include <doctest/doctest.h>
#include "core/memory/pool_allocator.h"
#include "core/memory/block_allocator.h"

#include <thread>
#include <vector>
#include <chrono>

using namespace seed::memory;

// ---------------------------------------------------------------------------
// Basic functionality
// ---------------------------------------------------------------------------
TEST_CASE("PoolAllocator_SingleThread_Basic") {
    BlockAllocator blockAlloc;
    PoolAllocator<uint64_t> pool(&blockAlloc);

    uint64_t* p = pool.construct(42ULL);
    CHECK(p != nullptr);
    CHECK(*p == 42);

    pool.destroy(p);
    CHECK(pool.numAllocated() == 0);
}

TEST_CASE("PoolAllocator_SingleThread_ManyObjects") {
    BlockAllocator blockAlloc;
    PoolAllocator<uint64_t> pool(&blockAlloc);

    constexpr size_t N = 1000;
    std::vector<uint64_t*> ptrs;
    ptrs.reserve(N);

    for (size_t i = 0; i < N; ++i) {
        uint64_t* p = pool.construct(i);
        REQUIRE(p != nullptr);
        *p = i;
        ptrs.push_back(p);
    }

    CHECK(pool.numAllocated() == N);

    for (size_t i = 0; i < N; ++i) {
        CHECK(*ptrs[i] == i);
        pool.destroy(ptrs[i]);
    }

    CHECK(pool.numAllocated() == 0);
}

// ---------------------------------------------------------------------------
// Performance: 1M allocs/deallocs < 1 second
// ---------------------------------------------------------------------------
TEST_CASE("PoolAllocator_SingleThread_Performance") {
    BlockAllocator blockAlloc;
    PoolAllocator<uint64_t> pool(&blockAlloc);

    constexpr size_t N = 1'000'000;

    auto start = std::chrono::high_resolution_clock::now();
    for (size_t i = 0; i < N; ++i) {
        auto* p = pool.construct(i);
        pool.destroy(p);
    }
    auto elapsed = std::chrono::high_resolution_clock::now() - start;
    auto ns = std::chrono::duration_cast<std::chrono::nanoseconds>(elapsed).count();

    CHECK(pool.numAllocated() == 0);
    CHECK(ns < 1'000'000'000);
}

// ---------------------------------------------------------------------------
// Multi-threaded safety
// ---------------------------------------------------------------------------
TEST_CASE("PoolAllocator_MultiThread_Safety") {
    BlockAllocator blockAlloc;
    PoolAllocator<uint64_t> pool(&blockAlloc);

    constexpr size_t THREADS = 10;
    constexpr size_t OPS = 100'000;

    std::vector<std::thread> threads;
    for (size_t t = 0; t < THREADS; ++t) {
        threads.emplace_back([&]() {
            for (size_t i = 0; i < OPS; ++i) {
                auto* p = pool.construct(i);
                if (p) {
                    *p = i;
                    pool.destroy(p);
                }
            }
        });
    }

    for (auto& t : threads) t.join();

    CHECK(pool.numAllocated() == 0);
}

// ---------------------------------------------------------------------------
// Allocator interface (polymorphic)
// ---------------------------------------------------------------------------
TEST_CASE("PoolAllocator_PolymorphicInterface") {
    BlockAllocator blockAlloc;
    PoolAllocator<uint64_t> pool(&blockAlloc);

    void* p = pool.allocate(sizeof(uint64_t), alignof(uint64_t));
    CHECK(p != nullptr);

    uint64_t* val = static_cast<uint64_t*>(p);
    *val = 12345;
    CHECK(*val == 12345);

    pool.deallocate(p, sizeof(uint64_t));
}

// ---------------------------------------------------------------------------
// Object with constructor/destructor
// ---------------------------------------------------------------------------
struct Counter {
    static std::atomic<int> constructed;
    static std::atomic<int> destroyed;

    uint64_t padding; // Ensure sizeof(Counter) >= sizeof(void*)

    Counter() : padding(0) { constructed.fetch_add(1, std::memory_order_relaxed); }
    ~Counter() { destroyed.fetch_add(1, std::memory_order_relaxed); }
};

std::atomic<int> Counter::constructed{0};
std::atomic<int> Counter::destroyed{0};

TEST_CASE("PoolAllocator_ConstructorDestructor") {
    Counter::constructed.store(0);
    Counter::destroyed.store(0);

    BlockAllocator blockAlloc;
    PoolAllocator<Counter> pool(&blockAlloc);

    {
        Counter* c = pool.construct();
        CHECK(Counter::constructed.load() == 1);
        pool.destroy(c);
    }

    CHECK(Counter::destroyed.load() == 1);
}
