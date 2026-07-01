#pragma once
// =============================================================================
// server/combat/CombatSystem.h — Erweitertes Combat-System (AP-53) C++23
// =============================================================================
// VOLLSTÄNDIGE IMPLEMENTIERUNG:
// • Hitbox-System (AABB, Sphere, Capsule)
// • Combo-System (Chain-Attacks, Timing-Windows)
// • Directional Blocking (Winkel-basiert)
// • Damage-Typen (Physical, Fire, Ice, Lightning, Poison)
// • Status-Effekte (Stun, Slow, Burn, Freeze)
// • Aggro & Threat-Table (AP-51)
// • std::expected für Fehlerbehandlung
// =============================================================================

#include "../../ecs/ecs_EcsWorld.h"
#include "../../ecs/Components.h"
#include "../../math/Vector.h"
#include "../../core/Log.h"

#include <vector>
#include <span>
#include <expected>
#include <chrono>
#include <unordered_map>
#include <random>

namespace combat {

// =============================================================================
// DAMAGE TYPES
// =============================================================================
enum class DamageType : uint8_t {
    Physical = 0,
    Fire = 1,
    Ice = 2,
    Lightning = 3,
    Poison = 4,
    Holy = 5,
    Dark = 6,
    Count
};

// =============================================================================
// HITBOX SHAPES
// =============================================================================
struct HitboxAABB {
    math::Vector3 min, max;

    [[nodiscard]] bool Contains(const math::Vector3& point) const {
        return point.x >= min.x && point.x <= max.x &&
               point.y >= min.y && point.y <= max.y &&
               point.z >= min.z && point.z <= max.z;
    }

    [[nodiscard]] bool Intersects(const HitboxAABB& other) const {
        return min.x <= other.max.x && max.x >= other.min.x &&
               min.y <= other.max.y && max.y >= other.min.y &&
               min.z <= other.max.z && max.z >= other.min.z;
    }
};

struct HitboxSphere {
    math::Vector3 center;
    float radius = 0.0f;

    [[nodiscard]] bool Contains(const math::Vector3& point) const {
        return (point - center).LengthSquared() <= radius * radius;
    }
};

struct HitboxCapsule {
    math::Vector3 a, b;  // Endpunkte
    float radius = 0.0f;
};

// =============================================================================
// ATTACK DATA
// =============================================================================
struct AttackData {
    float baseDamage = 10.0f;
    DamageType damageType = DamageType::Physical;

    // Hitbox
    HitboxAABB hitboxAABB;
    HitboxSphere hitboxSphere;
    bool useSphere = false;  // True = Sphere, False = AABB

    // Timing
    float windupTime = 0.3f;      // Aufladezeit
    float activeTime = 0.1f;      // Aktive Hitbox-Zeit
    float recoveryTime = 0.4f;    // Recovery

    // Combo
    uint8_t comboStep = 0;        // 0 = kein Combo
    uint8_t maxComboSteps = 3;
    float comboWindow = 0.5f;     // Zeit für nächsten Combo-Schritt
    float comboDamageMultiplier = 1.2f;

    // Direction
    math::Vector3 direction;       // Angriffsrichtung
    float coneAngle = 60.0f;       // Kegel-Winkel für Directional-Hit

    // Effects
    bool canBeBlocked = true;
    bool canBeParried = true;
    float knockbackForce = 0.0f;

    // Status-Effekt
    struct StatusEffectChance {
        StatusEffectType type;
        float chance;              // 0.0–1.0
        float duration;
        float tickDamage;
    };
    std::vector<StatusEffectChance> statusEffects;
};

// =============================================================================
// COMBO STATE
// =============================================================================
struct ComboState {
    uint8_t currentStep = 0;
    float damageMultiplier = 1.0f;
    std::chrono::steady_clock::time_point lastAttackTime;
    bool isInCombo = false;

    void Reset() {
        currentStep = 0;
        damageMultiplier = 1.0f;
        isInCombo = false;
    }
};

// =============================================================================
// THREAT ENTRY (für Aggro-System)
// =============================================================================
struct ThreatEntry {
    ecs::EntityHandle source;
    float threat = 0.0f;
    std::chrono::steady_clock::time_point lastUpdate;

    bool operator>(const ThreatEntry& other) const {
        return threat > other.threat;
    }
};

// =============================================================================
// AGGRO TABLE (AP-51)
// =============================================================================
class AggroTable {
    std::vector<ThreatEntry> entries;
    ecs::EntityHandle owner;
    float decayRate = 5.0f;  // Threat-Decay pro Sekunde

public:
    explicit AggroTable(ecs::EntityHandle entity) : owner(entity) {}

    void AddThreat(ecs::EntityHandle source, float amount);
    void DecayThreat(float deltaTime);

    [[nodiscard]] ecs::EntityHandle GetTopThreat() const;
    [[nodiscard]] float GetThreat(ecs::EntityHandle source) const;
    [[nodiscard]] bool HasTarget() const { return !entries.empty(); }

    void RemoveSource(ecs::EntityHandle source);
    void Clear() { entries.clear(); }

    [[nodiscard]] std::span<const ThreatEntry> GetEntries() const { 
        return std::span(entries); 
    }
};

// =============================================================================
// COMBAT SYSTEM
// =============================================================================
class CombatSystem {
    ecs::EcsWorld* world = nullptr;

    // Combo-States pro Entity
    std::unordered_map<uint32_t, ComboState> comboStates;

    // Aggro-Tables pro Entity
    std::unordered_map<uint32_t, std::unique_ptr<AggroTable>> aggroTables;

    // Aktive Attacken (Entity → AttackData + Timer)
    struct ActiveAttack {
        AttackData data;
        ecs::EntityHandle attacker;
        std::chrono::steady_clock::time_point startTime;
        enum Phase { Windup, Active, Recovery } phase = Phase::Windup;
    };
    std::vector<ActiveAttack> activeAttacks;

public:
    explicit CombatSystem(ecs::EcsWorld* ecsWorld) : world(ecsWorld) {}

    // Angriff starten
    [[nodiscard]] std::expected<void, std::string> StartAttack(
        ecs::EntityHandle attacker, const AttackData& data);

    // Blockieren
    [[nodiscard]] bool TryBlock(ecs::EntityHandle defender, 
                                 const math::Vector3& attackDirection);

    // Parieren
    [[nodiscard]] bool TryParry(ecs::EntityHandle defender,
                                 ecs::EntityHandle attacker,
                                 float timingWindow = 0.2f);

    // Schaden anwenden
    void ApplyDamage(ecs::EntityHandle target, 
                     ecs::EntityHandle source,
                     float damage,
                     DamageType type);

    // Aggro
    void AddThreat(ecs::EntityHandle npc, ecs::EntityHandle player, float threat);
    [[nodiscard]] ecs::EntityHandle GetTopThreatTarget(ecs::EntityHandle npc);

    // Update (Server-Tick)
    void Update(float deltaTime);

    // Combo-Query
    [[nodiscard]] ComboState* GetComboState(ecs::EntityHandle entity);
    [[nodiscard]] AggroTable* GetAggroTable(ecs::EntityHandle entity);

private:
    void ProcessActiveAttacks(float deltaTime);
    void CheckHitboxes(ActiveAttack& attack);
    [[nodiscard]] float CalculateDamageReduction(ecs::EntityHandle target, 
                                                  DamageType type) const;
    [[nodiscard]] bool IsInFront(const math::Vector3& defenderPos,
                                  const math::Vector3& defenderForward,
                                  const math::Vector3& attackerPos,
                                  float coneAngle) const;
};

} // namespace combat
