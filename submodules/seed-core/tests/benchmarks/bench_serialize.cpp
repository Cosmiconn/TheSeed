#include "core/memory/memory_system.h"
#include "core/ecs/world.h"
#include "core/ecs/component_traits.h"
#include "core/serialize/snapshot.h"
#include "core/serialize/delta.h"
#include "core/profiling/seed_assert.h"
#include <chrono>
#include <iostream>

using namespace seed;
using namespace seed::memory;
using namespace seed::ecs;
using namespace seed::serialize;

struct Position { float x, y, z; };
SEED_REGISTER_COMPONENT_WITH_ID(Position, 100)

struct Velocity { float vx, vy, vz; };
SEED_REGISTER_COMPONENT_WITH_ID(Velocity, 101)

struct Health { int32_t hp; int32_t maxHp; };
SEED_REGISTER_COMPONENT_WITH_ID(Health, 102)

int main() {
    BlockAllocator blockAlloc;
    MemoryTracker tracker;
    g_blockAllocator = &blockAlloc;
    g_memoryTracker = &tracker;

    World world(&blockAlloc);

    constexpr size_t N = 100'000;
    auto start = std::chrono::high_resolution_clock::now();

    for (size_t i = 0; i < N; ++i) {
        auto e = world.createEntity();
        world.addComponent<Position>(e, static_cast<float>(i), static_cast<float>(i*2), static_cast<float>(i*3));
        if (i % 2 == 0) world.addComponent<Velocity>(e, 1.0f, 0.0f, 0.0f);
        if (i % 3 == 0) world.addComponent<Health>(e, 100, 100);
    }

    auto createEnd = std::chrono::high_resolution_clock::now();
    std::cout << "Created " << N << " entities in "
              << std::chrono::duration<double, std::milli>(createEnd - start).count() << " ms\n";

    // --- Snapshot capture performance (Monat 5 spec: < 100 ms) ---
    auto snapStart = std::chrono::high_resolution_clock::now();
    auto snap = Snapshot::capture(world);
    auto snapEnd = std::chrono::high_resolution_clock::now();
    double snapMs = std::chrono::duration<double, std::milli>(snapEnd - snapStart).count();

    std::cout << "Snapshot: " << snap.serialize().size() << " bytes in " << snapMs << " ms\n";
    SEED_ASSERT(snapMs < 100.0, "Snapshot capture exceeded 100ms budget");
    SEED_ASSERT(snap.serialize().size() < 50ULL * 1024 * 1024, "Snapshot size exceeded 50MB budget");

    // Modify ~1% of entities for delta test
    for (auto [pos] : world.query<Position>()) {
        if (reinterpret_cast<uintptr_t>(pos) % 100 < 16) {
            pos->x += 1.0f;
        }
    }

    auto snap2 = Snapshot::capture(world);

    // --- Delta compression performance ---
    auto deltaStart = std::chrono::high_resolution_clock::now();
    auto delta = snap.computeDelta(snap2);
    auto deltaEnd = std::chrono::high_resolution_clock::now();
    double deltaMs = std::chrono::duration<double, std::milli>(deltaEnd - deltaStart).count();

    std::cout << "Delta: " << delta.size() << " bytes in " << deltaMs << " ms\n";
    std::cout << "Compression ratio: "
              << (100.0 * static_cast<double>(delta.size()) / static_cast<double>(snap2.serialize().size()))
              << "%\n";
    // Monat 5 spec: 10MB snapshot -> <100KB delta at 1% change (~95% compression)
    SEED_ASSERT(delta.size() < 100 * 1024, "Delta compression exceeded 100KB budget for 1% change");

    // --- Deserialization performance (Monat 5 spec: < 50 ms) ---
    auto data = snap.serialize();
    World world2(&blockAlloc);
    auto deserStart = std::chrono::high_resolution_clock::now();
    auto snap3 = Snapshot::deserialize(data);
    snap3.apply(world2);
    auto deserEnd = std::chrono::high_resolution_clock::now();
    double deserMs = std::chrono::duration<double, std::milli>(deserEnd - deserStart).count();

    std::cout << "Deserialize+Apply: " << deserMs << " ms\n";
    SEED_ASSERT(deserMs < 50.0, "Snapshot deserialize exceeded 50ms budget");
    SEED_ASSERT(world2.entityCount() == N, "Entity count mismatch after deserialization");

    std::cout << "All Monat 5 performance budgets passed.\n";
    return 0;
}
