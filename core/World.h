#pragma once
// =============================================================================
// core/World.h — Globale Welt-Zustände (V13.1 Legacy)
// =============================================================================
// KORREKTUR: Zirkulärer Include zu ECS.h entfernt. Stattdessen Forward-
// Declaration von Entity, damit World.h unabhängig bleibt.
// =============================================================================
#include "Types.h"
#include <vector>
#include <mutex>
#include <unordered_map>
#include <set>

// Forward-Declaration (vermeidet zirkulären Include)
struct Entity;
struct ClientSession;

// Globale Registries (V13.1 Legacy)
extern std::vector<Entity> serverRegistry;
extern std::vector<Entity> clientRegistry;
extern std::vector<MonsterTemplate> monsterTemplates;
extern std::vector<SpawnPoint> spawnPoints;
extern std::vector<ClientSession> clientSessions;
extern std::mutex sessionsMutex;
extern std::mutex inventoryMutex;
extern uint32_t nextEntityId;

// Skill/Quest/NPC DBs
extern std::unordered_map<uint32_t, SkillTemplate> skillDatabase;
extern std::unordered_map<uint32_t, QuestTemplate> questDatabase;
extern std::unordered_map<uint32_t, NPCTemplate> npcDatabase;
extern std::unordered_map<uint32_t, ItemTemplate> itemDatabase;
extern std::unordered_map<uint32_t, std::vector<QuestProgress>> playerQuestLog;
extern std::unordered_map<uint32_t, std::vector<Item>> playerInventories;
