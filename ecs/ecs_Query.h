#pragma once
// =============================================================================
// ecs/ecs_Query.h — Typ-sicheres Query-System (AP-20)
// =============================================================================
// KORREKTUR: Iterator-basiertes Query-System für Range-based for loops.
// Korrekte Komponenten-Pointer-Berechnung.
// =============================================================================
#include "ecs_Types.h"
#include "ecs_Chunk.h"
#include "ecs_ComponentTraits.h"

#include <vector>
#include <tuple>
#include <cstddef>

namespace ecs {

// =============================================================================
// QUERY ITERATOR
// =============================================================================
template<typename... Components>
class QueryIterator {
private:
    std::vector<Chunk*> chunks;
    size_t chunkIndex = 0;
    size_t entityIndex = 0;

public:
    QueryIterator(std::vector<Chunk*> chunkList, size_t cIdx, size_t eIdx)
        : chunks(std::move(chunkList)), chunkIndex(cIdx), entityIndex(eIdx) {}

    // ===================================================================
    // Iterator-Interface
    // ===================================================================
    using value_type = std::tuple<EntityHandle, Components&...>;

    QueryIterator& operator++() {
        entityIndex++;
        if (chunks.empty()) return *this;

        while (chunkIndex < chunks.size() &&
               entityIndex >= chunks[chunkIndex]->GetEntityCount()) {
            chunkIndex++;
            entityIndex = 0;
        }
        return *this;
    }

    [[nodiscard]] bool operator!=(const QueryIterator& other) const {
        return chunkIndex != other.chunkIndex || entityIndex != other.entityIndex;
    }

    [[nodiscard]] value_type operator*() {
        Chunk* chunk = chunks[chunkIndex];
        EntityHandle entity = chunk->GetEntity(entityIndex);
        return std::tuple_cat(
            std::make_tuple(entity),
            std::make_tuple(std::ref(*chunk->GetComponent<Components>(entityIndex))...)
        );
    }
};

// =============================================================================
// QUERY
// =============================================================================
template<typename... Components>
class Query {
private:
    std::vector<Chunk*> chunks;

public:
    explicit Query(std::vector<Chunk*> chunkList) : chunks(std::move(chunkList)) {}

    [[nodiscard]] QueryIterator<Components...> begin() {
        size_t startChunk = 0;
        size_t startEntity = 0;

        // Überspringe leere Chunks
        while (startChunk < chunks.size() &&
               chunks[startChunk]->GetEntityCount() == 0) {
            startChunk++;
        }

        return QueryIterator<Components...>(chunks, startChunk, startEntity);
    }

    [[nodiscard]] QueryIterator<Components...> end() {
        return QueryIterator<Components...>(chunks, chunks.size(), 0);
    }

    [[nodiscard]] bool IsEmpty() const {
        for (auto* chunk : chunks) {
            if (chunk->GetEntityCount() > 0) return false;
        }
        return true;
    }

    [[nodiscard]] size_t Count() const {
        size_t count = 0;
        for (auto* chunk : chunks) {
            count += chunk->GetEntityCount();
        }
        return count;
    }
};

} // namespace ecs
