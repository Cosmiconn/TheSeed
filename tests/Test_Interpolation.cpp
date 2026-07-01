// =============================================================================
// tests/Test_Interpolation.cpp — Interpolation Unit Tests (P5-FIX)
// =============================================================================
#include "TestMain.h"
#include "../client/Interpolation.h"

#include <thread>
#include <chrono>

namespace tests {

// =============================================================================
// TEST: Basic Interpolation
// =============================================================================
TEST(BasicInterpolation) {
    client::EntityInterpolator interp;

    auto now = std::chrono::steady_clock::now();

    // Add two snapshots
    client::InterpSnapshot snap1;
    snap1.x = 0.0f; snap1.y = 0.0f; snap1.z = 0.0f;
    snap1.timestamp = now - std::chrono::milliseconds(200);
    snap1.sequenceId = 1;
    interp.AddSnapshot(snap1);

    client::InterpSnapshot snap2;
    snap2.x = 10.0f; snap2.y = 0.0f; snap2.z = 0.0f;
    snap2.timestamp = now - std::chrono::milliseconds(100);
    snap2.sequenceId = 2;
    interp.AddSnapshot(snap2);

    // Interpolate at current time (100ms delay)
    auto result = interp.Interpolate(now);

    // Should be between 0 and 10
    TEST_ASSERT(result.x >= 0.0f);
    TEST_ASSERT(result.x <= 10.0f);
}

// =============================================================================
// TEST: Extrapolation
// =============================================================================
TEST(Extrapolation) {
    client::EntityInterpolator interp;

    auto now = std::chrono::steady_clock::now();

    // Add one snapshot with velocity
    client::InterpSnapshot snap;
    snap.x = 0.0f; snap.y = 0.0f; snap.z = 0.0f;
    snap.vx = 10.0f; snap.vy = 0.0f; snap.vz = 0.0f;
    snap.timestamp = now - std::chrono::milliseconds(200);
    snap.sequenceId = 1;
    interp.AddSnapshot(snap);

    // Extrapolate 100ms into future
    auto future = now + std::chrono::milliseconds(100);
    auto result = interp.Interpolate(future);

    // Should have moved forward
    TEST_ASSERT(result.x > 0.0f);
}

// =============================================================================
// TEST: Dead Reckoning
// =============================================================================
TEST(DeadReckoning) {
    client::EntityInterpolator interp;

    // Initial position
    client::InterpSnapshot snap;
    snap.x = 0.0f; snap.y = 0.0f; snap.z = 0.0f;
    snap.vx = 5.0f; snap.vy = 0.0f; snap.vz = 0.0f;
    snap.timestamp = std::chrono::steady_clock::now();
    snap.sequenceId = 1;
    interp.AddSnapshot(snap);

    // Update with dead reckoning
    math::Vector3 velocity(5.0f, 0.0f, 0.0f);
    interp.UpdateDeadReckoning(velocity, 0.1f); // 100ms

    // Predicted position should be (0.5, 0, 0)
    auto result = interp.Interpolate(std::chrono::steady_clock::now());
    TEST_ASSERT_NEAR(0.5f, result.x, 0.1f);
}

// =============================================================================
// TEST: Snapshot History Limit
// =============================================================================
TEST(SnapshotHistoryLimit) {
    client::EntityInterpolator interp;

    auto now = std::chrono::steady_clock::now();

    // Add many old snapshots
    for (int i = 0; i < 100; ++i) {
        client::InterpSnapshot snap;
        snap.x = static_cast<float>(i);
        snap.timestamp = now - std::chrono::seconds(10) + std::chrono::milliseconds(i * 10);
        snap.sequenceId = static_cast<uint32_t>(i);
        interp.AddSnapshot(snap);
    }

    // Should only keep recent snapshots (< 2 seconds)
    TEST_ASSERT(interp.GetSnapshotCount() < 100);
}

// =============================================================================
// TEST: Interpolation Manager
// =============================================================================
TEST(InterpolationManager) {
    client::InterpolationManager manager;

    auto now = std::chrono::steady_clock::now();

    // Add snapshots for multiple entities
    for (int i = 0; i < 10; ++i) {
        client::InterpSnapshot snap;
        snap.x = static_cast<float>(i);
        snap.y = 0.0f;
        snap.z = 0.0f;
        snap.timestamp = now;
        snap.sequenceId = static_cast<uint32_t>(i);
        manager.AddSnapshot(static_cast<uint32_t>(i), snap);
    }

    TEST_ASSERT_EQ(10u, manager.GetEntityCount());

    // Update
    manager.Update(0.016f);

    // Get positions
    for (int i = 0; i < 10; ++i) {
        auto pos = manager.GetInterpolatedPosition(static_cast<uint32_t>(i));
        TEST_ASSERT_EQ(static_cast<float>(i), pos.x);
    }

    // Remove stale entities
    manager.RemoveStaleEntities({0, 1, 2, 3, 4});
    TEST_ASSERT_EQ(5u, manager.GetEntityCount());
}

// =============================================================================
// BENCHMARK: Interpolation
// =============================================================================
BENCHMARK(Interpolation, 10000) {
    client::EntityInterpolator interp;
    auto now = std::chrono::steady_clock::now();

    // Setup: 10 snapshots
    for (int i = 0; i < 10; ++i) {
        client::InterpSnapshot snap;
        snap.x = static_cast<float>(i);
        snap.timestamp = now - std::chrono::milliseconds(200 - i * 20);
        interp.AddSnapshot(snap);
    }

    // Benchmark
    for (int i = 0; i < 10000; ++i) {
        auto result = interp.Interpolate(now);
        (void)result;
    }
}

} // namespace tests
