// =============================================================================
// core/World.cpp — Globale Zustands-Definitionen
// =============================================================================
#include "World.h"

// =============================================================================
// AP-20: ECS Archetype Storage
// =============================================================================
std::unique_ptr<ecs::EcsWorld> gEcsWorld = std::make_unique<ecs::EcsWorld>();

// =============================================================================
// LEGACY REGISTRIES
// =============================================================================
std::vector<Entity> serverRegistry;
std::vector<Entity> clientRegistry;
uint32_t nextEntityId = 0;

// =============================================================================
// INVENTAR
// =============================================================================
std::map<uint32_t, ItemTemplate> itemDatabase;
std::map<uint32_t, std::vector<Item>> playerInventories;
std::mutex inventoryMutex;

// =============================================================================
// QUEST
// =============================================================================
std::map<uint32_t, QuestTemplate> questDatabase;
std::map<uint32_t, std::vector<PlayerQuestProgress>> playerQuestLog;

// =============================================================================
// SKILLS
// =============================================================================
std::map<uint32_t, SkillTemplate> skillDatabase;
std::map<uint32_t, std::map<uint32_t, TimePoint>> skillLastCastTime;
std::mutex skillCooldownMutex;

// =============================================================================
// STATUSEFFEKTE
// =============================================================================
std::map<uint32_t, std::vector<StatusEffect>> entityStatusEffects;
std::mutex statusEffectMutex;

// =============================================================================
// NPCs
// =============================================================================
std::map<uint32_t, NpcTemplate> npcDatabase;
std::mutex npcMutex;

// =============================================================================
// SPAWNS & RESPAWN
// =============================================================================
std::vector<SpawnPoint> sectorSpawns;
uint32_t nextSpawnPointId = 1;
std::vector<RespawnEntry> respawnQueue;
std::mutex respawnMutex;

// =============================================================================
// HANDEL
// =============================================================================
std::vector<TradeSession> activeTrades;
std::mutex tradeMutex;

// =============================================================================
// CHAT
// =============================================================================
std::vector<ChatMessage> chatHistory;
std::mutex chatMutex;

// =============================================================================
// TERRAIN
// =============================================================================
std::vector<float> heightMap(GRID_SIZE * GRID_SIZE, 0.0f);
int currentSectorX = 0;
int currentSectorZ = 0;

// =============================================================================
// KAMERA
// =============================================================================
Vector3 cameraPos = { 0.0f, 22.0f, 28.0f };

// =============================================================================
// HELPER FUNCTIONS
// =============================================================================
float GetHeightFromGrid(float wX, float wZ) {
    int gX = (wX + GRID_SIZE / 2.0f < 0) ? 0 :
             (static_cast<int>(wX + GRID_SIZE / 2.0f) >= GRID_SIZE) ? GRID_SIZE - 1 :
              static_cast<int>(wX + GRID_SIZE / 2.0f);
    int gZ = (wZ + GRID_SIZE / 2.0f < 0) ? 0 :
             (static_cast<int>(wZ + GRID_SIZE / 2.0f) >= GRID_SIZE) ? GRID_SIZE - 1 :
              static_cast<int>(wZ + GRID_SIZE / 2.0f);
    return heightMap[gZ * GRID_SIZE + gX];
}

std::string GetSectorName(int sx, int sz) {
    return std::format("{}_{}", sx, sz);
}
