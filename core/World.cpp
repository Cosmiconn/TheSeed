// =============================================================================
// core/World.cpp — Globale Welt-Zustände Definitionen
// =============================================================================
#include "World.h"
#include "ECS.h"  // Entity-Definition
#include "../server/Network.h"  // ClientSession-Definition

// Registries
std::vector<Entity> serverRegistry;
std::vector<Entity> clientRegistry;
std::vector<MonsterTemplate> monsterTemplates;
std::vector<SpawnPoint> spawnPoints;
std::vector<ClientSession> clientSessions;
std::mutex sessionsMutex;
std::mutex inventoryMutex;
uint32_t nextEntityId = 1;

// Datenbanken
std::unordered_map<uint32_t, SkillTemplate> skillDatabase;
std::unordered_map<uint32_t, QuestTemplate> questDatabase;
std::unordered_map<uint32_t, NPCTemplate> npcDatabase;
std::unordered_map<uint32_t, ItemTemplate> itemDatabase;
std::unordered_map<uint32_t, std::vector<QuestProgress>> playerQuestLog;
std::unordered_map<uint32_t, std::vector<Item>> playerInventories;
