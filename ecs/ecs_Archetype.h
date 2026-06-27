#pragma once
// =============================================================================
// ecs/Archetype.h — Archetype Definition and Chunk Management
// AP-20: Archetype Storage
// =============================================================================
#include "Types.h"
#include "ComponentTraits.h"
#include "Chunk.h"
#include <vector>
#include <memory>
#include <algorithm>
#include <ranges>

namespace ecs {

// An Archetype represents a unique combination of component types
// All entities with the same archetype are stored in chunks of SOA layout
class Archetype {
    ArchetypeSignature signature;
    std::vector<ComponentMeta> componentMetas;
    std::vector<std::unique_ptr<Chunk>> chunks;
    uint32_t totalEntityCount = 0;

public:
    Archetype() = default;

    explicit Archetype(const std::vector<ComponentMeta>& metas) : componentMetas(metas) {
        for (const auto& meta : metas) {
            signature.set(meta.typeId);
        }
    }

    [[nodiscard]] const ArchetypeSignature& GetSignature() const noexcept { return signature; }
    [[nodiscard]] const std::vector<ComponentMeta>& GetComponentMetas() const noexcept { return componentMetas; }
    [[nodiscard]] uint32_t GetEntityCount() const noexcept { return totalEntityCount; }

    // Check if this archetype has all components of another signature
    [[nodiscard]] bool HasComponents(const ArchetypeSignature& required) const noexcept {
        return (signature & required) == required;
    }

    // Check if this archetype has a specific component
    [[nodiscard]] bool HasComponent(ComponentTypeId typeId) const noexcept {
        return signature.test(typeId);
    }

    // Allocate a new entity in this archetype
    // Returns: (chunk pointer, dense index within chunk)
    [[nodiscard]] std::pair<Chunk*, ArchetypeIndex> AllocateEntity(EntityHandle handle) {
        // Find chunk with space
        Chunk* targetChunk = nullptr;
        for (auto& chunk : chunks) {
            if (!chunk->IsFull()) {
                targetChunk = chunk.get();
                break;
            }
        }

        // Create new chunk if all full
        if (!targetChunk) {
            auto newChunk = std::make_unique<Chunk>(componentMetas);
            targetChunk = newChunk.get();
            chunks.push_back(std::move(newChunk));
        }

        ArchetypeIndex idx = targetChunk->AllocateEntity(handle);
        totalEntityCount++;

        return {targetChunk, idx};
    }

    // Remove entity from chunk (swap-remove)
    // Returns: handle that was moved into this slot (for sparse array update)
    [[nodiscard]] EntityHandle RemoveEntity(Chunk* chunk, ArchetypeIndex idx) {
        EntityHandle moved = chunk->RemoveEntity(idx);
        totalEntityCount--;

        // Remove empty chunks (except keep one for reuse)
        chunks.erase(
            std::ranges::remove_if(chunks,
                [](const auto& c){ return c->IsEmpty() && c->Capacity() > 0; }).begin(),
            chunks.end());

        return moved;
    }

    // Get all chunks for iteration
    [[nodiscard]] const std::vector<std::unique_ptr<Chunk>>& GetChunks() const noexcept { return chunks; }
    [[nodiscard]] std::vector<std::unique_ptr<Chunk>>& GetChunks() noexcept { return chunks; }

    // Find which chunk contains an entity (linear search - could be optimized with reverse mapping)
    [[nodiscard]] std::pair<Chunk*, ArchetypeIndex> FindEntity(EntityHandle handle) const {
        for (auto& chunk : chunks) {
            for (uint32_t i = 0; i < chunk->Count(); ++i) {
                if (chunk->GetEntityHandle(i) == handle) {
                    return {chunk.get(), i};
                }
            }
        }
        return {nullptr, 0};
    }

    // Iterate over all entities across all chunks
    template<typename Func>
    void ForEach(Func&& func) {
        for (auto& chunk : chunks) {
            for (uint32_t i = 0; i < chunk->Count(); ++i) {
                func(chunk.get(), i);
            }
        }
    }

    // Get component meta by type ID
    [[nodiscard]] const ComponentMeta* GetComponentMeta(ComponentTypeId typeId) const {
        auto it = std::ranges::find_if(componentMetas,
            [typeId](const auto& m){ return m.typeId == typeId; });
        return it != componentMetas.end() ? &(*it) : nullptr;
    }
};

} // namespace ecs
