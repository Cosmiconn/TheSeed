#pragma once
// =============================================================================
// server/ai/AIBehavior.h — AI Behavior Tree System (AP-47–AP-49)
// Patrol → Chase → Attack → Return → Dead
// =============================================================================
#include "../../ecs/Components.h"
#include "../../ecs/ecs_EcsWorld.h"
#include <cstdint>
#include <vector>
#include <memory>
#include <functional>
#include <random>

namespace ai {

// =============================================================================
// AI Context (blackboard for behavior tree)
// =============================================================================
struct AIContext {
    ecs::EntityHandle self;
    ecs::EcsWorld* world = nullptr;

    // Perception
    float detectionRange = 15.0f;
    float attackRange = 2.0f;
    float chaseTimeout = 10.0f;
    float patrolWaitTime = 3.0f;

    // State
    float timeInState = 0.0f;
    float lastAttackTime = 0.0f;
    uint32_t targetEntity = 0;
    uint32_t currentWaypoint = 0;

    // Combat
    float attackCooldown = 1.5f;
    float aggroResetTime = 5.0f;
    float lastAggroTime = 0.0f;

    // Random
    std::mt19937 rng;

    AIContext() : rng(std::random_device{}()) {}
};

// =============================================================================
// Behavior Node Base
// =============================================================================
enum class NodeStatus { Success, Failure, Running };

class BehaviorNode {
public:
    virtual ~BehaviorNode() = default;
    virtual NodeStatus Tick(AIContext& ctx, float deltaTime) = 0;
    virtual void OnEnter(AIContext& ctx) {}
    virtual void OnExit(AIContext& ctx) {}
    virtual std::string GetName() const = 0;
};

using BehaviorNodePtr = std::unique_ptr<BehaviorNode>;

// =============================================================================
// Composite Nodes
// =============================================================================
class SequenceNode : public BehaviorNode {
    std::vector<BehaviorNodePtr> children;
    size_t currentChild = 0;

public:
    void AddChild(BehaviorNodePtr child) { children.push_back(std::move(child)); }
    NodeStatus Tick(AIContext& ctx, float deltaTime) override;
    std::string GetName() const override { return "Sequence"; }
};

class SelectorNode : public BehaviorNode {
    std::vector<BehaviorNodePtr> children;
    size_t currentChild = 0;

public:
    void AddChild(BehaviorNodePtr child) { children.push_back(std::move(child)); }
    NodeStatus Tick(AIContext& ctx, float deltaTime) override;
    std::string GetName() const override { return "Selector"; }
};

class ParallelNode : public BehaviorNode {
    std::vector<BehaviorNodePtr> children;
    size_t successThreshold = 1;

public:
    ParallelNode(size_t threshold = 1) : successThreshold(threshold) {}
    void AddChild(BehaviorNodePtr child) { children.push_back(std::move(child)); }
    NodeStatus Tick(AIContext& ctx, float deltaTime) override;
    std::string GetName() const override { return "Parallel"; }
};

// =============================================================================
// Decorator Nodes
// =============================================================================
class InverterNode : public BehaviorNode {
    BehaviorNodePtr child;

public:
    explicit InverterNode(BehaviorNodePtr c) : child(std::move(c)) {}
    NodeStatus Tick(AIContext& ctx, float deltaTime) override;
    std::string GetName() const override { return "Inverter"; }
};

class CooldownNode : public BehaviorNode {
    BehaviorNodePtr child;
    float cooldown = 0.0f;
    float timer = 0.0f;

public:
    CooldownNode(BehaviorNodePtr c, float cd) : child(std::move(c)), cooldown(cd) {}
    NodeStatus Tick(AIContext& ctx, float deltaTime) override;
    std::string GetName() const override { return "Cooldown"; }
};

class RepeatNode : public BehaviorNode {
    BehaviorNodePtr child;
    int maxRepeats = -1; // -1 = infinite
    int currentCount = 0;

public:
    RepeatNode(BehaviorNodePtr c, int count = -1) : child(std::move(c)), maxRepeats(count) {}
    NodeStatus Tick(AIContext& ctx, float deltaTime) override;
    std::string GetName() const override { return "Repeat"; }
};

// =============================================================================
// Leaf Nodes — Conditions
// =============================================================================
class HasTargetCondition : public BehaviorNode {
public:
    NodeStatus Tick(AIContext& ctx, float deltaTime) override;
    std::string GetName() const override { return "HasTarget"; }
};

class TargetInRangeCondition : public BehaviorNode {
    float range;

public:
    explicit TargetInRangeCondition(float r) : range(r) {}
    NodeStatus Tick(AIContext& ctx, float deltaTime) override;
    std::string GetName() const override { return "TargetInRange"; }
};

class TargetInAttackRangeCondition : public BehaviorNode {
public:
    NodeStatus Tick(AIContext& ctx, float deltaTime) override;
    std::string GetName() const override { return "TargetInAttackRange"; }
};

class IsHealthBelowCondition : public BehaviorNode {
    float threshold;

public:
    explicit IsHealthBelowCondition(float t) : threshold(t) {}
    NodeStatus Tick(AIContext& ctx, float deltaTime) override;
    std::string GetName() const override { return "IsHealthBelow"; }
};

class ShouldReturnHomeCondition : public BehaviorNode {
    float maxDistance;

public:
    explicit ShouldReturnHomeCondition(float d) : maxDistance(d) {}
    NodeStatus Tick(AIContext& ctx, float deltaTime) override;
    std::string GetName() const override { return "ShouldReturnHome"; }
};

// =============================================================================
// Leaf Nodes — Actions
// =============================================================================
class PatrolAction : public BehaviorNode {
    float moveSpeed = 2.0f;
    float waypointThreshold = 0.5f;

public:
    explicit PatrolAction(float speed = 2.0f) : moveSpeed(speed) {}
    NodeStatus Tick(AIContext& ctx, float deltaTime) override;
    std::string GetName() const override { return "Patrol"; }
};

class ChaseAction : public BehaviorNode {
    float moveSpeed = 5.0f;
    float loseInterestTime = 10.0f;

public:
    explicit ChaseAction(float speed = 5.0f) : moveSpeed(speed) {}
    NodeStatus Tick(AIContext& ctx, float deltaTime) override;
    std::string GetName() const override { return "Chase"; }
};

class AttackAction : public BehaviorNode {
    float damage = 10.0f;
    float attackRange = 2.0f;

public:
    explicit AttackAction(float dmg = 10.0f) : damage(dmg) {}
    NodeStatus Tick(AIContext& ctx, float deltaTime) override;
    std::string GetName() const override { return "Attack"; }
};

class ReturnHomeAction : public BehaviorNode {
    float moveSpeed = 3.0f;

public:
    explicit ReturnHomeAction(float speed = 3.0f) : moveSpeed(speed) {}
    NodeStatus Tick(AIContext& ctx, float deltaTime) override;
    std::string GetName() const override { return "ReturnHome"; }
};

class IdleAction : public BehaviorNode {
    float idleTime = 2.0f;
    float timer = 0.0f;

public:
    explicit IdleAction(float time = 2.0f) : idleTime(time) {}
    NodeStatus Tick(AIContext& ctx, float deltaTime) override;
    std::string GetName() const override { return "Idle"; }
};

class FleeAction : public BehaviorNode {
    float moveSpeed = 6.0f;
    float fleeDistance = 20.0f;

public:
    explicit FleeAction(float speed = 6.0f) : moveSpeed(speed) {}
    NodeStatus Tick(AIContext& ctx, float deltaTime) override;
    std::string GetName() const override { return "Flee"; }
};

// =============================================================================
// Behavior Tree Builder
// =============================================================================
class BehaviorTreeBuilder {
    std::vector<BehaviorNodePtr> stack;
    BehaviorNodePtr root;

public:
    BehaviorTreeBuilder& Sequence();
    BehaviorTreeBuilder& Selector();
    BehaviorTreeBuilder& Parallel(size_t successThreshold = 1);

    BehaviorTreeBuilder& Inverter();
    BehaviorTreeBuilder& Cooldown(float seconds);
    BehaviorTreeBuilder& Repeat(int count = -1);

    BehaviorTreeBuilder& HasTarget();
    BehaviorTreeBuilder& TargetInRange(float range);
    BehaviorTreeBuilder& TargetInAttackRange();
    BehaviorTreeBuilder& IsHealthBelow(float threshold);
    BehaviorTreeBuilder& ShouldReturnHome(float maxDistance);

    BehaviorTreeBuilder& Patrol(float speed = 2.0f);
    BehaviorTreeBuilder& Chase(float speed = 5.0f);
    BehaviorTreeBuilder& Attack(float damage = 10.0f);
    BehaviorTreeBuilder& ReturnHome(float speed = 3.0f);
    BehaviorTreeBuilder& Idle(float time = 2.0f);
    BehaviorTreeBuilder& Flee(float speed = 6.0f);

    BehaviorTreeBuilder& End(); // Pop from stack

    [[nodiscard]] BehaviorNodePtr Build();
};

// =============================================================================
// AI System (ECS System)
// =============================================================================
class AISystem {
    std::unordered_map<uint32_t, BehaviorNodePtr> behaviorTrees;
    std::unordered_map<uint32_t, AIContext> contexts;
    uint32_t nextTreeId = 1;

public:
    // Register a behavior tree for a monster type
    [[nodiscard]] uint32_t RegisterBehaviorTree(BehaviorNodePtr tree);

    // Assign tree to entity
    void AssignToEntity(ecs::EntityHandle entity, uint32_t treeId, ecs::EcsWorld& world);

    // Update all AI entities (call every tick)
    void Update(ecs::EcsWorld& world, float deltaTime);

    // Get context for debugging
    [[nodiscard]] const AIContext* GetContext(ecs::EntityHandle entity) const;

    // Factory methods for common monster types
    [[nodiscard]] static BehaviorNodePtr CreateMeleeBehavior();
    [[nodiscard]] static BehaviorNodePtr CreateRangedBehavior();
    [[nodiscard]] static BehaviorNodePtr CreatePassiveBehavior();
    [[nodiscard]] static BehaviorNodePtr CreateBossBehavior();
};

} // namespace ai
