#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>

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
