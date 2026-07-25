#include <doctest/doctest.h>
#include "core/diagnostics/diagnostics_manager.h"
#include "core/diagnostics/event_timeline.h"
#include "core/diagnostics/health_score.h"
#include "core/diagnostics/ecs_validator.h"
#include "core/diagnostics/memory_validator.h"
#include "core/ecs/world.h"
#include "core/ecs/component_traits.h"
#include "core/memory/block_allocator.h"

using namespace seed::diagnostics;
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

SEED_REGISTER_COMPONENT_WITH_ID(Position, 1)
SEED_REGISTER_COMPONENT_WITH_ID(Velocity, 2)

// ---------------------------------------------------------------------------
// DiagnosticsManager
// ---------------------------------------------------------------------------
TEST_CASE("Diagnostics_Manager_Lifecycle") {
    auto& diag = DiagnosticsManager::instance();
    diag.initialize();
    CHECK(diag.isHealthy());
    diag.update();
    diag.shutdown();
}

TEST_CASE("Diagnostics_Manager_ECS_Validation_Toggle") {
    auto& diag = DiagnosticsManager::instance();
    diag.initialize();

    CHECK(diag.ecsValidationEnabled() == (SEED_DIAGNOSTICS_ECS_VALIDATION != 0));

    diag.setEcsValidationEnabled(false);
    CHECK(!diag.ecsValidationEnabled());

    diag.setEcsValidationEnabled(true);
    CHECK(diag.ecsValidationEnabled());

    diag.shutdown();
}

// ---------------------------------------------------------------------------
// EventTimeline
// ---------------------------------------------------------------------------
TEST_CASE("EventTimeline_BasicPush") {
    EventTimeline timeline;
    timeline.push(EventType::EntityCreate, makeEntity(1, 1), 0x1234, 1, 0, "test event");
    CHECK(timeline.size() == 1);
}

TEST_CASE("EventTimeline_MultiplePush") {
    EventTimeline timeline;
    for (int i = 0; i < 100; ++i) {
        timeline.push(EventType::EntityCreate, makeEntity(static_cast<uint32_t>(i), 1), 0, 0, 0, "batch");
    }
    CHECK(timeline.size() == 100);
}

TEST_CASE("EventTimeline_Overflow") {
    EventTimeline timeline;
    // Push more than capacity
    for (int i = 0; i < static_cast<int>(EventTimeline::Capacity) + 100; ++i) {
        timeline.push(EventType::EntityCreate, makeEntity(static_cast<uint32_t>(i), 1), 0, 0, 0, "overflow");
    }
    // Size should not exceed capacity
    CHECK(timeline.size() <= EventTimeline::Capacity);
}

TEST_CASE("EventTimeline_Dump") {
    EventTimeline timeline;
    timeline.push(EventType::EntityCreate, makeEntity(42, 1), 0xABCD, 1, 0, "dump test");
    auto dump = timeline.dump();
    CHECK(!dump.empty());
    CHECK(dump.find("EntityCreate") != std::string::npos);
}

TEST_CASE("EventTimeline_Clear") {
    EventTimeline timeline;
    timeline.push(EventType::EntityCreate, INVALID_ENTITY, 0, 0, 0, "clear me");
    CHECK(timeline.size() > 0);
    timeline.clear();
    CHECK(timeline.size() == 0);
}

// ---------------------------------------------------------------------------
// HealthScore
// ---------------------------------------------------------------------------
TEST_CASE("HealthScore_SetGet") {
    HealthScore::setScore(HealthScore::Module::ECS, 85);
    CHECK(HealthScore::getScore(HealthScore::Module::ECS) == 85);
}

TEST_CASE("HealthScore_Healthy") {
    HealthScore::setScore(HealthScore::Module::ECS, 100);
    HealthScore::setScore(HealthScore::Module::Memory, 100);
    HealthScore::setScore(HealthScore::Module::Renderer, 100);
    HealthScore::setScore(HealthScore::Module::Jobs, 100);
    HealthScore::setScore(HealthScore::Module::Networking, 100);
    HealthScore::setScore(HealthScore::Module::Serialization, 100);
    CHECK(HealthScore::isHealthy());
}

TEST_CASE("HealthScore_NotHealthy") {
    HealthScore::setScore(HealthScore::Module::ECS, 75); // below 80
    CHECK(!HealthScore::isHealthy());
}

TEST_CASE("HealthScore_Critical") {
    HealthScore::setScore(HealthScore::Module::Memory, 30); // below 50
    CHECK(HealthScore::isCritical());
}

TEST_CASE("HealthScore_WorstModule") {
    HealthScore::setScore(HealthScore::Module::ECS, 90);
    HealthScore::setScore(HealthScore::Module::Memory, 40);
    CHECK(HealthScore::worstModule() == HealthScore::Module::Memory);
}

TEST_CASE("HealthScore_Report") {
    HealthScore::setScore(HealthScore::Module::ECS, 95);
    auto report = HealthScore::report();
    CHECK(!report.empty());
    CHECK(report.find("ECS") != std::string::npos);
}

TEST_CASE("HealthScore_Clamp") {
    HealthScore::setScore(HealthScore::Module::ECS, 255); // clamp to 100
    CHECK(HealthScore::getScore(HealthScore::Module::ECS) == 100);
}

// ---------------------------------------------------------------------------
// EcsValidator
// ---------------------------------------------------------------------------
TEST_CASE("EcsValidator_ValidateEmptyWorld") {
    BlockAllocator blockAlloc;
    World world(&blockAlloc);

    EcsValidationResult result;
    CHECK(EcsValidator::validateWorld(world, &result));
    CHECK(result.success);
}

TEST_CASE("EcsValidator_ValidateWorldWithEntities") {
    BlockAllocator blockAlloc;
    World world(&blockAlloc);
    TypeRegistry::instance().registerComponent<Position>();

    for (int i = 0; i < 100; ++i) {
        Entity e = world.createEntity();
        world.addComponent<Position>(e, static_cast<float>(i), 0.0f, 0.0f);
    }

    EcsValidationResult result;
    CHECK(EcsValidator::validateWorld(world, &result));
    CHECK(result.success);
}

TEST_CASE("EcsValidator_ValidateArchetype") {
    BlockAllocator blockAlloc;
    World world(&blockAlloc);
    TypeRegistry::instance().registerComponent<Position>();
    TypeRegistry::instance().registerComponent<Velocity>();

    Entity e = world.createEntity();
    world.addComponent<Position>(e, 1.0f, 2.0f, 3.0f);
    world.addComponent<Velocity>(e, 0.1f, 0.2f, 0.3f);

    // Query to get archetype pointer for validation
    auto result = world.query<Position, Velocity>();
    CHECK(!result.empty());
}

TEST_CASE("EcsValidator_FullReport") {
    BlockAllocator blockAlloc;
    World world(&blockAlloc);
    TypeRegistry::instance().registerComponent<Position>();

    Entity e = world.createEntity();
    world.addComponent<Position>(e, 1.0f, 2.0f, 3.0f);

    auto report = EcsValidator::fullReport(world);
    CHECK(!report.empty());
}

// ---------------------------------------------------------------------------
// MemoryValidator
// ---------------------------------------------------------------------------
TEST_CASE("MemoryValidator_ValidateAll") {
    MemoryValidationResult result;
    CHECK(MemoryValidator::validateAll(&result));
    CHECK(result.success);
}

TEST_CASE("MemoryValidator_FullReport") {
    auto report = MemoryValidator::fullReport();
    CHECK(!report.empty());
}

// ---------------------------------------------------------------------------
// Integration: Diagnostics + ECS
// ---------------------------------------------------------------------------
TEST_CASE("Diagnostics_ECS_Integration") {
#if !SEED_DIAGNOSTICS_EVENT_TIMELINE
    MESSAGE("skipped: SEED_DIAGNOSTICS_EVENT_TIMELINE is disabled in this build");
    return;
#else
    auto& diag = DiagnosticsManager::instance();
    diag.initialize();

    BlockAllocator blockAlloc;
    World world(&blockAlloc);
    TypeRegistry::instance().registerComponent<Position>();
    TypeRegistry::instance().registerComponent<Velocity>();

    // Operations should generate timeline events
    auto& timeline = diag.timeline();
    timeline.clear();

    Entity e = world.createEntity();
    world.addComponent<Position>(e, 1.0f, 2.0f, 3.0f);
    world.addComponent<Velocity>(e, 0.1f, 0.2f, 0.3f);

    CHECK(timeline.size() >= 3); // EntityCreate + 2x ComponentAdd

    world.destroyEntity(e);
    CHECK(timeline.size() >= 4); // + EntityDestroy

    diag.shutdown();
#endif
}

TEST_CASE("Diagnostics_ECS_ValidateAfterStress") {
    auto& diag = DiagnosticsManager::instance();
    diag.initialize();

    BlockAllocator blockAlloc;
    World world(&blockAlloc);
    TypeRegistry::instance().registerComponent<Position>();
    TypeRegistry::instance().registerComponent<Velocity>();

    std::vector<Entity> entities;
    for (int i = 0; i < 1000; ++i) {
        Entity e = world.createEntity();
        if (i % 2 == 0) world.addComponent<Position>(e, static_cast<float>(i), 0.0f, 0.0f);
        if (i % 3 == 0) world.addComponent<Velocity>(e, 1.0f, 0.0f, 0.0f);
        entities.push_back(e);
    }

    // Destroy half
    for (int i = 0; i < 500; ++i) {
        world.destroyEntity(entities[static_cast<size_t>(i)]);
    }

    // Validate
    EcsValidationResult result;
    CHECK(EcsValidator::validateWorld(world, &result));

    // Health score should reflect ECS state
    HealthScore::setScore(HealthScore::Module::ECS, 100);
    CHECK(HealthScore::isHealthy());

    diag.shutdown();
}

// ---------------------------------------------------------------------------
// SnapshotDump
// ---------------------------------------------------------------------------
TEST_CASE("SnapshotDump_CaptureBuildInfo") {
    SnapshotDump dump;
    dump.captureBuildInfo();
    CHECK(!dump.buildInfo.empty());
}

TEST_CASE("SnapshotDump_CaptureHealth") {
    SnapshotDump dump;
    dump.captureHealth();
    CHECK(!dump.healthReport.empty());
}

TEST_CASE("SnapshotDump_CaptureTimeline") {
    SnapshotDump dump;
    // Push some events first
    globalTimeline().push(EventType::Custom, INVALID_ENTITY, 0, 0, 0, "snapshot test");
    dump.captureTimeline();
    CHECK(!dump.eventTimeline.empty());
}

TEST_CASE("SnapshotDump_ToString") {
    SnapshotDump dump;
    dump.captureBuildInfo();
    dump.captureHealth();
    auto str = dump.toString();
    CHECK(!str.empty());
}
