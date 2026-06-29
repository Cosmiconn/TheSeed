#pragma once
// =============================================================================
// server/network/Snapshot.h — World Snapshot Builder (AP-37)
// Serializes ECS world state for network replication
// =============================================================================
#include "../../ecs/ecs_EcsWorld.h"
#include "../../ecs/Components.h"
#include "../../network/network_ReliableUdp.h"
#include <cstdint>
#include <vector>
#include <span>
#include <memory>
#include <unordered_map>

namespace net {

// =============================================================================
// Snapshot Data Structures
// =============================================================================
struct EntityState {
    uint32_t entityId = 0;
    uint32_t archetypeHash = 0;

    // Transform (most common, always included if entity has it)
    float posX = 0.0f, posY = 0.0f, posZ = 0.0f;
    float rotY = 0.0f;

    // Health (if changed)
    int32_t currentHP = -1;  // -1 = not present
    int32_t maxHP = -1;

    // Animation state
    uint8_t animState = 0;   // 0=idle, 1=walk, 2=attack, 3=death
    float animTime = 0.0f;

    // Status effects (bitmask)
    uint32_t statusMask = 0;

    // Dirty flags: which fields are actually present
    uint16_t fieldMask = 0;

    static constexpr uint16_t FIELD_POS = 0x0001;
    static constexpr uint16_t FIELD_ROT = 0x0002;
    static constexpr uint16_t FIELD_HP = 0x0004;
    static constexpr uint16_t FIELD_ANIM = 0x0008;
    static constexpr uint16_t FIELD_STATUS = 0x0010;
};

struct Snapshot {
    uint32_t sequence = 0;
    float serverTime = 0.0f;
    std::vector<EntityState> entities;
    std::vector<uint32_t> destroyedEntities;

    // Delta compression: which entities changed since base snapshot
    uint32_t baseSequence = 0;
    bool isFullSnapshot = false;
};

// =============================================================================
// Snapshot Builder
// =============================================================================
class SnapshotBuilder {
    std::unordered_map<uint32_t, EntityState> lastEntityStates;
    uint32_t nextSequence = 1;

    // Config
    float snapshotRate = 20.0f;  // 20Hz default
    float maxEntityDistance = 200.0f;  // Cull entities beyond this
    size_t maxEntitiesPerSnapshot = 1000;

public:
    explicit SnapshotBuilder(float rate = 20.0f);

    // Build full snapshot of entire ECS world
    [[nodiscard]] Snapshot BuildFull(const ecs::EcsWorld& world);

    // Build delta snapshot (only changed entities since last ack)
    [[nodiscard]] Snapshot BuildDelta(const ecs::EcsWorld& world, uint32_t ackedSequence);

    // Serialize snapshot to binary (for network transmission)
    [[nodiscard]] std::vector<uint8_t> Serialize(const Snapshot& snapshot) const;

    // Deserialize from binary
    [[nodiscard]] std::optional<Snapshot> Deserialize(std::span<const uint8_t> data) const;

    // Get current sequence number
    [[nodiscard]] uint32_t GetCurrentSequence() const { return nextSequence; }

    // Set max entities per snapshot (for bandwidth limiting)
    void SetMaxEntities(size_t max) { maxEntitiesPerSnapshot = max; }

private:
    [[nodiscard]] EntityState ExtractEntityState(ecs::EntityHandle handle, const ecs::EcsWorld& world);
    [[nodiscard]] bool HasEntityChanged(uint32_t entityId, const EntityState& current) const;
    void UpdateLastState(uint32_t entityId, const EntityState& state);
    void CleanupOldStates(uint32_t minSequence);

    // Write helpers for serialization
    void WriteUInt32(std::vector<uint8_t>& buf, uint32_t val) const;
    void WriteFloat(std::vector<uint8_t>& buf, float val) const;
    void WriteUInt16(std::vector<uint8_t>& buf, uint16_t val) const;
    void WriteUInt8(std::vector<uint8_t>& buf, uint8_t val) const;

    [[nodiscard]] uint32_t ReadUInt32(std::span<const uint8_t>& buf) const;
    [[nodiscard]] float ReadFloat(std::span<const uint8_t>& buf) const;
    [[nodiscard]] uint16_t ReadUInt16(std::span<const uint8_t>& buf) const;
    [[nodiscard]] uint8_t ReadUInt8(std::span<const uint8_t>& buf) const;
};

// =============================================================================
// Interest Management (AP-40)
// =============================================================================
class InterestManager {
    struct ClientView {
        float x = 0.0f, z = 0.0f;
        float radius = 100.0f;
        uint32_t priority = 0;
    };

    std::unordered_map<uint32_t, ClientView> clientViews;

public:
    void UpdateClientView(uint32_t clientId, float x, float z, float radius);
    void RemoveClient(uint32_t clientId);

    // Filter entities: only return those relevant to client
    [[nodiscard]] std::vector<uint32_t> FilterEntities(
        uint32_t clientId,
        std::span<const EntityState> allEntities) const;

    // LOD-based priority: closer entities = higher priority
    [[nodiscard]] uint32_t CalculatePriority(
        float clientX, float clientZ,
        float entityX, float entityZ) const;
};

} // namespace net
