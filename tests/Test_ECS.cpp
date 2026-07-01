// =============================================================================
// tests/Test_ECS.cpp — ECS Unit Tests (P5-FIX)
// =============================================================================
#include "TestMain.h"
#include "../ecs/ecs_EcsWorld.h"
#include "../ecs/Components.h"
#include "../ecs/ecs_Query.h"

#include <thread>
#include <vector>

namespace tests {

// =============================================================================
// TEST: Entity Creation & Destruction
// =============================================================================
TEST(EntityCreation) {
    ecs::EcsWorld world;
    world.Initialize();

    // Create 1000 entities
    std::vector<ecs::EntityHandle> handles;
    for (int i = 0; i < 1000; ++i) {
        auto handle = world.CreateEntity();
        TEST_ASSERT(handle != ecs::INVALID_ENTITY);
        handles.push_back(handle);
    }

    TEST_ASSERT_EQ(1000u, world.GetEntityCount());

    // Destroy all
    for (auto& h : handles) {
        world.DestroyEntity(h);
    }

    TEST_ASSERT_EQ(0u, world.GetEntityCount());
    world.Shutdown();
}

// =============================================================================
// TEST: Component Add/Remove
// =============================================================================
TEST(ComponentAddRemove) {
    ecs::EcsWorld world;
    world.Initialize();

    auto entity = world.CreateEntity();

    // Add PositionComponent
    world.AddComponent(entity, ecs::PositionComponent{1.0f, 2.0f, 3.0f});
    auto* pos = world.GetComponent<ecs::PositionComponent>(entity);
    TEST_ASSERT(pos != nullptr);
    TEST_ASSERT_EQ(1.0f, pos->x);
    TEST_ASSERT_EQ(2.0f, pos->y);
    TEST_ASSERT_EQ(3.0f, pos->z);

    // Add HealthComponent
    world.AddComponent(entity, ecs::HealthComponent{100, 100, true});
    auto* health = world.GetComponent<ecs::HealthComponent>(entity);
    TEST_ASSERT(health != nullptr);
    TEST_ASSERT_EQ(100, health->currentHP);

    // Remove PositionComponent
    world.RemoveComponent<ecs::PositionComponent>(entity);
    pos = world.GetComponent<ecs::PositionComponent>(entity);
    TEST_ASSERT(pos == nullptr);

    // Health should still exist
    health = world.GetComponent<ecs::HealthComponent>(entity);
    TEST_ASSERT(health != nullptr);

    world.Shutdown();
}

// =============================================================================
// TEST: Query System
// =============================================================================
TEST(QuerySystem) {
    ecs::EcsWorld world;
    world.Initialize();

    // Create entities with different component combinations
    auto e1 = world.CreateEntity();
    world.AddComponent(e1, ecs::PositionComponent{0.0f, 0.0f, 0.0f});
    world.AddComponent(e1, ecs::VelocityComponent{1.0f, 0.0f, 0.0f});

    auto e2 = world.CreateEntity();
    world.AddComponent(e2, ecs::PositionComponent{1.0f, 1.0f, 1.0f});
    world.AddComponent(e2, ecs::VelocityComponent{0.0f, 1.0f, 0.0f});

    auto e3 = world.CreateEntity();
    world.AddComponent(e3, ecs::PositionComponent{2.0f, 2.0f, 2.0f});
    // No Velocity

    // Query Position + Velocity
    auto query = world.QueryEntities<ecs::PositionComponent, ecs::VelocityComponent>();
    size_t count = 0;
    for (auto [entity, pos, vel] : query) {
        (void)entity;
        (void)pos;
        (void)vel;
        count++;
    }
    TEST_ASSERT_EQ(2u, count);

    // Query only Position
    auto query2 = world.QueryEntities<ecs::PositionComponent>();
    count = 0;
    for (auto [entity, pos] : query2) {
        (void)entity;
        (void)pos;
        count++;
    }
    TEST_ASSERT_EQ(3u, count);

    world.Shutdown();
}

// =============================================================================
// TEST: Archetype Migration
// =============================================================================
TEST(ArchetypeMigration) {
    ecs::EcsWorld world;
    world.Initialize();

    auto entity = world.CreateEntity();
    world.AddComponent(entity, ecs::PositionComponent{0.0f, 0.0f, 0.0f});

    // Check initial archetype count
    size_t archetypesBefore = world.GetArchetypeCount();

    // Add Velocity → should create new archetype
    world.AddComponent(entity, ecs::VelocityComponent{1.0f, 0.0f, 0.0f});
    size_t archetypesAfter = world.GetArchetypeCount();
    TEST_ASSERT(archetypesAfter > archetypesBefore);

    // Remove Velocity → should migrate back
    world.RemoveComponent<ecs::VelocityComponent>(entity);
    size_t archetypesFinal = world.GetArchetypeCount();
    TEST_ASSERT_EQ(archetypesBefore, archetypesFinal);

    world.Shutdown();
}

// =============================================================================
// TEST: Thread Safety
// =============================================================================
TEST(ThreadSafety) {
    ecs::EcsWorld world;
    world.Initialize();

    std::vector<std::thread> threads;
    std::atomic<int> successCount{0};

    // 4 threads creating entities concurrently
    for (int t = 0; t < 4; ++t) {
        threads.emplace_back([&world, &successCount]() {
            for (int i = 0; i < 100; ++i) {
                auto entity = world.CreateEntity();
                if (entity != ecs::INVALID_ENTITY) {
                    world.AddComponent(entity, ecs::PositionComponent{
                        static_cast<float>(i), 0.0f, 0.0f
                    });
                    successCount++;
                }
            }
        });
    }

    for (auto& t : threads) {
        t.join();
    }

    TEST_ASSERT_EQ(400, successCount.load());
    TEST_ASSERT_EQ(400u, world.GetEntityCount());

    world.Shutdown();
}

// =============================================================================
// TEST: Memory Usage
// =============================================================================
TEST(MemoryUsage) {
    ecs::EcsWorld world;
    world.Initialize();

    size_t memBefore = world.GetTotalMemoryUsage();

    // Create 1000 entities with 3 components each
    for (int i = 0; i < 1000; ++i) {
        auto entity = world.CreateEntity();
        world.AddComponent(entity, ecs::PositionComponent{0.0f, 0.0f, 0.0f});
        world.AddComponent(entity, ecs::HealthComponent{100, 100, true});
        world.AddComponent(entity, ecs::NameComponent{"TestEntity"});
    }

    size_t memAfter = world.GetTotalMemoryUsage();
    TEST_ASSERT(memAfter > memBefore);

    // Memory per entity should be reasonable (< 1KB)
    size_t memPerEntity = (memAfter - memBefore) / 1000;
    TEST_ASSERT(memPerEntity < 1024);

    world.Shutdown();
}

// =============================================================================
// BENCHMARK: Entity Creation
// =============================================================================
BENCHMARK(EntityCreation, 10000) {
    ecs::EcsWorld world;
    world.Initialize();

    for (int i = 0; i < 10000; ++i) {
        auto entity = world.CreateEntity();
        world.AddComponent(entity, ecs::PositionComponent{0.0f, 0.0f, 0.0f});
    }

    world.Shutdown();
}

// =============================================================================
// BENCHMARK: Query Iteration
// =============================================================================
BENCHMARK(QueryIteration, 1000) {
    ecs::EcsWorld world;
    world.Initialize();

    // Setup
    for (int i = 0; i < 10000; ++i) {
        auto entity = world.CreateEntity();
        world.AddComponent(entity, ecs::PositionComponent{0.0f, 0.0f, 0.0f});
        world.AddComponent(entity, ecs::VelocityComponent{1.0f, 0.0f, 0.0f});
    }

    // Benchmark query
    for (int iter = 0; iter < 1000; ++iter) {
        auto query = world.QueryEntities<ecs::PositionComponent, ecs::VelocityComponent>();
        for (auto [entity, pos, vel] : query) {
            (void)entity;
            pos->x += vel.vx * 0.016f;
        }
    }

    world.Shutdown();
}

} // namespace tests
