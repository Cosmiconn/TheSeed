#pragma once
// =============================================================================
// server/combat/CombatSystem.h — Combat Engine (AP-53)
// Hitbox detection, combo system, damage calculation, knockback
// =============================================================================
#include "../../ecs/Components.h"
#include "../../ecs/ecs_EcsWorld.h"
#include <cstdint>
#include <vector>
#include <functional>
#include <span>

namespace combat {

// =============================================================================
// Combat Events
// =============================================================================
struct HitEvent {
    uint32_t attackerId = 0;
    uint32_t victimId = 0;
    int damage = 0;
    bool isCritical = false;
    bool isBlocked = false;
    float knockbackX = 0.0f;
    float knockbackZ = 0.0f;
    uint32_t skillId = 0;
};

struct DeathEvent {
    uint32_t entityId = 0;
    uint32_t killerId = 0;
    uint32_t experienceReward = 0;
    uint32_t goldReward = 0;
};

struct ComboEvent {
    uint32_t entityId = 0;
    uint32_t comboCount = 0;
    float damageMultiplier = 1.0f;
};

// =============================================================================
// Damage Calculation
// =============================================================================
struct DamageResult {
    int finalDamage = 0;
    bool isCritical = false;
    bool isBlocked = false;
    bool isDodged = false;
    float damageReduction = 0.0f;
};

class DamageCalculator {
public:
    // Calculate damage from attacker to victim
    [[nodiscard]] static DamageResult Calculate(
        const game::CombatStats& attacker,
        const game::CombatStats& victim,
        int baseDamage,
        bool isSkill = false);

    // Apply combo multiplier
    [[nodiscard]] static int ApplyCombo(int baseDamage, uint32_t comboCount);

    // Apply status effect modifiers
    [[nodiscard]] static int ApplyStatusEffects(int damage, 
        std::span<const game::StatusEffect> attackerEffects,
        std::span<const game::StatusEffect> victimEffects);
};

// =============================================================================
// Hitbox / Collision Detection
// =============================================================================
class HitboxSystem {
public:
    // Check if attacker's hitbox overlaps victim's hitbox
    [[nodiscard]] static bool CheckHit(
        const game::Transform& attackerPos,
        const game::Hitbox& attackerHitbox,
        const game::Transform& victimPos,
        const game::Hitbox& victimHitbox);

    // Check if victim is within attacker's attack arc (cone)
    [[nodiscard]] static bool CheckAttackArc(
        const game::Transform& attacker,
        const game::Transform& victim,
        float attackAngle,      // Total angle in degrees
        float attackRange);

    // Get all entities within range of attacker
    [[nodiscard]] static std::vector<uint32_t> GetEntitiesInRange(
        ecs::EcsWorld& world,
        const game::Transform& center,
        float radius,
        uint32_t excludeEntity = 0);

    // Raycast for ranged attacks
    [[nodiscard]] static std::optional<uint32_t> Raycast(
        ecs::EcsWorld& world,
        float startX, float startY, float startZ,
        float dirX, float dirY, float dirZ,
        float maxDistance,
        uint32_t excludeEntity = 0);
};

// =============================================================================
// Combo System
// =============================================================================
class ComboSystem {
public:
    static constexpr float COMBO_WINDOW = 2.0f;        // Seconds to continue combo
    static constexpr float COMBO_DAMAGE_BONUS = 0.15f; // +15% per combo hit
    static constexpr uint32_t MAX_COMBO = 10;

    // Process a hit for combo tracking
    static void RegisterHit(ecs::EcsWorld& world, ecs::EntityHandle entity);

    // Get current combo info
    [[nodiscard]] static uint32_t GetComboCount(ecs::EcsWorld& world, ecs::EntityHandle entity);
    [[nodiscard]] static float GetComboMultiplier(ecs::EcsWorld& world, ecs::EntityHandle entity);
    [[nodiscard]] static float GetComboTimeRemaining(ecs::EcsWorld& world, ecs::EntityHandle entity);

    // Reset combo (on miss, death, or timeout)
    static void ResetCombo(ecs::EcsWorld& world, ecs::EntityHandle entity);

    // Update all combos (call every tick)
    static void Update(ecs::EcsWorld& world, float deltaTime);
};

// =============================================================================
// Combat System (main ECS system)
// =============================================================================
class CombatSystem {
public:
    using HitCallback = std::function<void(const HitEvent&)>;
    using DeathCallback = std::function<void(const DeathEvent&)>;
    using ComboCallback = std::function<void(const ComboEvent&)>;

private:
    HitCallback onHit;
    DeathCallback onDeath;
    ComboCallback onCombo;

public:
    void SetHitCallback(HitCallback cb) { onHit = std::move(cb); }
    void SetDeathCallback(DeathCallback cb) { onDeath = std::move(cb); }
    void SetComboCallback(ComboCallback cb) { onCombo = std::move(cb); }

    // Process an attack request
    void ProcessAttack(ecs::EcsWorld& world, ecs::EntityHandle attacker, 
                        uint32_t skillId = 0);

    // Process a skill use (with area of effect)
    void ProcessSkill(ecs::EcsWorld& world, ecs::EntityHandle caster,
                       uint32_t skillId, float targetX, float targetZ);

    // Apply damage to entity
    void ApplyDamage(ecs::EcsWorld& world, ecs::EntityHandle victim,
                      int damage, uint32_t attackerId = 0);

    // Apply healing
    void ApplyHeal(ecs::EcsWorld& world, ecs::EntityHandle target, int amount);

    // Apply knockback
    void ApplyKnockback(ecs::EcsWorld& world, ecs::EntityHandle target,
                         float dirX, float dirZ, float force);

    // Main update (call every tick)
    void Update(ecs::EcsWorld& world, float deltaTime);

    // Death handling
    void HandleDeath(ecs::EcsWorld& world, ecs::EntityHandle entity, 
                       uint32_t killerId);

    // Respawn handling
    void RespawnEntity(ecs::EcsWorld& world, ecs::EntityHandle entity,
                        float spawnX, float spawnY, float spawnZ);

private:
    void ExecuteAttack(ecs::EcsWorld& world, ecs::EntityHandle attacker,
                        const game::Transform& attackerTransform,
                        const game::Hitbox& attackerHitbox,
                        const game::CombatStats& attackerStats,
                        uint32_t skillId);

    void BroadcastHit(const HitEvent& event);
    void BroadcastDeath(const DeathEvent& event);
    void BroadcastCombo(const ComboEvent& event);
};

// =============================================================================
// Skill Execution
// =============================================================================
struct SkillExecution {
    uint32_t skillId = 0;
    ecs::EntityHandle caster;
    float castTime = 0.0f;
    float remainingCastTime = 0.0f;
    float targetX = 0.0f;
    float targetZ = 0.0f;
    bool isChanneling = false;
};

class SkillExecutor {
    std::vector<SkillExecution> activeSkills;

public:
    void StartSkill(ecs::EcsWorld& world, ecs::EntityHandle caster,
                     uint32_t skillId, float targetX, float targetZ);

    void Update(ecs::EcsWorld& world, CombatSystem& combat, float deltaTime);
    void CancelSkill(ecs::EntityHandle caster);

    [[nodiscard]] bool IsCasting(ecs::EntityHandle caster) const;
    [[nodiscard]] float GetCastProgress(ecs::EntityHandle caster) const;
};

} // namespace combat
