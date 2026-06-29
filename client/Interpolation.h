#pragma once
// =============================================================================
// client/Interpolation.h — Client-Side Snapshot Interpolation (AP-38)
// Smooth 60fps rendering from 20Hz server snapshots
// =============================================================================
#include "../server/network/Snapshot.h"
#include "../ecs/Components.h"
#include <cstdint>
#include <array>
#include <optional>
#include <vector>

namespace client {

// =============================================================================
// Interpolated Entity State
// =============================================================================
struct InterpolatedEntity {
    uint32_t entityId = 0;

    // Current interpolated position
    float x = 0.0f, y = 0.0f, z = 0.0f;
    float rotY = 0.0f;

    // Velocity for extrapolation
    float velX = 0.0f, velZ = 0.0f;

    // Animation
    uint8_t animState = 0;
    float animTime = 0.0f;

    // Health
    int32_t currentHP = 100;
    int32_t maxHP = 100;

    // Status effects
    uint32_t statusMask = 0;

    // Interpolation state
    bool isExtrapolating = false;
    float extrapolationTime = 0.0f;

    // Last update time (for stale detection)
    float lastUpdateTime = 0.0f;
};

// =============================================================================
// Snapshot Buffer
// =============================================================================
class SnapshotBuffer {
    static constexpr size_t MAX_SNAPSHOTS = 32;

    struct BufferedSnapshot {
        net::Snapshot snapshot;
        float localReceiveTime = 0.0f;
        bool processed = false;
    };

    std::array<BufferedSnapshot, MAX_SNAPSHOTS> snapshots{};
    size_t writeIndex = 0;
    size_t snapshotCount = 0;

    float localTime = 0.0f;

public:
    void AddSnapshot(const net::Snapshot& snapshot, float receiveTime);

    // Get interpolated state at render time
    // renderTime should be localTime - interpolationDelay (e.g., 100ms)
    [[nodiscard]] std::optional<net::EntityState> Interpolate(
        uint32_t entityId, float renderTime) const;

    // Get latest known state for extrapolation
    [[nodiscard]] std::optional<net::EntityState> GetLatestState(uint32_t entityId) const;

    [[nodiscard]] bool HasEnoughSnapshots() const { return snapshotCount >= 3; }
    [[nodiscard]] float GetInterpolationDelay() const;

    void UpdateLocalTime(float deltaTime) { localTime += deltaTime; }
    [[nodiscard]] float GetLocalTime() const { return localTime; }

    void CleanupOldSnapshots(float maxAge);

private:
    [[nodiscard]] std::optional<std::pair<net::EntityState, net::EntityState>> 
        FindSurroundingStates(uint32_t entityId, float renderTime) const;
};

// =============================================================================
// Entity Interpolator
// =============================================================================
class EntityInterpolator {
    std::unordered_map<uint32_t, InterpolatedEntity> entities;
    SnapshotBuffer snapshotBuffer;

    // Config
    float interpolationDelay = 0.1f;      // 100ms buffer
    float maxExtrapolationTime = 0.2f;      // 200ms max extrapolation
    float lerpSpeed = 10.0f;                // Position smoothing

public:
    explicit EntityInterpolator(float delay = 0.1f);

    // Called when new snapshot arrives from server
    void ReceiveSnapshot(const net::Snapshot& snapshot);

    // Called every render frame (60Hz)
    void Update(float deltaTime);

    // Get interpolated entity for rendering
    [[nodiscard]] const InterpolatedEntity* GetEntity(uint32_t entityId) const;
    [[nodiscard]] std::vector<uint32_t> GetAllEntityIds() const;

    // Remove entity (despawn)
    void RemoveEntity(uint32_t entityId);

    // Stats
    [[nodiscard]] bool IsJitterAcceptable() const;
    [[nodiscard]] float GetAverageJitter() const;

private:
    void InterpolateEntity(InterpolatedEntity& entity, float renderTime);
    void ExtrapolateEntity(InterpolatedEntity& entity, float deltaTime);
    void ApplySnapshotState(InterpolatedEntity& entity, const net::EntityState& state);
};

} // namespace client
