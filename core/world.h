#pragma once
// =============================================================================
// core/World.h — Globale Zustände mit ECS-Migrations-Vorbereitung
// =============================================================================

#include "ECS.h"
#include <vector>
#include <map>
#include <mutex>
#include <set>
#include <chrono>
#include <optional>
#include <memory>

// =============================================================================
#warning "LEGACY ECS: std::vector<Entity> wird durch ecs::EcsWorld ersetzt (AP-20-AP-23)"
// =============================================================================

// AP-20: ECS Archetype Storage
#include "ecs/EcsWorld.h"
#include "ecs/ECS.h"

// =============================================================================
// MUTEX-REIHENFOLGE (Deadlock-Prävention — nie umkehren)
// =============================================================================
// 1. sessionsMutex  2. inventoryMutex  3. tradeMutex  4. skillCooldownMutex
// 5. npcMutex       6. respawnMutex    7. statusEffectMutex  8. logMutex
// 9. chatMutex      10. qMutex (DatabaseManager intern)

// =============================================================================
// NEUES ECS (wird initialisiert nach AP-20)
// =============================================================================
extern std::unique_ptr<ecs::EcsWorld> gEcsWorld;  // AP-20: Initialized in main()

// =============================================================================
// LEGACY REGISTRIES (werden zu ecs::EcsWorld migriert)
// =============================================================================
extern std::vector<Entity> serverRegistry;   // [[deprecated: AP-23]]
extern std::vector<Entity> clientRegistry;   // [[deprecated: AP-23]]
extern uint32_t nextEntityId;

// =============================================================================
// INVENTAR
// =============================================================================
extern std::map<uint32_t, ItemTemplate> itemDatabase;
extern std::map<uint32_t, std::vector<Item>> playerInventories;
extern std::mutex inventoryMutex;

// =============================================================================
// QUEST
// =============================================================================
extern std::map<uint32_t, QuestTemplate> questDatabase;
extern std::map<uint32_t, std::vector<PlayerQuestProgress>> playerQuestLog;

// =============================================================================
// SKILLS
// =============================================================================
extern std::map<uint32_t, SkillTemplate> skillDatabase;
using TimePoint = std::chrono::steady_clock::time_point;
extern std::map<uint32_t, std::map<uint32_t, TimePoint>> skillLastCastTime;
extern std::mutex skillCooldownMutex;

// =============================================================================
// STATUSEFFEKTE
// =============================================================================
extern std::map<uint32_t, std::vector<StatusEffect>> entityStatusEffects;
extern std::mutex statusEffectMutex;

// =============================================================================
// NPCs
// =============================================================================
extern std::map<uint32_t, NpcTemplate> npcDatabase;
extern std::mutex npcMutex;

// =============================================================================
// SPAWNS & RESPAWN
// =============================================================================
extern std::vector<SpawnPoint> sectorSpawns;
extern uint32_t nextSpawnPointId;
extern std::vector<RespawnEntry> respawnQueue;
extern std::mutex respawnMutex;

// =============================================================================
// HANDEL
// =============================================================================
extern std::vector<TradeSession> activeTrades;
extern std::mutex tradeMutex;

// =============================================================================
// CHAT
// =============================================================================
extern std::vector<ChatMessage> chatHistory;
extern std::mutex chatMutex;
inline constexpr size_t MAX_CHAT_HISTORY = 100;

// =============================================================================
// TERRAIN
// =============================================================================
extern std::vector<float> heightMap;
extern int currentSectorX;
extern int currentSectorZ;

// =============================================================================
// KAMERA (Client)
// =============================================================================
extern Vector3 cameraPos;

// =============================================================================
// WORLD HELPER FUNCTIONS
// =============================================================================
[[nodiscard]] float GetHeightFromGrid(float wX, float wZ);
[[nodiscard]] std::string GetSectorName(int sx, int sz);

// Renderer interface (AP-04 will replace with proper abstraction)
void RebuildTerrainMeshOnGPU();
