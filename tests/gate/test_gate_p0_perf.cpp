#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>

#include <seed/memory.h>
#include <seed/ecs.h>
#include <seed/jobs.h>
#include <seed/serialize.h>
#include <chrono>
#include <atomic>

using namespace std::chrono;

// ============================================================================
// Gate P0: Performance Tests (informational, blockieren CI NICHT)
// ============================================================================
// Diese Tests messen Performance-Budgets. Sie verwenden CHECK statt REQUIRE,
// damit ein Budget-Miss die CI nicht blockiert. Die Messwerte dienen der
// Trend-Analyse auf dedizierter Hardware (siehe ADR-006).
//
// Aspirational Budgets (Roadmap):
//   - 100k Entities erstellen: < 100 ms
//   - 10 Systeme @ 100k Entities: < 16 ms/Frame
//   - 1M Tasks: < 1 s
//   - 100k Serialize-Ops: < 10 ms
//   - Clean Build: < 3 min
// ============================================================================

TEST_CASE("gate_p0_perf_ecs_create_100k") {
    seed::memory::BlockAllocator blockAlloc(256 * 1024 * 1024);
    seed::ecs::World world(&blockAlloc);

    auto start = high_resolution_clock::now();
    for (int i = 0; i < 100'000; ++i) {
        auto e = world.createEntity();
        (void)e;
    }
    auto elapsed = duration_cast<milliseconds>(high_resolution_clock::now() - start).count();

    MESSAGE("100k Entity creation: ", elapsed, " ms (budget: <100ms)");
    CHECK(elapsed < 100);  // Aspirational – nicht blockierend
}

TEST_CASE("gate_p0_perf_ecs_update_100k") {
    seed::memory::BlockAllocator blockAlloc(256 * 1024 * 1024);
    seed::ecs::World world(&blockAlloc);

    for (int i = 0; i < 100'000; ++i) {
        auto e = world.createEntity();
        (void)e;
    }

    auto start = high_resolution_clock::now();
    for (int i = 0; i < 50'000; ++i) {
        auto e = world.createEntity();
        world.destroyEntity(e);
    }
    auto elapsed = duration_cast<microseconds>(high_resolution_clock::now() - start).count();

    MESSAGE("50k create+destroy: ", elapsed, " us (budget: <500ms)");
    CHECK(elapsed < 500'000);  // Aspirational
}

TEST_CASE("gate_p0_perf_jobs_1m_tasks") {
    seed::jobs::JobSystem js;
    std::atomic<uint32_t> counter{0};

    auto start = high_resolution_clock::now();
    for (uint32_t i = 0; i < 1'000'000; ++i) {
        js.schedule([&]() { counter.fetch_add(1, std::memory_order_relaxed); });
    }
    js.waitForAll();
    auto elapsed = duration_cast<seconds>(high_resolution_clock::now() - start).count();

    REQUIRE(counter == 1'000'000);  // Funktional: muss passen
    MESSAGE("1M Tasks: ", elapsed, " s (budget: <1s)");
    CHECK(elapsed < 1);  // Performance: aspirational
}

TEST_CASE("gate_p0_perf_serialize_100k") {
    seed::serialize::BinaryWriter writer;

    auto start = high_resolution_clock::now();
    for (int i = 0; i < 100'000; ++i) {
        writer.writePOD(i);
        writer.writePOD(static_cast<float>(i) * 0.5f);
    }
    auto elapsed = duration_cast<milliseconds>(high_resolution_clock::now() - start).count();

    REQUIRE(writer.data().size() > 0);  // Funktional
    MESSAGE("100k serialize ops: ", elapsed, " ms (budget: <10ms)");
    CHECK(elapsed < 10);  // Aspirational
}
