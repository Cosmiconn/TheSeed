// =============================================================================
// core/GameSystems.cpp — GameSystems Implementation
// =============================================================================
#include "GameSystems.h"
#include "World.h"
#include "ECS.h"
#include "EventSystem.h"
#include "EventTypes.h"
#include "ByteBuffer.h"
#include "Log.h"

#include <algorithm>
#include <sstream>
#include <cmath>
#include <ranges>
#include <format>
#include <utility>
#include <functional>
#include <cstring>
#include <chrono>
#include <mutex>

// =============================================================================
// ITEM SYSTEM
// =============================================================================

void LoadItemDatabase() {
    itemDatabase.clear();

    // Default items
    itemDatabase[10] = ItemTemplate{
        .id = 10,
        .name = "Health Potion",
        .isStackable = true,
        .maxStack = 99,
        .slot = 0,
        .minLevel = 1
    };
    itemDatabase[20] = ItemTemplate{
        .id = 20,
        .name = "Iron Sword",
        .isStackable = false,
        .maxStack = 1,
        .slot = 1,
        .minLevel = 5
    };
    itemDatabase[30] = ItemTemplate{
        .id = 30,
        .name = "Leather Armor",
        .isStackable = false,
        .maxStack = 1,
        .slot = 2,
        .minLevel = 3
    };
    itemDatabase[50] = ItemTemplate{
        .id = 50,
        .name = "Mana Potion",
        .isStackable = true,
        .maxStack = 99,
        .slot = 0,
        .minLevel = 1
    };

    AddLog("[ItemDB] {} items loaded.", itemDatabase.size());
}

bool AddItemToPlayer(uint32_t playerId, uint32_t templateId, uint32_t count) {
    if (!itemDatabase.contains(templateId)) {
        AddLog("[ItemDB] Invalid templateId: {}", templateId);
        return false;
    }

    std::lock_guard lock(inventoryMutex);
    auto& inv = playerInventories[playerId];
    if (inv.empty()) inv.resize(INVENTORY_SIZE);

    const auto& tmpl = itemDatabase[templateId];

    // Try to stack first
    if (tmpl.isStackable) {
        for (auto& item : inv) {
            if (item.templateId == templateId && item.count < tmpl.maxStack) {
                uint32_t space = tmpl.maxStack - item.count;
                uint32_t add = std::min(count, space);
                item.count += add;
                count -= add;
                if (count == 0) {
                    AddLog("[ItemDB] Added {}x {} to player {}", add, tmpl.name, playerId);
                    return true;
                }
            }
        }
    }

    // Find empty slot
    for (auto& item : inv) {
        if (item.templateId == 0) {
            item.templateId = templateId;
            item.count = std::min(count, tmpl.maxStack);
            AddLog("[ItemDB] Added {}x {} to player {}", item.count, tmpl.name, playerId);
            return true;
        }
    }

    AddLog("[ItemDB] Inventory full for player {}", playerId);
    return false;
}

// =============================================================================
// QUEST SYSTEM
// =============================================================================

void LoadQuestsFromCSV(std::string_view csv) {
    questDatabase.clear();

    std::istringstream stream(std::string(csv));
    std::string line;

    // Skip header
    std::getline(stream, line);

    while (std::getline(stream, line)) {
        if (line.empty()) continue;

        std::istringstream row(line);
        std::string cell;
        std::vector<std::string> cells;

        while (std::getline(row, cell, ',')) {
            cells.push_back(cell);
        }

        if (cells.size() < 8) continue;

        try {
            uint32_t questId = static_cast<uint32_t>(std::stoul(cells[0]));

            QuestTemplate qt;
            qt.id = questId;
            qt.title = cells[1];
            qt.description = cells[2];

            QuestObjective obj;
            obj.type = static_cast<QuestObjectiveType>(std::stoul(cells[3]));
            obj.targetId = static_cast<uint32_t>(std::stoul(cells[4]));
            obj.requiredCount = static_cast<uint32_t>(std::stoul(cells[5]));
            obj.currentCount = 0;
            obj.description = obj.type == QuestObjectiveType::KillMonster ? "Defeat targets" :
                             obj.type == QuestObjectiveType::ReachZone ? "Reach destination" :
                             "Talk to NPC";

            qt.objectives.push_back(obj);
            qt.rewardGold = static_cast<uint32_t>(std::stoul(cells[6]));
            qt.rewardXP = static_cast<uint32_t>(std::stoul(cells[7]));

            questDatabase[questId] = qt;
        } catch (...) {
            AddLog("[QuestDB] Failed to parse line: {}", line);
        }
    }

    AddLog("[QuestDB] {} quests loaded.", questDatabase.size());
}

bool UpdateQuestProgress(uint32_t playerId, QuestObjectiveType type, uint32_t targetId) {
    if (!playerQuestLog.contains(playerId)) return false;

    bool anyUpdated = false;
    for (auto& progress : playerQuestLog[playerId]) {
        if (progress.state != QuestState::Active) continue;

        for (auto& obj : progress.objectives) {
            if (obj.type == type && obj.targetId == targetId) {
                if (obj.currentCount < obj.requiredCount) {
                    obj.currentCount++;
                    anyUpdated = true;

                    gEventBus.Publish(QuestUpdatedEvent{
                        .playerId = playerId,
                        .questId = progress.questId,
                        .objectiveIndex = 0,
                        .currentCount = obj.currentCount,
                        .requiredCount = obj.requiredCount
                    });

                    if (obj.currentCount >= obj.requiredCount) {
                        progress.state = QuestState::Completed;

                        gEventBus.Publish(QuestCompletedEvent{
                            .playerId = playerId,
                            .questId = progress.questId,
                            .rewardGold = questDatabase[progress.questId].rewardGold,
                            .rewardXP = questDatabase[progress.questId].rewardXP
                        });

                        AddLog("[Quest] Player {} completed quest {}", playerId, progress.questId);
                    }
                }
            }
        }
    }

    return anyUpdated;
}

void AcceptQuest(uint32_t playerId, uint32_t questId) {
    if (!questDatabase.contains(questId)) {
        AddLog("[Quest] Invalid questId: {}", questId);
        return;
    }

    auto& log = playerQuestLog[playerId];

    // Check if already has this quest
    for (const auto& progress : log) {
        if (progress.questId == questId) {
            AddLog("[Quest] Player {} already has quest {}", playerId, questId);
            return;
        }
    }

    PlayerQuestProgress progress;
    progress.questId = questId;
    progress.state = QuestState::Active;

    // Copy objectives from template
    const auto& qt = questDatabase[questId];
    for (const auto& obj : qt.objectives) {
        QuestObjective copy = obj;
        copy.currentCount = 0;
        progress.objectives.push_back(copy);
    }

    log.push_back(progress);

    gEventBus.Publish(QuestAcceptedEvent{
        .playerId = playerId,
        .questId = questId
    });

    AddLog("[Quest] Player {} accepted quest {}: {}", playerId, questId, qt.title);
}

// =============================================================================
// SKILL SYSTEM
// =============================================================================

void LoadSkillsFromCSV(std::string_view csv) {
    skillDatabase.clear();

    std::istringstream stream(std::string(csv));
    std::string line;

    // Skip header
    std::getline(stream, line);

    while (std::getline(stream, line)) {
        if (line.empty()) continue;

        std::istringstream row(line);
        std::string cell;
        std::vector<std::string> cells;

        while (std::getline(row, cell, ',')) {
            cells.push_back(cell);
        }

        if (cells.size() < 9) continue;

        try {
            SkillTemplate sk;
            sk.id = static_cast<uint32_t>(std::stoul(cells[0]));
            sk.name = cells[1];
            sk.range = std::stof(cells[2]);
            sk.fov = std::stof(cells[3]);
            sk.damage = static_cast<uint32_t>(std::stoul(cells[4]));
            sk.cooldown = std::stof(cells[5]);
            sk.statusEffectType = std::stoi(cells[6]);
            sk.statusEffectDur = std::stof(cells[7]);
            sk.statusEffectTickDmg = static_cast<uint32_t>(std::stoul(cells[8]));

            skillDatabase[sk.id] = sk;
        } catch (...) {
            AddLog("[SkillDB] Failed to parse line: {}", line);
        }
    }

    AddLog("[SkillDB] {} skills loaded.", skillDatabase.size());
}

bool CheckAndUpdateCooldown(uint32_t entityId, uint32_t skillId) {
    if (!skillDatabase.contains(skillId)) return false;

    std::lock_guard lock(skillCooldownMutex);

    auto& entityCooldowns = skillLastCastTime[entityId];
    auto now = std::chrono::steady_clock::now();

    if (entityCooldowns.contains(skillId)) {
        float elapsed = std::chrono::duration<float>(now - entityCooldowns[skillId]).count();
        float remaining = skillDatabase[skillId].cooldown - elapsed;
        if (remaining > 0.0f) {
            AddLog("[Skill] Cooldown active: {}s remaining for entity {}, skill {}", 
                   remaining, entityId, skillId);
            return false;
        }
    }

    entityCooldowns[skillId] = now;
    return true;
}

// =============================================================================
// STATUS EFFECTS
// =============================================================================

void ApplyStatusEffect(uint32_t targetEntityId, StatusEffectType type,
                       float durationSec, uint32_t sourceId, uint32_t tickDmg,
                       const BroadcastCallback& broadcast) {
    std::lock_guard lock(statusEffectMutex);

    // Remove existing effect of same type
    auto& effects = entityStatusEffects[targetEntityId];
    effects.erase(
        std::ranges::remove_if(effects,
            [type](const StatusEffect& e){ return e.type == type; }).begin(),
        effects.end());

    StatusEffect eff;
    eff.type = type;
    eff.durationSec = durationSec;
    eff.elapsedSec = 0.0f;
    eff.sourceEntityId = sourceId;
    eff.tickDamagePer = tickDmg;
    eff.tickIntervalSec = 1.0f;
    eff.tickAccumulator = 0.0f;

    effects.push_back(eff);

    gEventBus.Publish(StatusEffectAppliedEvent{
        .targetId = targetEntityId,
        .sourceId = sourceId,
        .type = type,
        .durationSec = durationSec,
        .tickDamage = tickDmg
    });

    AddLog("[Status] Applied {} to entity {} for {}s",
           type == StatusEffectType::Poison ? "Poison" :
           type == StatusEffectType::Slow ? "Slow" : "Stun",
           targetEntityId, durationSec);

    // Broadcast to clients
    if (broadcast) {
        ByteBuffer pkt;
        pkt.WriteUInt8(std::to_underlying(PacketType::MSG_STATUS_EFFECT));
        pkt.WriteUInt32(targetEntityId);
        pkt.WriteUInt8(std::to_underlying(type));
        pkt.WriteUInt8(1); // applied
        pkt.WriteFloat(durationSec);
        pkt.WriteUInt32(tickDmg);
        broadcast(std::span(pkt.data));
    }
}

void RemoveStatusEffect(uint32_t targetEntityId, StatusEffectType type,
                        const BroadcastCallback& broadcast) {
    std::lock_guard lock(statusEffectMutex);

    if (!entityStatusEffects.contains(targetEntityId)) return;

    auto& effects = entityStatusEffects[targetEntityId];
    auto it = std::ranges::find_if(effects,
        [type](const StatusEffect& e){ return e.type == type; });

    if (it != effects.end()) {
        effects.erase(it);

        gEventBus.Publish(StatusEffectRemovedEvent{
            .targetId = targetEntityId,
            .type = type
        });

        AddLog("[Status] Removed {} from entity {}",
               type == StatusEffectType::Poison ? "Poison" :
               type == StatusEffectType::Slow ? "Slow" : "Stun",
               targetEntityId);

        // Broadcast to clients
        if (broadcast) {
            ByteBuffer pkt;
            pkt.WriteUInt8(std::to_underlying(PacketType::MSG_STATUS_EFFECT));
            pkt.WriteUInt32(targetEntityId);
            pkt.WriteUInt8(std::to_underlying(type));
            pkt.WriteUInt8(0); // removed
            pkt.WriteFloat(0.0f);
            pkt.WriteUInt32(0);
            broadcast(std::span(pkt.data));
        }
    }

    if (effects.empty()) {
        entityStatusEffects.erase(targetEntityId);
    }
}

bool HasStatusEffect(uint32_t entityId, StatusEffectType type) {
    std::lock_guard lock(statusEffectMutex);

    if (!entityStatusEffects.contains(entityId)) return false;

    const auto& effects = entityStatusEffects[entityId];
    return std::ranges::any_of(effects,
        [type](const StatusEffect& e){ return e.type == type; });
}

void ProcessStatusEffects(float deltaSec, const BroadcastCallback& broadcast) {
    std::lock_guard lock(statusEffectMutex);

    std::vector<std::pair<uint32_t, StatusEffectType>> toRemove;

    for (auto& [entityId, effects] : entityStatusEffects) {
        for (auto& eff : effects) {
            eff.elapsedSec += deltaSec;
            eff.tickAccumulator += deltaSec;

            // Process tick damage for Poison
            if (eff.type == StatusEffectType::Poison && eff.tickDamagePer > 0) {
                while (eff.tickAccumulator >= eff.tickIntervalSec) {
                    eff.tickAccumulator -= eff.tickIntervalSec;

                    // Find entity and apply damage
                    auto it = std::ranges::find_if(serverRegistry,
                        [entityId](const Entity& e){ return e.id == entityId; });

                    if (it != serverRegistry.end()) {
                        it->currentHP -= static_cast<int>(eff.tickDamagePer);
                        if (it->currentHP < 0) it->currentHP = 0;

                        gEventBus.Publish(EntityDamagedEvent{
                            .targetId = entityId,
                            .sourceId = eff.sourceEntityId,
                            .newHP = it->currentHP,
                            .damage = static_cast<int>(eff.tickDamagePer)
                        });

                        if (broadcast) {
                            ByteBuffer pkt;
                            pkt.WriteUInt8(std::to_underlying(PacketType::MSG_COMBAT_NOTIFY));
                            pkt.WriteUInt32(entityId);
                            pkt.WriteUInt32(static_cast<uint32_t>(it->currentHP));
                            broadcast(std::span(pkt.data));
                        }

                        if (it->currentHP == 0) {
                            gEventBus.Publish(EntityDiedEvent{
                                .entityId = entityId,
                                .killerId = eff.sourceEntityId,
                                .monsterTemplateId = it->monsterTemplateId,
                                .originSpawnId = it->originSpawnId,
                                .x = it->transform.x,
                                .z = it->transform.z
                            });
                            toRemove.push_back({entityId, eff.type});
                            break;
                        }
                    }
                }
            }

            // Check expiration
            if (eff.elapsedSec >= eff.durationSec) {
                toRemove.push_back({entityId, eff.type});
            }
        }
    }

    // Remove expired effects
    for (const auto& [entityId, type] : toRemove) {
        if (entityStatusEffects.contains(entityId)) {
            auto& effects = entityStatusEffects[entityId];
            effects.erase(
                std::ranges::remove_if(effects,
                    [type](const StatusEffect& e){ return e.type == type; }).begin(),
                effects.end());

            if (effects.empty()) {
                entityStatusEffects.erase(entityId);
            }

            gEventBus.Publish(StatusEffectRemovedEvent{
                .targetId = entityId,
                .type = type
            });

            AddLog("[Status] {} expired on entity {}",
                   type == StatusEffectType::Poison ? "Poison" :
                   type == StatusEffectType::Slow ? "Slow" : "Stun",
                   entityId);

            if (broadcast) {
                ByteBuffer pkt;
                pkt.WriteUInt8(std::to_underlying(PacketType::MSG_STATUS_EFFECT));
                pkt.WriteUInt32(entityId);
                pkt.WriteUInt8(std::to_underlying(type));
                pkt.WriteUInt8(0); // removed
                pkt.WriteFloat(0.0f);
                pkt.WriteUInt32(0);
                broadcast(std::span(pkt.data));
            }
        }
    }
}

// =============================================================================
// NPC SYSTEM
// =============================================================================

void LoadNpcsFromCSV(std::string_view csv) {
    npcDatabase.clear();

    std::istringstream stream(std::string(csv));
    std::string line;

    // Skip header
    std::getline(stream, line);

    while (std::getline(stream, line)) {
        if (line.empty()) continue;

        std::istringstream row(line);
        std::string cell;
        std::vector<std::string> cells;

        while (std::getline(row, cell, ',')) {
            cells.push_back(cell);
        }

        if (cells.size() < 6) continue;

        try {
            NpcTemplate npc;
            npc.id = static_cast<uint32_t>(std::stoul(cells[0]));
            npc.name = cells[1];
            npc.x = std::stof(cells[2]);
            npc.z = std::stof(cells[3]);
            npc.greeting = cells[4];
            npc.offeredQuestId = static_cast<uint32_t>(std::stoul(cells[5]));

            npcDatabase[npc.id] = npc;
        } catch (...) {
            AddLog("[NPCDB] Failed to parse line: {}", line);
        }
    }

    AddLog("[NPCDB] {} NPCs loaded.", npcDatabase.size());
}

// =============================================================================
// SPAWN & RESPAWN
// =============================================================================

void ScheduleRespawn(uint32_t spawnId, uint32_t templateId,
                     float x, float z, float respawnTimeSec) {
    std::lock_guard lock(respawnMutex);

    RespawnEntry entry;
    entry.spawnPointId = spawnId;
    entry.monsterTemplateId = templateId;
    entry.x = x;
    entry.z = z;
    entry.respawnTime = respawnTimeSec;
    entry.elapsed = 0.0f;

    respawnQueue.push_back(entry);

    AddLog("[Respawn] Scheduled template {} in {:.1f}s at ({:.1f}, {:.1f})",
           templateId, respawnTimeSec, x, z);
}

void ProcessRespawnQueue(float deltaSec) {
    std::lock_guard lock(respawnMutex);

    std::vector<RespawnEntry> ready;

    for (auto it = respawnQueue.begin(); it != respawnQueue.end();) {
        it->elapsed += deltaSec;
        if (it->elapsed >= it->respawnTime) {
            ready.push_back(*it);
            it = respawnQueue.erase(it);
        } else {
            ++it;
        }
    }

    // Spawn ready monsters
    for (const auto& entry : ready) {
        Entity m;
        m.id = nextEntityId++;
        m.isMonster = true;
        m.originSpawnId = entry.spawnPointId;
        m.currentHP = 100;
        m.monsterTemplateId = entry.monsterTemplateId;
        m.transform.x = m.transform.targetX = m.transform.lerpX = entry.x;
        m.transform.z = m.transform.targetZ = m.transform.lerpZ = entry.z;
        m.transform.y = GetHeightFromGrid(entry.x, entry.z) + 0.5f;

        if (entry.monsterTemplateId == 101) {
            m.name = "Slimy";
            m.render = {"mat_slimy", 0.6f, "cube"};
        } else if (entry.monsterTemplateId == 102) {
            m.name = "Orc";
            m.render = {"mat_orc", 1.2f, "pyramid"};
        } else {
            m.name = std::format("Monster_{}", entry.monsterTemplateId);
            m.render = {"mat_default", 1.0f, "cube"};
        }

        serverRegistry.push_back(m);

        gEventBus.Publish(EntitySpawnedEvent{
            .entityId = m.id,
            .name = m.name,
            .isMonster = true,
            .x = m.transform.x,
            .z = m.transform.z,
            .materialId = m.render.materialId,
            .scaleY = m.render.scaleY,
            .meshId = m.render.meshId
        });

        AddLog("[Respawn] Spawned {} (ID:{}) at ({:.1f}, {:.1f})",
               m.name, m.id, entry.x, entry.z);
    }
}

// =============================================================================
// PASSWORD SECURITY — LEGACY (UNSAFE)
// =============================================================================
#warning "SECURITY: Argon2IdHash() is cryptographically UNSAFE. libsodium required (AP-45)"

[[deprecated("UNSAFE: Replace with libsodium crypto_pwhash_argon2id (AP-45)")]]
std::string Argon2IdHash(std::string_view password, std::string_view salt) {
    AddLog("[SECURITY] WARNING: Fake-Argon2 used! Not for production.");

    uint32_t t_cost = 2, m_cost = 1024;
    std::vector<uint32_t> memory(m_cost, 0);
    for (size_t i = 0; i < memory.size(); ++i) {
        size_t p_idx = (i * 4) % (password.size() + 1);
        size_t s_idx = (i * 4) % (salt.size() + 1);
        uint8_t b0 = p_idx < password.size() ? password[p_idx] : 0;
        uint8_t b1 = s_idx < salt.size() ? salt[s_idx] : 0;
        memory[i] = (static_cast<uint32_t>(b0) << 24) ^
                    (static_cast<uint32_t>(b1) << 16) ^
                    static_cast<uint32_t>(i);
    }
    for (uint32_t t = 0; t < t_cost; ++t) {
        for (uint32_t i = 0; i < m_cost; ++i) {
            uint32_t prev = memory[(i == 0) ? m_cost - 1 : i - 1];
            uint32_t ref = memory[(prev ^ i) % m_cost];
            uint32_t value = memory[i] ^ prev ^ ref;
            value = std::rotl(value, 13);
            memory[i] = value * 0x5bd1e995;
        }
    }
    uint32_t h0 = 0x6a09e667, h1 = 0xbb67ae85;
    for (uint32_t val : memory) { h0 ^= val; h1 = (h1 + val) * 31; }
    return std::format("{:08x}{:08x}", h0, h1);
}
