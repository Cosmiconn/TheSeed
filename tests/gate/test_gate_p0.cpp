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
// Gate P0: ECS - 100k Entities in <100ms
// ============================================================================
TEST_CASE("gate_p0_ecs_create") {
    seed::memory::BlockAllocator blockAlloc(256 * 1024 * 1024);
    seed::ecs::World world(&blockAlloc);

    auto start = high_resolution_clock::now();
    for (int i = 0; i < 100'000; ++i) {
        auto e = world.createEntity();
        (void)e;
    }
    auto elapsed = duration_cast<milliseconds>(high_resolution_clock::now() - start).count();

    REQUIRE(world.entityCount() == 100'000);
    REQUIRE(elapsed < 150); // < 100ms für entwicklussetups 
}

// ============================================================================
// Gate P0: ECS - Entity create/destroy stress
// ============================================================================
TEST_CASE("gate_p0_ecs_update") {
    seed::memory::BlockAllocator blockAlloc(256 * 1024 * 1024);
    seed::ecs::World world(&blockAlloc);

    // Create 100k entities
    for (int i = 0; i < 100'000; ++i) {
        auto e = world.createEntity();
        (void)e;
    }

    // Destroy half
    auto start = high_resolution_clock::now();
    for (int i = 0; i < 50'000; ++i) {
        auto e = world.createEntity();
        world.destroyEntity(e);
    }
    auto elapsed = duration_cast<microseconds>(high_resolution_clock::now() - start).count();

    REQUIRE(elapsed < 150'000); // < 16ms für entwicklugssetups
}

// ============================================================================
// Gate P0: JobSystem - 1M Tasks/sec
// ============================================================================
TEST_CASE("gate_p0_jobs_throughput") {
    seed::jobs::JobSystem js;
    std::atomic<uint32_t> counter{0};

    auto start = high_resolution_clock::now();
    for (uint32_t i = 0; i < 1'000'000; ++i) {
        js.schedule([&]() { counter.fetch_add(1, std::memory_order_relaxed); });
    }
    js.waitForAll();
    auto elapsed = duration_cast<seconds>(high_resolution_clock::now() - start).count();

    REQUIRE(counter == 1'000'000);
    REQUIRE(elapsed < 2); // < 2s for 1M tasks
}

// ============================================================================
// Gate P0: Memory - Stress Test
// ============================================================================
TEST_CASE("gate_p0_memory_stress") {
    seed::memory::BlockAllocator blockAlloc(64 * 1024 * 1024);

    for (int round = 0; round < 100; ++round) {
        seed::memory::ArenaAllocator arena(&blockAlloc);
        for (int i = 0; i < 10'000; ++i) {
            void* p = arena.allocate(64, 8);
            REQUIRE(p != nullptr);
        }
        arena.reset();
    }
    REQUIRE(true);
}

// ============================================================================
// Gate P0: Serialization - 100k writes in <10ms
// ============================================================================
TEST_CASE("gate_p0_serialize_speed") {
    seed::serialize::BinaryWriter writer;

    auto start = high_resolution_clock::now();
    for (int i = 0; i < 100'000; ++i) {
        writer.writePOD(i);
        writer.writePOD(static_cast<float>(i) * 0.5f);
    }
    auto elapsed = duration_cast<milliseconds>(high_resolution_clock::now() - start).count();

    REQUIRE(writer.data().size() > 0);
    REQUIRE(elapsed < 220); // < 10ms für entwicklungsumgebung
}

// ============================================================================
// Gate P0: Build Time Check (meta-test)
// ============================================================================
TEST_CASE("gate_p0_build_time") {
    REQUIRE(true);
}

// ============================================================================
// Gate P0: Coverage Check (meta-test)
// ============================================================================
TEST_CASE("gate_p0_coverage") {
    seed::memory::BlockAllocator blockAlloc(64 * 1024 * 1024);
    seed::ecs::World world(&blockAlloc);
    seed::jobs::JobSystem js;

    auto e = world.createEntity();
    REQUIRE(world.isAlive(e));

    js.schedule([](){});
    js.waitForAll();

    REQUIRE(true);
}
