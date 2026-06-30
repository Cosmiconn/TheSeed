#pragma once
// =============================================================================
// ecs/ecs_EntityManager.h — Entity Lifecycle Management (AP-20)
// =============================================================================
// KORREKTUR: Thread-sichere Entity-Erzeugung. Freie Entities werden aus
// einem Pool wiederverwendet. Records werden korrekt aktualisiert.
// =============================================================================
#include "ecs_Types.h"
#include "ecs_Chunk.h"
#include "../core/Log.h"

#include <vector>
#include <queue>
#include <unordered_map>
#include <mutex>

namespace ecs {

// =============================================================================
// ENTITY RECORD — Verknüpfung Entity → Chunk + Index
// =============================================================================
struct EntityRecord {
    Chunk* chunk = nullptr;
    size_t denseIndex = SIZE_MAX;
    uint32_t generation = 0;
    bool isAlive = false;
};

// =============================================================================
// ENTITY MANAGER
// =============================================================================
class EntityManager {
private:
    std::vector<EntityRecord> records;
    std::queue<EntityHandle> freeEntities;
    std::mutex mutex;
    EntityHandle nextEntityId = 1;

public:
    EntityManager() = default;
    ~EntityManager() = default;

    EntityManager(const EntityManager&) = delete;
    EntityManager& operator=(const EntityManager&) = delete;

    // ===================================================================
    // Entity Lifecycle
    // ===================================================================
    [[nodiscard]] EntityHandle CreateEntity() {
        std::lock_guard lock(mutex);

        EntityHandle entity;
        if (!freeEntities.empty()) {
            entity = freeEntities.front();
            freeEntities.pop();
            records[entity].generation++;
            records[entity].isAlive = true;
            records[entity].chunk = nullptr;
            records[entity].denseIndex = SIZE_MAX;
        } else {
            entity = nextEntityId++;
            records.resize(entity + 1);
            records[entity] = EntityRecord{
                .chunk = nullptr,
                .denseIndex = SIZE_MAX,
                .generation = 1,
                .isAlive = true
            };
        }

        return entity;
    }

    void DestroyEntity(EntityHandle entity) {
        std::lock_guard lock(mutex);
        if (entity >= records.size() || !records[entity].isAlive) return;

        records[entity].isAlive = false;
        records[entity].chunk = nullptr;
        records[entity].denseIndex = SIZE_MAX;
        freeEntities.push(entity);
    }

    [[nodiscard]] bool IsAlive(EntityHandle entity) const {
        if (entity >= records.size()) return false;
        return records[entity].isAlive;
    }

    // ===================================================================
    // Record Management
    // ===================================================================
    void UpdateRecord(EntityHandle entity, Chunk* chunk, size_t denseIndex) {
        std::lock_guard lock(mutex);
        if (entity >= records.size()) return;
        records[entity].chunk = chunk;
        records[entity].denseIndex = denseIndex;
    }

    [[nodiscard]] EntityRecord GetRecord(EntityHandle entity) const {
        if (entity >= records.size()) return EntityRecord{};
        return records[entity];
    }

    void Clear() {
        std::lock_guard lock(mutex);
        records.clear();
        while (!freeEntities.empty()) freeEntities.pop();
        nextEntityId = 1;
    }

    [[nodiscard]] size_t GetAliveCount() const {
        size_t count = 0;
        for (const auto& r : records) {
            if (r.isAlive) count++;
        }
        return count;
    }
};

} // namespace ecs
