#pragma once
// =============================================================================
// core/ECS.h — Legacy ECS Wrapper (V13.1 → V13.2 Migration)
// AP-23: Adapter-Layer für schrittweise Migration zum Archetype-ECS
// =============================================================================
// KORREKTUR: serverRegistry und clientRegistry wurden entfernt, da sie
// bereits in core/World.h als extern deklariert sind. Doppelte Definition
// verursachte ODR-Verletzung (One Definition Rule).
// =============================================================================
#include "Types.h"

// Forward decl für neues ECS
namespace ecs { class EcsWorld; }

// Legacy-Entity bleibt während der Übergangsphase aktiv
struct Entity {
    uint32_t id = 0;
    bool isMonster = false;
    std::string name;
    TransformComponent transform;
    RenderComponent render;
    int currentHP = 100;
    int maxHP = 100;
    uint32_t monsterTemplateId = 0;
    PersistenceData persistence;
    std::vector<StatusEffect> statusEffects;
    float skillCooldownRemaining = 0.0f;
};

// Neue ECS-World (wird von main.cpp initialisiert)
extern std::unique_ptr<ecs::EcsWorld> gEcsWorld;
extern bool gUseEcs;
