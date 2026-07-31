#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>

#include <fstream>
#include <string>

#include <seed/memory.h>
#include <seed/ecs.h>
#include <seed/jobs.h>
#include <seed/serialize.h>
#include <seed/diagnostics.h>
#include <seed/profiling.h>

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
    std::ifstream presets("CMakePresets.json");
    REQUIRE(presets.good());
}

TEST_CASE("MetaBuild_CMakePresets_HasRequiredPresets") {
    std::ifstream presets("CMakePresets.json");
    REQUIRE(presets.good());

    std::string content((std::istreambuf_iterator<char>(presets)),
                         std::istreambuf_iterator<char>());

    REQUIRE(content.find("linux-debug") != std::string::npos);
    REQUIRE(content.find("linux-release") != std::string::npos);
    REQUIRE(content.find("windows-debug") != std::string::npos);
    REQUIRE(content.find("windows-release") != std::string::npos);
}

TEST_CASE("MetaBuild_vcpkgJson_Exists") {
    std::ifstream vcpkg("vcpkg.json");
    REQUIRE(vcpkg.good());
}

TEST_CASE("MetaBuild_vcpkgJson_HasRequiredDeps") {
    std::ifstream vcpkg("vcpkg.json");
    REQUIRE(vcpkg.good());

    std::string content((std::istreambuf_iterator<char>(vcpkg)),
                         std::istreambuf_iterator<char>());

    REQUIRE(content.find("doctest") != std::string::npos);
    REQUIRE(content.find("spdlog") != std::string::npos);
    REQUIRE(content.find("fmt") != std::string::npos);
}

TEST_CASE("MetaBuild_RootCMake_Exists") {
    std::ifstream cmake("CMakeLists.txt");
    REQUIRE(cmake.good());
}

TEST_CASE("MetaBuild_SeedCoreCMake_Exists") {
    std::ifstream cmake("submodules/seed-core/CMakeLists.txt");
    REQUIRE(cmake.good());
}

TEST_CASE("MetaBuild_Scripts_Exist") {
    std::ifstream build_sh("scripts/build.sh");
    REQUIRE(build_sh.good());

    std::ifstream test_sh("scripts/test.sh");
    REQUIRE(test_sh.good());

    std::ifstream setup_sh("scripts/setup.sh");
    REQUIRE(setup_sh.good());
}
