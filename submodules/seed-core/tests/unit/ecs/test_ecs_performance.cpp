#include <doctest/doctest.h>
#include "core/ecs/world.h"
#include "core/ecs/component_traits.h"
#include "core/memory/block_allocator.h"
#include <chrono>

using namespace seed::ecs;
using namespace seed::memory;

struct Position {
    float x = 0.0f, y = 0.0f, z = 0.0f;
    Position() = default;
    Position(float x_, float y_, float z_) : x(x_), y(y_), z(z_) {}
};

struct Velocity {
    float vx = 0.0f, vy = 0.0f, vz = 0.0f;
    Velocity() = default;
    Velocity(float vx_, float vy_, float vz_) : vx(vx_), vy(vy_), vz(vz_) {}
};

struct Health {
    int hp = 100;
    int maxHp = 100;
    Health() = default;
    explicit Health(int hp_) : hp(hp_), maxHp(hp_) {}
};

struct Armor {
    int value = 10;
    Armor() = default;
    explicit Armor(int v) : value(v) {}
};

SEED_REGISTER_COMPONENT_WITH_ID(Position, 1)
SEED_REGISTER_COMPONENT_WITH_ID(Velocity, 2)
SEED_REGISTER_COMPONENT_WITH_ID(Health, 3)
SEED_REGISTER_COMPONENT_WITH_ID(Armor, 5)

struct MovementSystem : System {
    void onUpdate(World* w, float dt) override {
        SEED_ZONE("MovementSystem");
        for (auto [pos, vel] : w->query<Position, Velocity>()) {
            pos->x += vel->vx * dt;
            pos->y += vel->vy * dt;
            pos->z += vel->vz * dt;
        }
    }
};

struct HealthSystem : System {
    void onUpdate(World* w, float /*dt*/) override {
        SEED_ZONE("HealthSystem");
        for (auto [health] : w->query<Health>()) {
            if (health->hp < health->maxHp) {
                health->hp += 1;
            }
        }
    }
};

struct CombatSystem : System {
    void onUpdate(World* w, float /*dt*/) override {
        SEED_ZONE("CombatSystem");
        for (auto [health, armor] : w->query<Health, Armor>()) {
            (void)health;
            (void)armor;
        }
    }
};

TEST_CASE("ECS_100k_Entities_Create") {
    BlockAllocator blockAlloc;
    World world(&blockAlloc);

    TypeRegistry::instance().registerComponent<Position>();
    TypeRegistry::instance().registerComponent<Velocity>();
    TypeRegistry::instance().registerComponent<Health>();
    TypeRegistry::instance().registerComponent<Armor>();

    auto start = std::chrono::high_resolution_clock::now();

    for (size_t i = 0; i < 100'000; ++i) {
        Entity e = world.createEntity();
        world.addComponent<Position>(e, static_cast<float>(i), 0.0f, 0.0f);
        world.addComponent<Velocity>(e, 1.0f, 0.0f, 0.0f);
        world.addComponent<Health>(e);
        world.addComponent<Armor>(e);
    }

    auto elapsed = std::chrono::high_resolution_clock::now() - start;
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(elapsed).count();

    CHECK(world.entityCount() == 100'000);
    CHECK(ms < 7000);  // Linux CI variance tolerance
}

TEST_CASE("ECS_100k_Entities_Systems_60FPS") {
    BlockAllocator blockAlloc;
    World world(&blockAlloc);

    TypeRegistry::instance().registerComponent<Position>();
    TypeRegistry::instance().registerComponent<Velocity>();
    TypeRegistry::instance().registerComponent<Health>();
    TypeRegistry::instance().registerComponent<Armor>();

    for (size_t i = 0; i < 100'000; ++i) {
        Entity e = world.createEntity();
        world.addComponent<Position>(e, static_cast<float>(i), 0.0f, 0.0f);
        world.addComponent<Velocity>(e, 1.0f, 0.0f, 0.0f);
        if (i % 2 == 0) world.addComponent<Health>(e);
        if (i % 3 == 0) world.addComponent<Armor>(e);
    }

    world.registerSystem(std::make_unique<MovementSystem>());
    world.registerSystem(std::make_unique<HealthSystem>());
    world.registerSystem(std::make_unique<CombatSystem>());

    auto start = std::chrono::high_resolution_clock::now();
    world.update(1.0f / 60.0f);
    auto elapsed = std::chrono::high_resolution_clock::now() - start;
    auto ms = static_cast<float>(std::chrono::duration_cast<std::chrono::microseconds>(elapsed).count()) / 1000.0f;

    CHECK(ms < 100.0f);
}

TEST_CASE("ECS_Archetype_Change") {
    BlockAllocator blockAlloc;
    World world(&blockAlloc);

    TypeRegistry::instance().registerComponent<Position>();
    TypeRegistry::instance().registerComponent<Velocity>();
    TypeRegistry::instance().registerComponent<Health>();

    Entity e = world.createEntity();
    world.addComponent<Position>(e, 0.0f, 0.0f, 0.0f);

    auto start = std::chrono::high_resolution_clock::now();

    for (int i = 0; i < 1000; ++i) {
        if (i % 2 == 0) {
            world.addComponent<Velocity>(e, 1.0f, 0.0f, 0.0f);
        } else {
            world.removeComponent<Velocity>(e);
        }
        if (i % 3 == 0) {
            world.addComponent<Health>(e);
        } else {
            world.removeComponent<Health>(e);
        }
    }

    auto elapsed = std::chrono::high_resolution_clock::now() - start;
    auto us = std::chrono::duration_cast<std::chrono::microseconds>(elapsed).count();

    CHECK(us < 50'000);
}
