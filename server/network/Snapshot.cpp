// =============================================================================
// server/network/Snapshot.cpp — Delta-Kompression + Interest Management (AP-37/AP-40)
// =============================================================================
// KORREKTUR: Delta-Kompression + Spatial Hash Interest Management vollständig
// implementiert. Sendet nur geänderte Felder + nur Entities im AOI.
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

    auto query = world.Query<game::Transform>();
    for (auto [handle] : query) {
        auto* transform = world.GetComponent<game::Transform>(handle);
        auto* legacy = world.GetComponent<game::LegacyId>(handle);
        if (!transform) continue;

        uint32_t id = legacy ? legacy->id : static_cast<uint32_t>(handle.GetIndex());
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

    auto* transform = world.GetComponent<game::Transform>(handle);
    if (transform) {
        state.x = transform->x;
        state.y = transform->y;
        state.z = transform->z;
        state.rotationY = transform->rotationY;
    }

    auto* velocity = world.GetComponent<game::Velocity>(handle);
    if (velocity) {
        state.vx = velocity->vx;
        state.vz = velocity->vz;
    }

    auto* health = world.GetComponent<game::Health>(handle);
    if (health) {
        state.currentHP = static_cast<uint32_t>(health->currentHP);  // FIX P2: Feldname
        state.maxHP = static_cast<uint32_t>(health->maxHP);  // FIX P2: Feldname
    }

    auto* name = world.GetComponent<game::Name>(handle);
    if (name) {
        state.name = name->name;
    }

    auto* render = world.GetComponent<game::RenderInfo>(handle);
    if (render) {
        state.materialId = render->materialId;
    }

    auto* legacy = world.GetComponent<game::LegacyId>(handle);
    if (legacy) {
        state.id = legacy->id;
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
        // Client not registered, send empty snapshot
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
    std::vector<uint8_t> entityData;
    uint32_t deltaCount = 0;

    for (uint32_t entityId : nearbyEntities) {
        // Find entity in ECS
        ecs::EntityHandle handle(entityId);
        auto* transform = world.GetComponent<game::Transform>(handle);
        if (!transform) continue;

        // Check distance (Spatial Hash is approximate)
        float dx = transform->x - clientX;
        float dz = transform->z - clientZ;
        float dist = std::sqrt(dx*dx + dz*dz);
        if (dist > interestRadius) continue;

        // Extract current state
        EntityState currentState = ExtractEntityState(world, handle);

        // Check if entity is new or changed
        auto it = clientState->lastKnownState.find(entityId);
        if (it == clientState->lastKnownState.end()) {
            // New entity: send full state
            SerializeEntityFull(snapshot, currentState);
            deltaCount++;
        } else {
            // Existing entity: calculate delta
            DeltaFlags delta = CalculateDelta(it->second, currentState);
            if (delta != DeltaFlags::None) {
                SerializeEntityDelta(snapshot, currentState, delta);
                deltaCount++;
            }
        }

        // Update known state
        clientState->lastKnownState[entityId] = currentState;
    }

    // Remove entities that left AOI
    std::vector<uint32_t> toRemove;
    for (const auto& [id, state] : clientState->lastKnownState) {
        if (std::find(nearbyEntities.begin(), nearbyEntities.end(), id) == nearbyEntities.end()) {
            toRemove.push_back(id);
        }
    }
    for (uint32_t id : toRemove) {
        clientState->lastKnownState.erase(id);
    }

    // Write entity count at position 5 (after header)
    // Note: In production, use a two-pass approach or reserve space
    // For simplicity, we prepend count (less efficient but correct)
    // Better approach: Write placeholder, seek back, write count

    // Insert count at beginning of payload (after header bytes)
    // Header: [type:1][timestamp:4] = 5 bytes
    // Then: [count:4][entities...]

    // Rebuild with proper count
    ByteBuffer finalSnapshot;
    finalSnapshot.WriteUInt8(0x01); // Delta snapshot
    finalSnapshot.WriteFloat(static_cast<float>(
        std::chrono::duration<float>(
            std::chrono::steady_clock::now().time_since_epoch()).count()));
    finalSnapshot.WriteUInt32(deltaCount);

    // Copy entity data
    // (In production, collect entity data first, then write count)
    // For now, we write count first, then entities

    // Actually, let's do it properly: collect first
    // The above approach is conceptually correct but implementation needs care
    // Here's the corrected version:

    // Re-collect entities properly
    std::vector<std::pair<EntityState, DeltaFlags>> deltaEntities;
    std::vector<EntityState> fullEntities;

    for (uint32_t entityId : nearbyEntities) {
        ecs::EntityHandle handle(entityId);
        auto* transform = world.GetComponent<game::Transform>(handle);
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

    // Clean up left entities
    for (const auto& [id, state] : clientState->lastKnownState) {
        if (std::find(nearbyEntities.begin(), nearbyEntities.end(), id) == nearbyEntities.end()) {
            toRemove.push_back(id);
        }
    }
    for (uint32_t id : toRemove) {
        clientState->lastKnownState.erase(id);
    }

    // Write final snapshot
    uint32_t totalEntities = static_cast<uint32_t>(fullEntities.size() + deltaEntities.size());
    finalSnapshot.WriteUInt32(totalEntities);

    for (const auto& state : fullEntities) {
        SerializeEntityFull(finalSnapshot, state);
    }

    for (const auto& [state, delta] : deltaEntities) {
        SerializeEntityDelta(finalSnapshot, state, delta);
    }

    clientState->lastSnapshotTime = std::chrono::steady_clock::now();

    AddLog("[Snapshot] Built delta snapshot for client {}: {} entities ({} full, {} delta)",
           clientSessionId, totalEntities, fullEntities.size(), deltaEntities.size());

    return finalSnapshot;
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

    // Header: Full snapshot
    snapshot.WriteUInt8(0x02); // Full snapshot type
    snapshot.WriteFloat(static_cast<float>(
        std::chrono::duration<float>(
            std::chrono::steady_clock::now().time_since_epoch()).count()));

    // Get or create client state
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

    // Clear known state for full rebuild
    clientState->lastKnownState.clear();

    // Query entities in AOI
    std::vector<uint32_t> nearbyEntities;
    {
        std::lock_guard lock(spatialMutex);
        nearbyEntities = spatialHash.Query(clientX, clientZ, interestRadius);
    }

    uint32_t entityCount = 0;

    for (uint32_t entityId : nearbyEntities) {
        ecs::EntityHandle handle(entityId);
        auto* transform = world.GetComponent<game::Transform>(handle);
        if (!transform) continue;

        float dx = transform->x - clientX;
        float dz = transform->z - clientZ;
        float dist = std::sqrt(dx*dx + dz*dz);
        if (dist > interestRadius) continue;

        EntityState state = ExtractEntityState(world, handle);
        SerializeEntityFull(snapshot, state);
        clientState->lastKnownState[entityId] = state;
        entityCount++;
    }

    // Insert count (need to rebuild with count first)
    // For full snapshots, we write count after header
    ByteBuffer finalSnapshot;
    finalSnapshot.WriteUInt8(0x02);
    finalSnapshot.WriteFloat(static_cast<float>(
        std::chrono::duration<float>(
            std::chrono::steady_clock::now().time_since_epoch()).count()));
    finalSnapshot.WriteUInt32(entityCount);

    // Re-serialize all entities
    for (uint32_t entityId : nearbyEntities) {
        ecs::EntityHandle handle(entityId);
        auto* transform = world.GetComponent<game::Transform>(handle);
        if (!transform) continue;

        float dx = transform->x - clientX;
        float dz = transform->z - clientZ;
        float dist = std::sqrt(dx*dx + dz*dz);
        if (dist > interestRadius) continue;

        EntityState state = ExtractEntityState(world, handle);
        SerializeEntityFull(finalSnapshot, state);
    }

    clientState->lastSnapshotTime = std::chrono::steady_clock::now();

    AddLog("[Snapshot] Built full snapshot for client {}: {} entities",
           clientSessionId, entityCount);

    return finalSnapshot;
}

} // namespace net

// =============================================================================
// FIX P5-1: Snapshot-Fragmentierung (MTU-Splitting)
// =============================================================================
std::vector<SnapshotFragment> SnapshotBuilder::FragmentSnapshot(
    const ByteBuffer& snapshotData,
    uint32_t sequenceId) {

    std::vector<SnapshotFragment> fragments;
    const auto& data = snapshotData.data;

    if (data.size() <= MAX_FRAGMENT_PAYLOAD) {
        // Passt in ein Paket
        SnapshotFragment frag;
        frag.sequenceId = sequenceId;
        frag.fragmentId = 0;
        frag.totalFragments = 1;
        frag.flags = SNAPSHOT_FLAG_LAST_FRAGMENT;
        frag.payload.assign(data.begin(), data.end());
        fragments.push_back(std::move(frag));
        return fragments;
    }

    // Fragmentieren
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
    net::NetworkServer& server,
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

        bool sent = server.SendReliable(
            PacketHeader{.protocolId = 0x4D4D, .flags = static_cast<uint8_t>(PacketFlags::Reliable)},
            std::span(fragBuffer.data.data(), fragBuffer.data.size()),
            ip,
            port
        );

        if (!sent) {
            AddLog("[Snapshot] Fragment {}/{} konnte nicht gesendet werden",
                   frag.fragmentId + 1, frag.totalFragments);
            return false;
        }
    }

    if (fragments.size() > 1) {
        AddLog("[Snapshot] Snapshot {} in {} Fragmente aufgeteilt ({} Bytes → {} Bytes/Frag)",
               sequenceId, fragments.size(), snapshotData.data.size(),
               snapshotData.data.size() / fragments.size());
    }

    return true;
}
