#pragma once
// =============================================================================
// core/Types.h — Grundtypen, Enums und Konstanten (V13.2)
// =============================================================================
// KORREKTUR: Keine Änderungen nötig, aber vollständig dokumentiert.
// =============================================================================

#include <cstdint>
#include <cmath>  // FIX TERRAIN-1
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
// FIX TERRAIN-1: Einfaches Terrain-Grid mit Perlin-Noise-ähnlicher Funktion
inline float Noise2D(float x, float z) {
    // Simple value noise for terrain generation
    int ix = static_cast<int>(std::floor(x));
    int iz = static_cast<int>(std::floor(z));
    float fx = x - ix;
    float fz = z - iz;

    // Smooth interpolation
    float u = fx * fx * (3.0f - 2.0f * fx);
    float v = fz * fz * (3.0f - 2.0f * fz);

    // Pseudo-random hash
    auto hash = [](int x, int z) -> float {
        uint32_t h = static_cast<uint32_t>(x * 374761393u + z * 668265263u);
        h = (h ^ (h >> 13)) * 1274126177u;
        return static_cast<float>(h) / static_cast<float>(UINT32_MAX);
    };

    float n00 = hash(ix, iz);
    float n10 = hash(ix + 1, iz);
    float n01 = hash(ix, iz + 1);
    float n11 = hash(ix + 1, iz + 1);

    return n00 * (1.0f - u) * (1.0f - v) +
           n10 * u * (1.0f - v) +
           n01 * (1.0f - u) * v +
           n11 * u * v;
}

inline float FbmNoise(float x, float z, int octaves = 4) {
    float value = 0.0f;
    float amplitude = 1.0f;
    float frequency = 0.01f;
    float maxValue = 0.0f;

    for (int i = 0; i < octaves; ++i) {
        value += amplitude * Noise2D(x * frequency, z * frequency);
        maxValue += amplitude;
        amplitude *= 0.5f;
        frequency *= 2.0f;
    }

    return value / maxValue;
}

[[nodiscard]] inline float GetHeightFromGrid(float x, float z) {
    // FIX TERRAIN-1: Prozedurales Terrain mit mehreren Ebenen
    float baseHeight = FbmNoise(x, z, 4) * 50.0f;      // Hügel (0-50m)
    float detail = FbmNoise(x + 100.0f, z + 100.0f, 2) * 5.0f;  // Kleine Details
    float mountains = std::pow(FbmNoise(x * 0.5f, z * 0.5f, 3), 2.0f) * 100.0f; // Berge

    return baseHeight + detail + mountains;
}
