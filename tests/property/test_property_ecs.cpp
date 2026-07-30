// TheSeed Engine – Meta-Level Property Tests: ECS (rapidcheck + doctest)
// ---------------------------------------------------------------------------
// Cross-submodule property tests for ECS behavior.
// ---------------------------------------------------------------------------
#include <doctest/doctest.h>
#include <rapidcheck.h>
#include <seed/memory.h>
#include <seed/ecs.h>

using namespace seed::ecs;
using namespace seed::memory;

struct Position { float x, y, z; };
struct Velocity { float x, y, z; };

SEED_REGISTER_COMPONENT_WITH_ID(Position, 1)
SEED_REGISTER_COMPONENT_WITH_ID(Velocity, 2)

TEST_CASE("Meta_Property_ECS_CreateDestroyConsistency") {
    rc::check("Meta-level: entity count consistent", []() {
        BlockAllocator blockAlloc;
        ChunkAllocator chunkAlloc(&blockAlloc, 64 * 1024);
        World world(&chunkAlloc);

        auto numCreate = *rc::gen::inRange(0, 500);
        auto numDestroy = *rc::gen::inRange(0, numCreate + 1);

        std::vector<Entity> entities;
        for (int i = 0; i < numCreate; ++i) {
            entities.push_back(world.createEntity());
        }
        RC_ASSERT(world.entityCount() == static_cast<size_t>(numCreate));

        for (int i = 0; i < numDestroy; ++i) {
            world.destroyEntity(entities[static_cast<size_t>(i)]);
        }
        RC_ASSERT(world.entityCount() == static_cast<size_t>(numCreate - numDestroy));
    });
}

TEST_CASE("Meta_Property_ECS_QueryCompleteness") {
    rc::check("Meta-level: query returns all matching entities", []() {
        BlockAllocator blockAlloc;
        ChunkAllocator chunkAlloc(&blockAlloc, 64 * 1024);
        World world(&chunkAlloc);

        auto numEntities = *rc::gen::inRange(10, 200);
        int withBoth = 0;

        for (int i = 0; i < numEntities; ++i) {
            auto e = world.createEntity();
            world.addComponent<Position>(e, static_cast<float>(i), 0.0f, 0.0f);
            if (*rc::gen::arbitrary<bool>()) {
                world.addComponent<Velocity>(e, 1.0f, 0.0f, 0.0f);
                ++withBoth;
            }
        }

        size_t count = 0;
        for (auto [pos, vel] : world.query<Position, Velocity>()) {
            (void)pos; (void)vel;
            ++count;
        }
        RC_ASSERT(count == static_cast<size_t>(withBoth));
    });
}
