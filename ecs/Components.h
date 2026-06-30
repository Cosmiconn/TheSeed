#pragma once
// =============================================================================
// ecs/Components.h — ECS Game Components (AP-20)
// =============================================================================
// KORREKTUR: Vollständige Komponenten-Definitionen für das Archetype-ECS.
// Alle Komponenten sind POD-Typen für effiziente SOA-Speicherung.
// =============================================================================
#include <string>
#include <vector>
#include <cstdint>

namespace ecs {

// =============================================================================
// BASIS-KOMPONENTEN
// =============================================================================
struct PositionComponent {
    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;
};

struct RotationComponent {
    float pitch = 0.0f;
    float yaw = 0.0f;
    float roll = 0.0f;
};

struct ScaleComponent {
    float x = 1.0f;
    float y = 1.0f;
    float z = 1.0f;
};

struct VelocityComponent {
    float vx = 0.0f;
    float vy = 0.0f;
    float vz = 0.0f;
};

// =============================================================================
// GAMEPLAY-KOMPONENTEN
// =============================================================================
struct HealthComponent {
    int currentHP = 100;
    int maxHP = 100;
    bool isAlive = true;
};

struct NameComponent {
    std::string name = "Unknown";
};

struct RenderComponentECS {
    std::string materialId = "default";
    std::string meshId = "default";
};

// =============================================================================
// AI-KOMPONENTEN
// =============================================================================
struct AIComponent {
    AIBehaviorType behaviorType = AIBehaviorType::None;
    float aggroRadius = 10.0f;
    float moveSpeed = 2.0f;
    float attackCooldown = 0.0f;
};

// =============================================================================
// TAG-KOMPONENTEN (0 Bytes, nur für Filterung)
// =============================================================================
struct PlayerTag {};

// =============================================================================
// MIGRATIONS-KOMPONENTEN
// =============================================================================
struct LegacyIdComponent {
    uint32_t legacyId = 0;
};

// =============================================================================
// STATUS-EFFEKTE
// =============================================================================
struct StatusEffectData {
    StatusEffectType type = StatusEffectType::None;
    float remainingDuration = 0.0f;
    float timeSinceLastTick = 0.0f;
    uint32_t tickDamage = 0;
    bool isActive = false;
};

struct StatusEffectComponent {
    std::vector<StatusEffectData> effects;
};

// =============================================================================
// KAMPF
// =============================================================================
struct CombatComponent {
    uint32_t incomingDamage = 0;
    uint32_t outgoingDamage = 0;
    float attackRange = 2.0f;
    float attackCooldown = 0.0f;
};

} // namespace ecs
