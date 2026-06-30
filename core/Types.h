#pragma once
// =============================================================================
// core/Types.h — Grundtypen, Enums und Konstanten (V13.2)
// =============================================================================
// KORREKTUR: Keine Änderungen nötig, aber vollständig dokumentiert.
// =============================================================================

#include <cstdint>
#include <string>
#include <vector>
#include <cstddef>

// =============================================================================
// FORWARD DECLARATIONS
// =============================================================================
struct Entity;
struct ClientSession;

// =============================================================================
// ENUMS
// =============================================================================
enum class PacketType : uint8_t {
    MSG_NONE = 0,
    MSG_MOVE_REQUEST = 1,
    MSG_MOVE_CONFIRM = 2,
    MSG_COMBAT_ACTION = 3,
    MSG_COMBAT_RESULT = 4,
    MSG_CHAT = 5,
    MSG_INTERACT = 6,
    MSG_SNAPSHOT = 7,
    MSG_AUTH = 8,
    MSG_DISCONNECT = 9,
};

enum class StatusEffectType : uint8_t {
    None = 0,
    Poison = 1,
    Burn = 2,
    Freeze = 3,
    Stun = 4,
    HealOverTime = 5,
};

enum class AIBehaviorType : uint8_t {
    None = 0,
    Passive = 1,
    Aggressive = 2,
    Defensive = 3,
};

// =============================================================================
// KOMPONENTEN (Legacy)
// =============================================================================
struct TransformComponent {
    float x = 0.0f, y = 0.0f, z = 0.0f;
    float targetX = 0.0f, targetZ = 0.0f;
    float lerpX = 0.0f, lerpZ = 0.0f;
    float lerpT = 0.0f;
};

struct RenderComponent {
    std::string materialId;
    float scaleY = 1.0f;
    std::string meshId;
};

struct PersistenceData {
    bool dirty = false;
    bool loaded = false;
};

struct StatusEffect {
    StatusEffectType type = StatusEffectType::None;
    float durationSec = 0.0f;
    uint32_t tickDamage = 0;
    float elapsed = 0.0f;
};

// =============================================================================
// TEMPLATES
// =============================================================================
struct SkillTemplate {
    uint32_t id = 0;
    std::string name;
    float cooldownSec = 0.0f;
    uint32_t damage = 0;
    float range = 0.0f;
};

struct QuestTemplate {
    uint32_t id = 0;
    std::string name;
    uint32_t rewardGold = 0;
    uint32_t rewardXP = 0;
    std::vector<std::string> objectives;
    uint32_t startNPC = 0;
};

struct QuestProgress {
    uint32_t questId = 0;
    uint32_t currentObjective = 0;
    bool completed = false;
};

struct NPCTemplate {
    uint32_t id = 0;
    std::string name;
    std::string dialogueText;
    std::string shopItemIds; // Komma-separiert
};

struct ItemTemplate {
    uint32_t id = 0;
    std::string name;
    uint8_t slot = 0;
    uint32_t minLevel = 0;
    uint32_t attackPower = 0;
    uint32_t defensePower = 0;
};

struct Item {
    uint32_t templateId = 0;
    uint32_t count = 0;
};

struct MonsterTemplate {
    uint32_t id = 0;
    std::string name;
    uint32_t baseHP = 100;
    uint32_t attackPower = 10;
    std::string materialId;
    std::string meshId;
    float scaleY = 1.0f;
};

struct SpawnPoint {
    float x = 0.0f, z = 0.0f;
    uint32_t monsterTemplateId = 0;
    float respawnTimeSec = 30.0f;
    float timer = 0.0f;
    bool active = true;
};

// =============================================================================
// HILFSFUNKTIONEN
// =============================================================================
[[nodiscard]] inline float GetHeightFromGrid(float x, float z) {
    (void)x; (void)z;
    // TODO: Terrain-Grid implementieren
    return 0.0f;
}
