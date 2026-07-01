// =============================================================================
// server/combat/CombatSystem.cpp — Erweitertes Combat-System (P2-FIX)
// =============================================================================
// KORREKTUR P2: ApplyStatusEffect() mit korrekten 6 Parametern aufgerufen.
// =============================================================================

#include "CombatSystem.h"
#include "../../core/GameSystems.h"

#include <cmath>
#include <random>

namespace combat {

// =============================================================================
// AGGRO TABLE
// =============================================================================

void AggroTable::AddThreat(ecs::EntityHandle source, float amount) {
    auto it = std::ranges::find_if(entries, [source](const auto& e) {
        return e.source.GetIndex() == source.GetIndex();
    });

    if (it != entries.end()) {
        it->threat += amount;
        it->lastUpdate = std::chrono::steady_clock::now();
    } else {
        entries.push_back(ThreatEntry{source, amount, std::chrono::steady_clock::now()});
    }

    std::ranges::sort(entries, std::greater{});
}

void AggroTable::DecayThreat(float deltaTime) {
    auto now = std::chrono::steady_clock::now();

    for (auto& entry : entries) {
        auto elapsed = std::chrono::duration<float>(now - entry.lastUpdate).count();
        entry.threat = std::max(0.0f, entry.threat - decayRate * deltaTime);
    }

    std::erase_if(entries, [](const auto& e) { return e.threat <= 0.0f; });
    std::ranges::sort(entries, std::greater{});
}

ecs::EntityHandle AggroTable::GetTopThreat() const {
    if (entries.empty()) return ecs::EntityHandle{};
    return entries.front().source;
}

float AggroTable::GetThreat(ecs::EntityHandle source) const {
    auto it = std::ranges::find_if(entries, [source](const auto& e) {
        return e.source.GetIndex() == source.GetIndex();
    });
    return it != entries.end() ? it->threat : 0.0f;
}

void AggroTable::RemoveSource(ecs::EntityHandle source) {
    std::erase_if(entries, [source](const auto& e) {
        return e.source.GetIndex() == source.GetIndex();
    });
}

// =============================================================================
// COMBAT SYSTEM
// =============================================================================

std::expected<void, std::string> CombatSystem::StartAttack(
    ecs::EntityHandle attacker, const AttackData& data) {

    if (!world) return std::unexpected("ECS World not set");

    auto* pos = world->GetComponent<ecs::PositionComponent>(attacker);
    if (!pos) return std::unexpected("Attacker has no position");

    auto* combo = GetComboState(attacker);
    AttackData modifiedData = data;

    if (combo && combo->isInCombo) {
        auto now = std::chrono::steady_clock::now();
        auto elapsed = std::chrono::duration<float>(now - combo->lastAttackTime).count();

        if (elapsed <= data.comboWindow && combo->currentStep < data.maxComboSteps) {
            combo->currentStep++;
            combo->damageMultiplier *= data.comboDamageMultiplier;
            modifiedData.comboStep = combo->currentStep;
            modifiedData.baseDamage *= combo->damageMultiplier;
        } else {
            combo->Reset();
        }
    }

    ActiveAttack attack;
    attack.data = modifiedData;
    attack.attacker = attacker;
    attack.startTime = std::chrono::steady_clock::now();
    attack.phase = ActiveAttack::Phase::Windup;

    activeAttacks.push_back(std::move(attack));

    if (combo) {
        combo->lastAttackTime = std::chrono::steady_clock::now();
        combo->isInCombo = true;
    }

    AddLog("[Combat] Entity {} starts attack (Combo: {}/{}, Dmg: {:.1f})",
           attacker.GetIndex(), modifiedData.comboStep,
           data.maxComboSteps, modifiedData.baseDamage);

    return {};
}

bool CombatSystem::TryBlock(ecs::EntityHandle defender,
    const math::Vector3& attackDirection) {
    if (!world) return false;

    auto* pos = world->GetComponent<ecs::PositionComponent>(defender);
    auto* combat = world->GetComponent<ecs::CombatComponent>(defender);
    if (!pos || !combat) return false;

    math::Vector3 defenderForward(0.0f, 0.0f, 1.0f);
    auto* vel = world->GetComponent<ecs::VelocityComponent>(defender);
    if (vel) {
        float speed = std::sqrt(vel->vx * vel->vx + vel->vz * vel->vz);
        if (speed > 0.1f) {
            defenderForward = math::Vector3(vel->vx / speed, 0.0f, vel->vz / speed);
        }
    }

    float dot = defenderForward.x * (-attackDirection.x) +
                defenderForward.z * (-attackDirection.z);
    float angle = std::acos(std::clamp(dot, -1.0f, 1.0f)) * 180.0f / 3.14159265f;

    bool isBlocking = angle <= 60.0f;

    if (isBlocking) {
        AddLog("[Combat] Entity {} blocks attack (angle: {:.1f}°)",
               defender.GetIndex(), angle);
    }

    return isBlocking;
}

bool CombatSystem::TryParry(ecs::EntityHandle defender,
    ecs::EntityHandle attacker,
    float timingWindow) {
    (void)attacker;

    if (!world) return false;

    auto* combat = world->GetComponent<ecs::CombatComponent>(defender);
    if (!combat) return false;

    static thread_local std::mt19937 rng(
        static_cast<unsigned>(std::chrono::steady_clock::now().time_since_epoch().count())
    );

    std::uniform_real_distribution<float> dist(0.0f, 1.0f);
    float parryChance = 0.3f;
    parryChance += timingWindow;

    bool success = dist(rng) < parryChance;

    if (success) {
        AddLog("[Combat] Entity {} parries!", defender.GetIndex());
    }

    return success;
}

void CombatSystem::ApplyDamage(ecs::EntityHandle target,
    ecs::EntityHandle source,
    float damage,
    DamageType type) {
    if (!world) return;

    auto* health = world->GetComponent<ecs::HealthComponent>(target);
    if (!health) return;

    float reduction = CalculateDamageReduction(target, type);
    float finalDamage = damage * (1.0f - reduction);

    if (health->currentHP > static_cast<int>(finalDamage)) {
        health->currentHP -= static_cast<int>(finalDamage);
    } else {
        health->currentHP = 0;
    }

    AddThreat(target, source, finalDamage * 2.0f);

    AddLog("[Combat] Entity {} takes {:.1f} {} damage from Entity {} (HP: {}/{})",
           target.GetIndex(), finalDamage,
           static_cast<int>(type), source.GetIndex(),
           health->currentHP, health->maxHP);

    if (health->currentHP == 0) {
        AddLog("[Combat] Entity {} died!", target.GetIndex());
    }
}

void CombatSystem::AddThreat(ecs::EntityHandle npc,
    ecs::EntityHandle player,
    float threat) {
    uint32_t npcIdx = npc.GetIndex();

    if (!aggroTables.contains(npcIdx)) {
        aggroTables[npcIdx] = std::make_unique<AggroTable>(npc);
    }

    aggroTables[npcIdx]->AddThreat(player, threat);
}

ecs::EntityHandle CombatSystem::GetTopThreatTarget(ecs::EntityHandle npc) {
    uint32_t npcIdx = npc.GetIndex();

    auto it = aggroTables.find(npcIdx);
    if (it == aggroTables.end()) return ecs::EntityHandle{};

    return it->second->GetTopThreat();
}

// =============================================================================
// UPDATE
// =============================================================================

void CombatSystem::Update(float deltaTime) {
    ProcessActiveAttacks(deltaTime);

    for (auto& [idx, table] : aggroTables) {
        table->DecayThreat(deltaTime);
    }

    auto now = std::chrono::steady_clock::now();
    std::erase_if(comboStates, [&now](const auto& pair) {
        const auto& combo = pair.second;
        if (!combo.isInCombo) return false;
        auto elapsed = std::chrono::duration<float>(now - combo.lastAttackTime).count();
        return elapsed > 2.0f;
    });
}

void CombatSystem::ProcessActiveAttacks(float deltaTime) {
    (void)deltaTime;

    auto now = std::chrono::steady_clock::now();

    std::erase_if(activeAttacks, [this, &now](ActiveAttack& attack) {
        auto elapsed = std::chrono::duration<float>(now - attack.startTime).count();

        switch (attack.phase) {
            case ActiveAttack::Phase::Windup:
                if (elapsed >= attack.data.windupTime) {
                    attack.phase = ActiveAttack::Phase::Active;
                    attack.startTime = now;
                }
                return false;

            case ActiveAttack::Phase::Active:
                CheckHitboxes(attack);
                if (elapsed >= attack.data.activeTime) {
                    attack.phase = ActiveAttack::Phase::Recovery;
                    attack.startTime = now;
                }
                return false;

            case ActiveAttack::Phase::Recovery:
                return elapsed >= attack.data.recoveryTime;
        }

        return true;
    });
}

void CombatSystem::CheckHitboxes(ActiveAttack& attack) {
    if (!world) return;

    auto* attackerPos = world->GetComponent<ecs::PositionComponent>(attack.attacker);
    if (!attackerPos) return;

    math::Vector3 hitboxCenter(
        attackerPos->x + attack.data.direction.x * 1.5f,
        attackerPos->y + 1.0f,
        attackerPos->z + attack.data.direction.z * 1.5f
    );

    auto query = world->QueryEntities<ecs::PositionComponent, ecs::HealthComponent>();
    for (auto [target, pos, health] : query) {
        if (target == attack.attacker) continue;
        if (health->currentHP == 0) continue;

        math::Vector3 targetPos(pos->x, pos->y, pos->z);

        bool hit = false;
        if (attack.data.useSphere) {
            HitboxSphere sphere{hitboxCenter, attack.data.hitboxSphere.radius};
            hit = sphere.Contains(targetPos);
        } else {
            HitboxAABB aabb = attack.data.hitboxAABB;
            aabb.min = aabb.min + hitboxCenter;
            aabb.max = aabb.max + hitboxCenter;
            hit = aabb.Contains(targetPos);
        }

        if (hit) {
            math::Vector3 toTarget = targetPos - math::Vector3(attackerPos->x, attackerPos->y, attackerPos->z);
            toTarget = toTarget.Normalized();

            float angle = std::acos(std::clamp(
                attack.data.direction.x * toTarget.x +
                attack.data.direction.z * toTarget.z, -1.0f, 1.0f
            )) * 180.0f / 3.14159265f;

            if (angle <= attack.data.coneAngle / 2.0f) {
                ApplyDamage(target, attack.attacker, attack.data.baseDamage,
                    attack.data.damageType);

                if (attack.data.knockbackForce > 0.0f) {
                    auto* vel = world->GetComponent<ecs::VelocityComponent>(target);
                    if (vel) {
                        math::Vector3 knockDir = toTarget;
                        vel->vx += knockDir.x * attack.data.knockbackForce;
                        vel->vz += knockDir.z * attack.data.knockbackForce;
                    }
                }

                // FIX P2: ApplyStatusEffect mit korrekten 6 Parametern
                for (const auto& se : attack.data.statusEffects) {
                    static thread_local std::mt19937 rng(
                        static_cast<unsigned>(std::chrono::steady_clock::now().time_since_epoch().count())
                    );
                    std::uniform_real_distribution<float> dist(0.0f, 1.0f);

                    if (dist(rng) < se.chance) {
                        ApplyStatusEffect(
                            target.GetIndex(),
                            se.type,
                            se.duration,
                            attack.attacker.GetIndex(),  // sourceId
                            se.tickDamage,
                            [](std::span<const uint8_t>) {}  // BroadcastCallback
                        );
                    }
                }
            }
        }
    }
}

float CombatSystem::CalculateDamageReduction(ecs::EntityHandle target,
    DamageType type) const {
    (void)type;

    auto* combat = world->GetComponent<ecs::CombatComponent>(target);
    if (!combat) return 0.0f;

    return combat->armor / (combat->armor + 100.0f);
}

bool CombatSystem::IsInFront(const math::Vector3& defenderPos,
    const math::Vector3& defenderForward,
    const math::Vector3& attackerPos,
    float coneAngle) const {
    math::Vector3 toAttacker = attackerPos - defenderPos;
    toAttacker = toAttacker.Normalized();

    float dot = defenderForward.x * toAttacker.x + defenderForward.z * toAttacker.z;
    float angle = std::acos(std::clamp(dot, -1.0f, 1.0f)) * 180.0f / 3.14159265f;

    return angle <= coneAngle / 2.0f;
}

// =============================================================================
// COMBO STATE
// =============================================================================

ComboState* CombatSystem::GetComboState(ecs::EntityHandle entity) {
    uint32_t idx = entity.GetIndex();
    if (!comboStates.contains(idx)) {
        comboStates[idx] = ComboState{};
    }
    return &comboStates[idx];
}

AggroTable* CombatSystem::GetAggroTable(ecs::EntityHandle entity) {
    uint32_t idx = entity.GetIndex();
    auto it = aggroTables.find(idx);
    if (it == aggroTables.end()) return nullptr;
    return it->second.get();
}

} // namespace combat
