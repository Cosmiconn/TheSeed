#pragma once
// =============================================================================
// ecs/Query.h — Type-safe Query System for Archetype Matching
// AP-20: Archetype Storage
// =============================================================================
#include "Types.h"
#include "ComponentTraits.h"
#include "Archetype.h"
#include <vector>
#include <type_traits>
#include <tuple>

namespace ecs {

// Query filter: which components an archetype must have
class QueryFilter {
    ArchetypeSignature required;
    ArchetypeSignature excluded;

public:
    QueryFilter() = default;

    template<typename... Components>
    [[nodiscard]] static QueryFilter With() {
        QueryFilter filter;
        (filter.required.set(ComponentId<Components>()), ...);
        return filter;
    }

    template<typename... Components>
    [[nodiscard]] static QueryFilter Without() {
        QueryFilter filter;
        (filter.excluded.set(ComponentId<Components>()), ...);
        return filter;
    }

    template<typename... Components>
    QueryFilter& With() {
        (required.set(ComponentId<Components>()), ...);
        return *this;
    }

    template<typename... Components>
    QueryFilter& Without() {
        (excluded.set(ComponentId<Components>()), ...);
        return *this;
    }

    [[nodiscard]] bool Matches(const ArchetypeSignature& archetype) const noexcept {
        return (archetype & required) == required && (archetype & excluded).none();
    }

    [[nodiscard]] const ArchetypeSignature& GetRequired() const noexcept { return required; }
    [[nodiscard]] const ArchetypeSignature& GetExcluded() const noexcept { return excluded; }
};

// Query result iterator
class QueryResult {
    std::vector<Archetype*> matchingArchetypes;

public:
    explicit QueryResult(std::vector<Archetype*> archetypes) 
        : matchingArchetypes(std::move(archetypes)) {}

    [[nodiscard]] bool IsEmpty() const noexcept { return matchingArchetypes.empty(); }
    [[nodiscard]] size_t GetArchetypeCount() const noexcept { return matchingArchetypes.size(); }

    // Iterate over all matching archetypes
    template<typename Func>
    void ForEachArchetype(Func&& func) {
        for (auto* archetype : matchingArchetypes) {
            func(*archetype);
        }
    }

    // Iterate over all entities in all matching archetypes
    // Func signature: void(Chunk* chunk, ArchetypeIndex idx)
    template<typename Func>
    void ForEachEntity(Func&& func) {
        for (auto* archetype : matchingArchetypes) {
            archetype->ForEach(func);
        }
    }

    // Get component arrays for batch processing (SIMD-friendly)
    // Returns all chunks from matching archetypes
    [[nodiscard]] std::vector<Chunk*> GetChunks() const {
        std::vector<Chunk*> result;
        for (auto* archetype : matchingArchetypes) {
            for (auto& chunk : archetype->GetChunks()) {
                result.push_back(chunk.get());
            }
        }
        return result;
    }
};

// Typed query for specific component combinations
// Usage: auto query = world.Query<TransformComponent, HealthComponent>();
template<typename... Components>
class TypedQuery {
    static_assert(sizeof...(Components) > 0, "Query must have at least one component");

    QueryResult result;

public:
    explicit TypedQuery(QueryResult r) : result(std::move(r)) {}

    // Iterate with typed component pointers
    // Func signature: void(EntityHandle, Components*...)
    template<typename Func>
    void ForEach(Func&& func) {
        static constexpr std::array<ComponentTypeId, sizeof...(Components)> componentIds = {
            ComponentId<Components>()...
        };

        result.ForEachEntity([&](Chunk* chunk, ArchetypeIndex idx) {
            EntityHandle handle = chunk->GetEntityHandle(idx);
            // Get all component pointers
            auto pointers = std::make_tuple(chunk->GetComponent<Components>(idx, ComponentId<Components>())...);
            std::apply([&](auto*... ptrs) {
                func(handle, ptrs...);
            }, pointers);
        });
    }

    [[nodiscard]] bool IsEmpty() const noexcept { return result.IsEmpty(); }
};

} // namespace ecs
