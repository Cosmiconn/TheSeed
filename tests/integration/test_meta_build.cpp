#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>

#include <atomic>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

#include <seed/memory.h>
#include <seed/ecs.h>
#include <seed/jobs.h>
#include <seed/serialize.h>
#include <seed/diagnostics.h>
#include <seed/profiling.h>

#ifndef SEED_SOURCE_DIR
#define SEED_SOURCE_DIR "."
#endif
static const std::filesystem::path kProjectRoot = SEED_SOURCE_DIR;

TEST_CASE("MetaBuild_PublicHeaders_Compile") {
    // This test verifies that all public headers compile together
    // without ODR violations or missing includes.
    REQUIRE(true);
}

TEST_CASE("MetaBuild_ECS_WithMemory") {
    // Integration: ECS using custom allocator
    seed::memory::BlockAllocator blockAlloc(64 * 1024 * 1024);
    seed::ecs::World world(&blockAlloc);

    auto e = world.createEntity();
    REQUIRE(world.isAlive(e));

    world.destroyEntity(e);
    REQUIRE(!world.isAlive(e));
}

TEST_CASE("MetaBuild_Diagnostics_Timeline") {
    // Integration: Diagnostics + ECS events
    auto& timeline = seed::diagnostics::globalTimeline();
    timeline.clear();

    timeline.push(seed::diagnostics::EventType::Custom,
                  seed::ecs::INVALID_ENTITY, 0, 0, 0,
                  "meta build test", __FILE__, __LINE__);

    REQUIRE(timeline.size() == 1);
}

// ============================================================================
// Monat 1 Deliverable: Build-System Validation
// ============================================================================

TEST_CASE("MetaBuild_CMakePresets_Exists") {
    std::ifstream presets(kProjectRoot / "CMakePresets.json");
    REQUIRE(presets.good());
}

TEST_CASE("MetaBuild_CMakePresets_HasRequiredPresets") {
    std::ifstream presets(kProjectRoot / "CMakePresets.json");
    REQUIRE(presets.good());

    std::ostringstream ss;
    ss << presets.rdbuf();
    std::string content = ss.str();

    REQUIRE(content.find("linux-debug") != std::string::npos);
    REQUIRE(content.find("linux-release") != std::string::npos);
    REQUIRE(content.find("windows-debug") != std::string::npos);
    REQUIRE(content.find("windows-release") != std::string::npos);
}

TEST_CASE("MetaBuild_vcpkgJson_Exists") {
    std::ifstream vcpkg(kProjectRoot / "vcpkg.json");
    REQUIRE(vcpkg.good());
}

TEST_CASE("MetaBuild_vcpkgJson_HasRequiredDeps") {
    std::ifstream vcpkg(kProjectRoot / "vcpkg.json");
    REQUIRE(vcpkg.good());

    std::ostringstream ss;
    ss << vcpkg.rdbuf();
    std::string content = ss.str();

    REQUIRE(content.find("doctest") != std::string::npos);
    REQUIRE(content.find("spdlog") != std::string::npos);
    REQUIRE(content.find("fmt") != std::string::npos);
}

TEST_CASE("MetaBuild_RootCMake_Exists") {
    std::ifstream cmake(kProjectRoot / "CMakeLists.txt");
    REQUIRE(cmake.good());
}

TEST_CASE("MetaBuild_SeedCoreCMake_Exists") {
    std::ifstream cmake(kProjectRoot / "submodules/seed-core/CMakeLists.txt");
    REQUIRE(cmake.good());
}

TEST_CASE("MetaBuild_Scripts_Exist") {
    std::ifstream build_sh(kProjectRoot / "scripts/build.sh");
    REQUIRE(build_sh.good());

    std::ifstream test_sh(kProjectRoot / "scripts/test.sh");
    REQUIRE(test_sh.good());

    std::ifstream setup_sh(kProjectRoot / "scripts/setup.sh");
    REQUIRE(setup_sh.good());
}

// ============================================================================
// Stress Tests (werden von CI x10 wiederholt)
// ============================================================================

TEST_CASE("Integration_MultiThreadStress") {
    seed::jobs::JobSystem js;
    constexpr uint32_t kNumTasks = 10'000;
    std::atomic<uint32_t> counter{0};

    for (uint32_t i = 0; i < kNumTasks; ++i) {
        js.schedule([&]() { counter.fetch_add(1, std::memory_order_relaxed); });
    }
    js.waitForAll();

    REQUIRE(counter == kNumTasks);
}

TEST_CASE("Integration_100kEntities_Stress") {
    seed::memory::BlockAllocator blockAlloc(256 * 1024 * 1024);
    seed::ecs::World world(&blockAlloc);

    for (int i = 0; i < 100'000; ++i) {
        auto e = world.createEntity();
        (void)e;
    }

    REQUIRE(world.entityCount() == 100'000);
}
