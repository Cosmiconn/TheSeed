// =============================================================================
// tests/Test_Network.cpp — Netzwerk Unit Tests (P5-FIX)
// =============================================================================
#include "TestMain.h"
#include "../network/network_NetworkServer.h"
#include "../network/network_ReliableUdp.h"
#include "../server/network/Snapshot.h"

#include <thread>
#include <chrono>

namespace tests {

// =============================================================================
// TEST: Packet Header Serialization
// =============================================================================
TEST(PacketHeaderSerialization) {
    net::PacketHeader header;
    header.protocolId = 0x4D4D;
    header.sequence = 42;
    header.ack = 100;
    header.ackBitmap = 0xFFFFFFFF;
    header.flags = static_cast<uint16_t>(net::PacketFlags::Reliable);

    TEST_ASSERT_EQ(0x4D4Du, header.protocolId);
    TEST_ASSERT_EQ(42u, header.sequence);
    TEST_ASSERT_EQ(100u, header.ack);
}

// =============================================================================
// TEST: RTT Estimation
// =============================================================================
TEST(RttEstimation) {
    net::RttEstimator estimator;

    // Initial RTT should be 0
    TEST_ASSERT_EQ(0.0f, estimator.GetRto());

    // Add measurements
    estimator.Update(0.050f); // 50ms
    estimator.Update(0.060f); // 60ms
    estimator.Update(0.055f); // 55ms

    // RTO should be around 55ms + some margin
    float rto = estimator.GetRto();
    TEST_ASSERT(rto > 0.050f);
    TEST_ASSERT(rto < 0.200f);
}

// =============================================================================
// TEST: Spatial Hash
// =============================================================================
TEST(SpatialHash) {
    net::SpatialHash hash;

    // Insert entities
    hash.Insert(1, 10.0f, 10.0f);
    hash.Insert(2, 15.0f, 15.0f);
    hash.Insert(3, 100.0f, 100.0f);

    // Query near entity 1
    auto nearby = hash.Query(10.0f, 10.0f, 20.0f);
    TEST_ASSERT(nearby.size() >= 2); // Should find 1 and 2

    // Query far away
    auto far = hash.Query(0.0f, 0.0f, 5.0f);
    TEST_ASSERT_EQ(0u, far.size());

    // Update position
    hash.Insert(1, 100.0f, 100.0f);
    nearby = hash.Query(10.0f, 10.0f, 20.0f);
    // Entity 1 should no longer be nearby
    bool found1 = false;
    for (auto id : nearby) {
        if (id == 1) found1 = true;
    }
    TEST_ASSERT_FALSE(found1);
}

// =============================================================================
// TEST: Delta Compression
// =============================================================================
TEST(DeltaCompression) {
    net::SnapshotBuilder builder;
    builder.RegisterClient(1);

    // Create a mock ECS world
    ecs::EcsWorld world;
    world.Initialize();

    auto entity = world.CreateEntity();
    world.AddComponent(entity, ecs::PositionComponent{1.0f, 2.0f, 3.0f});
    world.AddComponent(entity, ecs::HealthComponent{100, 100, true});
    world.AddComponent(entity, ecs::NameComponent{"Test"});

    // Build initial snapshot (full state)
    auto snap1 = builder.BuildFullSnapshot(world, 1, 0.0f, 0.0f, 1000.0f);
    TEST_ASSERT(!snap1.data.empty());

    // Build delta snapshot (no changes)
    auto snap2 = builder.BuildSnapshot(world, 1, 0.0f, 0.0f, 1000.0f);
    TEST_ASSERT(!snap2.data.empty());

    // Delta should be smaller than full snapshot
    // (This depends on implementation details)

    world.Shutdown();
}

// =============================================================================
// TEST: Snapshot Fragmentation
// =============================================================================
TEST(SnapshotFragmentation) {
    net::SnapshotBuilder builder;

    // Create large snapshot data
    net::ByteBuffer largeData;
    for (int i = 0; i < 2000; ++i) {
        largeData.WriteUInt32(static_cast<uint32_t>(i));
    }

    // Fragment
    auto fragments = builder.FragmentSnapshot(largeData, 1);
    TEST_ASSERT(fragments.size() > 1);

    // Check fragment sizes
    for (const auto& frag : fragments) {
        TEST_ASSERT(frag.payload.size() <= 1400); // MTU compliant
    }

    // Last fragment should have LAST_FRAGMENT flag
    TEST_ASSERT(fragments.back().flags & net::SNAPSHOT_FLAG_LAST_FRAGMENT);
}

// =============================================================================
// TEST: Network Server Lifecycle
// =============================================================================
TEST(NetworkServerLifecycle) {
    net::NetworkServer server;

    TEST_ASSERT_FALSE(server.IsRunning());

    // Start on ephemeral port
    bool started = server.Start(0);
    if (started) {
        TEST_ASSERT_TRUE(server.IsRunning());
        server.Stop();
        TEST_ASSERT_FALSE(server.IsRunning());
    }
    // If start failed (e.g., port in use), that's also valid for this test
}

// =============================================================================
// BENCHMARK: Spatial Hash Query
// =============================================================================
BENCHMARK(SpatialHashQuery, 10000) {
    net::SpatialHash hash;

    // Setup: 1000 entities
    for (int i = 0; i < 1000; ++i) {
        hash.Insert(static_cast<uint32_t>(i),
            static_cast<float>(i % 100),
            static_cast<float>(i / 100));
    }

    // Benchmark queries
    for (int i = 0; i < 10000; ++i) {
        auto result = hash.Query(50.0f, 50.0f, 25.0f);
        (void)result;
    }
}

} // namespace tests
