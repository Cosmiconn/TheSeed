#pragma once
// =============================================================================
// core/EventTypes.h — Engine Event Definitions
// C++23: Designated Initializers, std::format
// =============================================================================
#include "Types.h"
#include "ECS.h"
#include <cstdint>
#include <string>

// ---------------------------------------------------------------------------
// Core Entity Events
// ---------------------------------------------------------------------------
struct EntitySpawnedEvent {
    uint32_t    entityId = 0;
    std::string name;
    bool        isMonster = false;
    float       x = 0.0f;
    float       z = 0.0f;
    std::string materialId;
    float       scaleY = 1.0f;
    std::string meshId;
};

struct EntityDespawnedEvent {
    uint32_t entityId = 0;
};

struct EntityMovedEvent {
    uint32_t entityId = 0;
    float    x = 0.0f;
    float    z = 0.0f;
    float    targetX = 0.0f;
    float    targetZ = 0.0f;
};

struct EntityDamagedEvent {
    uint32_t targetId = 0;
    uint32_t sourceId = 0;
    int      newHP = 0;
    int      damage = 0;
};

struct EntityDiedEvent {
    uint32_t entityId = 0;
    uint32_t killerId = 0;
    uint32_t monsterTemplateId = 0;
    uint32_t originSpawnId = 0;
    float    x = 0.0f;
    float    z = 0.0f;
};

// ---------------------------------------------------------------------------
// Quest Events
// ---------------------------------------------------------------------------
struct QuestAcceptedEvent {
    uint32_t playerId = 0;
    uint32_t questId = 0;
};

struct QuestCompletedEvent {
    uint32_t playerId = 0;
    uint32_t questId = 0;
    uint32_t rewardGold = 0;
    uint32_t rewardXP = 0;
};

struct QuestUpdatedEvent {
    uint32_t playerId = 0;
    uint32_t questId = 0;
    uint32_t objectiveIndex = 0;
    uint32_t currentCount = 0;
    uint32_t requiredCount = 0;
};

// ---------------------------------------------------------------------------
// Status Effect Events
// ---------------------------------------------------------------------------
struct StatusEffectAppliedEvent {
    uint32_t         targetId = 0;
    uint32_t         sourceId = 0;
    StatusEffectType type = StatusEffectType::Poison;
    float            durationSec = 0.0f;
    uint32_t         tickDamage = 0;
};

struct StatusEffectRemovedEvent {
    uint32_t         targetId = 0;
    StatusEffectType type = StatusEffectType::Poison;
};

// ---------------------------------------------------------------------------
// Inventory & Trade Events
// ---------------------------------------------------------------------------
struct ItemEquippedEvent {
    uint32_t playerId = 0;
    uint32_t slotIndex = 0;
    uint32_t templateId = 0;
    bool     equipped = false;
};

struct InventoryChangedEvent {
    uint32_t playerId = 0;
};

struct TradeCompletedEvent {
    uint32_t player1Id = 0;
    uint32_t player2Id = 0;
};

// ---------------------------------------------------------------------------
// World & Chat Events
// ---------------------------------------------------------------------------
struct ChatMessageEvent {
    std::string sender;
    std::string text;
    uint32_t    channel = 0;
    bool        isWhisper = false;
    std::string target;
};

struct SectorSwitchedEvent {
    int   oldSectorX = 0;
    int   oldSectorZ = 0;
    int   newSectorX = 0;
    int   newSectorZ = 0;
    float exitX = 0.0f;
    float exitZ = 0.0f;
};

struct PlayerLoggedInEvent {
    uint32_t    entityId = 0;
    std::string name;
    float       x = 0.0f;
    float       y = 0.0f;
    float       z = 0.0f;
};

struct PlayerLoggedOutEvent {
    uint32_t    entityId = 0;
    std::string name;
};

struct TerrainModifiedEvent {
    int   brushX = 0;
    int   brushZ = 0;
    float brushRadius = 0.0f;
    float intensity = 0.0f;
    bool  raise = true;
};
