#pragma once

#include <memory>
#include <unordered_map>
#include <vector>

#include "core/ecs/archetype.h"
#include "core/memory/allocator.h"

namespace seed::ecs {

class ArchetypeManager {
public:
    explicit ArchetypeManager(seed::memory::Allocator* allocator);

    Archetype* findOrCreateArchetype(const std::vector<ComponentType>& types);
    Archetype* getArchetype(ArchetypeId id) const;

    size_t archetypeCount() const { return m_archetypes.size(); }

    // Iteration support for queries
    auto begin() const { return m_archetypes.begin(); }
    auto end() const { return m_archetypes.end(); }

    void dump() const;

private:
    seed::memory::Allocator* m_allocator;
    std::unordered_map<ArchetypeId, std::unique_ptr<Archetype>, ArchetypeIdHash> m_archetypes;
};

} // namespace seed::ecs
