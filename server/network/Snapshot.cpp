// =============================================================================
// server/network/Snapshot.cpp — World Snapshot Implementation (AP-37)
// =============================================================================
#include "Snapshot.h"
#include "../../core/Log.h"
#include <cstring>
#include <algorithm>

namespace net {

// =============================================================================
// SnapshotBuilder
// =============================================================================
SnapshotBuilder::SnapshotBuilder(float rate) : snapshotRate(rate) {}

Snapshot SnapshotBuilder::BuildFull(const ecs::EcsWorld& world) {
    Snapshot snapshot;
    snapshot.sequence = nextSequence++;
    snapshot.serverTime = static_cast<float>(std::chrono::duration<double>(
        std::chrono::steady_clock::now().time_since_epoch()).count());
    snapshot.isFullSnapshot = true;
    snapshot.baseSequence = 0;

    // Query all entities with Transform (minimum for replication)
    auto query = world.Query<ecs::All<game::Transform>>();

    for (auto [handle] : query) {
        if (snapshot.entities.size() >= maxEntitiesPerSnapshot) break;

        auto state = ExtractEntityState(handle, world);
        snapshot.entities.push_back(state);
        UpdateLastState(state.entityId, state);
    }

    AddLog("[Snapshot] Full snapshot #{} with {} entities", snapshot.sequence, snapshot.entities.size());
    return snapshot;
}

Snapshot SnapshotBuilder::BuildDelta(const ecs::EcsWorld& world, uint32_t ackedSequence) {
    Snapshot snapshot;
    snapshot.sequence = nextSequence++;
    snapshot.serverTime = static_cast<float>(std::chrono::duration<double>(
        std::chrono::steady_clock::now().time_since_epoch()).count());
    snapshot.isFullSnapshot = false;
    snapshot.baseSequence = ackedSequence;

    auto query = world.Query<ecs::All<game::Transform>>();

    for (auto [handle] : query) {
        if (snapshot.entities.size() >= maxEntitiesPerSnapshot) break;

        auto state = ExtractEntityState(handle, world);

        // Only include if entity is new or changed
        if (HasEntityChanged(state.entityId, state)) {
            snapshot.entities.push_back(state);
            UpdateLastState(state.entityId, state);
        }
    }

    // Cleanup old states periodically
    if (ackedSequence > 0) {
        CleanupOldStates(ackedSequence);
    }

    AddLog("[Snapshot] Delta snapshot #{} with {} changed entities", 
           snapshot.sequence, snapshot.entities.size());
    return snapshot;
}

EntityState SnapshotBuilder::ExtractEntityState(ecs::EntityHandle handle, const ecs::EcsWorld& world) {
    EntityState state;
    state.entityId = handle.GetIndex(); // Or however EcsWorld exposes this

    // Get Transform (always present in query)
    auto* transform = world.GetComponent<game::Transform>(handle);
    if (transform) {
        state.posX = transform->x;
        state.posY = transform->y;
        state.posZ = transform->z;
        state.rotY = transform->rotationY;
        state.fieldMask |= EntityState::FIELD_POS | EntityState::FIELD_ROT;
    }

    // Get Health
    auto* health = world.GetComponent<game::Health>(handle);
    if (health) {
        state.currentHP = health->current;
        state.maxHP = health->max;
        state.fieldMask |= EntityState::FIELD_HP;
    }

    // Get Animation
    auto* anim = world.GetComponent<game::Animation>(handle);
    if (anim) {
        // Map string state to uint8
        if (anim->currentState == "idle") state.animState = 0;
        else if (anim->currentState == "walk") state.animState = 1;
        else if (anim->currentState == "attack") state.animState = 2;
        else if (anim->currentState == "death") state.animState = 3;
        state.animTime = anim->normalizedTime;
        state.fieldMask |= EntityState::FIELD_ANIM;
    }

    // Get Status Effects
    auto* statusList = world.GetComponent<game::StatusEffectList>(handle);
    if (statusList) {
        for (const auto& effect : statusList->effects) {
            state.statusMask |= (1u << static_cast<uint8_t>(effect.type));
        }
        state.fieldMask |= EntityState::FIELD_STATUS;
    }

    return state;
}

bool SnapshotBuilder::HasEntityChanged(uint32_t entityId, const EntityState& current) const {
    auto it = lastEntityStates.find(entityId);
    if (it == lastEntityStates.end()) return true; // New entity

    const auto& last = it->second;

    // Compare relevant fields
    if ((current.fieldMask & EntityState::FIELD_POS) && 
        (last.posX != current.posX || last.posY != current.posY || last.posZ != current.posZ)) {
        return true;
    }
    if ((current.fieldMask & EntityState::FIELD_ROT) && last.rotY != current.rotY) return true;
    if ((current.fieldMask & EntityState::FIELD_HP) && last.currentHP != current.currentHP) return true;
    if ((current.fieldMask & EntityState::FIELD_ANIM) && 
        (last.animState != current.animState || std::abs(last.animTime - current.animTime) > 0.1f)) {
        return true;
    }
    if ((current.fieldMask & EntityState::FIELD_STATUS) && last.statusMask != current.statusMask) return true;

    return false;
}

void SnapshotBuilder::UpdateLastState(uint32_t entityId, const EntityState& state) {
    lastEntityStates[entityId] = state;
}

void SnapshotBuilder::CleanupOldStates(uint32_t minSequence) {
    // Remove entities that no longer exist (not in any recent snapshot)
    std::erase_if(lastEntityStates, [](const auto& pair) {
        // Keep for a few snapshots, then remove if not seen
        // Simplified: just keep all for now, memory is cheap
        return false;
    });
}

// =============================================================================
// Serialization (Binary, network-efficient)
// =============================================================================
std::vector<uint8_t> SnapshotBuilder::Serialize(const Snapshot& snapshot) const {
    std::vector<uint8_t> buffer;
    buffer.reserve(1024 + snapshot.entities.size() * 32); // Rough estimate

    // Header
    WriteUInt32(buffer, snapshot.sequence);
    WriteFloat(buffer, snapshot.serverTime);
    WriteUInt32(buffer, snapshot.baseSequence);
    WriteUInt8(buffer, snapshot.isFullSnapshot ? 1 : 0);

    // Destroyed entities
    WriteUInt16(buffer, static_cast<uint16_t>(snapshot.destroyedEntities.size()));
    for (uint32_t id : snapshot.destroyedEntities) {
        WriteUInt32(buffer, id);
    }

    // Active entities
    WriteUInt16(buffer, static_cast<uint16_t>(snapshot.entities.size()));
    for (const auto& entity : snapshot.entities) {
        WriteUInt32(buffer, entity.entityId);
        WriteUInt16(buffer, entity.fieldMask);

        if (entity.fieldMask & EntityState::FIELD_POS) {
            WriteFloat(buffer, entity.posX);
            WriteFloat(buffer, entity.posY);
            WriteFloat(buffer, entity.posZ);
        }
        if (entity.fieldMask & EntityState::FIELD_ROT) {
            WriteFloat(buffer, entity.rotY);
        }
        if (entity.fieldMask & EntityState::FIELD_HP) {
            WriteUInt16(buffer, static_cast<uint16_t>(entity.currentHP));
            WriteUInt16(buffer, static_cast<uint16_t>(entity.maxHP));
        }
        if (entity.fieldMask & EntityState::FIELD_ANIM) {
            WriteUInt8(buffer, entity.animState);
            WriteFloat(buffer, entity.animTime);
        }
        if (entity.fieldMask & EntityState::FIELD_STATUS) {
            WriteUInt32(buffer, entity.statusMask);
        }
    }

    return buffer;
}

std::optional<Snapshot> SnapshotBuilder::Deserialize(std::span<const uint8_t> data) const {
    if (data.size() < 14) return std::nullopt; // Minimum header size

    Snapshot snapshot;
    auto buf = data;

    try {
        snapshot.sequence = ReadUInt32(buf);
        snapshot.serverTime = ReadFloat(buf);
        snapshot.baseSequence = ReadUInt32(buf);
        snapshot.isFullSnapshot = ReadUInt8(buf) != 0;

        // Destroyed entities
        uint16_t destroyedCount = ReadUInt16(buf);
        for (uint16_t i = 0; i < destroyedCount; ++i) {
            snapshot.destroyedEntities.push_back(ReadUInt32(buf));
        }

        // Active entities
        uint16_t entityCount = ReadUInt16(buf);
        for (uint16_t i = 0; i < entityCount; ++i) {
            EntityState entity;
            entity.entityId = ReadUInt32(buf);
            entity.fieldMask = ReadUInt16(buf);

            if (entity.fieldMask & EntityState::FIELD_POS) {
                entity.posX = ReadFloat(buf);
                entity.posY = ReadFloat(buf);
                entity.posZ = ReadFloat(buf);
            }
            if (entity.fieldMask & EntityState::FIELD_ROT) {
                entity.rotY = ReadFloat(buf);
            }
            if (entity.fieldMask & EntityState::FIELD_HP) {
                entity.currentHP = static_cast<int32_t>(ReadUInt16(buf));
                entity.maxHP = static_cast<int32_t>(ReadUInt16(buf));
            }
            if (entity.fieldMask & EntityState::FIELD_ANIM) {
                entity.animState = ReadUInt8(buf);
                entity.animTime = ReadFloat(buf);
            }
            if (entity.fieldMask & EntityState::FIELD_STATUS) {
                entity.statusMask = ReadUInt32(buf);
            }

            snapshot.entities.push_back(entity);
        }
    } catch (...) {
        return std::nullopt;
    }

    return snapshot;
}

// =============================================================================
// Write/Read Helpers (Little Endian)
// =============================================================================
void SnapshotBuilder::WriteUInt32(std::vector<uint8_t>& buf, uint32_t val) const {
    buf.push_back(static_cast<uint8_t>(val & 0xFF));
    buf.push_back(static_cast<uint8_t>((val >> 8) & 0xFF));
    buf.push_back(static_cast<uint8_t>((val >> 16) & 0xFF));
    buf.push_back(static_cast<uint8_t>((val >> 24) & 0xFF));
}

void SnapshotBuilder::WriteFloat(std::vector<uint8_t>& buf, float val) const {
    uint32_t bits;
    static_assert(sizeof(bits) == sizeof(val), "Float size mismatch");
    std::memcpy(&bits, &val, sizeof(val));
    WriteUInt32(buf, bits);
}

void SnapshotBuilder::WriteUInt16(std::vector<uint8_t>& buf, uint16_t val) const {
    buf.push_back(static_cast<uint8_t>(val & 0xFF));
    buf.push_back(static_cast<uint8_t>((val >> 8) & 0xFF));
}

void SnapshotBuilder::WriteUInt8(std::vector<uint8_t>& buf, uint8_t val) const {
    buf.push_back(val);
}

uint32_t SnapshotBuilder::ReadUInt32(std::span<const uint8_t>& buf) const {
    if (buf.size() < 4) throw std::runtime_error("Buffer underrun");
    uint32_t val = static_cast<uint32_t>(buf[0]) |
                   (static_cast<uint32_t>(buf[1]) << 8) |
                   (static_cast<uint32_t>(buf[2]) << 16) |
                   (static_cast<uint32_t>(buf[3]) << 24);
    buf = buf.subspan(4);
    return val;
}

float SnapshotBuilder::ReadFloat(std::span<const uint8_t>& buf) const {
    uint32_t bits = ReadUInt32(buf);
    float val;
    std::memcpy(&val, &bits, sizeof(val));
    return val;
}

uint16_t SnapshotBuilder::ReadUInt16(std::span<const uint8_t>& buf) const {
    if (buf.size() < 2) throw std::runtime_error("Buffer underrun");
    uint16_t val = static_cast<uint16_t>(buf[0]) |
                   (static_cast<uint16_t>(buf[1]) << 8);
    buf = buf.subspan(2);
    return val;
}

uint8_t SnapshotBuilder::ReadUInt8(std::span<const uint8_t>& buf) const {
    if (buf.empty()) throw std::runtime_error("Buffer underrun");
    uint8_t val = buf[0];
    buf = buf.subspan(1);
    return val;
}

// =============================================================================
// InterestManager
// =============================================================================
void InterestManager::UpdateClientView(uint32_t clientId, float x, float z, float radius) {
    clientViews[clientId] = {x, z, radius, 0};
}

void InterestManager::RemoveClient(uint32_t clientId) {
    clientViews.erase(clientId);
}

std::vector<uint32_t> InterestManager::FilterEntities(
    uint32_t clientId,
    std::span<const EntityState> allEntities) const {

    auto it = clientViews.find(clientId);
    if (it == clientViews.end()) return {};

    const auto& view = it->second;
    std::vector<uint32_t> result;

    for (const auto& entity : allEntities) {
        float dx = entity.posX - view.x;
        float dz = entity.posZ - view.z;
        float dist = std::sqrt(dx*dx + dz*dz);

        if (dist <= view.radius) {
            result.push_back(entity.entityId);
        }
    }

    return result;
}

uint32_t InterestManager::CalculatePriority(
    float clientX, float clientZ,
    float entityX, float entityZ) const {

    float dx = entityX - clientX;
    float dz = entityZ - clientZ;
    float dist = std::sqrt(dx*dx + dz*dz);

    // Closer = higher priority (0-255)
    if (dist < 10.0f) return 255;
    if (dist < 50.0f) return 200;
    if (dist < 100.0f) return 150;
    if (dist < 200.0f) return 100;
    return 50;
}

} // namespace net
