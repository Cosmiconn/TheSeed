#pragma once
// =============================================================================
// ecs/Components.h — Standard Game Components (AP-20 / AP-53)
// All components must be trivially copyable for SOA storage
// =============================================================================
#include <cstdint>
#include <string>
#include <array>

namespace game {

// =============================================================================
// Core Components
// =============================================================================
struct Transform {
    float x = 0.0f, y = 0.5f, z = 0.0f;
    float targetX = 0.0f, targetZ = 0.0f;
    float lerpX = 0.0f, lerpZ = 0.0f;
    float rotationY = 0.0f;
    float scale = 1.0f;
};

struct Velocity {
    float vx = 0.0f, vy = 0.0f, vz = 0.0f;
    float maxSpeed = 5.0f;
    float acceleration = 10.0f;
};

struct Health {
    int current = 100;
    int max = 100;
    int regeneration = 0;  // HP per second
};

struct Mana {
    int current = 100;
    int max = 100;
    int regeneration = 5;    // MP per second
};

// =============================================================================
// Combat Components (AP-53)
// =============================================================================
struct Hitbox {
    enum Type : uint8_t { Box = 0, Sphere = 1, Capsule = 2 };
    Type type = Sphere;
    float radius = 0.5f;       // For sphere/capsule
    float height = 1.0f;       // For capsule
    std::array<float, 3> extents{0.5f, 0.5f, 0.5f}; // For box
};

struct CombatStats {
    float armor = 0.0f;
    float resistance = 0.0f;
    float critChance = 0.05f;      // 5% base
    float critMultiplier = 1.5f;   // 150% damage
    float dodgeChance = 0.0f;
    float blockChance = 0.0f;
};

struct ComboState {
    uint32_t currentCombo = 0;
    float windowRemaining = 0.0f;  // Time left to continue combo
    float lastHitTime = 0.0f;
};

struct DamageRequest {
    int amount = 0;
    uint32_t sourceEntity = 0;
    bool isCrit = false;
    bool isBlocked = false;
};

// =============================================================================
// Identity Components
// =============================================================================
struct Name {
    std::string value;
};

struct PlayerTag {
    uint32_t clientId = 0;
    uint32_t accountId = 0;
    uint8_t privilegeLevel = 0;  // 0=Player, 1=GM, 2=Admin
};

struct MonsterTag {
    uint32_t templateId = 0;
    uint32_t spawnPointId = 0;
    float aggroRange = 10.0f;
    float leashRange = 20.0f;
    float spawnX = 0.0f, spawnZ = 0.0f;
};

struct NpcTag {
    uint32_t templateId = 0;
    uint32_t offeredQuestId = 0;
};

// =============================================================================
// Rendering Components
// =============================================================================
struct Renderable {
    std::string materialId;
    std::string meshId;
    float scaleY = 1.0f;
    uint32_t renderLayer = 0;
    bool visible = true;
    bool castShadow = true;
};

struct Animation {
    std::string currentState;    // "idle", "walk", "attack", "death"
    float normalizedTime = 0.0f;
    float speed = 1.0f;
    bool loop = true;
};

// =============================================================================
// Status Effect Component
// =============================================================================
struct StatusEffect {
    enum Type : uint8_t { 
        None = 0, 
        Burn = 1, 
        Freeze = 2, 
        Poison = 3, 
        Stun = 4, 
        Slow = 5, 
        Buff = 6, 
        Debuff = 7 
    };

    Type type = None;
    float duration = 0.0f;
    float tickInterval = 1.0f;
    float tickTimer = 0.0f;
    int tickDamage = 0;
    uint32_t sourceEntity = 0;
};

struct StatusEffectList {
    std::vector<StatusEffect> effects;
};

// =============================================================================
// Inventory Component
// =============================================================================
struct ItemSlot {
    uint32_t templateId = 0;
    uint32_t count = 0;
    uint32_t durability = 0;
};

struct Inventory {
    static constexpr size_t MAX_SLOTS = 64;
    std::array<ItemSlot, MAX_SLOTS> slots{};
    uint32_t gold = 0;
};

// =============================================================================
// Quest Component
// =============================================================================
struct QuestLogEntry {
    uint32_t questId = 0;
    uint32_t currentCount = 0;
    bool completed = false;
};

struct QuestLog {
    std::vector<QuestLogEntry> entries;
};

// =============================================================================
// Skill Component
// =============================================================================
struct SkillCooldown {
    uint32_t skillId = 0;
    float remaining = 0.0f;
};

struct SkillDeck {
    std::array<uint32_t, 8> activeSkills{0};
    std::vector<SkillCooldown> cooldowns;
};

// =============================================================================
// AI Components (AP-47–AP-49)
// =============================================================================
struct AIState {
    enum State : uint8_t { Idle = 0, Patrol = 1, Chase = 2, Attack = 3, Return = 4, Dead = 5 };
    State current = Idle;
    float stateTimer = 0.0f;
    uint32_t targetEntity = 0;
};

struct PatrolPath {
    std::vector<std::array<float, 3>> waypoints;
    uint32_t currentWaypoint = 0;
    bool loop = true;
};

// =============================================================================
// Network Sync Component (AP-37)
// =============================================================================
struct NetworkSync {
    uint32_t lastSnapshotSequence = 0;
    bool dirty = true;  // Needs to be included in next snapshot
    uint32_t priority = 0;  // Higher = more frequent updates
};

} // namespace game
