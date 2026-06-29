// =============================================================================
// server/ai/AIBehavior.cpp — AI Behavior Tree Implementation (AP-47–AP-49)
// =============================================================================
#include "AIBehavior.h"
#include "../../core/Log.h"
#include <cmath>
#include <format>

namespace ai {

// =============================================================================
// Composite Nodes
// =============================================================================
NodeStatus SequenceNode::Tick(AIContext& ctx, float deltaTime) {
    while (currentChild < children.size()) {
        auto status = children[currentChild]->Tick(ctx, deltaTime);
        if (status == NodeStatus::Failure) {
            currentChild = 0;
            return NodeStatus::Failure;
        }
        if (status == NodeStatus::Running) {
            return NodeStatus::Running;
        }
        currentChild++;
    }
    currentChild = 0;
    return NodeStatus::Success;
}

NodeStatus SelectorNode::Tick(AIContext& ctx, float deltaTime) {
    while (currentChild < children.size()) {
        auto status = children[currentChild]->Tick(ctx, deltaTime);
        if (status == NodeStatus::Success) {
            currentChild = 0;
            return NodeStatus::Success;
        }
        if (status == NodeStatus::Running) {
            return NodeStatus::Running;
        }
        currentChild++;
    }
    currentChild = 0;
    return NodeStatus::Failure;
}

NodeStatus ParallelNode::Tick(AIContext& ctx, float deltaTime) {
    size_t successes = 0;
    size_t failures = 0;

    for (auto& child : children) {
        auto status = child->Tick(ctx, deltaTime);
        if (status == NodeStatus::Success) successes++;
        if (status == NodeStatus::Failure) failures++;
    }

    if (successes >= successThreshold) return NodeStatus::Success;
    if (failures > children.size() - successThreshold) return NodeStatus::Failure;
    return NodeStatus::Running;
}

// =============================================================================
// Decorator Nodes
// =============================================================================
NodeStatus InverterNode::Tick(AIContext& ctx, float deltaTime) {
    auto status = child->Tick(ctx, deltaTime);
    if (status == NodeStatus::Success) return NodeStatus::Failure;
    if (status == NodeStatus::Failure) return NodeStatus::Success;
    return status;
}

NodeStatus CooldownNode::Tick(AIContext& ctx, float deltaTime) {
    timer -= deltaTime;
    if (timer > 0.0f) return NodeStatus::Failure;

    auto status = child->Tick(ctx, deltaTime);
    if (status == NodeStatus::Success) {
        timer = cooldown;
    }
    return status;
}

NodeStatus RepeatNode::Tick(AIContext& ctx, float deltaTime) {
    auto status = child->Tick(ctx, deltaTime);
    if (status == NodeStatus::Success) {
        currentCount++;
        if (maxRepeats > 0 && currentCount >= maxRepeats) {
            currentCount = 0;
            return NodeStatus::Success;
        }
        return NodeStatus::Running;
    }
    return status;
}

// =============================================================================
// Condition Nodes
// =============================================================================
NodeStatus HasTargetCondition::Tick(AIContext& ctx, float deltaTime) {
    (void)deltaTime;
    return ctx.targetEntity != 0 ? NodeStatus::Success : NodeStatus::Failure;
}

NodeStatus TargetInRangeCondition::Tick(AIContext& ctx, float deltaTime) {
    (void)deltaTime;
    if (ctx.targetEntity == 0) return NodeStatus::Failure;

    auto* transform = ctx.world->GetComponent<game::Transform>(ctx.self);
    auto* targetTransform = ctx.world->GetComponent<game::Transform>(
        ecs::EntityHandle(ctx.targetEntity));

    if (!transform || !targetTransform) return NodeStatus::Failure;

    float dx = targetTransform->x - transform->x;
    float dz = targetTransform->z - transform->z;
    float dist = std::sqrt(dx*dx + dz*dz);

    return dist <= range ? NodeStatus::Success : NodeStatus::Failure;
}

NodeStatus TargetInAttackRangeCondition::Tick(AIContext& ctx, float deltaTime) {
    (void)deltaTime;
    if (ctx.targetEntity == 0) return NodeStatus::Failure;

    auto* transform = ctx.world->GetComponent<game::Transform>(ctx.self);
    auto* targetTransform = ctx.world->GetComponent<game::Transform>(
        ecs::EntityHandle(ctx.targetEntity));

    if (!transform || !targetTransform) return NodeStatus::Failure;

    float dx = targetTransform->x - transform->x;
    float dz = targetTransform->z - transform->z;
    float dist = std::sqrt(dx*dx + dz*dz);

    auto* monster = ctx.world->GetComponent<game::MonsterTag>(ctx.self);
    float attackRange = monster ? monster->aggroRange * 0.3f : 2.0f; // Attack range is 30% of aggro

    return dist <= attackRange ? NodeStatus::Success : NodeStatus::Failure;
}

NodeStatus IsHealthBelowCondition::Tick(AIContext& ctx, float deltaTime) {
    (void)deltaTime;
    auto* health = ctx.world->GetComponent<game::Health>(ctx.self);
    if (!health) return NodeStatus::Failure;

    float pct = static_cast<float>(health->current) / static_cast<float>(health->max);
    return pct < threshold ? NodeStatus::Success : NodeStatus::Failure;
}

NodeStatus ShouldReturnHomeCondition::Tick(AIContext& ctx, float deltaTime) {
    (void)deltaTime;
    auto* transform = ctx.world->GetComponent<game::Transform>(ctx.self);
    auto* monster = ctx.world->GetComponent<game::MonsterTag>(ctx.self);

    if (!transform || !monster) return NodeStatus::Failure;

    float dx = transform->x - monster->spawnX;
    float dz = transform->z - monster->spawnZ;
    float dist = std::sqrt(dx*dx + dz*dz);

    return dist > maxDistance ? NodeStatus::Success : NodeStatus::Failure;
}

// =============================================================================
// Action Nodes
// =============================================================================
NodeStatus PatrolAction::Tick(AIContext& ctx, float deltaTime) {
    auto* transform = ctx.world->GetComponent<game::Transform>(ctx.self);
    auto* monster = ctx.world->GetComponent<game::MonsterTag>(ctx.self);
    auto* ai = ctx.world->GetComponent<game::AIState>(ctx.self);

    if (!transform || !monster || !ai) return NodeStatus::Failure;

    ai->current = game::AIState::Patrol;

    // Simple patrol: move in a small circle around spawn point
    float angle = ctx.timeInState * 0.5f;
    float radius = 5.0f;
    float targetX = monster->spawnX + std::cos(angle) * radius;
    float targetZ = monster->spawnZ + std::sin(angle) * radius;

    float dx = targetX - transform->x;
    float dz = targetZ - transform->z;
    float dist = std::sqrt(dx*dx + dz*dz);

    if (dist > 0.1f) {
        transform->x += (dx / dist) * moveSpeed * deltaTime;
        transform->z += (dz / dist) * moveSpeed * deltaTime;
        transform->rotationY = std::atan2(dx, dz);
    }

    ctx.timeInState += deltaTime;

    // Check for players in detection range
    auto query = ctx.world->Query<ecs::All<game::Transform, game::PlayerTag>>();
    for (auto [playerHandle] : query) {
        auto* playerTransform = ctx.world->GetComponent<game::Transform>(playerHandle);
        if (!playerTransform) continue;

        float pdx = playerTransform->x - transform->x;
        float pdz = playerTransform->z - transform->z;
        float pdist = std::sqrt(pdx*pdx + pdz*pdz);

        if (pdist <= monster->aggroRange) {
            ctx.targetEntity = playerHandle.GetIndex();
            ctx.lastAggroTime = ctx.timeInState;
            return NodeStatus::Failure; // Interrupt patrol, switch to chase
        }
    }

    return NodeStatus::Running;
}

NodeStatus ChaseAction::Tick(AIContext& ctx, float deltaTime) {
    auto* transform = ctx.world->GetComponent<game::Transform>(ctx.self);
    auto* monster = ctx.world->GetComponent<game::MonsterTag>(ctx.self);
    auto* ai = ctx.world->GetComponent<game::AIState>(ctx.self);
    auto* targetTransform = ctx.world->GetComponent<game::Transform>(
        ecs::EntityHandle(ctx.targetEntity));

    if (!transform || !monster || !ai || !targetTransform) {
        ctx.targetEntity = 0;
        return NodeStatus::Failure;
    }

    ai->current = game::AIState::Chase;

    // Check if target is dead
    auto* targetHealth = ctx.world->GetComponent<game::Health>(ecs::EntityHandle(ctx.targetEntity));
    if (targetHealth && targetHealth->current <= 0) {
        ctx.targetEntity = 0;
        return NodeStatus::Failure;
    }

    // Check leash range
    float homeDx = transform->x - monster->spawnX;
    float homeDz = transform->z - monster->spawnZ;
    float homeDist = std::sqrt(homeDx*homeDx + homeDz*homeDz);

    if (homeDist > monster->leashRange) {
        ctx.targetEntity = 0;
        return NodeStatus::Failure; // Return home
    }

    // Move toward target
    float dx = targetTransform->x - transform->x;
    float dz = targetTransform->z - transform->z;
    float dist = std::sqrt(dx*dx + dz*dz);

    if (dist > 0.1f) {
        transform->x += (dx / dist) * moveSpeed * deltaTime;
        transform->z += (dz / dist) * moveSpeed * deltaTime;
        transform->rotationY = std::atan2(dx, dz);
    }

    ctx.timeInState += deltaTime;

    // Check if target is still in detection range
    if (dist > monster->aggroRange * 1.5f) {
        ctx.targetEntity = 0;
        return NodeStatus::Failure;
    }

    return NodeStatus::Running;
}

NodeStatus AttackAction::Tick(AIContext& ctx, float deltaTime) {
    auto* transform = ctx.world->GetComponent<game::Transform>(ctx.self);
    auto* ai = ctx.world->GetComponent<game::AIState>(ctx.self);
    auto* targetHealth = ctx.world->GetComponent<game::Health>(
        ecs::EntityHandle(ctx.targetEntity));

    if (!transform || !ai || !targetHealth) return NodeStatus::Failure;

    ai->current = game::AIState::Attack;

    float now = ctx.timeInState;
    if (now - ctx.lastAttackTime >= ctx.attackCooldown) {
        // Deal damage
        targetHealth->current -= static_cast<int>(damage);
        ctx.lastAttackTime = now;

        AddLog("[AI] Entity {} attacked target {} for {} damage",
               ctx.self.GetIndex(), ctx.targetEntity, damage);

        // Check if target died
        if (targetHealth->current <= 0) {
            ctx.targetEntity = 0;
            return NodeStatus::Success;
        }
    }

    ctx.timeInState += deltaTime;
    return NodeStatus::Running;
}

NodeStatus ReturnHomeAction::Tick(AIContext& ctx, float deltaTime) {
    auto* transform = ctx.world->GetComponent<game::Transform>(ctx.self);
    auto* monster = ctx.world->GetComponent<game::MonsterTag>(ctx.self);
    auto* ai = ctx.world->GetComponent<game::AIState>(ctx.self);

    if (!transform || !monster || !ai) return NodeStatus::Failure;

    ai->current = game::AIState::Return;

    float dx = monster->spawnX - transform->x;
    float dz = monster->spawnZ - transform->z;
    float dist = std::sqrt(dx*dx + dz*dz);

    if (dist < 0.5f) {
        // Heal to full when back home
        auto* health = ctx.world->GetComponent<game::Health>(ctx.self);
        if (health) health->current = health->max;

        ctx.targetEntity = 0;
        return NodeStatus::Success;
    }

    transform->x += (dx / dist) * moveSpeed * deltaTime;
    transform->z += (dz / dist) * moveSpeed * deltaTime;
    transform->rotationY = std::atan2(dx, dz);

    ctx.timeInState += deltaTime;
    return NodeStatus::Running;
}

NodeStatus IdleAction::Tick(AIContext& ctx, float deltaTime) {
    auto* ai = ctx.world->GetComponent<game::AIState>(ctx.self);
    if (ai) ai->current = game::AIState::Idle;

    ctx.timeInState += deltaTime;

    if (ctx.timeInState >= idleTime) {
        ctx.timeInState = 0.0f;
        return NodeStatus::Success;
    }
    return NodeStatus::Running;
}

NodeStatus FleeAction::Tick(AIContext& ctx, float deltaTime) {
    auto* transform = ctx.world->GetComponent<game::Transform>(ctx.self);
    auto* ai = ctx.world->GetComponent<game::AIState>(ctx.self);
    auto* targetTransform = ctx.world->GetComponent<game::Transform>(
        ecs::EntityHandle(ctx.targetEntity));

    if (!transform || !ai || !targetTransform) return NodeStatus::Failure;

    // Run away from target
    float dx = transform->x - targetTransform->x;
    float dz = transform->z - targetTransform->z;
    float dist = std::sqrt(dx*dx + dz*dz);

    if (dist > fleeDistance) {
        return NodeStatus::Success;
    }

    if (dist > 0.1f) {
        transform->x += (dx / dist) * moveSpeed * deltaTime;
        transform->z += (dz / dist) * moveSpeed * deltaTime;
        transform->rotationY = std::atan2(dx, dz);
    }

    ctx.timeInState += deltaTime;
    return NodeStatus::Running;
}

// =============================================================================
// Behavior Tree Builder
// =============================================================================
BehaviorTreeBuilder& BehaviorTreeBuilder::Sequence() {
    auto node = std::make_unique<SequenceNode>();
    if (!stack.empty()) {
        if (auto* seq = dynamic_cast<SequenceNode*>(stack.back().get())) {
            seq->AddChild(std::move(node));
        } else if (auto* sel = dynamic_cast<SelectorNode*>(stack.back().get())) {
            sel->AddChild(std::move(node));
        } else if (auto* par = dynamic_cast<ParallelNode*>(stack.back().get())) {
            par->AddChild(std::move(node));
        }
    }
    stack.push_back(std::move(node));
    return *this;
}

BehaviorTreeBuilder& BehaviorTreeBuilder::Selector() {
    auto node = std::make_unique<SelectorNode>();
    if (!stack.empty()) {
        if (auto* seq = dynamic_cast<SequenceNode*>(stack.back().get())) {
            seq->AddChild(std::move(node));
        } else if (auto* sel = dynamic_cast<SelectorNode*>(stack.back().get())) {
            sel->AddChild(std::move(node));
        } else if (auto* par = dynamic_cast<ParallelNode*>(stack.back().get())) {
            par->AddChild(std::move(node));
        }
    }
    stack.push_back(std::move(node));
    return *this;
}

BehaviorTreeBuilder& BehaviorTreeBuilder::End() {
    if (stack.size() == 1) {
        root = std::move(stack.back());
        stack.clear();
    } else {
        stack.pop_back();
    }
    return *this;
}

BehaviorNodePtr BehaviorTreeBuilder::Build() {
    if (!root && !stack.empty()) {
        root = std::move(stack.back());
        stack.clear();
    }
    return std::move(root);
}

// Leaf node helpers
#define ADD_LEAF(type, ...)     do {         auto node = std::make_unique<type>(__VA_ARGS__);         if (!stack.empty()) {             if (auto* seq = dynamic_cast<SequenceNode*>(stack.back().get())) {                 seq->AddChild(std::move(node));             } else if (auto* sel = dynamic_cast<SelectorNode*>(stack.back().get())) {                 sel->AddChild(std::move(node));             } else if (auto* par = dynamic_cast<ParallelNode*>(stack.back().get())) {                 par->AddChild(std::move(node));             }         }     } while(0)

BehaviorTreeBuilder& BehaviorTreeBuilder::HasTarget() { ADD_LEAF(HasTargetCondition); return *this; }
BehaviorTreeBuilder& BehaviorTreeBuilder::TargetInRange(float range) { ADD_LEAF(TargetInRangeCondition, range); return *this; }
BehaviorTreeBuilder& BehaviorTreeBuilder::TargetInAttackRange() { ADD_LEAF(TargetInAttackRangeCondition); return *this; }
BehaviorTreeBuilder& BehaviorTreeBuilder::IsHealthBelow(float threshold) { ADD_LEAF(IsHealthBelowCondition, threshold); return *this; }
BehaviorTreeBuilder& BehaviorTreeBuilder::ShouldReturnHome(float maxDistance) { ADD_LEAF(ShouldReturnHomeCondition, maxDistance); return *this; }
BehaviorTreeBuilder& BehaviorTreeBuilder::Patrol(float speed) { ADD_LEAF(PatrolAction, speed); return *this; }
BehaviorTreeBuilder& BehaviorTreeBuilder::Chase(float speed) { ADD_LEAF(ChaseAction, speed); return *this; }
BehaviorTreeBuilder& BehaviorTreeBuilder::Attack(float damage) { ADD_LEAF(AttackAction, damage); return *this; }
BehaviorTreeBuilder& BehaviorTreeBuilder::ReturnHome(float speed) { ADD_LEAF(ReturnHomeAction, speed); return *this; }
BehaviorTreeBuilder& BehaviorTreeBuilder::Idle(float time) { ADD_LEAF(IdleAction, time); return *this; }
BehaviorTreeBuilder& BehaviorTreeBuilder::Flee(float speed) { ADD_LEAF(FleeAction, speed); return *this; }

#undef ADD_LEAF

// =============================================================================
// AI System
// =============================================================================
uint32_t AISystem::RegisterBehaviorTree(BehaviorNodePtr tree) {
    uint32_t id = nextTreeId++;
    behaviorTrees[id] = std::move(tree);
    return id;
}

void AISystem::AssignToEntity(ecs::EntityHandle entity, uint32_t treeId, ecs::EcsWorld& world) {
    auto* ai = world.GetComponent<game::AIState>(entity);
    if (!ai) {
        world.AddComponent(entity, game::AIState{});
        ai = world.GetComponent<game::AIState>(entity);
    }

    auto& ctx = contexts[entity.GetIndex()];
    ctx.self = entity;
    ctx.world = &world;

    auto* monster = world.GetComponent<game::MonsterTag>(entity);
    if (monster) {
        ctx.detectionRange = monster->aggroRange;
    }

    ai->targetEntity = 0;
    ai->stateTimer = 0.0f;
}

void AISystem::Update(ecs::EcsWorld& world, float deltaTime) {
    for (auto& [entityId, ctx] : contexts) {
        auto it = behaviorTrees.find(entityId);
        if (it == behaviorTrees.end()) continue;

        ctx.timeInState += deltaTime;
        it->second->Tick(ctx, deltaTime);
    }
}

const AIContext* AISystem::GetContext(ecs::EntityHandle entity) const {
    auto it = contexts.find(entity.GetIndex());
    if (it != contexts.end()) return &it->second;
    return nullptr;
}

// =============================================================================
// Factory Methods
// =============================================================================
BehaviorNodePtr AISystem::CreateMeleeBehavior() {
    // Melee: Patrol → (if player detected) → Chase → (if in range) → Attack
    //        → (if health < 30%) → Flee → ReturnHome
    BehaviorTreeBuilder builder;

    builder.Selector()
        .Sequence()
            .ShouldReturnHome(25.0f)
            .ReturnHome(4.0f)
        .End()
        .Sequence()
            .IsHealthBelow(0.3f)
            .Flee(6.0f)
        .End()
        .Sequence()
            .HasTarget()
            .TargetInAttackRange()
            .Attack(15.0f)
        .End()
        .Sequence()
            .HasTarget()
            .TargetInRange(15.0f)
            .Chase(5.0f)
        .End()
        .Patrol(2.0f)
    .End();

    return builder.Build();
}

BehaviorNodePtr AISystem::CreateRangedBehavior() {
    // Ranged: Keep distance, attack from range
    BehaviorTreeBuilder builder;

    builder.Selector()
        .Sequence()
            .ShouldReturnHome(30.0f)
            .ReturnHome(4.0f)
        .End()
        .Sequence()
            .HasTarget()
            .TargetInRange(20.0f)
            .TargetInAttackRange()
            .Attack(10.0f)
        .End()
        .Sequence()
            .HasTarget()
            .TargetInRange(25.0f)
            .Chase(3.5f)
        .End()
        .Patrol(1.5f)
    .End();

    return builder.Build();
}

BehaviorNodePtr AISystem::CreatePassiveBehavior() {
    // Passive: Only patrol, flee if attacked
    BehaviorTreeBuilder builder;

    builder.Selector()
        .Sequence()
            .IsHealthBelow(1.0f) // Always true if has health (was attacked)
            .Flee(7.0f)
        .End()
        .Patrol(1.0f)
    .End();

    return builder.Build();
}

BehaviorNodePtr AISystem::CreateBossBehavior() {
    // Boss: Complex behavior with phases
    BehaviorTreeBuilder builder;

    builder.Selector()
        .Sequence()
            .IsHealthBelow(0.25f)
            .Cooldown(3.0f)
            .Attack(50.0f) // Enrage phase
        .End()
        .Sequence()
            .IsHealthBelow(0.5f)
            .Cooldown(2.0f)
            .Attack(30.0f) // Phase 2
        .End()
        .Sequence()
            .HasTarget()
            .TargetInAttackRange()
            .Cooldown(1.5f)
            .Attack(20.0f)
        .End()
        .Sequence()
            .HasTarget()
            .Chase(4.0f)
        .End()
        .Idle(1.0f)
    .End();

    return builder.Build();
}

} // namespace ai
