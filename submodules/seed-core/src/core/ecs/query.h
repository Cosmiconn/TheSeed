#pragma once

#include "core/ecs/archetype.h"
#include "core/ecs/component_traits.h"
#include "core/ecs/entity.h"
#include "core/profiling/tracy_seed.h"
#include <tuple>
#include <vector>

namespace seed::ecs {

class World;

template<typename... Components>
class QueryResult {
public:
    struct Iterator {
        using ArchetypeList = std::vector<Archetype*>;
        using value_type = std::tuple<Components*...>;

        ArchetypeList* archetypes;
        size_t archetypeIdx;
        size_t entityIdx;

        Iterator(ArchetypeList* arcs, size_t ai, size_t ei)
            : archetypes(arcs), archetypeIdx(ai), entityIdx(ei) {}

        bool operator!=(const Iterator& other) const {
            return archetypeIdx != other.archetypeIdx || entityIdx != other.entityIdx;
        }

        Iterator& operator++() {
            ++entityIdx;
            // Skip to next non-empty archetype
            while (archetypes && archetypeIdx < archetypes->size()) {
                if (entityIdx >= (*archetypes)[archetypeIdx]->size()) {
                    entityIdx = 0;
                    ++archetypeIdx;
                } else {
                    break;
                }
            }
            return *this;
        }

        value_type operator*() const {
            Archetype* arch = (*archetypes)[archetypeIdx];
            return std::make_tuple(arch->getComponent<Components>(entityIdx)...);
        }
    };

    explicit QueryResult(std::vector<Archetype*>&& archetypes)
        : m_archetypes(std::move(archetypes)) {}

    Iterator begin() const {
        size_t ai = 0;
        while (ai < m_archetypes.size() && m_archetypes[ai]->empty()) {
            ++ai;
        }
        return Iterator(const_cast<std::vector<Archetype*>*>(&m_archetypes), ai, 0);
    }

    Iterator end() const {
        return Iterator(const_cast<std::vector<Archetype*>*>(&m_archetypes),
                        m_archetypes.size(), 0);
    }

    size_t count() const {
        size_t total = 0;
        for (auto* arch : m_archetypes) {
            total += arch->size();
        }
        return total;
    }

    bool empty() const { return count() == 0; }

private:
    std::vector<Archetype*> m_archetypes;
};

} // namespace seed::ecs
