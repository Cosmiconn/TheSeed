#pragma once

#include "core/diagnostics/diagnostics_config.h"
#include "core/diagnostics/event_timeline.h"
#include "core/diagnostics/snapshot_dump.h"
#include "core/ecs/entity.h"
#include "core/ecs/archetype.h"
#include <string>
#include <vector>

namespace seed::ecs {
    class World;
    class Archetype;
    class ArchetypeManager;
}

namespace seed::diagnostics {

// ---------------------------------------------------------------------------
// EcsValidationResult – detailed result of a validation pass
// ---------------------------------------------------------------------------
struct EcsValidationResult {
    bool        success = true;
    std::string message;
    EventType   failingEvent = EventType::WorldValidate;
    seed::ecs::Entity failingEntity = seed::ecs::INVALID_ENTITY;
    uint32_t    failingArchetypeHash = 0;
    const char* file = "";
    int         line = 0;

    void fail(const char* msg,
              EventType ev = EventType::InvariantFail,
              seed::ecs::Entity e = seed::ecs::INVALID_ENTITY,
              uint32_t archHash = 0,
              const char* f = "",
              int l = 0) noexcept {
        success = false;
        message = msg;
        failingEvent = ev;
        failingEntity = e;
        failingArchetypeHash = archHash;
        file = f;
        line = l;
    }
};

// ---------------------------------------------------------------------------
// EcsValidator – comprehensive ECS validation (TEDF Layer 2)
// ---------------------------------------------------------------------------
// Validates:
//   - Entity ↔ Record consistency
//   - Entity ↔ Archetype row mapping
//   - ComponentArray size consistency
//   - Version integrity
//   - Archetype signature ↔ column consistency
//   - No duplicate components per entity
//   - Memory integrity (no nullptr columns)
// ---------------------------------------------------------------------------
class EcsValidator {
public:
    // Validate entire world state. Returns true if all invariants hold.
    static bool validateWorld(const seed::ecs::World& world,
                              EcsValidationResult* outResult = nullptr);

    // Validate single archetype
    static bool validateArchetype(const seed::ecs::Archetype& arch,
                                  EcsValidationResult* outResult = nullptr);

    // Validate archetype manager consistency
    static bool validateArchetypeManager(const seed::ecs::ArchetypeManager& mgr,
                                         EcsValidationResult* outResult = nullptr);

    // Validate entity record after mutation
    static bool validateEntityRecord(const seed::ecs::World& world,
                                     seed::ecs::Entity e,
                                     EcsValidationResult* outResult = nullptr);

    // Deep validation: check component memory integrity (no corruption)
    static bool validateComponentMemory(const seed::ecs::Archetype& arch,
                                        EcsValidationResult* outResult = nullptr);

    // Dump full validation report to string
    static std::string fullReport(const seed::ecs::World& world);

private:
    static bool checkEntityRecordConsistency(const seed::ecs::World& world,
                                             EcsValidationResult& result);
    static bool checkArchetypeEntityMapping(const seed::ecs::World& world,
                                            EcsValidationResult& result);
    static bool checkComponentArraySizes(const seed::ecs::World& world,
                                         EcsValidationResult& result);
    static bool checkNoDuplicateComponents(const seed::ecs::World& world,
                                           EcsValidationResult& result);
    static bool checkVersionIntegrity(const seed::ecs::World& world,
                                      EcsValidationResult& result);
    static bool checkNullColumns(const seed::ecs::World& world,
                                 EcsValidationResult& result);
};

// ---------------------------------------------------------------------------
// Validation macros – auto-log to timeline on failure
// ---------------------------------------------------------------------------
#if SEED_DIAGNOSTICS_ECS_VALIDATION
#  define SEED_VALIDATE_WORLD(world)      do {          ::seed::diagnostics::EcsValidationResult _vr;          if (!::seed::diagnostics::EcsValidator::validateWorld((world), &_vr)) {              ::seed::diagnostics::globalTimeline().push(                  ::seed::diagnostics::EventType::InvariantFail,                  _vr.failingEntity, _vr.failingArchetypeHash, 0, 0,                  _vr.message.c_str(), _vr.file, _vr.line);              /* Snapshot before assert to capture ECS state for post-mortem */              ::seed::diagnostics::SnapshotOnFailure::trigger(                  _vr.message.c_str(), _vr.file, _vr.line, &(world));              SEED_ASSERT(false, _vr.message.c_str());          }      } while(0)
#else
#  define SEED_VALIDATE_WORLD(world) ((void)0)
#endif

} // namespace seed::diagnostics
