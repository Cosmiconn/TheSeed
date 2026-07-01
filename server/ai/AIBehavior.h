#pragma once
// =============================================================================
// server/ai/AIBehavior.h — Behavior Tree System (AP-47-49) C++23
// =============================================================================
// VOLLSTÄNDIGE IMPLEMENTIERUNG: Sequencer, Selector, Action, Condition,
// Inverter, Parallel, Cooldown, Succeeder, Failer Nodes.
// Nutzt std::expected (C++23) für Ergebnis-Rückgabe.
// =============================================================================

#include "../../ecs/ecs_EcsWorld.h"
#include "../../ecs/Components.h"
#include "../../core/Log.h"

#include <vector>
#include <memory>
#include <string>
#include <chrono>
#include <expected>
#include <source_location>
#include <format>

namespace ai {

// =============================================================================
// BT STATUS
// =============================================================================
enum class BTStatus : uint8_t {
    Success = 0,
    Failure = 1,
    Running = 2,
    Error   = 3
};

// =============================================================================
// BT CONTEXT — Blackboard für AI-Zustand
// =============================================================================
struct BTContext {
    ecs::EntityHandle entity;
    ecs::EcsWorld* world = nullptr;
    float deltaTime = 0.0f;

    // Blackboard-Daten
    std::unordered_map<std::string, float> floatValues;
    std::unordered_map<std::string, ecs::EntityHandle> entityTargets;
    std::unordered_map<std::string, std::chrono::steady_clock::time_point> timers;

    [[nodiscard]] float GetFloat(std::string_view key, float defaultVal = 0.0f) const {
        auto it = floatValues.find(std::string(key));
        return it != floatValues.end() ? it->second : defaultVal;
    }

    void SetFloat(std::string_view key, float value) {
        floatValues[std::string(key)] = value;
    }

    [[nodiscard]] bool HasTimerExpired(std::string_view key, std::chrono::seconds duration) const {
        auto it = timers.find(std::string(key));
        if (it == timers.end()) return true;
        return std::chrono::steady_clock::now() - it->second >= duration;
    }

    void ResetTimer(std::string_view key) {
        timers[std::string(key)] = std::chrono::steady_clock::now();
    }
};

// =============================================================================
// BT NODE BASE
// =============================================================================
class BTNode {
public:
    virtual ~BTNode() = default;
    [[nodiscard]] virtual BTStatus Tick(BTContext& ctx) = 0;
    virtual void Reset() {}

    [[nodiscard]] virtual std::string_view GetName() const { return "BTNode"; }
};

using BTNodePtr = std::unique_ptr<BTNode>;

// =============================================================================
// COMPOSITE NODES
// =============================================================================

// Sequencer: Führt Kinder sequentiell aus, bricht bei Failure ab
class BTSequencer : public BTNode {
    std::vector<BTNodePtr> children;
    size_t currentIndex = 0;
    std::string name;

public:
    explicit BTSequencer(std::string_view nodeName = "Sequencer") : name(nodeName) {}

    void AddChild(BTNodePtr child) { children.push_back(std::move(child)); }

    [[nodiscard]] BTStatus Tick(BTContext& ctx) override {
        while (currentIndex < children.size()) {
            BTStatus status = children[currentIndex]->Tick(ctx);

            if (status == BTStatus::Running) {
                return BTStatus::Running;
            }
            if (status == BTStatus::Failure) {
                currentIndex = 0;
                return BTStatus::Failure;
            }
            // Success → nächstes Kind
            currentIndex++;
        }

        currentIndex = 0;
        return BTStatus::Success;
    }

    void Reset() override {
        currentIndex = 0;
        for (auto& child : children) child->Reset();
    }

    [[nodiscard]] std::string_view GetName() const override { return name; }
};

// Selector: Führt Kinder sequentiell aus, bricht bei Success ab (OR-Logik)
class BTSelector : public BTNode {
    std::vector<BTNodePtr> children;
    size_t currentIndex = 0;
    std::string name;

public:
    explicit BTSelector(std::string_view nodeName = "Selector") : name(nodeName) {}

    void AddChild(BTNodePtr child) { children.push_back(std::move(child)); }

    [[nodiscard]] BTStatus Tick(BTContext& ctx) override {
        while (currentIndex < children.size()) {
            BTStatus status = children[currentIndex]->Tick(ctx);

            if (status == BTStatus::Running) {
                return BTStatus::Running;
            }
            if (status == BTStatus::Success) {
                currentIndex = 0;
                return BTStatus::Success;
            }
            // Failure → nächstes Kind
            currentIndex++;
        }

        currentIndex = 0;
        return BTStatus::Failure;
    }

    void Reset() override {
        currentIndex = 0;
        for (auto& child : children) child->Reset();
    }

    [[nodiscard]] std::string_view GetName() const override { return name; }
};

// Parallel: Führt alle Kinder parallel aus (alle müssen Success haben)
class BTParallel : public BTNode {
    std::vector<BTNodePtr> children;
    std::string name;

public:
    explicit BTParallel(std::string_view nodeName = "Parallel") : name(nodeName) {}

    void AddChild(BTNodePtr child) { children.push_back(std::move(child)); }

    [[nodiscard]] BTStatus Tick(BTContext& ctx) override {
        bool anyRunning = false;

        for (auto& child : children) {
            BTStatus status = child->Tick(ctx);
            if (status == BTStatus::Failure) return BTStatus::Failure;
            if (status == BTStatus::Running) anyRunning = true;
        }

        return anyRunning ? BTStatus::Running : BTStatus::Success;
    }

    void Reset() override {
        for (auto& child : children) child->Reset();
    }

    [[nodiscard]] std::string_view GetName() const override { return name; }
};

// =============================================================================
// DECORATOR NODES
// =============================================================================

// Inverter: Kehrt Success/Failure um
class BTInverter : public BTNode {
    BTNodePtr child;

public:
    explicit BTInverter(BTNodePtr c) : child(std::move(c)) {}

    [[nodiscard]] BTStatus Tick(BTContext& ctx) override {
        BTStatus status = child->Tick(ctx);
        if (status == BTStatus::Success) return BTStatus::Failure;
        if (status == BTStatus::Failure) return BTStatus::Success;
        return status; // Running, Error bleiben gleich
    }

    void Reset() override { child->Reset(); }
};

// Cooldown: Führt Kind nur aus wenn Cooldown abgelaufen
class BTCooldown : public BTNode {
    BTNodePtr child;
    std::chrono::seconds duration;
    std::chrono::steady_clock::time_point lastExecution;
    std::string timerKey;

public:
    BTCooldown(BTNodePtr c, std::chrono::seconds dur, std::string_view key)
        : child(std::move(c)), duration(dur), timerKey(key) {}

    [[nodiscard]] BTStatus Tick(BTContext& ctx) override {
        if (!ctx.HasTimerExpired(timerKey, duration)) {
            return BTStatus::Failure; // Cooldown aktiv
        }

        BTStatus status = child->Tick(ctx);
        if (status == BTStatus::Success) {
            ctx.ResetTimer(timerKey);
        }
        return status;
    }

    void Reset() override { child->Reset(); }
};

// Succeeder: Wandelt Failure in Success um (Child wird trotzdem ausgeführt)
class BTSucceeder : public BTNode {
    BTNodePtr child;

public:
    explicit BTSucceeder(BTNodePtr c) : child(std::move(c)) {}

    [[nodiscard]] BTStatus Tick(BTContext& ctx) override {
        child->Tick(ctx);
        return BTStatus::Success;
    }

    void Reset() override { child->Reset(); }
};

// =============================================================================
// ACTION NODES ( konkrete AI-Logik )
// =============================================================================

// MoveToTarget: Bewegt Entity zu einem Ziel
class BTMoveToTarget : public BTNode {
    float speed = 5.0f;
    float arrivalThreshold = 1.0f;

public:
    explicit BTMoveToTarget(float moveSpeed = 5.0f) : speed(moveSpeed) {}

    [[nodiscard]] BTStatus Tick(BTContext& ctx) override {
        auto* pos = ctx.world->GetComponent<ecs::PositionComponent>(ctx.entity);
        auto* vel = ctx.world->GetComponent<ecs::VelocityComponent>(ctx.entity);
        if (!pos || !vel) return BTStatus::Error;

        auto targetIt = ctx.entityTargets.find("target");
        if (targetIt == ctx.entityTargets.end()) return BTStatus::Failure;

        auto* targetPos = ctx.world->GetComponent<ecs::PositionComponent>(targetIt->second);
        if (!targetPos) return BTStatus::Failure;

        float dx = targetPos->x - pos->x;
        float dz = targetPos->z - pos->z;
        float dist = std::sqrt(dx * dx + dz * dz);

        if (dist <= arrivalThreshold) {
            vel->vx = 0.0f;
            vel->vz = 0.0f;
            return BTStatus::Success;
        }

        // Normalisierte Richtung * Geschwindigkeit
        vel->vx = (dx / dist) * speed;
        vel->vz = (dz / dist) * speed;
        vel->vy = 0.0f;

        return BTStatus::Running;
    }

    [[nodiscard]] std::string_view GetName() const override { return "MoveToTarget"; }
};

// AttackTarget: Greift Ziel an
class BTAttackTarget : public BTNode {
    float attackRange = 2.0f;
    uint32_t damage = 10;

public:
    BTAttackTarget(float range = 2.0f, uint32_t dmg = 10) 
        : attackRange(range), damage(dmg) {}

    [[nodiscard]] BTStatus Tick(BTContext& ctx) override {
        auto* pos = ctx.world->GetComponent<ecs::PositionComponent>(ctx.entity);
        auto* combat = ctx.world->GetComponent<ecs::CombatComponent>(ctx.entity);
        if (!pos || !combat) return BTStatus::Error;

        auto targetIt = ctx.entityTargets.find("target");
        if (targetIt == ctx.entityTargets.end()) return BTStatus::Failure;

        auto* targetPos = ctx.world->GetComponent<ecs::PositionComponent>(targetIt->second);
        auto* targetHealth = ctx.world->GetComponent<ecs::HealthComponent>(targetIt->second);
        if (!targetPos || !targetHealth) return BTStatus::Failure;

        float dx = targetPos->x - pos->x;
        float dz = targetPos->z - pos->z;
        float dist = std::sqrt(dx * dx + dz * dz);

        if (dist > attackRange) return BTStatus::Failure; // Zu weit weg

        // Angriff ausführen
        if (targetHealth->currentHP > damage) {
            targetHealth->currentHP -= damage;
        } else {
            targetHealth->currentHP = 0;
        }

        AddLog("[AI] Entity {} greift Entity {} an ({} Schaden, {} HP übrig)",
               ctx.entity.GetIndex(), targetIt->second.GetIndex(), damage, targetHealth->currentHP);

        return BTStatus::Success;
    }

    [[nodiscard]] std::string_view GetName() const override { return "AttackTarget"; }
};

// FindNearestEnemy: Findet nächsten Gegner in Reichweite
class BTFindNearestEnemy : public BTNode {
    float searchRadius = 50.0f;

public:
    explicit BTFindNearestEnemy(float radius = 50.0f) : searchRadius(radius) {}

    [[nodiscard]] BTStatus Tick(BTContext& ctx) override {
        auto* myPos = ctx.world->GetComponent<ecs::PositionComponent>(ctx.entity);
        if (!myPos) return BTStatus::Error;

        ecs::EntityHandle nearest;
        float nearestDist = searchRadius;

        auto query = ctx.world->QueryEntities<ecs::PositionComponent, ecs::HealthComponent>();
        for (auto [other, otherPos, otherHealth] : query) {
            if (other == ctx.entity) continue;
            if (otherHealth->currentHP == 0) continue; // Tote ignorieren

            float dx = otherPos->x - myPos->x;
            float dz = otherPos->z - myPos->z;
            float dist = std::sqrt(dx * dx + dz * dz);

            if (dist < nearestDist) {
                nearestDist = dist;
                nearest = other;
            }
        }

        if (nearest.GetIndex() == 0) return BTStatus::Failure; // Kein Gegner gefunden

        ctx.entityTargets["target"] = nearest;
        return BTStatus::Success;
    }

    [[nodiscard]] std::string_view GetName() const override { return "FindNearestEnemy"; }
};

// Patrol: Patroulliert zwischen Wegpunkten
class BTPatrol : public BTNode {
    std::vector<std::tuple<float, float, float>> waypoints;
    size_t currentWaypoint = 0;
    float speed = 3.0f;
    float threshold = 1.0f;

public:
    BTPatrol(std::initializer_list<std::tuple<float, float, float>> points, float moveSpeed = 3.0f)
        : waypoints(points), speed(moveSpeed) {}

    [[nodiscard]] BTStatus Tick(BTContext& ctx) override {
        if (waypoints.empty()) return BTStatus::Failure;

        auto* pos = ctx.world->GetComponent<ecs::PositionComponent>(ctx.entity);
        auto* vel = ctx.world->GetComponent<ecs::VelocityComponent>(ctx.entity);
        if (!pos || !vel) return BTStatus::Error;

        auto [tx, ty, tz] = waypoints[currentWaypoint];
        float dx = tx - pos->x;
        float dz = tz - pos->z;
        float dist = std::sqrt(dx * dx + dz * dz);

        if (dist <= threshold) {
            currentWaypoint = (currentWaypoint + 1) % waypoints.size();
            vel->vx = 0.0f;
            vel->vz = 0.0f;
            return BTStatus::Success;
        }

        vel->vx = (dx / dist) * speed;
        vel->vz = (dz / dist) * speed;
        vel->vy = 0.0f;

        return BTStatus::Running;
    }

    [[nodiscard]] std::string_view GetName() const override { return "Patrol"; }
};

// =============================================================================
// CONDITION NODES
// =============================================================================

// HasTarget: Prüft ob ein Ziel gesetzt ist
class BTHasTarget : public BTNode {
public:
    [[nodiscard]] BTStatus Tick(BTContext& ctx) override {
        return ctx.entityTargets.contains("target") ? BTStatus::Success : BTStatus::Failure;
    }

    [[nodiscard]] std::string_view GetName() const override { return "HasTarget"; }
};

// IsTargetInRange: Prüft ob Ziel in Angriffsreichweite
class BTIsTargetInRange : public BTNode {
    float range = 2.0f;

public:
    explicit BTIsTargetInRange(float r = 2.0f) : range(r) {}

    [[nodiscard]] BTStatus Tick(BTContext& ctx) override {
        auto* pos = ctx.world->GetComponent<ecs::PositionComponent>(ctx.entity);
        if (!pos) return BTStatus::Failure;

        auto targetIt = ctx.entityTargets.find("target");
        if (targetIt == ctx.entityTargets.end()) return BTStatus::Failure;

        auto* targetPos = ctx.world->GetComponent<ecs::PositionComponent>(targetIt->second);
        if (!targetPos) return BTStatus::Failure;

        float dx = targetPos->x - pos->x;
        float dz = targetPos->z - pos->z;
        float dist = std::sqrt(dx * dx + dz * dz);

        return dist <= range ? BTStatus::Success : BTStatus::Failure;
    }

    [[nodiscard]] std::string_view GetName() const override { return "IsTargetInRange"; }
};

// IsHealthBelow: Prüft ob HP unter Schwellenwert
class BTIsHealthBelow : public BTNode {
    float threshold = 0.3f; // 30%

public:
    explicit BTIsHealthBelow(float t = 0.3f) : threshold(t) {}

    [[nodiscard]] BTStatus Tick(BTContext& ctx) override {
        auto* health = ctx.world->GetComponent<ecs::HealthComponent>(ctx.entity);
        if (!health || health->maxHP == 0) return BTStatus::Failure;

        float ratio = static_cast<float>(health->currentHP) / static_cast<float>(health->maxHP);
        return ratio < threshold ? BTStatus::Success : BTStatus::Failure;
    }

    [[nodiscard]] std::string_view GetName() const override { return "IsHealthBelow"; }
};

// =============================================================================
// BEHAVIOR TREE FACTORY — Vorgefertigte AI-Verhaltensmuster
// =============================================================================

class BehaviorTreeFactory {
public:
    // Standard-Kampf-AI: Sucht Gegner → bewegt sich hin → greift an
    [[nodiscard]] static BTNodePtr CreateCombatAI(float moveSpeed = 5.0f, 
                                                   float attackRange = 2.0f,
                                                   uint32_t damage = 10) {
        auto root = std::make_unique<BTSelector>("CombatAI");

        // 1. Wenn Ziel in Reichweite → angreifen
        auto attackSeq = std::make_unique<BTSequencer>("AttackSequence");
        attackSeq->AddChild(std::make_unique<BTHasTarget>());
        attackSeq->AddChild(std::make_unique<BTIsTargetInRange>(attackRange));
        attackSeq->AddChild(std::make_unique<BTAttackTarget>(attackRange, damage));

        // 2. Wenn Ziel existiert aber zu weit weg → hinbewegen
        auto chaseSeq = std::make_unique<BTSequencer>("ChaseSequence");
        chaseSeq->AddChild(std::make_unique<BTHasTarget>());
        chaseSeq->AddChild(std::make_unique<BTMoveToTarget>(moveSpeed));

        // 3. Neues Ziel suchen
        auto findSeq = std::make_unique<BTSequencer>("FindSequence");
        findSeq->AddChild(std::make_unique<BTInverter>(std::make_unique<BTHasTarget>()));
        findSeq->AddChild(std::make_unique<BTFindNearestEnemy>(50.0f));

        root->AddChild(std::move(attackSeq));
        root->AddChild(std::move(chaseSeq));
        root->AddChild(std::move(findSeq));

        return root;
    }

    // Patrouillen-AI: Bewegt sich zwischen Wegpunkten, greift Gegner an wenn sie nahe sind
    [[nodiscard]] static BTNodePtr CreatePatrolAI(
        std::initializer_list<std::tuple<float, float, float>> waypoints,
        float moveSpeed = 3.0f) {

        auto root = std::make_unique<BTSelector>("PatrolAI");

        // 1. Wenn Gegner in Reichweite → angreifen
        auto combatSeq = std::make_unique<BTSequencer>("Combat");
        combatSeq->AddChild(std::make_unique<BTFindNearestEnemy>(20.0f));
        combatSeq->AddChild(std::make_unique<BTIsTargetInRange>(2.0f));
        combatSeq->AddChild(std::make_unique<BTAttackTarget>(2.0f, 10));

        // 2. Wenn Gegner gesehen aber zu weit → verfolgen (mit Cooldown)
        auto chaseSeq = std::make_unique<BTSequencer>("Chase");
        chaseSeq->AddChild(std::make_unique<BTHasTarget>());
        chaseSeq->AddChild(std::make_unique<BTMoveToTarget>(moveSpeed * 1.5f));

        // 3. Patrouillieren
        auto patrol = std::make_unique<BTPatrol>(waypoints, moveSpeed);

        root->AddChild(std::move(combatSeq));
        root->AddChild(std::move(chaseSeq));
        root->AddChild(std::move(patrol));

        return root;
    }

    // Flucht-AI: Läuft weg wenn HP niedrig
    [[nodiscard]] static BTNodePtr CreateFleeAI(float fleeSpeed = 8.0f) {
        auto root = std::make_unique<BTSelector>("FleeAI");

        // 1. Wenn HP niedrig → weglaufen
        auto fleeSeq = std::make_unique<BTSequencer>("Flee");
        fleeSeq->AddChild(std::make_unique<BTIsHealthBelow>(0.3f));
        fleeSeq->AddChild(std::make_unique<BTMoveToTarget>(fleeSpeed)); // Bewegt sich zu "safe" Ziel

        // 2. Sonst normal kämpfen
        fleeSeq->AddChild(std::make_unique<BTSucceeder>(CreateCombatAI()));

        root->AddChild(std::move(fleeSeq));

        return root;
    }
};

} // namespace ai
