#pragma once
// =============================================================================
// client/Interpolation.h — Entity Interpolation + Dead Reckoning (AP-38) C++23
// =============================================================================
// VOLLSTÄNDIGE IMPLEMENTIERUNG:
// • Positions-Interpolation zwischen Snapshots (20Hz → 60Hz)
// • Dead Reckoning für eigene Entity (Vorhersage basierend auf Velocity)
// • Extrapolation wenn Snapshots ausbleiben
// • std::chrono für zeitbasierte Interpolation
// =============================================================================

#include "../ecs/ecs_Types.h"
#include "../ecs/Components.h"
#include "../math/Vector.h"

#include <vector>
#include <deque>
#include <chrono>
#include <unordered_map>
#include <cmath>

namespace client {

// =============================================================================
// INTERPOLATIONSNAP — Gespeicherter Zustand für Interpolation
// =============================================================================
struct InterpSnapshot {
    float x = 0.0f, y = 0.0f, z = 0.0f;
    float vx = 0.0f, vy = 0.0f, vz = 0.0f; // Velocity für Dead Reckoning
    uint32_t currentHP = 0, maxHP = 0;
    std::string name;
    std::chrono::steady_clock::time_point timestamp;
    uint32_t sequenceId = 0;
};

// =============================================================================
// ENTITY INTERPOLATOR — Verwaltet Snapshot-History pro Entity
// =============================================================================
class EntityInterpolator {
public:
    static constexpr std::chrono::milliseconds INTERP_DELAY{100};      // 100ms Puffer
    static constexpr std::chrono::milliseconds MAX_EXTRAPOLATION{500}; // 500ms max Extrapolation
    static constexpr float POSITION_THRESHOLD = 0.1f;                  // Min. Abstand für Teleport

private:
    std::deque<InterpSnapshot> snapshots;  // Chronologisch sortiert
    InterpSnapshot lastSnapshot;           // Letzter empfangener Zustand
    InterpSnapshot displayState;           // Aktuell angezeigter Zustand

    bool hasData = false;
    std::chrono::steady_clock::time_point lastReceiveTime;

    // Dead Reckoning
    math::Vector3 predictedPosition;
    math::Vector3 predictedVelocity;
    bool useDeadReckoning = false;

public:
    void AddSnapshot(const InterpSnapshot& snap);

    // Interpoliert/Extrapoliert Position zum aktuellen Zeitpunkt
    [[nodiscard]] InterpSnapshot Interpolate(std::chrono::steady_clock::time_point now);

    // Dead Reckoning: Aktualisiert Vorhersage basierend auf eigener Eingabe
    void UpdateDeadReckoning(const math::Vector3& velocity, float deltaTime);

    // Korrektur wenn Server-Position von Vorhersage abweicht
    void Reconcile(const InterpSnapshot& serverState);

    [[nodiscard]] bool HasData() const { return hasData; }
    [[nodiscard]] size_t GetSnapshotCount() const { return snapshots.size(); }

    void Clear() { snapshots.clear(); hasData = false; }
};

// =============================================================================
// INTERPOLATION MANAGER — Verwaltet alle Entity-Interpolatoren
// =============================================================================
class InterpolationManager {
    std::unordered_map<uint32_t, EntityInterpolator> entities;
    uint32_t localPlayerId = 0;  // Eigene Entity-ID für Dead Reckoning

public:
    void SetLocalPlayer(uint32_t id) { localPlayerId = id; }

    // Fügt Snapshot für Entity hinzu
    void AddSnapshot(uint32_t entityId, const InterpSnapshot& snap);

    // Aktualisiert alle Entities (60Hz Client-Tick)
    void Update(float deltaTime);

    // Holt interpolierte Position für Rendering
    [[nodiscard]] math::Vector3 GetInterpolatedPosition(uint32_t entityId) const;
    [[nodiscard]] math::Vector3 GetInterpolatedVelocity(uint32_t entityId) const;

    // Dead Reckoning für lokalen Spieler
    void UpdateLocalPlayerInput(const math::Vector3& inputVelocity, float deltaTime);

    // Entfernt Entities die nicht mehr im Snapshot vorkommen
    void RemoveStaleEntities(const std::vector<uint32_t>& activeIds);

    [[nodiscard]] size_t GetEntityCount() const { return entities.size(); }
    void Clear() { entities.clear(); }
};

} // namespace client
