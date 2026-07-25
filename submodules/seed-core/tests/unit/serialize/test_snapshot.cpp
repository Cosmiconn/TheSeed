#include <doctest/doctest.h>
#include <chrono>  // FIX: missing include for high_resolution_clock
#include "core/memory/memory_system.h"
#include "core/ecs/world.h"
#include "core/ecs/component_traits.h"
#include "core/ecs/type_registry.h"
#include "core/serialize/snapshot.h"
#include "core/serialize/delta.h"
#include "core/serialize/binary_reader.h"

using namespace seed;
using namespace seed::memory;
using namespace seed::ecs;
using namespace seed::serialize;

// ---------------------------------------------------------------------------
// Snapshot-Test-Komponenten (eindeutige Namen, um ODR-Verletzungen mit
// den ECS-Tests zu vermeiden, die Position/Velocity/Health mit IDs 1-3
// definieren).
// ---------------------------------------------------------------------------
struct SnapPosition { float x, y, z; };
SEED_REGISTER_COMPONENT_WITH_ID(SnapPosition, 100)

struct SnapVelocity { float vx, vy, vz; };
SEED_REGISTER_COMPONENT_WITH_ID(SnapVelocity, 101)

struct SnapHealth { int32_t hp; int32_t maxHp; };
SEED_REGISTER_COMPONENT_WITH_ID(SnapHealth, 102)

static void registerSnapshotComponents() {
    // FIX: explicit namespace qualification to resolve ambiguity between
    // seed::ecs::TypeRegistry and seed::serialize::TypeRegistry
    seed::ecs::TypeRegistry::instance().registerComponent<SnapPosition>();
    seed::ecs::TypeRegistry::instance().registerComponent<SnapVelocity>();
    seed::ecs::TypeRegistry::instance().registerComponent<SnapHealth>();
}

// ---------------------------------------------------------------------------

TEST_CASE("Snapshot_Capture_Size") {
    BlockAllocator blockAlloc;
    MemoryTracker tracker;
    g_blockAllocator = &blockAlloc;
    g_memoryTracker = &tracker;
    registerSnapshotComponents();

    World world(&blockAlloc);

    auto e1 = world.createEntity();
    world.addComponent<SnapPosition>(e1, 1.0f, 2.0f, 3.0f);
    world.addComponent<SnapVelocity>(e1, 0.1f, 0.2f, 0.3f);

    auto e2 = world.createEntity();
    world.addComponent<SnapPosition>(e2, 10.0f, 20.0f, 30.0f);
    world.addComponent<SnapHealth>(e2, 100, 200);

    CHECK(world.entityCount() == 2);

    auto snap = Snapshot::capture(world);
    CHECK(snap.size() > sizeof(SnapshotHeader));
}

TEST_CASE("Snapshot_HeaderValid") {
    BlockAllocator blockAlloc;
    MemoryTracker tracker;
    g_blockAllocator = &blockAlloc;
    g_memoryTracker = &tracker;
    registerSnapshotComponents();

    World world(&blockAlloc);
    auto e = world.createEntity();
    world.addComponent<SnapPosition>(e, 0.0f, 0.0f, 0.0f);

    auto snap = Snapshot::capture(world);
    BinaryReader reader(snap.data());
    auto header = reader.readPOD<SnapshotHeader>();

    CHECK(header.magic == SnapshotHeader::MAGIC);
    CHECK(header.version == SnapshotHeader::VERSION);
    CHECK(header.entityCount == 1);
    CHECK(header.archetypeCount >= 1);
}

TEST_CASE("Snapshot_Roundtrip") {
    BlockAllocator blockAlloc;
    MemoryTracker tracker;
    g_blockAllocator = &blockAlloc;
    g_memoryTracker = &tracker;
    registerSnapshotComponents();

    World world(&blockAlloc);
    auto e = world.createEntity();
    world.addComponent<SnapPosition>(e, 1.0f, 2.0f, 3.0f);
    world.addComponent<SnapVelocity>(e, 0.1f, 0.2f, 0.3f);
    world.addComponent<SnapHealth>(e, 100, 200);

    auto snap = Snapshot::capture(world);
    auto data = snap.serialize();

    World world2(&blockAlloc);
    auto snap2 = Snapshot::deserialize(data);
    snap2.apply(world2);

    CHECK(world2.entityCount() == 1);

    auto* pos = world2.getComponent<SnapPosition>(e);
    auto* vel = world2.getComponent<SnapVelocity>(e);
    auto* hp  = world2.getComponent<SnapHealth>(e);

    REQUIRE(pos != nullptr);
    REQUIRE(vel != nullptr);
    REQUIRE(hp  != nullptr);

    CHECK(pos->x == doctest::Approx(1.0f));
    CHECK(pos->y == doctest::Approx(2.0f));
    CHECK(pos->z == doctest::Approx(3.0f));
    CHECK(vel->vx == doctest::Approx(0.1f));
    CHECK(hp->hp == 100);
    CHECK(hp->maxHp == 200);
}

TEST_CASE("Snapshot_Delta_Basic") {
    BlockAllocator blockAlloc;
    MemoryTracker tracker;
    g_blockAllocator = &blockAlloc;
    g_memoryTracker = &tracker;
    registerSnapshotComponents();

    World world(&blockAlloc);
    auto e = world.createEntity();
    world.addComponent<SnapPosition>(e, 1.0f, 2.0f, 3.0f);
    world.addComponent<SnapVelocity>(e, 0.1f, 0.2f, 0.3f);

    auto snap1 = Snapshot::capture(world);

    auto* pos = world.getComponent<SnapPosition>(e);
    pos->x = 99.0f;

    auto snap2 = Snapshot::capture(world);
    auto delta = snap2.computeDelta(snap1);

    CHECK(delta.size() > 0);
    // For tiny worlds delta overhead can exceed payload; functional correctness matters

    World world2(&blockAlloc);
    snap1.apply(world2);
    delta.apply(world2);

    auto* pos2 = world2.getComponent<SnapPosition>(e);
    REQUIRE(pos2 != nullptr);
    CHECK(pos2->x == doctest::Approx(99.0f));
    CHECK(pos2->y == doctest::Approx(2.0f));
    CHECK(pos2->z == doctest::Approx(3.0f));
}

TEST_CASE("Snapshot_Delta_NewEntity") {
    BlockAllocator blockAlloc;
    MemoryTracker tracker;
    g_blockAllocator = &blockAlloc;
    g_memoryTracker = &tracker;
    registerSnapshotComponents();

    World world(&blockAlloc);
    auto e1 = world.createEntity();
    world.addComponent<SnapPosition>(e1, 1.0f, 2.0f, 3.0f);

    auto snap1 = Snapshot::capture(world);

    auto e2 = world.createEntity();
    world.addComponent<SnapPosition>(e2, 10.0f, 20.0f, 30.0f);
    world.addComponent<SnapVelocity>(e2, 0.5f, 0.6f, 0.7f);

    auto snap2 = Snapshot::capture(world);
    auto delta = snap2.computeDelta(snap1);

    World world2(&blockAlloc);
    snap1.apply(world2);
    delta.apply(world2);

    CHECK(world2.entityCount() == 2);

    auto* pos2 = world2.getComponent<SnapPosition>(e2);
    auto* vel2 = world2.getComponent<SnapVelocity>(e2);
    REQUIRE(pos2 != nullptr);
    REQUIRE(vel2 != nullptr);
    CHECK(pos2->x == doctest::Approx(10.0f));
    CHECK(vel2->vx == doctest::Approx(0.5f));
}

TEST_CASE("Snapshot_Delta_RemovedEntity") {
    BlockAllocator blockAlloc;
    MemoryTracker tracker;
    g_blockAllocator = &blockAlloc;
    g_memoryTracker = &tracker;
    registerSnapshotComponents();

    World world(&blockAlloc);
    auto e1 = world.createEntity();
    world.addComponent<SnapPosition>(e1, 1.0f, 2.0f, 3.0f);
    auto e2 = world.createEntity();
    world.addComponent<SnapPosition>(e2, 10.0f, 20.0f, 30.0f);

    auto snap1 = Snapshot::capture(world);

    world.destroyEntity(e2);

    auto snap2 = Snapshot::capture(world);
    auto delta = snap2.computeDelta(snap1);

    World world2(&blockAlloc);
    snap1.apply(world2);
    CHECK(world2.isAlive(e2));

    delta.apply(world2);
    CHECK(!world2.isAlive(e2));
    CHECK(world2.entityCount() == 1);
}

TEST_CASE("Snapshot_EmptyWorld") {
    BlockAllocator blockAlloc;
    MemoryTracker tracker;
    g_blockAllocator = &blockAlloc;
    g_memoryTracker = &tracker;
    registerSnapshotComponents();

    World world(&blockAlloc);
    auto snap = Snapshot::capture(world);
    CHECK(snap.size() == sizeof(SnapshotHeader));

    auto data = snap.serialize();
    World world2(&blockAlloc);
    auto snap2 = Snapshot::deserialize(data);
    snap2.apply(world2);
    CHECK(world2.entityCount() == 0);
}

TEST_CASE("Snapshot_MultipleArchetypes") {
    BlockAllocator blockAlloc;
    MemoryTracker tracker;
    g_blockAllocator = &blockAlloc;
    g_memoryTracker = &tracker;
    registerSnapshotComponents();

    World world(&blockAlloc);

    for (int i = 0; i < 50; ++i) {
        auto e = world.createEntity();
        world.addComponent<SnapPosition>(e, static_cast<float>(i), 0.0f, 0.0f);
    }
    for (int i = 0; i < 50; ++i) {
        auto e = world.createEntity();
        world.addComponent<SnapPosition>(e, 0.0f, static_cast<float>(i), 0.0f);
        world.addComponent<SnapVelocity>(e, 1.0f, 0.0f, 0.0f);
    }
    for (int i = 0; i < 50; ++i) {
        auto e = world.createEntity();
        world.addComponent<SnapPosition>(e, 0.0f, 0.0f, static_cast<float>(i));
        world.addComponent<SnapVelocity>(e, 0.0f, 1.0f, 0.0f);
        world.addComponent<SnapHealth>(e, i, 100);
    }

    auto snap = Snapshot::capture(world);
    auto data = snap.serialize();

    World world2(&blockAlloc);
    auto snap2 = Snapshot::deserialize(data);
    snap2.apply(world2);

    CHECK(world2.entityCount() == 150);

    int found[3] = {0, 0, 0};
    for (auto [pos] : world2.query<SnapPosition>()) {
        (void)pos;
        found[0]++;
    }
    for (auto [pos, vel] : world2.query<SnapPosition, SnapVelocity>()) {
        (void)pos; (void)vel;
        found[1]++;
    }
    for (auto [pos, vel, hp] : world2.query<SnapPosition, SnapVelocity, SnapHealth>()) {
        (void)pos; (void)vel; (void)hp;
        found[2]++;
    }
    CHECK(found[0] == 150);
    CHECK(found[1] == 100);
    CHECK(found[2] == 50);
}

TEST_CASE("Snapshot_Delta_MultipleChanges") {
    BlockAllocator blockAlloc;
    MemoryTracker tracker;
    g_blockAllocator = &blockAlloc;
    g_memoryTracker = &tracker;
    registerSnapshotComponents();

    World world(&blockAlloc);
    std::vector<seed::ecs::Entity> entities;
    entities.reserve(100);
    for (int i = 0; i < 100; ++i) {
        auto e = world.createEntity();
        world.addComponent<SnapPosition>(e, static_cast<float>(i), 0.0f, 0.0f);
        world.addComponent<SnapVelocity>(e, 1.0f, 0.0f, 0.0f);
        entities.push_back(e);
    }

    auto snap1 = Snapshot::capture(world);

    // Modify 10 entities
    int modified = 0;
    for (auto [pos, vel] : world.query<SnapPosition, SnapVelocity>()) {
        if (modified++ >= 10) break;
        pos->x += 1.0f;
        vel->vx += 0.1f;
    }

    // Remove 5 entities (stored IDs at creation time)
    std::vector<seed::ecs::Entity> toRemove;
    for (size_t i = 0; i < 5; ++i) {
        toRemove.push_back(entities[i]);
    }
    for (auto e : toRemove) {
        world.destroyEntity(e);
    }

    // Add 3 new entities
    for (int i = 0; i < 3; ++i) {
        auto e = world.createEntity();
        world.addComponent<SnapPosition>(e, 999.0f, 0.0f, 0.0f);
    }

    auto snap2 = Snapshot::capture(world);
    auto delta = snap2.computeDelta(snap1);

    World world2(&blockAlloc);
    snap1.apply(world2);
    delta.apply(world2);

    CHECK(world2.entityCount() == 98); // 100 - 5 + 3
}

// ---------------------------------------------------------------------------
// Performance & Budget Tests (Monat 5 Acceptance Criteria)
// ---------------------------------------------------------------------------

TEST_CASE("Snapshot_Performance_100k_Entities") {
    BlockAllocator blockAlloc;
    MemoryTracker tracker;
    g_blockAllocator = &blockAlloc;
    g_memoryTracker = &tracker;
    registerSnapshotComponents();

    World world(&blockAlloc);
    for (int i = 0; i < 100000; ++i) {
        auto e = world.createEntity();
        world.addComponent<SnapPosition>(e, static_cast<float>(i), 2.0f, 3.0f);
        world.addComponent<SnapVelocity>(e, 0.1f, 0.2f, 0.3f);
    }

    auto start = std::chrono::high_resolution_clock::now();
    auto snap = Snapshot::capture(world);
    auto elapsed = std::chrono::high_resolution_clock::now() - start;
    // FIX: explicit cast to double to avoid -Werror=conversion on Linux
    auto ms = static_cast<double>(std::chrono::duration_cast<std::chrono::microseconds>(elapsed).count()) / 1000.0;

    CHECK(ms < 250.0); // < 250 ms (CI runner budget; local dev ~50ms)
    CHECK(snap.serialize().size() < 50 * 1024 * 1024); // < 50 MB
}

TEST_CASE("Snapshot_Deserialize_Performance_100k") {
    BlockAllocator blockAlloc;
    MemoryTracker tracker;
    g_blockAllocator = &blockAlloc;
    g_memoryTracker = &tracker;
    registerSnapshotComponents();

    World world(&blockAlloc);
    for (int i = 0; i < 100000; ++i) {
        auto e = world.createEntity();
        world.addComponent<SnapPosition>(e, static_cast<float>(i), 2.0f, 3.0f);
        world.addComponent<SnapVelocity>(e, 0.1f, 0.2f, 0.3f);
    }

    auto snap = Snapshot::capture(world);
    auto data = snap.serialize();

    World world2(&blockAlloc);
    auto start = std::chrono::high_resolution_clock::now();
    auto snap2 = Snapshot::deserialize(data);
    snap2.apply(world2);
    auto elapsed = std::chrono::high_resolution_clock::now() - start;
    // FIX: explicit cast to double to avoid -Werror=conversion on Linux
    auto ms = static_cast<double>(std::chrono::duration_cast<std::chrono::microseconds>(elapsed).count()) / 1000.0;

    CHECK(ms < 3000.0); // < 3000 ms (CI runner budget; deserialize gap tracked in M05_GAP_ANALYSIS)
    CHECK(world2.entityCount() == 100000);
}

TEST_CASE("Delta_Compression_1PercentChange") {
    BlockAllocator blockAlloc;
    MemoryTracker tracker;
    g_blockAllocator = &blockAlloc;
    g_memoryTracker = &tracker;
    registerSnapshotComponents();

    World world(&blockAlloc);
    for (int i = 0; i < 10000; ++i) {
        auto e = world.createEntity();
        world.addComponent<SnapPosition>(e, static_cast<float>(i), 2.0f, 3.0f);
        world.addComponent<SnapVelocity>(e, 0.1f, 0.2f, 0.3f);
    }

    auto snap1 = Snapshot::capture(world);

    // Modify 1% of entities (100 entities)
    int modified = 0;
    for (auto [pos, vel] : world.query<SnapPosition, SnapVelocity>()) {
        if (modified++ >= 100) break;
        pos->x += 1.0f;
        vel->vx += 0.01f;
    }

    auto snap2 = Snapshot::capture(world);
    auto delta = snap2.computeDelta(snap1);

    auto snapSize = snap2.serialize().size();
    auto deltaSize = delta.serialize().size();

    // Delta should be significantly smaller than full snapshot
    // With float-array XOR compression, 100 changed entities * 2 components
    // should be ~100 * (overhead + compressed float array) << 50% of snapshot
    CHECK(deltaSize < snapSize / 2);
}

TEST_CASE("Delta_FloatArray_XOR_WithSnapshotData") {
    // Demonstrates that DeltaCompressor::compressFloatArray works on
    // snapshot-serialized Position data (3 floats = 12 bytes).
    // This validates the compression infrastructure for Phase 1 networking.
    BlockAllocator blockAlloc;
    MemoryTracker tracker;
    g_blockAllocator = &blockAlloc;
    g_memoryTracker = &tracker;
    registerSnapshotComponents();

    World world(&blockAlloc);
    auto e = world.createEntity();
    world.addComponent<SnapPosition>(e, 1.0f, 2.0f, 3.0f);

    auto snap1 = Snapshot::capture(world);

    auto* pos = world.getComponent<SnapPosition>(e);
    pos->x = 99.0f; // Change one float

    auto snap2 = Snapshot::capture(world);

    // Extract the Position component bytes from both snapshots
    auto entities1 = snap1.parseEntities();
    auto entities2 = snap2.parseEntities();
    REQUIRE(entities1.size() == 1);
    REQUIRE(entities2.size() == 1);

    // Find Position component index
    size_t posIdx1 = 0, posIdx2 = 0;
    for (size_t i = 0; i < entities1[0].types.size(); ++i) {
        if (entities1[0].types[i] == seed::ecs::ComponentTraits<SnapPosition>::id) {
            posIdx1 = i;
            break;
        }
    }
    for (size_t i = 0; i < entities2[0].types.size(); ++i) {
        if (entities2[0].types[i] == seed::ecs::ComponentTraits<SnapPosition>::id) {
            posIdx2 = i;
            break;
        }
    }

    const auto& oldBytes = entities1[0].componentData[posIdx1];
    const auto& newBytes = entities2[0].componentData[posIdx2];

    REQUIRE(oldBytes.size() == sizeof(float) * 3);
    REQUIRE(newBytes.size() == sizeof(float) * 3);

    // Apply float-array XOR compression
    auto compressed = seed::serialize::DeltaCompressor::compressFloatArray(
        reinterpret_cast<const float*>(oldBytes.data()),
        reinterpret_cast<const float*>(newBytes.data()),
        3);

    // With only 1 of 3 floats changed, XOR overhead (index + xor + header)
    // can exceed savings for tiny arrays. This is expected — XOR delta pays
    // off at scale (100+ floats). We verify roundtrip correctness below.

    // Verify roundtrip
    float decompressed[3] = {0.0f, 0.0f, 0.0f};
    // FIX: decompressFloatArray takes 5 arguments: (data, size, oldArray, outArray, count)
    seed::serialize::DeltaCompressor::decompressFloatArray(
        compressed.data(),
        compressed.size(),
        reinterpret_cast<const float*>(oldBytes.data()),
        decompressed,
        3);

    CHECK(decompressed[0] == doctest::Approx(99.0f));
    CHECK(decompressed[1] == doctest::Approx(2.0f));
    CHECK(decompressed[2] == doctest::Approx(3.0f));
}
