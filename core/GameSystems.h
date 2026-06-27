#pragma once
// =============================================================================
// core/GameSystems.h  —  C++23 Modernized
// Deklarationen only. Implementation in GameSystems.cpp
// =============================================================================
#include "World.h"
#include "Log.h"
#include "ByteBuffer.h"
#include "EventSystem.h"
#include "EventTypes.h"
#include <algorithm>
#include <sstream>
#include <cmath>
#include <ranges>
#include <format>
#include <utility>
#include <functional>
#include <span>

// =============================================================================
// INVENTAR
// =============================================================================
void LoadItemDatabase();
[[nodiscard]] bool AddItemToPlayer(uint32_t playerId, uint32_t templateId, uint32_t count);

// =============================================================================
// QUEST-SYSTEM
// =============================================================================
void LoadQuestsFromCSV(std::string_view csv);
[[nodiscard]] bool UpdateQuestProgress(uint32_t playerId, QuestObjectiveType type, uint32_t targetId);
void AcceptQuest(uint32_t playerId, uint32_t questId);

// =============================================================================
// SKILL-SYSTEM
// =============================================================================
void LoadSkillsFromCSV(std::string_view csv);
[[nodiscard]] bool CheckAndUpdateCooldown(uint32_t entityId, uint32_t skillId);

// =============================================================================
// STATUSEFFEKTE
// =============================================================================
using BroadcastCallback = std::move_only_function<void(std::span<const uint8_t>)>;

void ApplyStatusEffect(uint32_t targetEntityId, StatusEffectType type,
                       float durationSec, uint32_t sourceId, uint32_t tickDmg,
                       const BroadcastCallback& broadcast);
void RemoveStatusEffect(uint32_t targetEntityId, StatusEffectType type,
                        const BroadcastCallback& broadcast);
[[nodiscard]] bool HasStatusEffect(uint32_t entityId, StatusEffectType type);
void ProcessStatusEffects(float deltaSec, const BroadcastCallback& broadcast);

// =============================================================================
// NPC-SYSTEM
// =============================================================================
void LoadNpcsFromCSV(std::string_view csv);

// =============================================================================
// SPAWN & RESPAWN
// =============================================================================
void ScheduleRespawn(uint32_t spawnId, uint32_t templateId,
                     float x, float z, float respawnTimeSec);
void ProcessRespawnQueue(float deltaSec);

// =============================================================================
// PASSWORT-SICHERHEIT
// =============================================================================
[[nodiscard]] std::string Argon2IdHash(std::string_view password, std::string_view salt);
