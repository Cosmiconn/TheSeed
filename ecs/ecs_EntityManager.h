#pragma once
// =============================================================================
// ecs/EntityManager.h — Entity Handle Lifecycle Management
// AP-20: Archetype Storage
// =============================================================================
#include "Types.h"
#include <vector>
#include <queue>
#include <mutex>

namespace ecs {

// Stores sparse array mapping: EntityHandle.index -> (generation, archetype, denseIndex)
struct EntityRecord {
    uint32_t generation = 0;
    bool isAlive = false;
    class Archetype* archetype = nullptr;  // Current archetype
    Chunk* chunk = nullptr;                // Current chunk
    ArchetypeIndex denseIndex = 0;         // Index within chunk
};

class EntityManager {
    std::vector<EntityRecord> sparseArray;
    std::queue<uint32_t> freeIndices;
    std::mutex mutex;
    uint32_t nextIndex = 0;

public:
    EntityManager() = default;

    // Create a new entity handle
    [[nodiscard]] EntityHandle CreateEntity() {
        std::lock_guard lock(mutex);

        uint32_t index;
        uint32_t generation = 1;

        if (!freeIndices.empty()) {
            index = freeIndices.front();
            freeIndices.pop();
            generation = sparseArray[index].generation + 1;
        } else {
            index = nextIndex++;
            if (index >= sparseArray.size()) {
                sparseArray.resize(index + 1);
            }
        }

        sparseArray[index] = {
            .generation = generation,
            .isAlive = true,
            .archetype = nullptr,
            .chunk = nullptr,
            .denseIndex = 0
        };

        return EntityHandle{index, generation};
    }

    // Destroy entity and recycle handle
    void DestroyEntity(EntityHandle handle) {
        std::lock_guard lock(mutex);

        if (!IsAlive(handle)) return;

        sparseArray[handle.index].isAlive = false;
        sparseArray[handle.index].archetype = nullptr;
        sparseArray[handle.index].chunk = nullptr;
        freeIndices.push(handle.index);
    }

    // Check if entity is alive
    [[nodiscard]] bool IsAlive(EntityHandle handle) const {
        if (handle.index >= sparseArray.size()) return false;
        const auto& record = sparseArray[handle.index];
        return record.isAlive && record.generation == handle.generation;
    }

    // Get entity record (for archetype transitions)
    [[nodiscard]] EntityRecord* GetRecord(EntityHandle handle) {
        if (handle.index >= sparseArray.size()) return nullptr;
        auto& record = sparseArray[handle.index];
        if (record.generation != handle.generation) return nullptr;
        return &record;
    }

    [[nodiscard]] const EntityRecord* GetRecord(EntityHandle handle) const {
        if (handle.index >= sparseArray.size()) return nullptr;
        const auto& record = sparseArray[handle.index];
        if (record.generation != handle.generation) return nullptr;
        return &record;
    }

    // Update entity location after archetype transition
    void UpdateEntityLocation(EntityHandle handle, Archetype* archetype, Chunk* chunk, ArchetypeIndex denseIndex) {
        std::lock_guard lock(mutex);
        if (handle.index >= sparseArray.size()) return;
        auto& record = sparseArray[handle.index];
        if (record.generation != handle.generation) return;

        record.archetype = archetype;
        record.chunk = chunk;
        record.denseIndex = denseIndex;
    }

    // Get current entity count
    [[nodiscard]] uint32_t GetAliveCount() const {
        uint32_t count = 0;
        for (const auto& record : sparseArray) {
            if (record.isAlive) count++;
        }
        return count;
    }

    // Get total allocated slots (including dead)
    [[nodiscard]] uint32_t GetCapacity() const {
        return static_cast<uint32_t>(sparseArray.size());
    }
};

} // namespace ecs
