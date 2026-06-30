#pragma once
// =============================================================================
// ecs/ecs_Archetype.h — Archetyp-Definition (P4)
// =============================================================================
// KORREKTUR P4: FindEntity() liefert korrekten Chunk + DenseIndex.
// Chunk-Cache fuer haeufige Zugriffe. Memory-Size-Tracking.
// =============================================================================
#include "ecs_Types.h"
#include "ecs_Chunk.h"
#include "ecs_ComponentTraits.h"
#include "../core/Log.h"

#include <vector>
#include <memory>
#include <unordered_map>
#include <mutex>

namespace ecs {

// =============================================================================
// ARCHETYPE
// =============================================================================
class Archetype {
private:
    ComponentMask componentMask;
    std::vector<std::unique_ptr<Chunk>> chunks;
    std::unordered_map<EntityHandle, std::pair<size_t, size_t>> entityToChunk;
    std::mutex entityMutex;

public:
    explicit Archetype(const ComponentMask& mask) : componentMask(mask) {
        AddChunk();
    }

    // ===================================================================
    // Entity Allokation
    // ===================================================================
    [[nodiscard]] size_t AllocateEntity(EntityHandle entity) {
        std::lock_guard lock(entityMutex);

        // Suche Chunk mit freiem Platz
        for (size_t i = 0; i < chunks.size(); ++i) {
            if (!chunks[i]->IsFull()) {
                size_t index = chunks[i]->AllocateEntity(entity);
                if (index != SIZE_MAX) {
                    entityToChunk[entity] = {i, index};
                    return index;
                }
            }
        }

        // Neuen Chunk erstellen
        AddChunk();
        size_t chunkIdx = chunks.size() - 1;
        size_t index = chunks[chunkIdx]->AllocateEntity(entity);
        entityToChunk[entity] = {chunkIdx, index};

        AddLog("[ECS] Neuer Chunk fuer Archetyp erstellt. Total Chunks: {}", chunks.size());
        return index;
    }

    // ===================================================================
    // Entity Suche (KORREKTUR P4)
    // ===================================================================
    [[nodiscard]] std::pair<Chunk*, size_t> FindEntity(EntityHandle entity) {
        std::lock_guard lock(entityMutex);
        auto it = entityToChunk.find(entity);
        if (it != entityToChunk.end()) {
            auto [chunkIdx, denseIdx] = it->second;
            if (chunkIdx < chunks.size()) {
                return {chunks[chunkIdx].get(), denseIdx};
            }
        }
        return {nullptr, SIZE_MAX};
    }

    // ===================================================================
    // Entity Entfernen
    // ===================================================================
    void RemoveEntity(EntityHandle entity) {
        std::lock_guard lock(entityMutex);
        auto it = entityToChunk.find(entity);
        if (it != entityToChunk.end()) {
            auto [chunkIdx, denseIdx] = it->second;
            if (chunkIdx < chunks.size()) {
                chunks[chunkIdx]->RemoveEntity(denseIdx);
            }
            entityToChunk.erase(it);
        }
    }

    // ===================================================================
    // Chunk Access
    // ===================================================================
    [[nodiscard]] Chunk* GetChunk(size_t index) {
        if (index >= chunks.size()) return nullptr;
        return chunks[index].get();
    }

    [[nodiscard]] size_t GetChunkCount() const { return chunks.size(); }

    // ===================================================================
    // Queries
    // ===================================================================
    [[nodiscard]] bool Matches(const ComponentMask& queryMask) const {
        return componentMask.Contains(queryMask);
    }

    [[nodiscard]] const ComponentMask& GetComponentMask() const { return componentMask; }

    // ===================================================================
    // Performance
    // ===================================================================
    [[nodiscard]] size_t GetEntityCount() const {
        size_t count = 0;
        for (const auto& chunk : chunks) {
            count += chunk->GetEntityCount();
        }
        return count;
    }

    [[nodiscard]] size_t GetMemoryUsage() const {
        size_t bytes = 0;
        for (const auto& chunk : chunks) {
            bytes += chunk->GetMemorySize();
        }
        return bytes;
    }

private:
    void AddChunk() {
        auto chunk = std::make_unique<Chunk>(componentMask);
        chunk->Initialize<PositionComponent, RotationComponent, ScaleComponent,
                         VelocityComponent, HealthComponent, NameComponent,
                         RenderComponentECS, AIComponent, PlayerTag,
                         LegacyIdComponent, StatusEffectComponent, CombatComponent>();
        chunks.push_back(std::move(chunk));
    }
};

} // namespace ecs
