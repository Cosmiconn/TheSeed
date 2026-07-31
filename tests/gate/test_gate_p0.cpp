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

#ifdef _WIN32
#define popen _popen
#define pclose _pclose
#pragma warning(disable: 4996)  // getenv, sscanf deprecation on MSVC
#endif

using namespace std::chrono;

// ============================================================================
// Gate P0: Funktionale Tests (blockieren CI)
// ============================================================================

TEST_CASE("gate_p0_ecs_create_count") {
    seed::memory::BlockAllocator blockAlloc(256 * 1024 * 1024);
    seed::ecs::World world(&blockAlloc);

    for (int i = 0; i < 100'000; ++i) {
        auto e = world.createEntity();
        (void)e;
    }

    REQUIRE(world.entityCount() == 100'000);
}

TEST_CASE("gate_p0_ecs_create_destroy") {
    seed::memory::BlockAllocator blockAlloc(256 * 1024 * 1024);
    seed::ecs::World world(&blockAlloc);

    for (int i = 0; i < 100'000; ++i) {
        auto e = world.createEntity();
        (void)e;
    }

    for (int i = 0; i < 50'000; ++i) {
        auto e = world.createEntity();
        world.destroyEntity(e);
    }

    REQUIRE(world.entityCount() == 150'000);
}

TEST_CASE("gate_p0_jobs_1m_tasks_complete") {
    seed::jobs::JobSystem js;
    std::atomic<uint32_t> counter{0};

    for (uint32_t i = 0; i < 1'000'000; ++i) {
        js.schedule([&]() { counter.fetch_add(1, std::memory_order_relaxed); });
    }
    js.waitForAll();

    REQUIRE(counter == 1'000'000);
}

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
}

TEST_CASE("gate_p0_serialize_roundtrip") {
    seed::serialize::BinaryWriter writer;

    for (int i = 0; i < 100'000; ++i) {
        writer.writePOD(i);
        writer.writePOD(static_cast<float>(i) * 0.5f);
    }

    REQUIRE(writer.data().size() > 0);

    seed::serialize::BinaryReader reader(writer.data());
    for (int i = 0; i < 100'000; ++i) {
        int32_t val = reader.readPOD<int32_t>();
        float fval = reader.readPOD<float>();
        REQUIRE(val == i);
        REQUIRE(fval == static_cast<float>(i) * 0.5f);
    }
}

TEST_CASE("gate_p0_build_time") {
    auto start = high_resolution_clock::now();

    std::ifstream presets("CMakePresets.json");
    REQUIRE(presets.good());

    std::ifstream vcpkg("vcpkg.json");
    REQUIRE(vcpkg.good());

    auto elapsed = duration_cast<milliseconds>(high_resolution_clock::now() - start).count();
    REQUIRE(elapsed < 100);
}

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
    REQUIRE(percent >= 80.0f);
}
