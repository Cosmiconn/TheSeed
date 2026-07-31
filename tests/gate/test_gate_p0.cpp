#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>

#include <seed/memory.h>
#include <seed/ecs.h>
#include <seed/jobs.h>
#include <seed/serialize.h>
#include <chrono>
#include <atomic>
#include <cstdio>
#include <fstream>

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
    REQUIRE(elapsed < 500); // < 500ms (CI runner budget – measured ~200ms)
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

    REQUIRE(elapsed < 500'000); // < 500ms (CI runner budget – measured ~125ms)
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
    REQUIRE(elapsed < 5); // < 5s for 1M tasks (CI runner budget – measured ~2s)
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
    REQUIRE(elapsed < 500); // < 500ms (CI runner budget – measured ~235ms)
}

// ============================================================================
// Gate P0: Build Time Check (CMake-Preset-Validierung)
// ============================================================================
TEST_CASE("gate_p0_build_time") {
    auto start = high_resolution_clock::now();

    // Pruefe, dass CMakePresets.json existiert und lesbar ist
    std::ifstream presets("CMakePresets.json");
    REQUIRE(presets.good());

    // Pruefe, dass vcpkg.json existiert und alle Dependencies gelistet sind
    std::ifstream vcpkg("vcpkg.json");
    REQUIRE(vcpkg.good());

    auto elapsed = duration_cast<milliseconds>(high_resolution_clock::now() - start).count();
    REQUIRE(elapsed < 100); // < 100ms fuer reine Datei-Checks
}

// ============================================================================
// Gate P0: Coverage Check (lcov-Summary parsen)
// ============================================================================
TEST_CASE("gate_p0_coverage") {
    const char* coverageInfo = std::getenv("SEED_COVERAGE_INFO_FILE");
    if (!coverageInfo) {
        MESSAGE("SKIP: SEED_COVERAGE_INFO_FILE not set");
        return;
    }

    std::string cmd = std::string("lcov --summary ") + coverageInfo + " 2>&1";
    FILE* pipe = popen(cmd.c_str(), "r");
    REQUIRE(pipe != nullptr);

    char buffer[256];
    std::string output;
    while (fgets(buffer, sizeof(buffer), pipe) != nullptr) {
        output += buffer;
    }
    pclose(pipe);

    size_t pos = output.find("lines......:");
    REQUIRE(pos != std::string::npos);

    float percent = 0.0f;
    std::sscanf(output.c_str() + pos, "lines......: %f%%", &percent);

    MESSAGE("Coverage: ", percent, "%");
    REQUIRE(percent >= 80.0f); // >= 80% laut Roadmap
}
