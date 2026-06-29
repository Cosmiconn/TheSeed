#pragma once
// =============================================================================
// core/ECS.h — Legacy ECS Wrapper (V13.1 → V13.2 Migration)
// AP-23: Adapter-Layer für schrittweise Migration zum Archetype-ECS
// =============================================================================
#include "Types.h"
#include "world.h"

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

// Globale Registries (V13.1 Legacy)
inline std::vector<Entity> serverRegistry;
inline std::vector<Entity> clientRegistry;
inline std::vector<MonsterTemplate> monsterTemplates;
inline std::vector<SpawnPoint> spawnPoints;
inline std::vector<ClientSession> clientSessions;
inline std::mutex sessionsMutex;
inline std::mutex inventoryMutex;
inline uint32_t nextEntityId = 1;

// Skill/Quest/NPC DBs
inline std::unordered_map<uint32_t, SkillTemplate> skillDatabase;
inline std::unordered_map<uint32_t, QuestTemplate> questDatabase;
inline std::unordered_map<uint32_t, NpcTemplate> npcDatabase;
inline std::unordered_map<uint32_t, ItemTemplate> itemDatabase;
inline std::unordered_map<uint32_t, std::vector<QuestLogEntry>> playerQuestLog;
inline std::unordered_map<uint32_t, std::vector<ItemSlot>> playerInventories;

// Neue ECS-World (wird von main.cpp initialisiert)
extern std::unique_ptr<ecs::EcsWorld> gEcsWorld;
extern bool gUseEcs;
