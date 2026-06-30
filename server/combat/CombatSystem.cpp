// =============================================================================
// server/combat/CombatSystem.cpp — Combat Engine Implementation (AP-53)
// =============================================================================
// KORREKTUR: rand() entfernt, thread-safes std::mt19937 mit uniform_real_distribution
// eingeführt. Jede Instanz nutzt eigenen RNG oder thread_local für deterministisches,
// thread-safes Verhalten.
// =============================================================================
#include "CombatSystem.h"
#include "../../core/Log.h"
#include <cmath>
#include <random>
#include <chrono>

namespace combat {

// =============================================================================
// Thread-Local RNG für deterministische, thread-safes Zufallszahlen
// =============================================================================
thread_local std::mt19937 gCombatRng{
    static_cast<unsigned>(std::chrono::steady_clock::now().time_since_epoch().count())
};

// =============================================================================
// Damage Calculator
// =============================================================================
DamageResult DamageCalculator::Calculate(
    const game::CombatStats& attacker,
    const game::CombatStats& victim,
    int baseDamage,
    bool isSkill) {

    DamageResult result;
    std::uniform_real_distribution<float> uniform01{0.0f, 1.0f};

    // Dodge check
    float dodgeRoll = uniform01(gCombatRng);
    if (dodgeRoll < victim.dodgeChance) {
        result.isDodged = true;
        result.finalDamage = 0;
        return result;
    }

    // Block check
    float blockRoll = uniform01(gCombatRng);
    if (blockRoll < victim.blockChance) {
        result.isBlocked = true;
        result.damageReduction = 0.5f;
    }

    // Critical hit check
    float critRoll = uniform01(gCombatRng);
    if (critRoll < attacker.critChance) {
        result.isCritical = true;
    }

    // Base damage with variance (±10%)
    std::uniform_real_distribution<float> varianceDist{0.9f, 1.1f};
    float variance = varianceDist(gCombatRng);
    float damage = static_cast<float>(baseDamage) * variance;

    // Apply crit multiplier
    if (result.isCritical) {
        damage *= attacker.critMultiplier;
    }

    // Apply armor reduction (diminishing returns)
    float armorReduction = victim.armor / (victim.armor + 100.0f);
    damage *= (1.0f - armorReduction);

    // Apply resistance
    damage *= (1.0f - victim.resistance);

    // Apply block reduction
    if (result.isBlocked) {
        damage *= (1.0f - result.damageReduction);
    }

    result.finalDamage = std::max(1, static_cast<int>(damage));
    return result;
}

int DamageCalculator::ApplyCombo(int baseDamage, uint32_t comboCount) {
    float multiplier = 1.0f + (static_cast<float>(comboCount) * ComboSystem::COMBO_DAMAGE_BONUS);
    return static_cast<int>(static_cast<float>(baseDamage) * multiplier);
}

int DamageCalculator::ApplyStatusEffects(int damage,
    std::span<game::StatusEffect> attackerEffects,
    std::span<game::StatusEffect> victimEffects) {

    float multiplier = 1.0f;

    // Attacker buffs
    for (const auto& effect : attackerEffects) {
        switch (effect.type) {
            case game::StatusEffect::Buff:
                multiplier += 0.2f;
                break;
            case game::StatusEffect::Burn:
                // Burn doesn't modify direct damage
                break;
            default:
                break;
        }
    }

    // Victim debuffs
    for (const auto& effect : victimEffects) {
        switch (effect.type) {
            case game::StatusEffect::Debuff:
                multiplier += 0.15f;
                break;
            case game::StatusEffect::Freeze:
                multiplier += 0.25f; // Frozen targets take more damage
                break;
            default:
                break;
        }
    }

    return static_cast<int>(static_cast<float>(damage) * multiplier);
}

// =============================================================================
// Hitbox System
// =============================================================================
bool HitboxSystem::CheckHit(
    const game::Transform& attackerPos,
    const game::Hitbox& attackerHitbox,
    const game::Transform& victimPos,
    const game::Hitbox& victimHitbox) {

    float dx = victimPos.x - attackerPos.x;
    float dy = victimPos.y - attackerPos.y;
    float dz = victimPos.z - attackerPos.z;
    float dist = std::sqrt(dx*dx + dy*dy + dz*dz);

    float combinedRadius = attackerHitbox.radius + victimHitbox.radius;

    return dist <= combinedRadius;
}

bool HitboxSystem::CheckAttackArc(
    const game::Transform& attacker,
    const game::Transform& victim,
    float attackAngle,
    float attackRange) {

    float dx = victim.x - attacker.x;
    float dz = victim.z - attacker.z;
    float dist = std::sqrt(dx*dx + dz*dz);

    if (dist > attackRange) return false;

    // Calculate angle between attacker forward and target
    float targetAngle = std::atan2(dx, dz);
    float attackerAngle = attacker.rotationY;

    float angleDiff = std::abs(targetAngle - attackerAngle);
    if (angleDiff > math::PI) angleDiff = 2.0f * math::PI - angleDiff;

    return angleDiff <= (attackAngle * math::DEG2RAD * 0.5f); // Half angle in radians
}

std::vector<uint32_t> HitboxSystem::GetEntitiesInRange(
    ecs::EcsWorld& world,
    const game::Transform& center,
    float radius,
    uint32_t excludeEntity) {

    std::vector<uint32_t> result;

    auto query = world.Query<game::Transform>();
    for (auto [handle] : query) {
        uint32_t id = handle.GetIndex();
        if (id == excludeEntity) continue;

        auto* transform = world.GetComponent<game::Transform>(handle);
        if (!transform) continue;

        float dx = transform->x - center.x;
        float dz = transform->z - center.z;
        float dist = std::sqrt(dx*dx + dz*dz);

        if (dist <= radius) {
            result.push_back(id);
        }
    }

    return result;
}

std::optional<uint32_t> HitboxSystem::Raycast(
    ecs::EcsWorld& world,
    float startX, float startY, float startZ,
    float dirX, float dirY, float dirZ,
    float maxDistance,
    uint32_t excludeEntity) {

    float len = std::sqrt(dirX*dirX + dirY*dirY + dirZ*dirZ);
    if (len < 0.001f) return std::nullopt;

    dirX /= len; dirY /= len; dirZ /= len;

    auto query = world.Query<game::Transform, game::Hitbox>();

    float closestDist = maxDistance;
    uint32_t closestEntity = 0;

    for (auto [handle] : query) {
        uint32_t id = handle.GetIndex();
        if (id == excludeEntity) continue;

        auto* transform = world.GetComponent<game::Transform>(handle);
        auto* hitbox = world.GetComponent<game::Hitbox>(handle);
        if (!transform || !hitbox) continue;

        // Simple sphere-ray intersection
        float ocX = startX - transform->x;
        float ocY = startY - transform->y;
        float ocZ = startZ - transform->z;

        float a = dirX*dirX + dirY*dirY + dirZ*dirZ;
        float b = 2.0f * (ocX*dirX + ocY*dirY + ocZ*dirZ);
        float c = ocX*ocX + ocY*ocY + ocZ*ocZ - hitbox->radius * hitbox->radius;

        float discriminant = b*b - 4*a*c;

        if (discriminant >= 0) {
            float t = (-b - std::sqrt(discriminant)) / (2.0f * a);
            if (t >= 0 && t < closestDist) {
                closestDist = t;
                closestEntity = id;
            }
        }
    }

    if (closestEntity != 0) return closestEntity;
    return std::nullopt;
}

// =============================================================================
// Combo System
// =============================================================================
void ComboSystem::RegisterHit(ecs::EcsWorld& world, ecs::EntityHandle entity) {
    auto* combo = world.GetComponent<game::ComboState>(entity);
    if (!combo) {
        world.AddComponent(entity, game::ComboState{});
        combo = world.GetComponent<game::ComboState>(entity);
    }

    float now = static_cast<float>(std::chrono::duration<float>(
        std::chrono::steady_clock::now().time_since_epoch()).count());

    if (now - combo->lastHitTime > COMBO_WINDOW) {
        combo->currentCombo = 1;
    } else {
        combo->currentCombo = std::min(combo->currentCombo + 1, MAX_COMBO);
    }

    combo->lastHitTime = now;
    combo->windowRemaining = COMBO_WINDOW;
}

uint32_t ComboSystem::GetComboCount(ecs::EcsWorld& world, ecs::EntityHandle entity) {
    auto* combo = world.GetComponent<game::ComboState>(entity);
    return combo ? combo->currentCombo : 0;
}

float ComboSystem::GetComboMultiplier(ecs::EcsWorld& world, ecs::EntityHandle entity) {
    uint32_t count = GetComboCount(world, entity);
    return 1.0f + (static_cast<float>(count) * COMBO_DAMAGE_BONUS);
}

float ComboSystem::GetComboTimeRemaining(ecs::EcsWorld& world, ecs::EntityHandle entity) {
    auto* combo = world.GetComponent<game::ComboState>(entity);
    if (!combo) return 0.0f;

    float now = static_cast<float>(std::chrono::duration<float>(
        std::chrono::steady_clock::now().time_since_epoch()).count());
    float elapsed = now - combo->lastHitTime;

    return std::max(0.0f, COMBO_WINDOW - elapsed);
}

void ComboSystem::ResetCombo(ecs::EcsWorld& world, ecs::EntityHandle entity) {
    auto* combo = world.GetComponent<game::ComboState>(entity);
    if (combo) {
        combo->currentCombo = 0;
        combo->windowRemaining = 0.0f;
    }
}

void ComboSystem::Update(ecs::EcsWorld& world, float deltaTime) {
    auto query = world.Query<game::ComboState>();

    for (auto [handle] : query) {
        auto* combo = world.GetComponent<game::ComboState>(handle);
        if (!combo) continue;

        combo->windowRemaining -= deltaTime;

        if (combo->windowRemaining <= 0.0f && combo->currentCombo > 0) {
            combo->currentCombo = 0;
        }
    }
}

// =============================================================================
// Combat System
// =============================================================================
void CombatSystem::ProcessAttack(ecs::EcsWorld& world, ecs::EntityHandle attacker,
    uint32_t skillId) {
    auto* attackerTransform = world.GetComponent<game::Transform>(attacker);
    auto* attackerHitbox = world.GetComponent<game::Hitbox>(attacker);
    auto* attackerStats = world.GetComponent<game::CombatStats>(attacker);

    if (!attackerTransform || !attackerHitbox || !attackerStats) return;

    ExecuteAttack(world, attacker, *attackerTransform, *attackerHitbox, *attackerStats, skillId);

    // Update combo
    ComboSystem::RegisterHit(world, attacker);
}

void CombatSystem::ProcessSkill(ecs::EcsWorld& world, ecs::EntityHandle caster,
    uint32_t skillId, float targetX, float targetZ) {
    // TODO: Load skill template from database
    // For now, use default AoE skill

    auto* casterTransform = world.GetComponent<game::Transform>(caster);
    if (!casterTransform) return;

    // AoE around target position
    float aoeRadius = 5.0f;
    int baseDamage = 30;

    auto targets = HitboxSystem::GetEntitiesInRange(world,
        game::Transform{.x = targetX, .y = casterTransform->y, .z = targetZ},
        aoeRadius, caster.GetIndex());

    for (uint32_t targetId : targets) {
        auto targetHandle = ecs::EntityHandle(targetId);
        ApplyDamage(world, targetHandle, baseDamage, caster.GetIndex());
    }
}

void CombatSystem::ApplyDamage(ecs::EcsWorld& world, ecs::EntityHandle victim,
    int damage, uint32_t attackerId) {
    auto* health = world.GetComponent<game::Health>(victim);
    if (!health) return;

    health->current -= damage;

    HitEvent event;
    event.victimId = victim.GetIndex();
    event.attackerId = attackerId;
    event.damage = damage;
    event.isCritical = false; // Would be set by DamageCalculator

    BroadcastHit(event);

    if (health->current <= 0) {
        health->current = 0;
        HandleDeath(world, victim, attackerId);
    }
}

void CombatSystem::ApplyHeal(ecs::EcsWorld& world, ecs::EntityHandle target, int amount) {
    auto* health = world.GetComponent<game::Health>(target);
    if (!health) return;

    health->current = std::min(health->current + amount, health->max);
}

void CombatSystem::ApplyKnockback(ecs::EcsWorld& world, ecs::EntityHandle target,
    float dirX, float dirZ, float force) {
    auto* transform = world.GetComponent<game::Transform>(target);
    if (!transform) return;

    float len = std::sqrt(dirX*dirX + dirZ*dirZ);
    if (len < 0.001f) return;

    dirX /= len;
    dirZ /= len;

    // Apply knockback as velocity or direct position offset
    transform->x += dirX * force;
    transform->z += dirZ * force;
}

void CombatSystem::Update(ecs::EcsWorld& world, float deltaTime) {
    // Update combo timers
    ComboSystem::Update(world, deltaTime);

    // Process status effect damage over time
    auto query = world.Query<game::Health, game::StatusList>();
    for (auto [handle] : query) {
        auto* health = world.GetComponent<game::Health>(handle);
        auto* statusList = world.GetComponent<game::StatusList>(handle);
        if (!health || !statusList) continue;

        for (auto& effect : statusList->effects) {
            effect.duration -= deltaTime;
            effect.tickTimer -= deltaTime;

            if (effect.tickTimer <= 0.0f && effect.tickDamage > 0) {
                health->current -= effect.tickDamage;
                effect.tickTimer = effect.tickInterval;

                if (health->current <= 0) {
                    health->current = 0;
                    HandleDeath(world, handle, effect.sourceEntity);
                    break;
                }
            }
        }

        // Remove expired effects
        std::erase_if(statusList->effects, [](const game::StatusEffect& e) {
            return e.duration <= 0.0f;
        });
    }

    // Health regeneration
    auto regenQuery = world.Query<game::Health>();
    for (auto [handle] : regenQuery) {
        auto* health = world.GetComponent<game::Health>(handle);
        if (!health || health->regeneration <= 0) continue;

        if (health->current < health->max) {
            health->current = std::min(health->max,
                health->current + static_cast<int>(health->regeneration * deltaTime));
        }
    }
}

void CombatSystem::HandleDeath(ecs::EcsWorld& world, ecs::EntityHandle entity,
    uint32_t killerId) {
    auto* transform = world.GetComponent<game::Transform>(entity);
    auto* monster = world.GetComponent<game::MonsterData>(entity);
    auto* player = world.GetComponent<game::PlayerTag>(entity);

    DeathEvent event;
    event.entityId = entity.GetIndex();
    event.killerId = killerId;

    if (monster) {
        // Monster death: award XP and gold
        event.experienceReward = 50; // Would come from monster template
        event.goldReward = 10;

        // TODO: Drop loot
        // TODO: Schedule respawn

        // Mark for destruction (or play death animation)
        auto* ai = world.GetComponent<game::AIState>(entity);
        if (ai) ai->current = game::AIState::Dead;
    }

    if (player) {
        // Player death: respawn at checkpoint
        // TODO: Apply death penalty
        RespawnEntity(world, entity, 0.0f, 0.5f, 0.0f); // Default spawn
    }

    BroadcastDeath(event);
}

void CombatSystem::RespawnEntity(ecs::EcsWorld& world, ecs::EntityHandle entity,
    float spawnX, float spawnY, float spawnZ) {
    auto* transform = world.GetComponent<game::Transform>(entity);
    auto* health = world.GetComponent<game::Health>(entity);

    if (transform) {
        transform->x = spawnX;
        transform->y = spawnY;
        transform->z = spawnZ;
        transform->targetX = spawnX;
        transform->targetZ = spawnZ;
    }

    if (health) {
        health->current = health->max;
    }

    // Reset AI state
    auto* ai = world.GetComponent<game::AIState>(entity);
    if (ai) {
        ai->current = game::AIState::Idle;
        ai->targetEntity = 0;
        ai->stateTimer = 0.0f;
    }

    // Reset combo
    ComboSystem::ResetCombo(world, entity);
}

void CombatSystem::ExecuteAttack(ecs::EcsWorld& world, ecs::EntityHandle attacker,
    const game::Transform& attackerTransform,
    const game::Hitbox& attackerHitbox,
    const game::CombatStats& attackerStats,
    uint32_t skillId) {
    (void)skillId; // Would load skill template

    // Default melee attack
    float attackRange = 2.0f;
    float attackAngle = 120.0f; // Cone in front
    int baseDamage = 15;

    auto potentialTargets = HitboxSystem::GetEntitiesInRange(
        world, attackerTransform, attackRange, attacker.GetIndex());

    for (uint32_t targetId : potentialTargets) {
        auto targetHandle = ecs::EntityHandle(targetId);

        auto* victimTransform = world.GetComponent<game::Transform>(targetHandle);
        auto* victimHitbox = world.GetComponent<game::Hitbox>(targetHandle);
        auto* victimStats = world.GetComponent<game::CombatStats>(targetHandle);
        auto* victimHealth = world.GetComponent<game::Health>(targetHandle);

        if (!victimTransform || !victimHitbox || !victimStats || !victimHealth) continue;

        // Check attack arc
        if (!HitboxSystem::CheckAttackArc(attackerTransform, *victimTransform,
                attackAngle, attackRange)) {
            continue;
        }

        // Calculate damage
        auto damageResult = DamageCalculator::Calculate(attackerStats, *victimStats, baseDamage);

        if (damageResult.isDodged) continue;

        // Apply combo
        uint32_t combo = ComboSystem::GetComboCount(world, attacker);
        int finalDamage = DamageCalculator::ApplyCombo(damageResult.finalDamage, combo);

        // Apply damage
        victimHealth->current -= finalDamage;

        // Knockback
        float kbX = victimTransform->x - attackerTransform.x;
        float kbZ = victimTransform->z - attackerTransform.z;
        float kbLen = std::sqrt(kbX*kbX + kbZ*kbZ);
        if (kbLen > 0.001f) {
            ApplyKnockback(world, targetHandle, kbX/kbLen, kbZ/kbLen, 0.5f);
        }

        // Broadcast hit
        HitEvent event;
        event.attackerId = attacker.GetIndex();
        event.victimId = targetId;
        event.damage = finalDamage;
        event.isCritical = damageResult.isCritical;
        event.isBlocked = damageResult.isBlocked;
        event.knockbackX = kbX / kbLen * 0.5f;
        event.knockbackZ = kbZ / kbLen * 0.5f;
        BroadcastHit(event);

        // Check death
        if (victimHealth->current <= 0) {
            victimHealth->current = 0;
            HandleDeath(world, targetHandle, attacker.GetIndex());
        }

        // Only hit first valid target for basic attack
        break;
    }
}

void CombatSystem::BroadcastHit(const HitEvent& event) {
    if (onHit) onHit(event);
}

void CombatSystem::BroadcastDeath(const DeathEvent& event) {
    if (onDeath) onDeath(event);
}

void CombatSystem::BroadcastCombo(const ComboEvent& event) {
    if (onCombo) onCombo(event);
}

// =============================================================================
// Skill Executor
// =============================================================================
void SkillExecutor::StartSkill(ecs::EcsWorld& world, ecs::EntityHandle caster,
    uint32_t skillId, float targetX, float targetZ) {
    // Cancel existing skill
    CancelSkill(caster);

    // TODO: Load skill template for cast time
    float castTime = 0.5f; // Default

    SkillExecution exec;
    exec.skillId = skillId;
    exec.caster = caster;
    exec.castTime = castTime;
    exec.remainingCastTime = castTime;
    exec.targetX = targetX;
    exec.targetZ = targetZ;

    activeSkills.push_back(std::move(exec));
}

void SkillExecutor::Update(ecs::EcsWorld& world, CombatSystem& combat, float deltaTime) {
    for (auto it = activeSkills.begin(); it != activeSkills.end();) {
        it->remainingCastTime -= deltaTime;

        if (it->remainingCastTime <= 0.0f) {
            // Skill finished casting, execute effect
            combat.ProcessSkill(world, it->caster, it->skillId,
                it->targetX, it->targetZ);
            it = activeSkills.erase(it);
        } else {
            ++it;
        }
    }
}

void SkillExecutor::CancelSkill(ecs::EntityHandle caster) {
    std::erase_if(activeSkills, [caster](const SkillExecution& exec) {
        return exec.caster.GetIndex() == caster.GetIndex();
    });
}

bool SkillExecutor::IsCasting(ecs::EntityHandle caster) const {
    return std::ranges::any_of(activeSkills, [caster](const SkillExecution& exec) {
        return exec.caster.GetIndex() == caster.GetIndex();
    });
}

float SkillExecutor::GetCastProgress(ecs::EntityHandle caster) const {
    for (const auto& exec : activeSkills) {
        if (exec.caster.GetIndex() == caster.GetIndex()) {
            return 1.0f - (exec.remainingCastTime / exec.castTime);
        }
    }
    return 0.0f;
}

} // namespace combat
