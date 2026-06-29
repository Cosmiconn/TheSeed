// =============================================================================
// client/Interpolation.cpp — Client-Side Interpolation Implementation (AP-38)
// =============================================================================
#include "Interpolation.h"
#include "../core/Log.h"
#include <cmath>
#include <algorithm>

namespace client {

// =============================================================================
// SnapshotBuffer
// =============================================================================
void SnapshotBuffer::AddSnapshot(const net::Snapshot& snapshot, float receiveTime) {
    auto& slot = snapshots[writeIndex % MAX_SNAPSHOTS];
    slot.snapshot = snapshot;
    slot.localReceiveTime = receiveTime;
    slot.processed = false;

    writeIndex++;
    if (snapshotCount < MAX_SNAPSHOTS) snapshotCount++;
}

std::optional<net::EntityState> SnapshotBuffer::Interpolate(
    uint32_t entityId, float renderTime) const {

    auto surrounding = FindSurroundingStates(entityId, renderTime);
    if (!surrounding) return std::nullopt;

    const auto& [from, to] = *surrounding;

    // Calculate interpolation factor
    float t = 0.0f;
    if (to.serverTime > from.serverTime) {
        t = (renderTime - from.serverTime) / (to.serverTime - from.serverTime);
    }
    t = std::clamp(t, 0.0f, 1.0f);

    // Interpolate position
    net::EntityState result;
    result.entityId = entityId;
    result.fieldMask = from.fieldMask | to.fieldMask;

    if ((from.fieldMask & net::EntityState::FIELD_POS) && 
        (to.fieldMask & net::EntityState::FIELD_POS)) {
        result.posX = from.posX + (to.posX - from.posX) * t;
        result.posY = from.posY + (to.posY - from.posY) * t;
        result.posZ = from.posZ + (to.posZ - from.posZ) * t;
    } else if (to.fieldMask & net::EntityState::FIELD_POS) {
        result.posX = to.posX;
        result.posY = to.posY;
        result.posZ = to.posZ;
    }

    // Interpolate rotation (shortest path)
    if ((from.fieldMask & net::EntityState::FIELD_ROT) && 
        (to.fieldMask & net::EntityState::FIELD_ROT)) {
        float diff = to.rotY - from.rotY;
        if (diff > 3.14159f) diff -= 2.0f * 3.14159f;
        if (diff < -3.14159f) diff += 2.0f * 3.14159f;
        result.rotY = from.rotY + diff * t;
    } else if (to.fieldMask & net::EntityState::FIELD_ROT) {
        result.rotY = to.rotY;
    }

    // Use latest HP (no interpolation for discrete values)
    if (to.fieldMask & net::EntityState::FIELD_HP) {
        result.currentHP = to.currentHP;
        result.maxHP = to.maxHP;
    }

    // Use latest animation state
    if (to.fieldMask & net::EntityState::FIELD_ANIM) {
        result.animState = to.animState;
        result.animTime = to.animTime;
    }

    // Use latest status
    if (to.fieldMask & net::EntityState::FIELD_STATUS) {
        result.statusMask = to.statusMask;
    }

    return result;
}

std::optional<net::EntityState> SnapshotBuffer::GetLatestState(uint32_t entityId) const {
    // Search from newest to oldest
    for (int i = static_cast<int>(snapshotCount) - 1; i >= 0; --i) {
        size_t idx = (writeIndex - 1 - i) % MAX_SNAPSHOTS;
        const auto& snap = snapshots[idx].snapshot;

        auto it = std::ranges::find_if(snap.entities, 
            [entityId](const auto& e) { return e.entityId == entityId; });
        if (it != snap.entities.end()) {
            return *it;
        }
    }
    return std::nullopt;
}

float SnapshotBuffer::GetInterpolationDelay() const {
    if (snapshotCount < 2) return 0.1f; // Default

    // Calculate average snapshot interval
    float totalDelta = 0.0f;
    int count = 0;
    for (size_t i = 1; i < snapshotCount; ++i) {
        size_t curr = (writeIndex - i) % MAX_SNAPSHOTS;
        size_t prev = (writeIndex - i - 1) % MAX_SNAPSHOTS;
        float dt = snapshots[curr].snapshot.serverTime - snapshots[prev].snapshot.serverTime;
        if (dt > 0.0f && dt < 1.0f) { // Sanity check
            totalDelta += dt;
            count++;
        }
    }

    if (count == 0) return 0.1f;
    float avgInterval = totalDelta / count;
    return avgInterval * 3.0f; // 3 snapshot buffer
}

void SnapshotBuffer::CleanupOldSnapshots(float maxAge) {
    float cutoff = localTime - maxAge;
    for (auto& snap : snapshots) {
        if (snap.localReceiveTime < cutoff) {
            snap.processed = true; // Mark as invalid
        }
    }
}

std::optional<std::pair<net::EntityState, net::EntityState>> 
SnapshotBuffer::FindSurroundingStates(uint32_t entityId, float renderTime) const {

    std::optional<net::EntityState> from;
    std::optional<net::EntityState> to;

    for (size_t i = 0; i < snapshotCount; ++i) {
        size_t idx = (writeIndex - snapshotCount + i) % MAX_SNAPSHOTS;
        if (snapshots[idx].processed) continue;

        const auto& snap = snapshots[idx].snapshot;
        auto it = std::ranges::find_if(snap.entities,
            [entityId](const auto& e) { return e.entityId == entityId; });

        if (it != snap.entities.end()) {
            if (snap.serverTime <= renderTime) {
                from = *it;
            } else if (!to) {
                to = *it;
                break;
            }
        }
    }

    if (from && to) {
        return std::pair(*from, *to);
    }
    return std::nullopt;
}

// =============================================================================
// EntityInterpolator
// =============================================================================
EntityInterpolator::EntityInterpolator(float delay) : interpolationDelay(delay) {
    AddLog("[Interpolation] Initialized with {}ms delay", static_cast<int>(delay * 1000));
}

void EntityInterpolator::ReceiveSnapshot(const net::Snapshot& snapshot) {
    float receiveTime = snapshotBuffer.GetLocalTime();
    snapshotBuffer.AddSnapshot(snapshot, receiveTime);

    // Update interpolation delay based on jitter
    float newDelay = snapshotBuffer.GetInterpolationDelay();
    interpolationDelay = interpolationDelay * 0.9f + newDelay * 0.1f; // Smooth adaptation
}

void EntityInterpolator::Update(float deltaTime) {
    snapshotBuffer.UpdateLocalTime(deltaTime);

    float renderTime = snapshotBuffer.GetLocalTime() - interpolationDelay;

    // Get all entity IDs from latest snapshot
    auto latestIds = GetAllEntityIds();

    for (uint32_t id : latestIds) {
        auto& entity = entities[id];
        entity.entityId = id;

        // Try interpolation first
        auto interp = snapshotBuffer.Interpolate(id, renderTime);
        if (interp) {
            entity.isExtrapolating = false;
            entity.extrapolationTime = 0.0f;
            ApplySnapshotState(entity, *interp);
        } else {
            // Fall back to extrapolation
            auto latest = snapshotBuffer.GetLatestState(id);
            if (latest) {
                if (!entity.isExtrapolating) {
                    // Start extrapolating from last known position
                    entity.isExtrapolating = true;
                    entity.extrapolationTime = 0.0f;
                    ApplySnapshotState(entity, *latest);
                }
                ExtrapolateEntity(entity, deltaTime);
            }
        }

        entity.lastUpdateTime = snapshotBuffer.GetLocalTime();
    }

    // Remove stale entities (not seen in recent snapshots)
    std::vector<uint32_t> toRemove;
    for (auto& [id, entity] : entities) {
        if (snapshotBuffer.GetLocalTime() - entity.lastUpdateTime > 5.0f) {
            toRemove.push_back(id);
        }
    }
    for (uint32_t id : toRemove) {
        entities.erase(id);
    }

    // Cleanup old snapshots
    snapshotBuffer.CleanupOldSnapshots(5.0f);
}

const InterpolatedEntity* EntityInterpolator::GetEntity(uint32_t entityId) const {
    auto it = entities.find(entityId);
    if (it != entities.end()) return &it->second;
    return nullptr;
}

std::vector<uint32_t> EntityInterpolator::GetAllEntityIds() const {
    std::vector<uint32_t> ids;
    ids.reserve(entities.size());
    for (const auto& [id, _] : entities) {
        ids.push_back(id);
    }
    return ids;
}

void EntityInterpolator::RemoveEntity(uint32_t entityId) {
    entities.erase(entityId);
}

void EntityInterpolator::InterpolateEntity(InterpolatedEntity& entity, float renderTime) {
    // Position is already interpolated in SnapshotBuffer::Interpolate
    // Just smooth the result
    // (Currently a no-op since interpolation happens in buffer)
}

void EntityInterpolator::ExtrapolateEntity(InterpolatedEntity& entity, float deltaTime) {
    entity.extrapolationTime += deltaTime;

    if (entity.extrapolationTime > maxExtrapolationTime) {
        // Freeze at last known position
        entity.velX = 0.0f;
        entity.velZ = 0.0f;
        return;
    }

    // Simple velocity-based extrapolation
    entity.x += entity.velX * deltaTime;
    entity.z += entity.velZ * deltaTime;
}

void EntityInterpolator::ApplySnapshotState(InterpolatedEntity& entity, const net::EntityState& state) {
    // Calculate velocity from position change
    if (entity.lastUpdateTime > 0.0f) {
        float dt = snapshotBuffer.GetLocalTime() - entity.lastUpdateTime;
        if (dt > 0.001f) {
            entity.velX = (state.posX - entity.x) / dt;
            entity.velZ = (state.posZ - entity.z) / dt;
        }
    }

    entity.x = state.posX;
    entity.y = state.posY;
    entity.z = state.posZ;
    entity.rotY = state.rotY;
    entity.currentHP = state.currentHP;
    entity.maxHP = state.maxHP;
    entity.animState = state.animState;
    entity.animTime = state.animTime;
    entity.statusMask = state.statusMask;
}

bool EntityInterpolator::IsJitterAcceptable() const {
    return GetAverageJitter() < 0.005f; // 5ms threshold
}

float EntityInterpolator::GetAverageJitter() const {
    // Simplified jitter calculation
    // In production: track arrival time variance
    return 0.0f; // Placeholder
}

} // namespace client
