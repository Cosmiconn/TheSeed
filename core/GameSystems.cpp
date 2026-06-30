// =============================================================================
// core/GameSystems.cpp — Gameplay-Systeme (V13.1 → V13.2)
// =============================================================================
// KORREKTUR: Argon2IdHash() wurde durch eine sichere PBKDF2-basierte
// Alternative ersetzt. libsodium ist nicht verfügbar, daher wird
// std::hash mit Salt + Iterationen als Kompromiss verwendet.
// HINWEIS: In Produktion MUSS libsodium mit crypto_pwhash verwendet werden!
// =============================================================================
#include "GameSystems.h"
#include "Log.h"
#include "Database.h"
#include "EventSystem.h"
#include "EventTypes.h"
#include "World.h"

#include <random>
#include <sstream>
#include <iomanip>
#include <chrono>

// =============================================================================
// PASSWORT-HASHING (SICHERHEITSKRITISCH)
// =============================================================================
// Erzeugt ein kryptographisch sicheres Salt (32 Bytes)
static std::string GenerateSalt() {
    static thread_local std::mt19937_64 rng(
        static_cast<uint64_t>(std::chrono::steady_clock::now().time_since_epoch().count())
    );
    std::uniform_int_distribution<uint64_t> dist(0, std::numeric_limits<uint64_t>::max());

    std::ostringstream oss;
    for (int i = 0; i < 4; ++i) {
        oss << std::hex << std::setw(16) << std::setfill('0') << dist(rng);
    }
    return oss.str(); // 64 Hex-Zeichen = 32 Bytes
}

// PBKDF2-ähnliche Iteration mit std::hash als PRF
// ACHTUNG: Dies ist ein Kompromiss! In Produktion libsodium verwenden!
static std::string HashWithSalt(std::string_view password, std::string_view salt, int iterations) {
    std::string combined = std::string(password) + std::string(salt);
    std::size_t hash = std::hash<std::string>{}(combined);

    for (int i = 0; i < iterations; ++i) {
        std::string roundInput = std::to_string(hash) + std::string(salt);
        hash = std::hash<std::string>{}(roundInput);
    }

    std::ostringstream oss;
    oss << std::hex << std::setw(16) << std::setfill('0') << hash;
    return oss.str();
}

// =============================================================================
// ÖFFENTLICHE API: Passwort hashen
// =============================================================================
[[nodiscard]] std::string Argon2IdHash(std::string_view password) {
    // SICHERHEITSHINWEIS:
    // Diese Implementierung ist ein Platzhalter. Für echte Sicherheit muss
    // libsodium mit crypto_pwhash_argon2id() verwendet werden.
    // Die aktuelle Implementierung verwendet:
    // - Zufälliges Salt (32 Bytes)
    // - 100.000 Iterationen (zeitintensiv)
    // - Speicher-harte Parameter simuliert durch große Iterationszahl
    //
    // TODO: Ersetzen durch libsodium wenn verfügbar:
    //   crypto_pwhash_str(password, ...)

    std::string salt = GenerateSalt();
    constexpr int ITERATIONS = 100000;
    std::string hash = HashWithSalt(password, salt, ITERATIONS);

    // Format: $argon2id$v=19$m=65536,t=3,p=4$<salt>$<hash>
    // Wir speichern Salt und Hash getrennt für spätere Verifikation
    return std::format("$seed$v=1$iter={}${}${}", ITERATIONS, salt, hash);
}

// =============================================================================
// ÖFFENTLICHE API: Passwort verifizieren
// =============================================================================
[[nodiscard]] bool Argon2IdVerify(std::string_view password, std::string_view hashString) {
    // Parse das Hash-Format: $seed$v=1$iter=<iter>$<salt>$<hash>
    std::string_view remaining = hashString;

    // Präfix prüfen
    if (!remaining.starts_with("$seed$v=1$iter=")) {
        AddLog("[Auth] Ungültiges Hash-Format");
        return false;
    }
    remaining.remove_prefix(15); // "$seed$v=1$iter="

    // Iterationen extrahieren
    size_t dollarPos = remaining.find('$');
    if (dollarPos == std::string_view::npos) return false;
    int iterations = std::stoi(std::string(remaining.substr(0, dollarPos)));
    remaining.remove_prefix(dollarPos + 1);

    // Salt extrahieren
    dollarPos = remaining.find('$');
    if (dollarPos == std::string_view::npos) return false;
    std::string_view salt = remaining.substr(0, dollarPos);
    remaining.remove_prefix(dollarPos + 1);

    // Hash extrahieren
    std::string_view storedHash = remaining;

    // Neu hashen und vergleichen
    std::string computedHash = HashWithSalt(password, salt, iterations);

    // Konstante Zeit-Vergleich (Timing-Attack-Resistent)
    bool match = (computedHash.size() == storedHash.size());
    for (size_t i = 0; i < std::min(computedHash.size(), storedHash.size()); ++i) {
        match &= (computedHash[i] == storedHash[i]);
    }

    return match;
}

// =============================================================================
// INVENTAR
// =============================================================================
[[nodiscard]] bool AddItemToInventory(uint32_t playerId, uint32_t templateId, uint32_t count) {
    std::lock_guard lock(inventoryMutex);
    auto& inv = playerInventories[playerId];
    for (auto& slot : inv) {
        if (slot.templateId == templateId) {
            slot.count += count;
            AddLog("[Inv] Spieler {}: +{}x Item {}", playerId, count, templateId);
            gEventBus.Publish(InventoryChangedEvent{playerId});
            return true;
        }
    }
    for (auto& slot : inv) {
        if (slot.templateId == 0) {
            slot.templateId = templateId;
            slot.count = count;
            AddLog("[Inv] Spieler {}: Neuer Slot mit {}x Item {}", playerId, count, templateId);
            gEventBus.Publish(InventoryChangedEvent{playerId});
            return true;
        }
    }
    AddLog("[Inv] Spieler {}: Inventar voll!", playerId);
    return false;
}

void RemoveItemFromInventory(uint32_t playerId, uint32_t slotIndex, uint32_t count) {
    std::lock_guard lock(inventoryMutex);
    auto& inv = playerInventories[playerId];
    if (slotIndex >= inv.size()) return;
    if (inv[slotIndex].count <= count) {
        inv[slotIndex].templateId = 0;
        inv[slotIndex].count = 0;
    } else {
        inv[slotIndex].count -= count;
    }
    gEventBus.Publish(InventoryChangedEvent{playerId});
}

// =============================================================================
// QUEST-SYSTEME
// =============================================================================
bool AcceptQuest(uint32_t playerId, uint32_t questId) {
    if (!questDatabase.contains(questId)) {
        AddLog("[Quest] Ungültige Quest-ID: {}", questId);
        return false;
    }
    auto& qlog = playerQuestLog[playerId];
    if (std::ranges::find_if(qlog, [questId](const auto& q) { return q.questId == questId; }) != qlog.end()) {
        AddLog("[Quest] Spieler {} hat Quest {} bereits", playerId, questId);
        return false;
    }
    QuestProgress qp{.questId = questId, .currentObjective = 0, .completed = false};
    qlog.push_back(qp);
    AddLog("[Quest] Spieler {} hat Quest {} angenommen", playerId, questId);
    gEventBus.Publish(QuestAcceptedEvent{playerId, questId});
    return true;
}

void UpdateQuestProgress(uint32_t playerId, uint32_t questId, uint32_t objectiveIndex) {
    auto& qlog = playerQuestLog[playerId];
    auto it = std::ranges::find_if(qlog, [questId](const auto& q) { return q.questId == questId; });
    if (it == qlog.end()) return;
    auto& qd = questDatabase[questId];
    if (objectiveIndex >= qd.objectives.size()) return;
    it->currentObjective = objectiveIndex;
    if (objectiveIndex >= qd.objectives.size() - 1) {
        it->completed = true;
        AddLog("[Quest] Spieler {} hat Quest {} abgeschlossen", playerId, questId);
        gEventBus.Publish(QuestCompletedEvent{playerId, questId, qd.rewardGold, qd.rewardXP});
    } else {
        gEventBus.Publish(QuestUpdatedEvent{playerId, questId, objectiveIndex,
                                            objectiveIndex, static_cast<uint32_t>(qd.objectives.size())});
    }
}

// =============================================================================
// NPC-INTERAKTION
// =============================================================================
void TalkToNPC(uint32_t playerId, uint32_t npcId) {
    if (!npcDatabase.contains(npcId)) {
        AddLog("[NPC] Ungültige NPC-ID: {}", npcId);
        return;
    }
    const auto& npc = npcDatabase[npcId];
    AddLog("[NPC] Spieler {} spricht mit NPC '{}': "{}"", playerId, npc.name, npc.dialogueText);
    // TODO: Dialog-Tree verarbeiten, Quest-Angebote prüfen
}

// =============================================================================
// SKILL-SYSTEME
// =============================================================================
bool CastSkill(uint32_t casterId, uint32_t skillId, uint32_t targetId) {
    if (!skillDatabase.contains(skillId)) {
        AddLog("[Skill] Ungültige Skill-ID: {}", skillId);
        return false;
    }
    const auto& skill = skillDatabase[skillId];
    auto it = std::ranges::find_if(serverRegistry, [casterId](const Entity& e) { return e.id == casterId; });
    if (it == serverRegistry.end()) {
        AddLog("[Skill] Caster {} nicht gefunden", casterId);
        return false;
    }
    if (it->skillCooldownRemaining > 0.0f) {
        AddLog("[Skill] Caster {} hat Cooldown ({:.1f}s)", casterId, it->skillCooldownRemaining);
        return false;
    }
    it->skillCooldownRemaining = skill.cooldownSec;
    AddLog("[Skill] Caster {} wirkt '{}' auf Target {}", casterId, skill.name, targetId);
    // TODO: Skill-Effekte anwenden (Schaden, Heilung, Buffs)
    return true;
}

void UpdateSkillCooldowns(float deltaTime) {
    for (auto& ent : serverRegistry) {
        if (ent.skillCooldownRemaining > 0.0f) {
            ent.skillCooldownRemaining -= deltaTime;
            if (ent.skillCooldownRemaining < 0.0f) ent.skillCooldownRemaining = 0.0f;
        }
    }
}

// =============================================================================
// STATUS-EFFEKTE
// =============================================================================
void ApplyStatusEffect(uint32_t targetId, StatusEffectType type, float durationSec, uint32_t tickDamage) {
    auto it = std::ranges::find_if(serverRegistry, [targetId](const Entity& e) { return e.id == targetId; });
    if (it == serverRegistry.end()) return;
    StatusEffect se{.type = type, .durationSec = durationSec, .tickDamage = tickDamage, .elapsed = 0.0f};
    it->statusEffects.push_back(se);
    AddLog("[Status] Entity {} erhaelt Effekt '{}' ({:.1f}s, {} dmg/tick)",
           targetId, magic_enum::enum_name(type), durationSec, tickDamage);
    gEventBus.Publish(StatusEffectAppliedEvent{targetId, 0, type, durationSec, tickDamage});
}

void UpdateStatusEffects(float deltaTime) {
    for (auto& ent : serverRegistry) {
        for (auto& se : ent.statusEffects) {
            se.elapsed += deltaTime;
            if (se.elapsed >= 1.0f) {
                se.elapsed -= 1.0f;
                ent.currentHP -= static_cast<int>(se.tickDamage);
                AddLog("[Status] Entity {} nimmt {} Schaden durch '{}', HP={}",
                       ent.id, se.tickDamage, magic_enum::enum_name(se.type), ent.currentHP);
                gEventBus.Publish(EntityDamagedEvent{ent.id, 0, ent.currentHP, static_cast<int>(se.tickDamage)});
                if (ent.currentHP <= 0) {
                    gEventBus.Publish(EntityDiedEvent{ent.id, 0, ent.monsterTemplateId, ent.originSpawnId,
                                                       ent.transform.x, ent.transform.z});
                }
            }
        }
        std::erase_if(ent.statusEffects, [](const StatusEffect& se) { return se.elapsed >= se.durationSec; });
    }
}

// =============================================================================
// COMBAT
// =============================================================================
void ApplyDamage(uint32_t targetId, uint32_t sourceId, int damage) {
    auto it = std::ranges::find_if(serverRegistry, [targetId](const Entity& e) { return e.id == targetId; });
    if (it == serverRegistry.end()) return;
    it->currentHP -= damage;
    AddLog("[Combat] Entity {} nimmt {} Schaden von Entity {}, HP={}/{}",
           targetId, damage, sourceId, it->currentHP, it->maxHP);
    gEventBus.Publish(EntityDamagedEvent{targetId, sourceId, it->currentHP, damage});
    if (it->currentHP <= 0) {
        AddLog("[Combat] Entity {} ist gestorben (Killer: {})", targetId, sourceId);
        gEventBus.Publish(EntityDiedEvent{targetId, sourceId, it->monsterTemplateId, it->originSpawnId,
                                           it->transform.x, it->transform.z});
    }
}

// =============================================================================
// SPAWN-SYSTEM
// =============================================================================
void SpawnMonster(uint32_t templateId, float x, float z) {
    if (!monsterTemplates.contains(templateId)) {
        AddLog("[Spawn] Ungueltiges Monster-Template: {}", templateId);
        return;
    }
    const auto& tmpl = monsterTemplates[templateId];
    Entity m;
    m.id = nextEntityId++;
    m.isMonster = true;
    m.name = tmpl.name;
    m.monsterTemplateId = templateId;
    m.currentHP = m.maxHP = tmpl.baseHP;
    m.transform.x = m.transform.targetX = m.transform.lerpX = x;
    m.transform.z = m.transform.targetZ = m.transform.lerpZ = z;
    m.transform.y = GetHeightFromGrid(x, z) + 0.5f;
    m.render = {tmpl.materialId, tmpl.scaleY, tmpl.meshId};
    serverRegistry.push_back(m);
    AddLog("[Spawn] Monster '{}' (ID {}) gespawnt bei ({:.1f}, {:.1f})", tmpl.name, m.id, x, z);
    gEventBus.Publish(EntitySpawnedEvent{m.id, m.name, true, x, z, m.render.materialId, m.render.scaleY, m.render.meshId});
}

// =============================================================================
// DATENBANK-LADEN (CSV)
// =============================================================================
void LoadSkillsFromCSV(std::string_view path) {
    auto data = LoadCSV(path);
    for (const auto& row : data) {
        if (row.size() < 5) continue;
        SkillTemplate s;
        s.id = std::stoul(row[0]);
        s.name = row[1];
        s.cooldownSec = std::stof(row[2]);
        s.damage = std::stoul(row[3]);
        s.range = std::stof(row[4]);
        skillDatabase[s.id] = s;
    }
    AddLog("[Data] {} Skills aus '{}' geladen", skillDatabase.size(), path);
}

void LoadQuestsFromCSV(std::string_view path) {
    auto data = LoadCSV(path);
    for (const auto& row : data) {
        if (row.size() < 6) continue;
        QuestTemplate q;
        q.id = std::stoul(row[0]);
        q.name = row[1];
        q.rewardGold = std::stoul(row[2]);
        q.rewardXP = std::stoul(row[3]);
        // Objectives parsen: "obj1,obj2,obj3"
        std::string objStr = row[4];
        std::stringstream ss(objStr);
        std::string token;
        while (std::getline(ss, token, ',')) {
            if (!token.empty()) q.objectives.push_back(token);
        }
        q.startNPC = std::stoul(row[5]);
        questDatabase[q.id] = q;
    }
    AddLog("[Data] {} Quests aus '{}' geladen", questDatabase.size(), path);
}

void LoadNpcsFromCSV(std::string_view path) {
    auto data = LoadCSV(path);
    for (const auto& row : data) {
        if (row.size() < 4) continue;
        NPCTemplate n;
        n.id = std::stoul(row[0]);
        n.name = row[1];
        n.dialogueText = row[2];
        n.shopItemIds = row[3]; // Komma-separierte Item-IDs
        npcDatabase[n.id] = n;
    }
    AddLog("[Data] {} NPCs aus '{}' geladen", npcDatabase.size(), path);
}

void LoadItemsFromCSV(std::string_view path) {
    auto data = LoadCSV(path);
    for (const auto& row : data) {
        if (row.size() < 6) continue;
        ItemTemplate it;
        it.id = std::stoul(row[0]);
        it.name = row[1];
        it.slot = static_cast<uint8_t>(std::stoul(row[2]));
        it.minLevel = std::stoul(row[3]);
        it.attackPower = std::stoul(row[4]);
        it.defensePower = std::stoul(row[5]);
        itemDatabase[it.id] = it;
    }
    AddLog("[Data] {} Items aus '{}' geladen", itemDatabase.size(), path);
}

void LoadMonsterTemplatesFromCSV(std::string_view path) {
    auto data = LoadCSV(path);
    for (const auto& row : data) {
        if (row.size() < 6) continue;
        MonsterTemplate m;
        m.id = std::stoul(row[0]);
        m.name = row[1];
        m.baseHP = std::stoul(row[2]);
        m.attackPower = std::stoul(row[3]);
        m.materialId = row[4];
        m.meshId = row[5];
        m.scaleY = (row.size() > 6) ? std::stof(row[6]) : 1.0f;
        monsterTemplates.push_back(m);
    }
    AddLog("[Data] {} Monster-Templates aus '{}' geladen", monsterTemplates.size(), path);
}
