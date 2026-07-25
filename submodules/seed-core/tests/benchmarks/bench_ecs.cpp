#include "core/ecs/world.h"
#include "core/ecs/component_traits.h"
#include "core/memory/block_allocator.h"
#include <chrono>
#include <iostream>
#include <cstdlib>

using namespace seed::ecs;
using namespace seed::memory;

struct Position {
    float x, y, z;
    Position() = default;
    Position(float x_, float y_, float z_) : x(x_), y(y_), z(z_) {}
};

struct Velocity {
    float vx, vy, vz;
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

struct Name {
    char data[32];
    Name() { data[0] = '\0'; }
    explicit Name(const char* s) {
        size_t len = std::strlen(s);
        if (len > 31) len = 31;
        std::memcpy(data, s, len);
        data[len] = '\0';
    }
};

SEED_REGISTER_COMPONENT_WITH_ID(Position, 1)
SEED_REGISTER_COMPONENT_WITH_ID(Velocity, 2)
SEED_REGISTER_COMPONENT_WITH_ID(Health, 3)
SEED_REGISTER_COMPONENT_WITH_ID(Armor, 4)
SEED_REGISTER_COMPONENT_WITH_ID(Name, 5)

// 10 Systeme (Spez: "10 Systeme verarbeiten 100k Entities in <16ms")
struct Sys1_Movement : System {
    void onUpdate(World* w, float dt) override {
        for (auto [pos, vel] : w->query<Position, Velocity>()) {
            pos->x += vel->vx * dt;
            pos->y += vel->vy * dt;
            pos->z += vel->vz * dt;
        }
    }
};
struct Sys2_HealthRegen : System {
    void onUpdate(World* w, float) override {
        for (auto [h] : w->query<Health>()) { if (h->hp < h->maxHp) h->hp += 1; }
    }
};
struct Sys3_Combat : System {
    void onUpdate(World* w, float) override {
        for (auto [h, a] : w->query<Health, Armor>()) { (void)h; (void)a; }
    }
};
struct Sys4_PositionClamp : System {
    void onUpdate(World* w, float) override {
        for (auto [p] : w->query<Position>()) {
            if (p->x > 1000.0f) p->x = 1000.0f;
        }
    }
};
struct Sys5_VelocityDamp : System {
    void onUpdate(World* w, float dt) override {
        for (auto [v] : w->query<Velocity>()) {
            v->vx *= 0.99f;
            v->vy *= 0.99f;
            v->vz *= 0.99f;
            (void)dt;
        }
    }
};
struct Sys6_NameTag : System {
    void onUpdate(World* w, float) override {
        for (auto [n] : w->query<Name>()) { (void)n; }
    }
};
struct Sys7_HealthArmor : System {
    void onUpdate(World* w, float) override {
        for (auto [h, a] : w->query<Health, Armor>()) { h->hp += a->value; }
    }
};
struct Sys8_PosVelHealth : System {
    void onUpdate(World* w, float) override {
        for (auto [p, v, h] : w->query<Position, Velocity, Health>()) {
            (void)p; (void)v; (void)h;
        }
    }
};
struct Sys9_PosArmor : System {
    void onUpdate(World* w, float) override {
        for (auto [p, a] : w->query<Position, Armor>()) { (void)p; (void)a; }
    }
};
struct Sys10_AllThree : System {
    void onUpdate(World* w, float) override {
        for (auto [p, v, h, a] : w->query<Position, Velocity, Health, Armor>()) {
            (void)p; (void)v; (void)h; (void)a;
        }
    }
};

int main() {
    BlockAllocator blockAlloc;
    World world(&blockAlloc);

    TypeRegistry::instance().registerComponent<Position>();
    TypeRegistry::instance().registerComponent<Velocity>();
    TypeRegistry::instance().registerComponent<Health>();
    TypeRegistry::instance().registerComponent<Armor>();
    TypeRegistry::instance().registerComponent<Name>();

    // -----------------------------------------------------------------------
    // Spez-Ziel: 100k Entities erstellen in < 100ms
    // -----------------------------------------------------------------------
    {
        auto start = std::chrono::high_resolution_clock::now();
        for (size_t i = 0; i < 100'000; ++i) {
            Entity e = world.createEntity();
            world.addComponent<Position>(e, static_cast<float>(i), 0.0f, 0.0f);
            world.addComponent<Velocity>(e, 1.0f, 0.0f, 0.0f);
            world.addComponent<Health>(e);
            world.addComponent<Armor>(e);
            world.addComponent<Name>(e, "Entity");
        }
        auto elapsed = std::chrono::high_resolution_clock::now() - start;
        auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(elapsed).count();
        std::cout << "100k entities created: " << ms << " ms\n";

        if (ms >= 100) {
            std::cerr << "FAIL: 100k Entity creation took " << ms << " ms (spec: <100ms)\n";
            return 1;
        }
    }

    // -----------------------------------------------------------------------
    // Spez-Ziel: Memory < 50MB fuer 100k Entities (5 Komponenten)
    // -----------------------------------------------------------------------
    size_t memUsed = blockAlloc.totalUsed();
    std::cout << "Memory used: " << (static_cast<double>(memUsed) / (1024.0 * 1024.0)) << " MiB\n";
    if (memUsed > 50 * 1024 * 1024) {
        std::cerr << "FAIL: Memory usage " << (memUsed / (1024*1024))
                  << " MiB exceeds spec limit of 50 MiB\n";
        return 1;
    }

    // -----------------------------------------------------------------------
    // Spez-Ziel: 10 Systeme verarbeiten 100k Entities in < 16ms (60 FPS)
    // -----------------------------------------------------------------------
    world.registerSystem(std::make_unique<Sys1_Movement>());
    world.registerSystem(std::make_unique<Sys2_HealthRegen>());
    world.registerSystem(std::make_unique<Sys3_Combat>());
    world.registerSystem(std::make_unique<Sys4_PositionClamp>());
    world.registerSystem(std::make_unique<Sys5_VelocityDamp>());
    world.registerSystem(std::make_unique<Sys6_NameTag>());
    world.registerSystem(std::make_unique<Sys7_HealthArmor>());
    world.registerSystem(std::make_unique<Sys8_PosVelHealth>());
    world.registerSystem(std::make_unique<Sys9_PosArmor>());
    world.registerSystem(std::make_unique<Sys10_AllThree>());

    {
        auto start = std::chrono::high_resolution_clock::now();
        world.update(1.0f / 60.0f);
        auto elapsed = std::chrono::high_resolution_clock::now() - start;
        auto us = std::chrono::duration_cast<std::chrono::microseconds>(elapsed).count();
        float ms = static_cast<float>(us) / 1000.0f;
        std::cout << "10 Systems @ 100k entities: " << ms << " ms\n";

        if (ms >= 16.0f) {
            std::cerr << "FAIL: 10 Systems took " << ms
                      << " ms (spec: <16ms for 60 FPS)\n";
            return 1;
        }
    }

    // -----------------------------------------------------------------------
    // Spez-Ziel: Archetype-Wechsel < 10us pro Entity (gemittelt)
    // -----------------------------------------------------------------------
    {
        Entity e = world.createEntity();
        world.addComponent<Position>(e, 0.0f, 0.0f, 0.0f);

        auto start = std::chrono::high_resolution_clock::now();
        for (int i = 0; i < 1000; ++i) {
            if (i % 2 == 0) world.addComponent<Velocity>(e, 1.0f, 0.0f, 0.0f);
            else            world.removeComponent<Velocity>(e);
        }
        auto elapsed = std::chrono::high_resolution_clock::now() - start;
        auto us = std::chrono::duration_cast<std::chrono::microseconds>(elapsed).count();
        float usPerChange = static_cast<float>(us) / 1000.0f;
        std::cout << "Archetype change: " << usPerChange << " us/entity\n";

        if (usPerChange >= 10.0f) {
            std::cerr << "FAIL: Archetype change took " << usPerChange
                      << " us/entity (spec: <10us)\n";
            return 1;
        }
    }

    std::cout << "\n=== ALL ECS BENCHMARKS PASSED ===\n";
    return 0;
}
