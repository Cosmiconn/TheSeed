#pragma once
// =============================================================================
// core/GameSystems.h — C++23 Modernized (P1-FIX)
// =============================================================================
// KORREKTUR P1: Fehlende Includes ergänzt. Alle Standard-Header vollständig.
// BroadcastCallback als std::move_only_function definiert.
// Argon2IdHash/Verify mit konsistenter Signatur.
// ApplyStatusEffect mit vollständiger 6-Parameter-Signatur.
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
#include <string>
#include <cstdint>
#include <vector>

// =============================================================================
// INVENTAR
// =============================================================================
void LoadItemDatabase();
[[nodiscard]] bool AddItemToInventory(uint32_t playerId, uint32_t templateId, uint32_t count);
void RemoveItemFromInventory(uint32_t playerId, uint32_t slotIndex, uint32_t count);

// =============================================================================
// QUEST-SYSTEM
// =============================================================================
void LoadQuestsFromCSV(std::string_view csv);
[[nodiscard]] bool UpdateQuestProgress(uint32_t playerId, uint32_t questId, uint32_t objectiveIndex);
void AcceptQuest(uint32_t playerId, uint32_t questId);

// =============================================================================
// SKILL-SYSTEM
// =============================================================================
void LoadSkillsFromCSV(std::string_view csv);
[[nodiscard]] bool CheckAndUpdateCooldown(uint32_t entityId, uint32_t skillId);
bool CastSkill(uint32_t casterId, uint32_t skillId, uint32_t targetId);
void UpdateSkillCooldowns(float deltaTime);

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
void ProcessStatusEffects(float deltaSec);

// =============================================================================
// NPC-SYSTEM
// =============================================================================
void LoadNpcsFromCSV(std::string_view csv);
void TalkToNPC(uint32_t playerId, uint32_t npcId);

// =============================================================================
// SPAWN & RESPAWN
// =============================================================================
void ScheduleRespawn(uint32_t spawnId, uint32_t templateId,
                       float x, float z, float respawnTimeSec);
void ProcessRespawnQueue(float deltaSec);
void SpawnMonster(uint32_t templateId, float x, float z);
void LoadMonsterTemplatesFromCSV(std::string_view csv);

// =============================================================================
// ECS-SYSTEM REGISTRIERUNG (FIX P1-1)
// =============================================================================
void RegisterECSSystems();

// =============================================================================
// COMBAT
// =============================================================================
void ApplyDamage(uint32_t targetId, uint32_t sourceId, int damage);

// =============================================================================
// DATENBANK-LADEN (CSV)
// =============================================================================
void LoadItemsFromCSV(std::string_view path);

// =============================================================================
// PASSWORT-SICHERHEIT
// =============================================================================
[[nodiscard]] std::string Argon2IdHash(std::string_view password);
[[nodiscard]] bool Argon2IdVerify(std::string_view password, std::string_view hashString);
