// =============================================================================
// client/Interpolation.cpp — Entity Interpolation + Dead Reckoning (P5-FIX)
// =============================================================================
// KORREKTUR P5:
// • Alle fehlenden Includes ergaenzt (<algorithm>, <cmath>, <ranges>)
// • Client-Prediction fuer lokale Bewegung implementiert
// • Server Reconciliation mit sanfter Korrektur
// • Extrapolation mit Velocity-Beschraenkung
// =============================================================================
#include "Interpolation.h"
#include "../core/Log.h"

#include <algorithm>
#include <cmath>
#include <ranges>

namespace client {

// =============================================================================
// ENTITY INTERPOLATOR
// =============================================================================

void EntityInterpolator::AddSnapshot(const InterpSnapshot& snap) {
    // Entferne alte Snapshots (aelter als 2 Sekunden)
    auto cutoff = snap.timestamp - std::chrono::seconds(2);
    std::erase_if(snapshots, [&cutoff](const auto& s) {
        return s.timestamp < cutoff;
    });

    // Fuege neuen Snapshot ein (chronologisch sortiert)
    auto it = std::ranges::upper_bound(snapshots, snap.timestamp, {},
        &InterpSnapshot::timestamp);
    snapshots.insert(it, snap);

    lastSnapshot = snap;
    lastReceiveTime = std::chrono::steady_clock::now();
    hasData = true;

    // Dead Reckoning: Aktualisiere Vorhersage mit Server-Daten
    predictedPosition = math::Vector3(snap.x, snap.y, snap.z);
    predictedVelocity = math::Vector3(snap.vx, snap.vy, snap.vz);
    useDeadReckoning = true;
}

InterpSnapshot EntityInterpolator::Interpolate(std::chrono::steady_clock::time_point now) {
    if (!hasData) return InterpSnapshot{};
    if (snapshots.size() < 2) return lastSnapshot;

    auto renderTime = now - INTERP_DELAY;

    // Finde zwei Snapshots um renderTime herum
    auto it = std::ranges::lower_bound(snapshots, renderTime, {},
        &InterpSnapshot::timestamp);

    if (it == snapshots.begin()) {
        // Zu frueh → ersten Snapshot verwenden
        return snapshots.front();
    }
    if (it == snapshots.end()) {
        // Zu spaet → Extrapolation
        auto& newest = snapshots.back();
        auto timeSinceNewest = std::chrono::duration<float>(now - newest.timestamp).count();

        if (timeSinceNewest * 1000.0f > MAX_EXTRAPOLATION.count()) {
            // Zu lange keine Daten → einfrieren
            return newest;
        }

        // Lineare Extrapolation mit Velocity-Beschraenkung
        InterpSnapshot result = newest;
        float maxExtrapDist = 5.0f; // Max. 5m Extrapolation
        float extrapX = newest.vx * timeSinceNewest;
        float extrapZ = newest.vz * timeSinceNewest;
        float extrapDist = std::sqrt(extrapX * extrapX + extrapZ * extrapZ);

        if (extrapDist > maxExtrapDist) {
            float scale = maxExtrapDist / extrapDist;
            extrapX *= scale;
            extrapZ *= scale;
        }

        result.x += extrapX;
        result.y += newest.vy * timeSinceNewest;
        result.z += extrapZ;
        return result;
    }

    // Interpolation zwischen zwei Snapshots
    auto& newer = *it;
    auto& older = *(it - 1);

    auto newerTime = std::chrono::duration<float>(newer.timestamp.time_since_epoch()).count();
    auto olderTime = std::chrono::duration<float>(older.timestamp.time_since_epoch()).count();
    auto renderTimeSec = std::chrono::duration<float>(renderTime.time_since_epoch()).count();

    float t = (renderTimeSec - olderTime) / (newerTime - olderTime);
    t = std::clamp(t, 0.0f, 1.0f);

    InterpSnapshot result;
    result.x = older.x + (newer.x - older.x) * t;
    result.y = older.y + (newer.y - older.y) * t;
    result.z = older.z + (newer.z - older.z) * t;
    result.vx = newer.vx;
    result.vy = newer.vy;
    result.vz = newer.vz;
    result.currentHP = newer.currentHP;
    result.maxHP = newer.maxHP;
    result.name = newer.name;
    result.sequenceId = newer.sequenceId;
    result.timestamp = renderTime;

    return result;
}

void EntityInterpolator::UpdateDeadReckoning(const math::Vector3& velocity, float deltaTime) {
    predictedPosition = predictedPosition + velocity * deltaTime;
    predictedVelocity = velocity;
}

void EntityInterpolator::Reconcile(const InterpSnapshot& serverState) {
    math::Vector3 serverPos(serverState.x, serverState.y, serverState.z);
    float error = (serverPos - predictedPosition).Length();

    if (error > POSITION_THRESHOLD) {
        // Sanfte Korrektur (nicht hartes Teleportieren)
        constexpr float RECONCILE_SPEED = 10.0f;
        math::Vector3 diff = serverPos - predictedPosition;
        float reconcileAmount = std::min(error, RECONCILE_SPEED * 0.016f); // ~60Hz

        if (error > 0.0f) {
            predictedPosition = predictedPosition + diff.Normalized() * reconcileAmount;
        }

        AddLog("[Interp] Reconcile: error={:.2f}m, corrected={:.2f}m", error, reconcileAmount);
    }
}

// =============================================================================
// INTERPOLATION MANAGER
// =============================================================================

void InterpolationManager::AddSnapshot(uint32_t entityId, const InterpSnapshot& snap) {
    entities[entityId].AddSnapshot(snap);
}

void InterpolationManager::Update(float deltaTime) {
    auto now = std::chrono::steady_clock::now();

    for (auto& [id, interpolator] : entities) {
        if (!interpolator.HasData()) continue;

        auto state = interpolator.Interpolate(now);

        // Speichere interpolierten Zustand fuer Rendering
        // (In echtem Renderer wuerde hier die Transform aktualisiert werden)
        (void)state; // Verwendet in GetInterpolatedPosition
    }
}

math::Vector3 InterpolationManager::GetInterpolatedPosition(uint32_t entityId) const {
    auto it = entities.find(entityId);
    if (it == entities.end()) return math::Vector3{};

    auto state = it->second.Interpolate(std::chrono::steady_clock::now());
    return math::Vector3(state.x, state.y, state.z);
}

math::Vector3 InterpolationManager::GetInterpolatedVelocity(uint32_t entityId) const {
    auto it = entities.find(entityId);
    if (it == entities.end()) return math::Vector3{};

    auto state = it->second.Interpolate(std::chrono::steady_clock::now());
    return math::Vector3(state.vx, state.vy, state.vz);
}

void InterpolationManager::UpdateLocalPlayerInput(const math::Vector3& inputVelocity,
    float deltaTime) {
    if (localPlayerId == 0) return;

    auto it = entities.find(localPlayerId);
    if (it != entities.end()) {
        it->second.UpdateDeadReckoning(inputVelocity, deltaTime);
    }
}

void InterpolationManager::RemoveStaleEntities(const std::vector<uint32_t>& activeIds) {
    std::unordered_set<uint32_t> activeSet(activeIds.begin(), activeIds.end());

    std::erase_if(entities, [&activeSet](const auto& pair) {
        return !activeSet.contains(pair.first);
    });
}

} // namespace client
