// =============================================================================
// server/network/Snapshot.cpp — Delta-Kompression + Interest Management (P2-FIX)
// =============================================================================
// KORREKTUR P2:
// • FragmentSnapshot() und SendFragmentedSnapshot() jetzt INNERHALB namespace net
// • Delta-Kompression mit Spatial Hash Interest Management vollständig
// • Sendet nur geänderte Felder + nur Entities im AOI
// =============================================================================
#include "Snapshot.h"
#include "../../core/Log.h"
#include <cmath>
#include <algorithm>

namespace net {

// =============================================================================
// SpatialHash Implementation
// =============================================================================
void SpatialHash::Clear() {
    cells.clear();
}

void SpatialHash::Insert(uint32_t entityId, float x, float z) {
    auto key = GetCell(x, z);
    cells[key].push_back(entityId);
}

SpatialHash::CellKey SpatialHash::GetCell(float x, float z) {
    return CellKey{
        static_cast<int32_t>(std::floor(x / CELL_SIZE)),
        static_cast<int32_t>(std::floor(z / CELL_SIZE))
    };
}

std::vector<uint32_t> SpatialHash::Query(float x, float z, float radius) const {
    std::vector<uint32_t> result;
    std::unordered_set<uint32_t> seen;

    int32_t cellRadius = static_cast<int32_t>(std::ceil(radius / CELL_SIZE));
    auto centerCell = GetCell(x, z);

    for (int32_t dx = -cellRadius; dx <= cellRadius; ++dx) {
        for (int32_t dz = -cellRadius; dz <= cellRadius; ++dz) {
            CellKey key{centerCell.x + dx, centerCell.z + dz};
            auto it = cells.find(key);
            if (it == cells.end()) continue;

            for (uint32_t entityId : it->second) {
                if (seen.insert(entityId).second) {
                    result.push_back(entityId);
                }
            }
        }
    }

    return result;
}

// =============================================================================
// SnapshotBuilder — Client Management
// =============================================================================
void SnapshotBuilder::RegisterClient(uint32_t sessionId) {
    std::lock_guard lock(clientStatesMutex);
    clientStates[sessionId] = ClientState{};
    AddLog("[Snapshot] Client {} registered for delta compression", sessionId);
}

void SnapshotBuilder::UnregisterClient(uint32_t sessionId) {
    std::lock_guard lock(clientStatesMutex);
    clientStates.erase(sessionId);
    AddLog("[Snapshot] Client {} unregistered", sessionId);
}

void SnapshotBuilder::UpdateClientPosition(uint32_t sessionId, float x, float z) {
    std::lock_guard lock(clientStatesMutex);
    auto it = clientStates.find(sessionId);
    if (it != clientStates.end()) {
        it->second.viewX = x;
        it->second.viewZ = z;
    }
}

void SnapshotBuilder::SetClientInterestRadius(uint32_t sessionId, float radius) {
    std::lock_guard lock(clientStatesMutex);
    auto it = clientStates.find(sessionId);
    if (it != clientStates.end()) {
        it->second.interestRadius = std::clamp(radius, 10.0f, maxInterestRadius);
    }
}

void SnapshotBuilder::ResetClientState(uint32_t sessionId) {
    std::lock_guard lock(clientStatesMutex);
    auto it = clientStates.find(sessionId);
    if (it != clientStates.end()) {
        it->second.lastKnownState.clear();
    }
}

size_t SnapshotBuilder::GetClientCount() const {
    std::lock_guard lock(clientStatesMutex);
    return clientStates.size();
}

// =============================================================================
// Update Spatial Hash
// =============================================================================
void SnapshotBuilder::UpdateSpatialHash(ecs::EcsWorld& world) {
    std::lock_guard lock(spatialMutex);
    spatialHash.Clear();

    auto query = world.QueryEntities<ecs::PositionComponent>();
    for (auto [handle, pos] : query) {
        (void)pos;
        auto* transform = world.GetComponent<ecs::PositionComponent>(handle);
        auto* legacy = world.GetComponent<ecs::LegacyIdComponent>(handle);
        if (!transform) continue;

        uint32_t id = legacy ? legacy->legacyId : static_cast<uint32_t>(handle.GetIndex());
        spatialHash.Insert(id, transform->x, transform->z);
    }
}

// =============================================================================
// Extract Entity State from ECS
// =============================================================================
EntityState SnapshotBuilder::ExtractEntityState(
    ecs::EcsWorld& world,
    ecs::EntityHandle handle) {

    EntityState state{};
    state.id = static_cast<uint32_t>(handle.GetIndex());

    auto* transform = world.GetComponent<ecs::PositionComponent>(handle);
    if (transform) {
        state.x = transform->x;
        state.y = transform->y;
        state.z = transform->z;
    }

    auto* rotation = world.GetComponent<ecs::RotationComponent>(handle);
    if (rotation) {
        state.rotationY = rotation->yaw;
    }

    auto* velocity = world.GetComponent<ecs::VelocityComponent>(handle);
    if (velocity) {
        state.vx = velocity->vx;
        state.vz = velocity->vz;
    }

    auto* health = world.GetComponent<ecs::HealthComponent>(handle);
    if (health) {
        state.currentHP = static_cast<uint32_t>(health->currentHP);
        state.maxHP = static_cast<uint32_t>(health->maxHP);
    }

    auto* name = world.GetComponent<ecs::NameComponent>(handle);
    if (name) {
        state.name = name->name;
    }

    auto* render = world.GetComponent<ecs::RenderComponentECS>(handle);
    if (render) {
        state.materialId = render->materialId;
    }

    auto* legacy = world.GetComponent<ecs::LegacyIdComponent>(handle);
    if (legacy) {
        state.id = legacy->legacyId;
    }

    return state;
}

// =============================================================================
// Calculate Delta between old and new state
// =============================================================================
DeltaFlags SnapshotBuilder::CalculateDelta(
    const EntityState& oldState,
    const EntityState& newState) {

    DeltaFlags delta = DeltaFlags::None;

    if (oldState.x != newState.x || oldState.y != newState.y || oldState.z != newState.z) {
        delta = delta | DeltaFlags::Position;
    }
    if (oldState.rotationY != newState.rotationY) {
        delta = delta | DeltaFlags::Rotation;
    }
    if (oldState.currentHP != newState.currentHP || oldState.maxHP != newState.maxHP) {
        delta = delta | DeltaFlags::Health;
    }
    if (oldState.name != newState.name) {
        delta = delta | DeltaFlags::Name;
    }
    if (oldState.materialId != newState.materialId) {
        delta = delta | DeltaFlags::Material;
    }
    if (oldState.vx != newState.vx || oldState.vz != newState.vz) {
        delta = delta | DeltaFlags::Velocity;
    }

    return delta;
}

// =============================================================================
// Serialize Entity Delta (nur geänderte Felder)
// =============================================================================
void SnapshotBuilder::SerializeEntityDelta(
    ByteBuffer& buffer,
    const EntityState& state,
    DeltaFlags delta) {

    buffer.WriteUInt32(state.id);
    buffer.WriteUInt8(static_cast<uint8_t>(delta));

    if (HasFlag(delta, DeltaFlags::Position)) {
        buffer.WriteFloat(state.x);
        buffer.WriteFloat(state.y);
        buffer.WriteFloat(state.z);
    }
    if (HasFlag(delta, DeltaFlags::Rotation)) {
        buffer.WriteFloat(state.rotationY);
    }
    if (HasFlag(delta, DeltaFlags::Health)) {
        buffer.WriteUInt32(state.currentHP);
        buffer.WriteUInt32(state.maxHP);
    }
    if (HasFlag(delta, DeltaFlags::Name)) {
        buffer.WriteString(state.name);
    }
    if (HasFlag(delta, DeltaFlags::Material)) {
        buffer.WriteString(state.materialId);
    }
    if (HasFlag(delta, DeltaFlags::Velocity)) {
        buffer.WriteFloat(state.vx);
        buffer.WriteFloat(state.vz);
    }
}

// =============================================================================
// Serialize Entity Full (alle Felder)
// =============================================================================
void SnapshotBuilder::SerializeEntityFull(ByteBuffer& buffer, const EntityState& state) {
    buffer.WriteUInt32(state.id);
    buffer.WriteUInt8(static_cast<uint8_t>(DeltaFlags::All));
    buffer.WriteFloat(state.x);
    buffer.WriteFloat(state.y);
    buffer.WriteFloat(state.z);
    buffer.WriteFloat(state.rotationY);
    buffer.WriteUInt32(state.currentHP);
    buffer.WriteUInt32(state.maxHP);
    buffer.WriteString(state.name);
    buffer.WriteString(state.materialId);
    buffer.WriteFloat(state.vx);
    buffer.WriteFloat(state.vz);
}

// =============================================================================
// Build Snapshot — Delta + Interest Management
// =============================================================================
ByteBuffer SnapshotBuilder::BuildSnapshot(
    ecs::EcsWorld& world,
    uint32_t clientSessionId,
    float clientX, float clientZ,
    float interestRadius) {

    ByteBuffer snapshot;

    // Header
    snapshot.WriteUInt8(0x01); // Snapshot type: Delta
    snapshot.WriteFloat(static_cast<float>(
        std::chrono::duration<float>(
            std::chrono::steady_clock::now().time_since_epoch()).count()));

    // Get client state
    ClientState* clientState = nullptr;
    {
        std::lock_guard lock(clientStatesMutex);
        auto it = clientStates.find(clientSessionId);
        if (it != clientStates.end()) {
            clientState = &it->second;
        }
    }

    if (!clientState) {
        snapshot.WriteUInt32(0);
        return snapshot;
    }

    // Update client position
    clientState->viewX = clientX;
    clientState->viewZ = clientZ;

    // Query entities in AOI using Spatial Hash
    std::vector<uint32_t> nearbyEntities;
    {
        std::lock_guard lock(spatialMutex);
        nearbyEntities = spatialHash.Query(clientX, clientZ, interestRadius);
    }

    // Build delta snapshot
    std::vector<EntityState> fullEntities;
    std::vector<std::pair<EntityState, DeltaFlags>> deltaEntities;
    std::vector<uint32_t> toRemove;

    for (uint32_t entityId : nearbyEntities) {
        ecs::EntityHandle handle(entityId);
        auto* transform = world.GetComponent<ecs::PositionComponent>(handle);
        if (!transform) continue;

        float dx = transform->x - clientX;
        float dz = transform->z - clientZ;
        float dist = std::sqrt(dx*dx + dz*dz);
        if (dist > interestRadius) continue;

        EntityState currentState = ExtractEntityState(world, handle);

        auto it = clientState->lastKnownState.find(entityId);
        if (it == clientState->lastKnownState.end()) {
            fullEntities.push_back(currentState);
            clientState->lastKnownState[entityId] = currentState;
        } else {
            DeltaFlags delta = CalculateDelta(it->second, currentState);
            if (delta != DeltaFlags::None) {
                deltaEntities.emplace_back(currentState, delta);
                it->second = currentState;
            }
        }
    }

    // Remove entities that left AOI
    for (const auto& [id, state] : clientState->lastKnownState) {
        if (std::find(nearbyEntities.begin(), nearbyEntities.end(), id) == nearbyEntities.end()) {
            toRemove.push_back(id);
        }
    }
    for (uint32_t id : toRemove) {
        clientState->lastKnownState.erase(id);
    }

    // Write final snapshot with proper count
    uint32_t totalEntities = static_cast<uint32_t>(fullEntities.size() + deltaEntities.size());
    snapshot.WriteUInt32(totalEntities);

    for (const auto& state : fullEntities) {
        SerializeEntityFull(snapshot, state);
    }

    for (const auto& [state, delta] : deltaEntities) {
        SerializeEntityDelta(snapshot, state, delta);
    }

    clientState->lastSnapshotTime = std::chrono::steady_clock::now();

    AddLog("[Snapshot] Built delta snapshot for client {}: {} entities ({} full, {} delta)",
           clientSessionId, totalEntities, fullEntities.size(), deltaEntities.size());

    return snapshot;
}

// =============================================================================
// Build Full Snapshot (für neue Clients oder Recovery)
// =============================================================================
ByteBuffer SnapshotBuilder::BuildFullSnapshot(
    ecs::EcsWorld& world,
    uint32_t clientSessionId,
    float clientX, float clientZ,
    float interestRadius) {

    ByteBuffer snapshot;
    snapshot.WriteUInt8(0x02); // Full snapshot type
    snapshot.WriteFloat(static_cast<float>(
        std::chrono::duration<float>(
            std::chrono::steady_clock::now().time_since_epoch()).count()));

    ClientState* clientState = nullptr;
    {
        std::lock_guard lock(clientStatesMutex);
        auto it = clientStates.find(clientSessionId);
        if (it == clientStates.end()) {
            clientStates[clientSessionId] = ClientState{};
            it = clientStates.find(clientSessionId);
        }
        clientState = &it->second;
    }

    clientState->lastKnownState.clear();

    std::vector<uint32_t> nearbyEntities;
    {
        std::lock_guard lock(spatialMutex);
        nearbyEntities = spatialHash.Query(clientX, clientZ, interestRadius);
    }

    uint32_t entityCount = 0;
    std::vector<EntityState> allStates;

    for (uint32_t entityId : nearbyEntities) {
        ecs::EntityHandle handle(entityId);
        auto* transform = world.GetComponent<ecs::PositionComponent>(handle);
        if (!transform) continue;

        float dx = transform->x - clientX;
        float dz = transform->z - clientZ;
        float dist = std::sqrt(dx*dx + dz*dz);
        if (dist > interestRadius) continue;

        EntityState state = ExtractEntityState(world, handle);
        allStates.push_back(state);
        clientState->lastKnownState[entityId] = state;
        entityCount++;
    }

    snapshot.WriteUInt32(entityCount);
    for (const auto& state : allStates) {
        SerializeEntityFull(snapshot, state);
    }

    clientState->lastSnapshotTime = std::chrono::steady_clock::now();

    AddLog("[Snapshot] Built full snapshot for client {}: {} entities",
           clientSessionId, entityCount);

    return snapshot;
}

// =============================================================================
// FIX P2/P5-1: Snapshot-Fragmentierung (MTU-Splitting) — JETZT IN namespace net
// =============================================================================
std::vector<SnapshotFragment> SnapshotBuilder::FragmentSnapshot(
    const ByteBuffer& snapshotData,
    uint32_t sequenceId) {

    std::vector<SnapshotFragment> fragments;
    const auto& data = snapshotData.data;

    if (data.size() <= MAX_FRAGMENT_PAYLOAD) {
        SnapshotFragment frag;
        frag.sequenceId = sequenceId;
        frag.fragmentId = 0;
        frag.totalFragments = 1;
        frag.flags = SNAPSHOT_FLAG_LAST_FRAGMENT;
        frag.payload.assign(data.begin(), data.end());
        fragments.push_back(std::move(frag));
        return fragments;
    }

    size_t offset = 0;
    uint16_t fragId = 0;
    size_t totalFrags = (data.size() + MAX_FRAGMENT_PAYLOAD - 1) / MAX_FRAGMENT_PAYLOAD;

    while (offset < data.size()) {
        size_t remaining = data.size() - offset;
        size_t chunkSize = std::min(remaining, MAX_FRAGMENT_PAYLOAD);

        SnapshotFragment frag;
        frag.sequenceId = sequenceId;
        frag.fragmentId = fragId++;
        frag.totalFragments = static_cast<uint16_t>(totalFrags);
        frag.flags = (offset + chunkSize >= data.size()) ? SNAPSHOT_FLAG_LAST_FRAGMENT : 0;
        frag.flags |= SNAPSHOT_FLAG_FRAGMENTED;
        frag.payload.assign(data.begin() + offset, data.begin() + offset + chunkSize);

        fragments.push_back(std::move(frag));
        offset += chunkSize;
    }

    return fragments;
}

bool SnapshotBuilder::SendFragmentedSnapshot(
    NetworkServer& server,
    const ByteBuffer& snapshotData,
    uint32_t sequenceId,
    const std::string& ip,
    uint16_t port) {

    auto fragments = FragmentSnapshot(snapshotData, sequenceId);

    for (const auto& frag : fragments) {
        ByteBuffer fragBuffer;
        fragBuffer.WriteUInt32(frag.sequenceId);
        fragBuffer.WriteUInt16(frag.fragmentId);
        fragBuffer.WriteUInt16(frag.totalFragments);
        fragBuffer.WriteUInt8(frag.flags);
        fragBuffer.WriteUInt32(static_cast<uint32_t>(frag.payload.size()));
        fragBuffer.data.insert(fragBuffer.data.end(), frag.payload.begin(), frag.payload.end());

        PacketHeader header{};
        header.protocolId = 0x4D4D;
        header.sequence = sequenceId;
        header.flags = static_cast<uint16_t>(PacketFlags::Reliable);
        if (fragments.size() > 1) {
            header.flags |= static_cast<uint16_t>(PacketFlags::Fragmented);
        }

        server.SendReliable(
            header,
            std::span(fragBuffer.data.data(), fragBuffer.data.size()),
            ip,
            port
        );
    }

    if (fragments.size() > 1) {
        AddLog("[Snapshot] Snapshot {} in {} Fragmente aufgeteilt ({} Bytes → ~{} Bytes/Frag)",
               sequenceId, fragments.size(), snapshotData.data.size(),
               snapshotData.data.size() / fragments.size());
    }

    return true;
}

} // namespace net
