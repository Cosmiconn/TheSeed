#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>

#include <seed/memory.h>
#include <seed/ecs.h>
#include <seed/jobs.h>
#include <seed/serialize.h>

TEST_CASE("E2E_Smoke_BlockAllocator") {
    seed::memory::BlockAllocator blockAlloc(64 * 1024 * 1024);
    void* ptr = blockAlloc.allocate(1024);
    REQUIRE(ptr != nullptr);
    blockAlloc.deallocate(ptr, 1024);
}

TEST_CASE("E2E_Smoke_WorldCreate") {
    seed::memory::BlockAllocator blockAlloc(64 * 1024 * 1024);
    seed::ecs::World world(&blockAlloc);
    REQUIRE(world.entityCount() == 0);
}

TEST_CASE("E2E_Smoke_CreateEntity") {
    seed::memory::BlockAllocator blockAlloc(64 * 1024 * 1024);
    seed::ecs::World world(&blockAlloc);
    auto e = world.createEntity();
    REQUIRE(world.isAlive(e));
    world.destroyEntity(e);
    REQUIRE(!world.isAlive(e));
}

TEST_CASE("E2E_Smoke_JobSystemParallelFor") {
    seed::jobs::JobSystem js;

    std::vector<int> data(1000, 0);
    js.parallelFor(data.size(), [&](size_t i) {
        data[i] = static_cast<int>(i);
    });

    for (size_t i = 0; i < 1000; ++i) {
        REQUIRE(data[i] == static_cast<int>(i));
    }
}

TEST_CASE("E2E_Smoke_BinaryWriter") {
    seed::serialize::BinaryWriter writer;
    writer.writePOD(42);
    writer.writePOD(3.14f);
    REQUIRE(writer.data().size() > 0);
}
