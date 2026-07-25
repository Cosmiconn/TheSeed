// Main defined in test_property_math.cpp
#include <doctest/doctest.h>
#include <seed/memory.h>
#include <seed/ecs.h>

using namespace seed::ecs;

// Property: Entity count is consistent after create/destroy
TEST_CASE("Property_ECS_EntityCountConsistent") {
    seed::memory::BlockAllocator blockAlloc(64 * 1024 * 1024);
    World world(&blockAlloc);

    auto e1 = world.createEntity();
    auto e2 = world.createEntity();
    REQUIRE(world.entityCount() == 2);

    world.destroyEntity(e1);
    REQUIRE(world.entityCount() == 1);

    world.destroyEntity(e2);
    REQUIRE(world.entityCount() == 0);
}

// Property: Dead entity handles are invalid
TEST_CASE("Property_ECS_DeadHandleInvalid") {
    seed::memory::BlockAllocator blockAlloc(64 * 1024 * 1024);
    World world(&blockAlloc);

    auto e = world.createEntity();
    world.destroyEntity(e);
    REQUIRE(!world.isAlive(e));
}

// Property: Entity recycling works
TEST_CASE("Property_ECS_EntityRecycling") {
    seed::memory::BlockAllocator blockAlloc(64 * 1024 * 1024);
    World world(&blockAlloc);

    auto e1 = world.createEntity();
    world.destroyEntity(e1);
    auto e2 = world.createEntity();
    // e2 should reuse e1's slot but with incremented version
    REQUIRE(!world.isAlive(e1));
    REQUIRE(world.isAlive(e2));
}
