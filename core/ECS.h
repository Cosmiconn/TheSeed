#pragma once
// =============================================================================
// core/ECS.h — Entity Component System (LEGACY → ECS MIGRATION)
// =============================================================================
// STATUS: V13.1 AOS-Struktur. Wird durch ecs::Archetype (AP-20) ersetzt.
// MIGRATION: AP-23 migriert Daten von std::vector<Entity> zu ecs::EcsWorld.
// =============================================================================

#include "Types.h"
#include <cstdint>
#include <string>
#include <vector>

// =============================================================================
// FORWÄRTSDEKLARATION — Neues ECS (AP-20)
// =============================================================================
namespace ecs {
    class EcsWorld;
    struct EntityHandle;
}

// =============================================================================
// KOMPONENTEN (SOA-kompatibel — einzeln in Chunks speicherbar)
// =============================================================================
struct TransformComponent {
    float x = 0.0f, y = 0.0f, z = 0.0f;
    float targetX = 0.0f, targetZ = 0.0f;
    float lerpX = 0.0f, lerpZ = 0.0f;
    float lookX = 0.0f, lookZ = 1.0f;
};

struct RenderComponent {
    // TODO-AP-27: Ersetze std::string durch StringHash32/64 für SOA-Chunks
    std::string materialId;
    float scaleY = 1.0f;
    std::string meshId;
};

struct PersistenceComponent {
    uint32_t level = 1;
    uint32_t gold = 0;
    bool isDirty = false;
};

// =============================================================================
// LEGACY ENTITY (AOS — wird durch ecs::EntityHandle + Komponenten-Archetype ersetzt)
// =============================================================================
struct Entity {
    uint32_t id = 0;
    std::string name;
    bool isMonster = false;
    uint32_t monsterTemplateId = 0;
    uint32_t originSpawnId = 0;
    int currentHP = 100;

    TransformComponent transform;
    RenderComponent render;
    PersistenceComponent persistence;

    // ECS-Migration: Diese Struktur wird in SOA-Chunks aufgeteilt
    // Archetype-Key: Position | Render | Health | Persistence
    [[deprecated("Wird durch ecs::EntityHandle ersetzt (AP-20). Nicht für neue Features verwenden.")]]
    static constexpr uint32_t INVALID_ID = UINT32_MAX;
};

// =============================================================================
// ITEM & QUEST (unverändert — keine ECS-Änderung nötig)
// =============================================================================
struct Item {
    uint32_t templateId = 0;
    uint32_t count = 0;
    bool isEquipped = false;
};

struct ItemTemplate {
    uint32_t id = 0;
    std::string name;
    bool isStackable = false;
    uint32_t maxStack = 1;
    uint32_t slot = 0;      // 0 = nicht ausrüstbar
    uint32_t minLevel = 1;
};

struct QuestObjective {
    QuestObjectiveType type = QuestObjectiveType::KillMonster;
    uint32_t targetId = 0;
    uint32_t requiredCount = 0;
    uint32_t currentCount = 0;
    std::string description;
};

struct QuestTemplate {
    uint32_t id = 0;
    std::string title;
    std::string description;
    std::vector<QuestObjective> objectives;
    uint32_t rewardGold = 0;
    uint32_t rewardXP = 0;
};

struct PlayerQuestProgress {
    uint32_t questId = 0;
    QuestState state = QuestState::Inactive;
    std::vector<QuestObjective> objectives;
};

// =============================================================================
// SKILL & STATUS (unverändert)
// =============================================================================
struct SkillTemplate {
    uint32_t id = 0;
    std::string name;
    float range = 5.0f;
    float fov = 360.0f;
    uint32_t damage = 0;
    float cooldown = 1.0f;
    int statusEffectType = -1;
    float statusEffectDur = 0.0f;
    uint32_t statusEffectTickDmg = 0;
};

enum class StatusEffectType : uint8_t {
    Poison = 0,
    Slow = 1,
    Stun = 2
};

struct StatusEffect {
    StatusEffectType type = StatusEffectType::Poison;
    float durationSec = 0.0f;
    float elapsedSec = 0.0f;
    uint32_t sourceEntityId = 0;
    uint32_t tickDamagePer = 0;
    float tickIntervalSec = 1.0f;
    float tickAccumulator = 0.0f;
};

// =============================================================================
// NPC & SPAWN (unverändert)
// =============================================================================
struct NpcTemplate {
    uint32_t id = 0;
    std::string name;
    float x = 0.0f, z = 0.0f;
    std::string greeting;
    uint32_t offeredQuestId = 0;
};

struct SpawnPoint {
    uint32_t id = 0;
    float x = 0.0f, z = 0.0f;
    uint32_t monsterTemplateId = 0;
    float respawnTime = 15.0f;
};

struct RespawnEntry {
    uint32_t spawnPointId = 0;
    uint32_t monsterTemplateId = 0;
    float x = 0.0f, z = 0.0f;
    float respawnTime = 15.0f;
    float elapsed = 0.0f;
};

// =============================================================================
// TRADE & CHAT (unverändert)
// =============================================================================
struct TradeSession {
    uint32_t p1Id = 0, p2Id = 0;
    uint32_t p1OfferSlot = 999, p1OfferCount = 0;
    uint32_t p2OfferSlot = 999, p2OfferCount = 0;
    bool p1Confirm = false, p2Confirm = false;
};

struct ChatMessage {
    std::string senderName;
    std::string text;
    uint32_t channel = 0;
};

// =============================================================================
// PLAYER PROFILE (unverändert — Database)
// =============================================================================
struct PlayerProfile {
    char username[32] = {};
    uint32_t level = 1;
    uint32_t gold = 0;
    float lastX = 0.0f, lastY = 0.5f, lastZ = 0.0f;
    char lastSector[32] = {};
};
