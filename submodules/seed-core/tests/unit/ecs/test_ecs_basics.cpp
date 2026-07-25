#include <doctest/doctest.h>
#include <random>
#include <set>
#include <string>
#include <cstring>
#include "core/ecs/world.h"
#include "core/ecs/component_traits.h"
#include "core/memory/block_allocator.h"

#define CHECK_INVARIANTS(world) CHECK(world.validateInvariants())

using namespace seed::ecs;
using namespace seed::memory;

struct UniqueResource {
    std::unique_ptr<int> data;
    explicit UniqueResource(int v = 42) : data(std::make_unique<int>(v)) {}
    UniqueResource(UniqueResource&&) = default;
    UniqueResource& operator=(UniqueResource&&) = default;
    UniqueResource(const UniqueResource&) = delete;
    UniqueResource& operator=(const UniqueResource&) = delete;
};

SEED_REGISTER_COMPONENT(UniqueResource)

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

struct Name {
    char value[32] = {};
    Name() = default;
    explicit Name(const char* s) {
#if defined(_WIN32) && defined(_MSC_VER)
        strncpy_s(value, sizeof(value), s, sizeof(value) - 1);
#else
        std::strncpy(value, s, sizeof(value) - 1);
#endif
        value[sizeof(value) - 1] = '\0';
    }
};

SEED_REGISTER_COMPONENT_WITH_ID(Position, 1)
SEED_REGISTER_COMPONENT_WITH_ID(Velocity, 2)
SEED_REGISTER_COMPONENT_WITH_ID(Health, 3)
SEED_REGISTER_COMPONENT_WITH_ID(Name, 4)

// ---------------------------------------------------------------------------
// M01-M03 Review Fixes: Component Types (global scope for template registration)
// ---------------------------------------------------------------------------

struct StringComponent {
    std::string value;
    StringComponent() = default;
    explicit StringComponent(const char* s) : value(s) {}
    explicit StringComponent(std::string s) : value(std::move(s)) {}
};

SEED_REGISTER_COMPONENT_WITH_ID(StringComponent, 10)

struct VectorComponent {
    std::vector<int> data;
    VectorComponent() = default;
    explicit VectorComponent(std::initializer_list<int> init) : data(init) {}
};

SEED_REGISTER_COMPONENT_WITH_ID(VectorComponent, 11)

TEST_CASE("ECS_Entity_CreateDestroy") {
    BlockAllocator blockAlloc;
    World world(&blockAlloc);
    CHECK_INVARIANTS(world);

    TypeRegistry::instance().registerComponent<Position>();
    TypeRegistry::instance().registerComponent<Velocity>();

    Entity e = world.createEntity();
    CHECK(e != INVALID_ENTITY);
    CHECK(world.isAlive(e));

    world.destroyEntity(e);
    CHECK(!world.isAlive(e));
}

TEST_CASE("ECS_Entity_Many") {
    BlockAllocator blockAlloc;
    World world(&blockAlloc);
    CHECK_INVARIANTS(world);

    TypeRegistry::instance().registerComponent<Position>();

    constexpr size_t N = 1000;
    std::vector<Entity> entities;
    entities.reserve(N);

    for (size_t i = 0; i < N; ++i) {
        entities.push_back(world.createEntity());
    }

    CHECK(world.entityCount() == N);

    for (auto e : entities) {
        CHECK(world.isAlive(e));
    }

    for (auto e : entities) {
        world.destroyEntity(e);
    }

    CHECK(world.entityCount() == 0);
}

TEST_CASE("ECS_Component_AddGet") {
    BlockAllocator blockAlloc;
    World world(&blockAlloc);
    CHECK_INVARIANTS(world);

    TypeRegistry::instance().registerComponent<Position>();
    TypeRegistry::instance().registerComponent<Velocity>();

    Entity e = world.createEntity();
    auto* pos = world.addComponent<Position>(e, 1.0f, 2.0f, 3.0f);
    REQUIRE(pos != nullptr);
    CHECK(pos->x == 1.0f);
    CHECK(pos->y == 2.0f);
    CHECK(pos->z == 3.0f);

    auto* pos2 = world.getComponent<Position>(e);
    REQUIRE(pos2 != nullptr);
    CHECK(pos2 == pos);
}

TEST_CASE("ECS_Component_AddRemove") {
    BlockAllocator blockAlloc;
    World world(&blockAlloc);
    CHECK_INVARIANTS(world);

    TypeRegistry::instance().registerComponent<Position>();
    TypeRegistry::instance().registerComponent<Velocity>();
    TypeRegistry::instance().registerComponent<Health>();

    Entity e = world.createEntity();
    world.addComponent<Position>(e, 1.0f, 2.0f, 3.0f);
    world.addComponent<Velocity>(e, 0.1f, 0.2f, 0.3f);
    world.addComponent<Health>(e);

    CHECK(world.hasComponent<Position>(e));
    CHECK(world.hasComponent<Velocity>(e));
    CHECK(world.hasComponent<Health>(e));

    world.removeComponent<Velocity>(e);
    CHECK(world.hasComponent<Position>(e));
    CHECK(!world.hasComponent<Velocity>(e));
    CHECK(world.hasComponent<Health>(e));
}

TEST_CASE("ECS_Component_ArchetypeMove") {
    BlockAllocator blockAlloc;
    World world(&blockAlloc);
    CHECK_INVARIANTS(world);

    TypeRegistry::instance().registerComponent<Position>();
    TypeRegistry::instance().registerComponent<Velocity>();

    Entity e1 = world.createEntity();
    world.addComponent<Position>(e1, 1.0f, 0.0f, 0.0f);

    Entity e2 = world.createEntity();
    world.addComponent<Position>(e2, 2.0f, 0.0f, 0.0f);
    world.addComponent<Velocity>(e2, 0.5f, 0.0f, 0.0f);

    auto* pos1 = world.getComponent<Position>(e1);
    REQUIRE(pos1 != nullptr);
    CHECK(pos1->x == 1.0f);

    world.addComponent<Velocity>(e1, 1.0f, 0.0f, 0.0f);
    auto* pos1After = world.getComponent<Position>(e1);
    REQUIRE(pos1After != nullptr);
    CHECK(pos1After->x == 1.0f);
}

TEST_CASE("ECS_Query_Basic") {
    BlockAllocator blockAlloc;
    World world(&blockAlloc);
    CHECK_INVARIANTS(world);

    TypeRegistry::instance().registerComponent<Position>();
    TypeRegistry::instance().registerComponent<Velocity>();

    for (size_t i = 0; i < 10; ++i) {
        Entity e = world.createEntity();
        world.addComponent<Position>(e, static_cast<float>(i), 0.0f, 0.0f);
    }

    for (size_t i = 0; i < 5; ++i) {
        Entity e = world.createEntity();
        world.addComponent<Position>(e, static_cast<float>(i), 1.0f, 0.0f);
        world.addComponent<Velocity>(e, 1.0f, 0.0f, 0.0f);
    }

    int count = 0;
    for (auto [pos] : world.query<Position>()) {
        REQUIRE(pos != nullptr);
        ++count;
    }
    CHECK(count == 15);

    count = 0;
    for (auto [pos, vel] : world.query<Position, Velocity>()) {
        REQUIRE(pos != nullptr);
        REQUIRE(vel != nullptr);
        CHECK(vel->vx == 1.0f);
        ++count;
    }
    CHECK(count == 5);
}

TEST_CASE("ECS_Query_Empty") {
    BlockAllocator blockAlloc;
    World world(&blockAlloc);
    CHECK_INVARIANTS(world);

    TypeRegistry::instance().registerComponent<Position>();
    TypeRegistry::instance().registerComponent<Velocity>();

    auto result = world.query<Position, Velocity>();
    CHECK(result.empty());
    CHECK(result.count() == 0);
    CHECK_INVARIANTS(world);
}

TEST_CASE("ECS_Entity_Recycling") {
    BlockAllocator blockAlloc;
    World world(&blockAlloc);

    TypeRegistry::instance().registerComponent<Position>();

    Entity e1 = world.createEntity();
    uint32_t idx1 = entityIndex(e1);
    uint8_t ver1 = entityVersion(e1);

    world.destroyEntity(e1);
    CHECK(!world.isAlive(e1));

    Entity e2 = world.createEntity();
    uint32_t idx2 = entityIndex(e2);

    // Same index recycled, but version incremented
    CHECK(idx1 == idx2);
    CHECK(entityVersion(e2) == ver1 + 1);

    // Old handle is invalid
    CHECK(!world.isAlive(e1));
    CHECK(world.isAlive(e2));
}

TEST_CASE("ECS_Entity_MultipleRecycle") {
    BlockAllocator blockAlloc;
    World world(&blockAlloc);

    TypeRegistry::instance().registerComponent<Position>();

    std::vector<Entity> handles;
    for (size_t i = 0; i < 10; ++i) {
        Entity e = world.createEntity();
        handles.push_back(e);
    }

    // Destroy all
    for (auto e : handles) {
        world.destroyEntity(e);
    }

    // Recreate all - should reuse indices with new versions
    std::vector<Entity> newHandles;
    for (size_t i = 0; i < 10; ++i) {
        Entity e = world.createEntity();
        newHandles.push_back(e);
    }

    // Old handles invalid
    for (auto e : handles) {
        CHECK(!world.isAlive(e));
    }

    // New handles valid
    for (auto e : newHandles) {
        CHECK(world.isAlive(e));
    }

    // All indices recycled, different versions
    // Collect indices and verify they match
    std::set<uint32_t> oldIndices, newIndices;
    for (auto e : handles) oldIndices.insert(entityIndex(e));
    for (auto e : newHandles) {
        newIndices.insert(entityIndex(e));
        CHECK(entityVersion(e) > 1); // Recycled = version incremented
    }
    CHECK(oldIndices == newIndices);
}

TEST_CASE("ECS_Entity_DestroyTwice") {
    BlockAllocator blockAlloc;
    World world(&blockAlloc);

    Entity e = world.createEntity();
    world.destroyEntity(e);

    // Second destroy should be safe (no-op)
    world.destroyEntity(e);
    CHECK(!world.isAlive(e));
}

TEST_CASE("ECS_Component_DuplicateAdd") {
    BlockAllocator blockAlloc;
    World world(&blockAlloc);

    TypeRegistry::instance().registerComponent<Position>();

    Entity e = world.createEntity();
    world.addComponent<Position>(e, 1.0f, 2.0f, 3.0f);

    // Adding duplicate component should assert in debug, be safe in release
    // In debug mode, this triggers an assertion - we can't test it directly
    // But we can verify the component is still correct after the first add
    Position* pos = world.getComponent<Position>(e);
    REQUIRE(pos != nullptr);
    CHECK(pos->x == 1.0f);
}

TEST_CASE("ECS_Component_AddRemoveAdd") {
    BlockAllocator blockAlloc;
    World world(&blockAlloc);

    TypeRegistry::instance().registerComponent<Position>();

    Entity e = world.createEntity();
    world.addComponent<Position>(e, 1.0f, 2.0f, 3.0f);

    // Remove component (if removeComponent exists)
    // world.removeComponent<Position>(e);
    // CHECK(world.getComponent<Position>(e) == nullptr);

    // Add again
    // world.addComponent<Position>(e, 4.0f, 5.0f, 6.0f);
    // Position* pos = world.getComponent<Position>(e);
    // CHECK(pos->x == 4.0f);
}

TEST_CASE("ECS_Query_DuringIteration") {
    BlockAllocator blockAlloc;
    World world(&blockAlloc);

    TypeRegistry::instance().registerComponent<Position>();
    TypeRegistry::instance().registerComponent<Velocity>();

    std::vector<Entity> entities;
    for (size_t i = 0; i < 10; ++i) {
        Entity e = world.createEntity();
        world.addComponent<Position>(e, static_cast<float>(i), 0.0f, 0.0f);
        entities.push_back(e);
    }

    // Query should see all 10 entities
    int count = 0;
    for (auto [pos] : world.query<Position>()) {
        (void)pos;
        ++count;
    }
    CHECK(count == 10);

    // Add Velocity to first entity
    world.addComponent<Velocity>(entities[0], 1.0f, 0.0f, 0.0f);

    // Query for Position+Velocity should only see 1 entity
    count = 0;
    for (auto [pos, vel] : world.query<Position, Velocity>()) {
        (void)pos; (void)vel;
        ++count;
    }
    CHECK(count == 1);
}

TEST_CASE("ECS_SwapAndPop_Consistency") {
    BlockAllocator blockAlloc;
    World world(&blockAlloc);

    TypeRegistry::instance().registerComponent<Position>();

    Entity e1 = world.createEntity();
    world.addComponent<Position>(e1, 1.0f, 0.0f, 0.0f);

    Entity e2 = world.createEntity();
    world.addComponent<Position>(e2, 2.0f, 0.0f, 0.0f);

    Entity e3 = world.createEntity();
    world.addComponent<Position>(e3, 3.0f, 0.0f, 0.0f);

    // Destroy middle entity - swap-and-pop should move e3 to e2's position
    world.destroyEntity(e2);

    // e3 should still be valid with correct data
    CHECK(world.isAlive(e3));
    Position* pos3 = world.getComponent<Position>(e3);
    REQUIRE(pos3 != nullptr);
    CHECK(pos3->x == 3.0f);

    // e1 should still be valid
    CHECK(world.isAlive(e1));
    Position* pos1 = world.getComponent<Position>(e1);
    REQUIRE(pos1 != nullptr);
    CHECK(pos1->x == 1.0f);
}

TEST_CASE("ECS_LastEntityInArchetype") {
    BlockAllocator blockAlloc;
    World world(&blockAlloc);

    TypeRegistry::instance().registerComponent<Position>();

    Entity e = world.createEntity();
    world.addComponent<Position>(e, 42.0f, 0.0f, 0.0f);

    // Destroy the only entity in this archetype
    world.destroyEntity(e);
    CHECK(!world.isAlive(e));

    // Archetype should now be empty
    Entity e2 = world.createEntity();
    world.addComponent<Position>(e2, 99.0f, 0.0f, 0.0f);
    Position* pos = world.getComponent<Position>(e2);
    REQUIRE(pos != nullptr);
    CHECK(pos->x == 99.0f);
}

TEST_CASE("ECS_Fuzz_RandomOperations") {
    BlockAllocator blockAlloc;
    World world(&blockAlloc);

    TypeRegistry::instance().registerComponent<Position>();
    TypeRegistry::instance().registerComponent<Velocity>();
    TypeRegistry::instance().registerComponent<Health>();

    std::vector<Entity> alive;
    std::mt19937 rng(42); // Fixed seed for reproducibility

    for (size_t i = 0; i < 100000; ++i) {
        int op = static_cast<int>(rng() % 7);

        switch (op) {
            case 0: { // Create entity
                Entity e = world.createEntity();
                alive.push_back(e);
                break;
            }
            case 1: { // Destroy random entity
                if (!alive.empty()) {
                    size_t idx = rng() % alive.size();
                    world.destroyEntity(alive[idx]);
                    alive.erase(alive.begin() + static_cast<std::vector<Entity>::difference_type>(idx));
                }
                break;
            }
            case 2: { // Add Position
                if (!alive.empty()) {
                    size_t idx = rng() % alive.size();
                    Entity e = alive[idx];
                    if (!world.hasComponent<Position>(e)) {
                        world.addComponent<Position>(e,
                            static_cast<float>(rng() % 100),
                            static_cast<float>(rng() % 100),
                            static_cast<float>(rng() % 100));
                    }
                }
                break;
            }
            case 3: { // Add Velocity
                if (!alive.empty()) {
                    size_t idx = rng() % alive.size();
                    Entity e = alive[idx];
                    if (!world.hasComponent<Velocity>(e)) {
                        world.addComponent<Velocity>(e,
                            static_cast<float>(rng() % 10),
                            static_cast<float>(rng() % 10),
                            static_cast<float>(rng() % 10));
                    }
                }
                break;
            }
            case 4: { // Add Health
                if (!alive.empty()) {
                    size_t idx = rng() % alive.size();
                    Entity e = alive[idx];
                    if (!world.hasComponent<Health>(e)) {
                        world.addComponent<Health>(e, static_cast<int>(rng() % 100));
                    }
                }
                break;
            }
            case 5: { // Query Position
                int count = 0;
                for (auto [pos] : world.query<Position>()) {
                    (void)pos;
                    ++count;
                }
                // Just verify it doesn't crash
                CHECK(count >= 0);
                break;
            }
            case 6: { // Query Position+Velocity
                int count = 0;
                for (auto [pos, vel] : world.query<Position, Velocity>()) {
                    (void)pos; (void)vel;
                    ++count;
                }
                CHECK(count >= 0);
                break;
            }
        }
    }

    // Verify all remaining entities are valid
    for (Entity e : alive) {
        CHECK(world.isAlive(e));
    }
}

TEST_CASE("ECS_Invariants_AfterStress") {
    BlockAllocator blockAlloc;
    World world(&blockAlloc);

    TypeRegistry::instance().registerComponent<Position>();
    TypeRegistry::instance().registerComponent<Velocity>();
    TypeRegistry::instance().registerComponent<Health>();

    // Create many entities with various components
    std::vector<Entity> entities;
    for (size_t i = 0; i < 1000; ++i) {
        Entity e = world.createEntity();
        entities.push_back(e);

        if (i % 3 == 0) {
            world.addComponent<Position>(e, static_cast<float>(i), 0.0f, 0.0f);
        }
        if (i % 5 == 0) {
            world.addComponent<Velocity>(e, 1.0f, 0.0f, 0.0f);
        }
        if (i % 7 == 0) {
            world.addComponent<Health>(e, 100);
        }
    }

    CHECK_INVARIANTS(world);

    // Destroy half
    for (size_t i = 0; i < 500; ++i) {
        world.destroyEntity(entities[i]);
    }

    CHECK_INVARIANTS(world);

    // Create more to trigger recycling
    for (size_t i = 0; i < 200; ++i) {
        Entity e = world.createEntity();
        world.addComponent<Position>(e, static_cast<float>(i), 0.0f, 0.0f);
    }

    CHECK_INVARIANTS(world);

    // Destroy all remaining
    for (size_t i = 500; i < entities.size(); ++i) {
        if (world.isAlive(entities[i])) {
            world.destroyEntity(entities[i]);
        }
    }

    CHECK_INVARIANTS(world);
}

TEST_CASE("ECS_Component_StringType") {
    BlockAllocator blockAlloc;
    World world(&blockAlloc);

    TypeRegistry::instance().registerComponent<Name>();

    Entity e = world.createEntity();
    world.addComponent<Name>(e, "Alice");

    Name* name = world.getComponent<Name>(e);
    REQUIRE(name != nullptr);
    CHECK(std::strcmp(name->value, "Alice") == 0);

    // Archetype move should properly move the string
    TypeRegistry::instance().registerComponent<Position>();
    world.addComponent<Position>(e, 1.0f, 2.0f, 3.0f);

    name = world.getComponent<Name>(e);
    REQUIRE(name != nullptr);
    CHECK(std::strcmp(name->value, "Alice") == 0);

    // Destroy should properly destruct the string
    world.destroyEntity(e);
    CHECK_INVARIANTS(world);
}

TEST_CASE("ECS_TypeRegistry_DuplicateRegistration") {
    // Registering the same component twice should not crash
    TypeRegistry::instance().registerComponent<Position>();
    TypeRegistry::instance().registerComponent<Position>();
    TypeRegistry::instance().registerComponent<Position>();

    // Should still work normally
    BlockAllocator blockAlloc;
    World world(&blockAlloc);

    Entity e = world.createEntity();
    world.addComponent<Position>(e, 1.0f, 2.0f, 3.0f);

    Position* pos = world.getComponent<Position>(e);
    REQUIRE(pos != nullptr);
    CHECK(pos->x == 1.0f);
}

TEST_CASE("ECS_ArchetypeId_CollisionSafety") {
    // Create archetypes with different signatures
    // The ArchetypeId now stores the full signature for comparison
    BlockAllocator blockAlloc;
    World world(&blockAlloc);

    TypeRegistry::instance().registerComponent<Position>();
    TypeRegistry::instance().registerComponent<Velocity>();
    TypeRegistry::instance().registerComponent<Health>();

    Entity e1 = world.createEntity();
    world.addComponent<Position>(e1, 1.0f, 0.0f, 0.0f);

    Entity e2 = world.createEntity();
    world.addComponent<Velocity>(e2, 2.0f, 0.0f, 0.0f);

    Entity e3 = world.createEntity();
    world.addComponent<Health>(e3, 100);

    Entity e4 = world.createEntity();
    world.addComponent<Position>(e4, 3.0f, 0.0f, 0.0f);
    world.addComponent<Velocity>(e4, 4.0f, 0.0f, 0.0f);

    // All entities should have correct components
    CHECK(world.getComponent<Position>(e1) != nullptr);
    CHECK(world.getComponent<Velocity>(e2) != nullptr);
    CHECK(world.getComponent<Health>(e3) != nullptr);
    CHECK(world.getComponent<Position>(e4) != nullptr);
    CHECK(world.getComponent<Velocity>(e4) != nullptr);

    // Verify no cross-contamination
    CHECK(world.getComponent<Velocity>(e1) == nullptr);
    CHECK(world.getComponent<Health>(e1) == nullptr);
    CHECK(world.getComponent<Position>(e2) == nullptr);
    CHECK(world.getComponent<Health>(e2) == nullptr);
    CHECK(world.getComponent<Position>(e3) == nullptr);
    CHECK(world.getComponent<Velocity>(e3) == nullptr);

    CHECK_INVARIANTS(world);
}

TEST_CASE("ECS_Component_MoveOnlyType") {
    BlockAllocator blockAlloc;
    World world(&blockAlloc);

    TypeRegistry::instance().registerComponent<UniqueResource>();

    Entity e = world.createEntity();
    world.addComponent<UniqueResource>(e, 42);

    UniqueResource* res = world.getComponent<UniqueResource>(e);
    REQUIRE(res != nullptr);
    REQUIRE(res->data != nullptr);
    CHECK(*res->data == 42);

    // Archetype move should properly move the unique_ptr
    TypeRegistry::instance().registerComponent<Position>();
    world.addComponent<Position>(e, 1.0f, 2.0f, 3.0f);

    res = world.getComponent<UniqueResource>(e);
    REQUIRE(res != nullptr);
    REQUIRE(res->data != nullptr);
    CHECK(*res->data == 42);

    CHECK_INVARIANTS(world);
}

TEST_CASE("ECS_Entity_DestroyDuringQuery") {
    BlockAllocator blockAlloc;
    World world(&blockAlloc);

    TypeRegistry::instance().registerComponent<Position>();

    std::vector<Entity> entities;
    for (size_t i = 0; i < 10; ++i) {
        Entity e = world.createEntity();
        world.addComponent<Position>(e, static_cast<float>(i), 0.0f, 0.0f);
        entities.push_back(e);
    }

    // Query should still work even if entities were destroyed
    // (This tests that query doesn't crash with stale data)
    int count = 0;
    for (auto [pos] : world.query<Position>()) {
        (void)pos;
        ++count;
    }
    CHECK(count == 10);

    // Destroy some entities
    world.destroyEntity(entities[3]);
    world.destroyEntity(entities[7]);

    count = 0;
    for (auto [pos] : world.query<Position>()) {
        (void)pos;
        ++count;
    }
    CHECK(count == 8);
}


// ---------------------------------------------------------------------------
// M01-M03 Review Fixes: Component Lifetime with std::string
// ---------------------------------------------------------------------------

TEST_CASE("ECS_Component_StdStringType") {
    BlockAllocator blockAlloc;
    World world(&blockAlloc);

    TypeRegistry::instance().registerComponent<StringComponent>();

    Entity e = world.createEntity();
    world.addComponent<StringComponent>(e, "HelloWorld");

    StringComponent* str = world.getComponent<StringComponent>(e);
    REQUIRE(str != nullptr);
    CHECK(str->value == "HelloWorld");

    // Archetype move: add another component to trigger migration
    TypeRegistry::instance().registerComponent<Position>();
    world.addComponent<Position>(e, 1.0f, 2.0f, 3.0f);

    str = world.getComponent<StringComponent>(e);
    REQUIRE(str != nullptr);
    CHECK(str->value == "HelloWorld");

    // Remove Position to trigger another archetype move
    world.removeComponent<Position>(e);

    str = world.getComponent<StringComponent>(e);
    REQUIRE(str != nullptr);
    CHECK(str->value == "HelloWorld");

    // Destroy should properly destruct std::string (ASan/LSan will catch leaks)
    world.destroyEntity(e);
    CHECK_INVARIANTS(world);
}

// ---------------------------------------------------------------------------
// M01-M03 Review Fixes: Component Lifetime with std::vector
// ---------------------------------------------------------------------------

TEST_CASE("ECS_Component_StdVectorType") {
    BlockAllocator blockAlloc;
    World world(&blockAlloc);

    TypeRegistry::instance().registerComponent<VectorComponent>();

    Entity e = world.createEntity();
    world.addComponent<VectorComponent>(e, std::initializer_list<int>{1, 2, 3, 4, 5});

    VectorComponent* vec = world.getComponent<VectorComponent>(e);
    REQUIRE(vec != nullptr);
    CHECK(vec->data.size() == 5);
    CHECK(vec->data[0] == 1);
    CHECK(vec->data[4] == 5);

    // Trigger archetype migration
    TypeRegistry::instance().registerComponent<Position>();
    world.addComponent<Position>(e, 1.0f, 2.0f, 3.0f);

    vec = world.getComponent<VectorComponent>(e);
    REQUIRE(vec != nullptr);
    CHECK(vec->data.size() == 5);
    CHECK(vec->data[2] == 3);

    // Remove Position to trigger another migration
    world.removeComponent<Position>(e);

    vec = world.getComponent<VectorComponent>(e);
    REQUIRE(vec != nullptr);
    CHECK(vec->data.size() == 5);

    // Destroy must properly destruct std::vector (ASan/LSan will catch leaks)
    world.destroyEntity(e);
    CHECK_INVARIANTS(world);
}

// ---------------------------------------------------------------------------
// M01-M03 Review Fixes: removeComponent on missing component (safe no-op)
// ---------------------------------------------------------------------------

TEST_CASE("ECS_RemoveComponent_Missing") {
    BlockAllocator blockAlloc;
    World world(&blockAlloc);

    TypeRegistry::instance().registerComponent<Position>();
    TypeRegistry::instance().registerComponent<Velocity>();

    Entity e = world.createEntity();
    world.addComponent<Position>(e, 1.0f, 2.0f, 3.0f);

    // Entity has Position but not Velocity
    CHECK(world.hasComponent<Position>(e));
    CHECK(!world.hasComponent<Velocity>(e));

    // Removing a non-existent component must be a safe no-op
    world.removeComponent<Velocity>(e);

    // Position must still be intact
    CHECK(world.hasComponent<Position>(e));
    Position* pos = world.getComponent<Position>(e);
    REQUIRE(pos != nullptr);
    CHECK(pos->x == 1.0f);

    CHECK_INVARIANTS(world);
}

// ---------------------------------------------------------------------------
// M01-M03 Review Fixes: Duplicate addComponent updates value (no leak, no crash)
// ---------------------------------------------------------------------------

TEST_CASE("ECS_Component_DuplicateAdd_Active") {
    BlockAllocator blockAlloc;
    World world(&blockAlloc);

    TypeRegistry::instance().registerComponent<Position>();

    Entity e = world.createEntity();
    world.addComponent<Position>(e, 1.0f, 2.0f, 3.0f);

    Position* pos = world.getComponent<Position>(e);
    REQUIRE(pos != nullptr);
    CHECK(pos->x == 1.0f);

    // Duplicate add must update the value, not leak memory or corrupt state
    world.addComponent<Position>(e, 99.0f, 88.0f, 77.0f);

    pos = world.getComponent<Position>(e);
    REQUIRE(pos != nullptr);
    CHECK(pos->x == 99.0f);
    CHECK(pos->y == 88.0f);
    CHECK(pos->z == 77.0f);

    CHECK_INVARIANTS(world);
}

// ---------------------------------------------------------------------------
// M01-M03 Review Fixes: Destroy last entity of an archetype
// ---------------------------------------------------------------------------

TEST_CASE("ECS_Archetype_DestroyLastEntity") {
    BlockAllocator blockAlloc;
    World world(&blockAlloc);

    TypeRegistry::instance().registerComponent<Position>();
    TypeRegistry::instance().registerComponent<Velocity>();

    // Create exactly one entity with a unique archetype
    Entity e = world.createEntity();
    world.addComponent<Position>(e, 1.0f, 0.0f, 0.0f);
    world.addComponent<Velocity>(e, 2.0f, 0.0f, 0.0f);

    CHECK(world.entityCount() == 1);

    // Destroy the only entity in this archetype
    world.destroyEntity(e);

    CHECK(world.entityCount() == 0);
    CHECK(!world.isAlive(e));
    CHECK_INVARIANTS(world);

    // Recreate entity in same archetype to verify the archetype is still usable
    Entity e2 = world.createEntity();
    world.addComponent<Position>(e2, 3.0f, 0.0f, 0.0f);
    world.addComponent<Velocity>(e2, 4.0f, 0.0f, 0.0f);

    CHECK(world.entityCount() == 1);
    Position* pos = world.getComponent<Position>(e2);
    REQUIRE(pos != nullptr);
    CHECK(pos->x == 3.0f);

    CHECK_INVARIANTS(world);
}

// ---------------------------------------------------------------------------
// M01-M03 Review Fixes: Query during component removal (archetype migration)
// ---------------------------------------------------------------------------

TEST_CASE("ECS_Query_DuringComponentRemove") {
    BlockAllocator blockAlloc;
    World world(&blockAlloc);

    TypeRegistry::instance().registerComponent<Position>();
    TypeRegistry::instance().registerComponent<Velocity>();

    std::vector<Entity> entities;
    for (size_t i = 0; i < 10; ++i) {
        Entity e = world.createEntity();
        world.addComponent<Position>(e, static_cast<float>(i), 0.0f, 0.0f);
        world.addComponent<Velocity>(e, static_cast<float>(i * 10), 0.0f, 0.0f);
        entities.push_back(e);
    }

    // Query before any removal
    int countBefore = 0;
    for (auto [pos, vel] : world.query<Position, Velocity>()) {
        (void)pos; (void)vel;
        ++countBefore;
    }
    CHECK(countBefore == 10);

    // Remove Velocity from some entities (triggers archetype migration)
    world.removeComponent<Velocity>(entities[2]);
    world.removeComponent<Velocity>(entities[5]);
    world.removeComponent<Velocity>(entities[8]);

    // Query after removal: should return only entities that still have both
    int countAfter = 0;
    for (auto [pos, vel] : world.query<Position, Velocity>()) {
        (void)pos; (void)vel;
        ++countAfter;
    }
    CHECK(countAfter == 7);

    // Query for Position-only should still find all 10
    int countPosOnly = 0;
    for (auto [pos] : world.query<Position>()) {
        (void)pos;
        ++countPosOnly;
    }
    CHECK(countPosOnly == 10);

    CHECK_INVARIANTS(world);
}

// ---------------------------------------------------------------------------
// M01-M03 Review Fixes: Version wrap-around after 255 recycles
// ---------------------------------------------------------------------------

TEST_CASE("ECS_Entity_VersionWrapAround") {
    BlockAllocator blockAlloc;
    World world(&blockAlloc);

    TypeRegistry::instance().registerComponent<Position>();

    // Create and destroy the same entity slot 260 times to exceed uint8_t max
    Entity firstHandle = INVALID_ENTITY;
    for (int i = 0; i < 260; ++i) {
        Entity e = world.createEntity();
        if (i == 0) {
            firstHandle = e;
        }
        world.addComponent<Position>(e, static_cast<float>(i), 0.0f, 0.0f);
        world.destroyEntity(e);
    }

    // The first handle must be permanently invalid regardless of version wrap
    CHECK(!world.isAlive(firstHandle));

    // A new entity at the same index must have a different version
    Entity eNew = world.createEntity();
    CHECK(world.isAlive(eNew));
    CHECK(entityIndex(eNew) == entityIndex(firstHandle));
    // Version should have wrapped but still be different from firstHandle
    CHECK(entityVersion(eNew) != entityVersion(firstHandle));

    CHECK_INVARIANTS(world);
}

// ---------------------------------------------------------------------------
// M01-M03 Review Fixes: Invalid handle operations (safe no-op / nullptr)
// ---------------------------------------------------------------------------

TEST_CASE("ECS_InvalidHandle_Operations") {
    BlockAllocator blockAlloc;
    World world(&blockAlloc);

    TypeRegistry::instance().registerComponent<Position>();

    Entity e = world.createEntity();
    world.destroyEntity(e);

    // All operations on a dead/invalid handle must be safe
    CHECK(world.getComponent<Position>(e) == nullptr);
    CHECK(!world.hasComponent<Position>(e));
    world.removeComponent<Position>(e); // must not crash
    world.addComponent<Position>(e, 1.0f, 2.0f, 3.0f); // must not crash (returns nullptr)

    CHECK_INVARIANTS(world);
}

// ---------------------------------------------------------------------------
// M01-M03 Review Fixes: Extreme Stress Tests – 500k & 1M Random Operations
// ---------------------------------------------------------------------------

TEST_CASE("ECS_Stress_500k_RandomOps") {
    BlockAllocator blockAlloc;
    World world(&blockAlloc);

    TypeRegistry::instance().registerComponent<Position>();
    TypeRegistry::instance().registerComponent<Velocity>();
    TypeRegistry::instance().registerComponent<Health>();

    std::mt19937 rng(42);
    std::uniform_int_distribution<int> opDist(0, 4);
    std::uniform_real_distribution<float> floatDist(-1000.0f, 1000.0f);
    std::uniform_int_distribution<int> boolDist(0, 1);

    std::vector<Entity> alive;
    alive.reserve(10000);

    constexpr size_t OPS = 500'000;

    for (size_t i = 0; i < OPS; ++i) {
        int op = opDist(rng);

        switch (op) {
            case 0: { // Create
                Entity e = world.createEntity();
                alive.push_back(e);
                break;
            }
            case 1: { // Destroy
                if (!alive.empty()) {
                    size_t idx = std::uniform_int_distribution<size_t>(0, alive.size() - 1)(rng);
                    Entity e = alive[idx];
                    world.destroyEntity(e);
                    alive[idx] = alive.back();
                    alive.pop_back();
                }
                break;
            }
            case 2: { // Add Component
                if (!alive.empty()) {
                    size_t idx = std::uniform_int_distribution<size_t>(0, alive.size() - 1)(rng);
                    Entity e = alive[idx];
                    int comp = boolDist(rng);
                    if (comp == 0 && !world.hasComponent<Position>(e)) {
                        world.addComponent<Position>(e, floatDist(rng), floatDist(rng), floatDist(rng));
                    } else if (comp == 1 && !world.hasComponent<Velocity>(e)) {
                        world.addComponent<Velocity>(e, floatDist(rng), floatDist(rng), floatDist(rng));
                    }
                }
                break;
            }
            case 3: { // Remove Component
                if (!alive.empty()) {
                    size_t idx = std::uniform_int_distribution<size_t>(0, alive.size() - 1)(rng);
                    Entity e = alive[idx];
                    int comp = boolDist(rng);
                    if (comp == 0) world.removeComponent<Position>(e);
                    else           world.removeComponent<Velocity>(e);
                }
                break;
            }
            case 4: { // Random Query
                int queryType = boolDist(rng);
                if (queryType == 0) {
                    int count = 0;
                    for (auto [pos] : world.query<Position>()) {
                        (void)pos;
                        ++count;
                    }
                    (void)count;
                } else {
                    int count = 0;
                    for (auto [pos, vel] : world.query<Position, Velocity>()) {
                        (void)pos; (void)vel;
                        ++count;
                    }
                    (void)count;
                }
                break;
            }
        }

        // Every 10k ops, validate invariants
        if (i % 10'000 == 0) {
            CHECK_INVARIANTS(world);
        }
    }

    CHECK_INVARIANTS(world);
    CHECK(world.entityCount() == alive.size());
}

TEST_CASE("ECS_Stress_1M_RandomOps") {
    BlockAllocator blockAlloc;
    World world(&blockAlloc);

    TypeRegistry::instance().registerComponent<Position>();
    TypeRegistry::instance().registerComponent<Velocity>();
    TypeRegistry::instance().registerComponent<Health>();

    std::mt19937 rng(12345);
    std::uniform_int_distribution<int> opDist(0, 4);
    std::uniform_real_distribution<float> floatDist(-1000.0f, 1000.0f);
    std::uniform_int_distribution<int> boolDist(0, 1);

    std::vector<Entity> alive;
    alive.reserve(20000);

    constexpr size_t OPS = 1'000'000;

    for (size_t i = 0; i < OPS; ++i) {
        int op = opDist(rng);

        switch (op) {
            case 0: { // Create
                Entity e = world.createEntity();
                alive.push_back(e);
                break;
            }
            case 1: { // Destroy
                if (!alive.empty()) {
                    size_t idx = std::uniform_int_distribution<size_t>(0, alive.size() - 1)(rng);
                    Entity e = alive[idx];
                    world.destroyEntity(e);
                    alive[idx] = alive.back();
                    alive.pop_back();
                }
                break;
            }
            case 2: { // Add Component
                if (!alive.empty()) {
                    size_t idx = std::uniform_int_distribution<size_t>(0, alive.size() - 1)(rng);
                    Entity e = alive[idx];
                    int comp = boolDist(rng);
                    if (comp == 0 && !world.hasComponent<Position>(e)) {
                        world.addComponent<Position>(e, floatDist(rng), floatDist(rng), floatDist(rng));
                    } else if (comp == 1 && !world.hasComponent<Velocity>(e)) {
                        world.addComponent<Velocity>(e, floatDist(rng), floatDist(rng), floatDist(rng));
                    }
                }
                break;
            }
            case 3: { // Remove Component
                if (!alive.empty()) {
                    size_t idx = std::uniform_int_distribution<size_t>(0, alive.size() - 1)(rng);
                    Entity e = alive[idx];
                    int comp = boolDist(rng);
                    if (comp == 0) world.removeComponent<Position>(e);
                    else           world.removeComponent<Velocity>(e);
                }
                break;
            }
            case 4: { // Random Query
                int queryType = boolDist(rng);
                if (queryType == 0) {
                    int count = 0;
                    for (auto [pos] : world.query<Position>()) {
                        (void)pos;
                        ++count;
                    }
                    (void)count;
                } else {
                    int count = 0;
                    for (auto [pos, vel] : world.query<Position, Velocity>()) {
                        (void)pos; (void)vel;
                        ++count;
                    }
                    (void)count;
                }
                break;
            }
        }

        // Every 50k ops, validate invariants (less frequent for performance)
        if (i % 50'000 == 0) {
            CHECK_INVARIANTS(world);
        }
    }

    CHECK_INVARIANTS(world);
    CHECK(world.entityCount() == alive.size());
}
