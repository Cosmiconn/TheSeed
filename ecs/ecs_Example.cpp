// =============================================================================
// ecs/Example.cpp — ECS Usage Example (AP-20)
// Demonstrates archetype storage, queries, and component operations
// =============================================================================
#include "ECS.h"
#include <iostream>
#include <format>

// Example components
struct Position {
    float x = 0.0f, y = 0.0f, z = 0.0f;
};

struct Velocity {
    float vx = 0.0f, vy = 0.0f, vz = 0.0f;
};

struct Health {
    int current = 100;
    int max = 100;
};

struct Name {
    char value[32] = {};
};

void ExampleECSUsage() {
    ecs::EcsWorld world;

    // Create entities with different archetypes
    // Archetype: Position + Velocity + Health
    auto hero = world.CreateEntity(
        Position{0.0f, 0.0f, 0.0f},
        Velocity{1.0f, 0.0f, 0.0f},
        Health{100, 100}
    );

    // Archetype: Position + Health (no velocity - static object)
    auto tree = world.CreateEntity(
        Position{10.0f, 0.0f, 5.0f},
        Health{50, 50}
    );

    // Archetype: Position + Velocity + Health + Name
    auto monster = world.CreateEntity(
        Position{20.0f, 0.0f, 10.0f},
        Velocity{-0.5f, 0.0f, 0.5f},
        Health{80, 80}
    );

    // Add Name component (archetype transition: 3 comps -> 4 comps)
    Name monsterName{};
    std::strncpy(monsterName.value, "Goblin", sizeof(monsterName.value) - 1);
    world.AddComponent(monster, monsterName);

    std::cout << std::format("Entities: {}, Archetypes: {}, ComponentTypes: {}\n",
                             world.GetEntityCount(),
                             world.GetArchetypeCount(),
                             world.GetComponentTypeCount());

    // Query all entities with Position + Velocity (movement system)
    std::cout << "\n=== Movement System ===\n";
    auto movers = world.Query<Position, Velocity>();
    movers.ForEach([](ecs::EntityHandle handle, Position* pos, Velocity* vel) {
        pos->x += vel->vx * 0.016f;
        pos->y += vel->vy * 0.016f;
        pos->z += vel->vz * 0.016f;
        std::cout << std::format("Entity {}: pos=({:.1f},{:.1f},{:.1f})\n",
                                 handle.index, pos->x, pos->y, pos->z);
    });

    // Query all entities with Health (health system)
    std::cout << "\n=== Health System ===\n";
    auto living = world.Query<Health>();
    living.ForEach([](ecs::EntityHandle handle, Health* hp) {
        std::cout << std::format("Entity {}: HP={}/{}\n",
                                 handle.index, hp->current, hp->max);
    });

    // Query entities with Position + Health but NO Velocity (static objects)
    std::cout << "\n=== Static Objects ===\n";
    auto staticObjects = world.QueryWithExclusion<Position, Health, Velocity>();
    staticObjects.ForEach([](ecs::EntityHandle handle, Position* pos, Health* hp) {
        std::cout << std::format("Static Entity {}: pos=({:.1f},{:.1f},{:.1f}) HP={}\n",
                                 handle.index, pos->x, pos->y, pos->z, hp->current);
    });

    // Remove Velocity from monster (archetype transition: 4 comps -> 3 comps)
    std::cout << "\n=== Removing Velocity from monster ===\n";
    world.RemoveComponent<Velocity>(monster);

    std::cout << std::format("After removal: Entities={}, Archetypes={}\n",
                             world.GetEntityCount(),
                             world.GetArchetypeCount());

    // Destroy entity
    world.DestroyEntity(tree);
    std::cout << std::format("After destroy: Entities={}\n", world.GetEntityCount());
}

// int main() { ExampleECSUsage(); return 0; }
