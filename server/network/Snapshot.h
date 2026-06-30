// =============================================================================
// server/network/Snapshot.h — Delta-Kompression + Interest Management (AP-37/AP-40)
// =============================================================================
// KORREKTUR: Delta-Kompression + Spatial Hash Interest Management zusammengeführt.
// Nur Entities innerhalb des AOI (Area of Interest) werden serialisiert.
// Delta-Kompression sendet nur geänderte Felder seit dem letzten Snapshot.
// =============================================================================
#pragma once
#include "../../ecs/ecs_EcsWorld.h"
#include "../../ecs/Components.h"
#include "../../core/ByteBuffer.h"
#include "../../math/Vector.h"

#include <vector>
#include <span>
#include <unordered_map>
#include <unordered_set>
#include <memory>
#include <chrono>
#include <cstdint>

namespace net {

// =============================================================================
// Delta Entity State — Was sich seit dem letzten Snapshot geändert hat
// =============================================================================
enum class DeltaFlags : uint8_t {
    None        = 0x00,
    Position    = 0x01,  // x, y, z
    Rotation    = 0x02,  // rotationY
    Health      = 0x04,  // currentHP, maxHP
    Name        = 0x08,  // name string
    Material    = 0x10,  // materialId
    Velocity    = 0x20,  // vx, vz
    All         = 0x3F
};

inline DeltaFlags operator|(DeltaFlags a, DeltaFlags b) {
    return static_cast<DeltaFlags>(static_cast<uint8_t>(a) | static_cast<uint8_t>(b));
}
inline DeltaFlags operator&(DeltaFlags a, DeltaFlags b) {
    return static_cast<DeltaFlags>(static_cast<uint8_t>(a) & static_cast<uint8_t>(b));
}
inline bool HasFlag(DeltaFlags flags, DeltaFlags check) {
    return (static_cast<uint8_t>(flags) & static_cast<uint8_t>(check)) != 0;
}

// =============================================================================
// Entity State (für Delta-Vergleich)
// =============================================================================
struct EntityState {
    uint32_t id = 0;
    float x = 0.0f, y = 0.0f, z = 0.0f;
    float rotationY = 0.0f;
    uint32_t currentHP = 0, maxHP = 0;
    std::string name;
    std::string materialId;
    float vx = 0.0f, vz = 0.0f;

    [[nodiscard]] bool operator==(const EntityState& other) const noexcept = default;
    [[nodiscard]] bool operator!=(const EntityState& other) const noexcept = default;
};

// =============================================================================
// Spatial Hash — Schnelle räumliche Abfragen für Interest Management
// =============================================================================
class SpatialHash {
    static constexpr float CELL_SIZE = 50.0f;  // 50m Zellen

    struct CellKey {
        int32_t x = 0, z = 0;
        [[nodiscard]] bool operator==(const CellKey& o) const noexcept = default;
    };

    struct CellKeyHash {
        [[nodiscard]] size_t operator()(const CellKey& k) const noexcept {
            return std::hash<int64_t>{}((static_cast<int64_t>(k.x) << 32) ^ static_cast<uint32_t>(k.z));
        }
    };

    std::unordered_map<CellKey, std::vector<uint32_t>, CellKeyHash> cells;

public:
    void Clear();
    void Insert(uint32_t entityId, float x, float z);
    [[nodiscard]] std::vector<uint32_t> Query(float x, float z, float radius) const;
    [[nodiscard]] static CellKey GetCell(float x, float z);
};

// =============================================================================
// Snapshot Builder — Delta-Kompression + Interest Management
// =============================================================================
class SnapshotBuilder {
    // Client-Zustände (für Delta-Kompression pro Client)
    struct ClientState {
        std::unordered_map<uint32_t, EntityState> lastKnownState;
        std::chrono::steady_clock::time_point lastSnapshotTime;
        float viewX = 0.0f, viewZ = 0.0f;
        float interestRadius = 100.0f;  // Default AOI: 100m
    };

    std::unordered_map<uint32_t, ClientState> clientStates;
    std::mutex clientStatesMutex;

    // Spatial Hash für Interest Management
    SpatialHash spatialHash;
    std::mutex spatialMutex;

    // Konfiguration
    float defaultInterestRadius = 100.0f;
    float maxInterestRadius = 500.0f;

public:
    SnapshotBuilder() = default;
    ~SnapshotBuilder() = default;

    SnapshotBuilder(const SnapshotBuilder&) = delete;
    SnapshotBuilder& operator=(const SnapshotBuilder&) = delete;

    // ===================================================================
    // Spatial Hash Update (wird pro Tick aufgerufen)
    // ===================================================================
    void UpdateSpatialHash(ecs::EcsWorld& world);

    // ===================================================================
    // Snapshot bauen für einen bestimmten Client
    // ===================================================================
    [[nodiscard]] ByteBuffer BuildSnapshot(
        ecs::EcsWorld& world,
        uint32_t clientSessionId,
        float clientX, float clientZ,
        float interestRadius);

    // ===================================================================
    // Full Snapshot (für neue Clients oder Recovery)
    // ===================================================================
    [[nodiscard]] ByteBuffer BuildFullSnapshot(
        ecs::EcsWorld& world,
        uint32_t clientSessionId,
        float clientX, float clientZ,
        float interestRadius);

    // ===================================================================
    // Client Management
    // ===================================================================
    void RegisterClient(uint32_t sessionId);
    void UnregisterClient(uint32_t sessionId);
    void UpdateClientPosition(uint32_t sessionId, float x, float z);
    void SetClientInterestRadius(uint32_t sessionId, float radius);

    // ===================================================================
    // Stats
    // ===================================================================
    [[nodiscard]] size_t GetClientCount() const;
    void ResetClientState(uint32_t sessionId);

private:
    [[nodiscard]] DeltaFlags CalculateDelta(
        const EntityState& oldState,
        const EntityState& newState);

    void SerializeEntityDelta(
        ByteBuffer& buffer,
        const EntityState& state,
        DeltaFlags delta);

    void SerializeEntityFull(ByteBuffer& buffer, const EntityState& state);

    [[nodiscard]] EntityState ExtractEntityState(
        ecs::EcsWorld& world,
        ecs::EntityHandle handle);
};

} // namespace net
