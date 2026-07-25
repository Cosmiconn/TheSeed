#include "core/ecs/archetype_manager.h"
#include "core/ecs/component_array.h"
#include "core/ecs/type_registry.h"
#include "core/profiling/seed_assert.h"
#include "core/profiling/tracy_seed.h"
#include <fmt/format.h>

namespace seed::ecs {

ArchetypeManager::ArchetypeManager(seed::memory::Allocator* allocator)
    : m_allocator(allocator)
{
    SEED_ASSERT(allocator != nullptr, "ArchetypeManager requires a valid allocator");
}

Archetype* ArchetypeManager::findOrCreateArchetype(const std::vector<ComponentType>& types) {
    SEED_ZONE("ArchetypeManager::findOrCreateArchetype");
    ArchetypeId id = makeArchetypeId(types);
    auto it = m_archetypes.find(id);
    if (it != m_archetypes.end()) {
        return it->second.get();
    }

    std::vector<std::unique_ptr<IComponentArray>> columns;
    columns.reserve(types.size());
    for (ComponentType t : types) {
        columns.push_back(TypeRegistry::instance().createArray(t, m_allocator));
    }

    auto arch = std::make_unique<Archetype>(id, std::vector<ComponentType>(types), std::move(columns), m_allocator);
    Archetype* ptr = arch.get();
    m_archetypes[id] = std::move(arch);
    return ptr;
}

Archetype* ArchetypeManager::getArchetype(ArchetypeId id) const {
    auto it = m_archetypes.find(id);
    return (it != m_archetypes.end()) ? it->second.get() : nullptr;
}

void ArchetypeManager::dump() const {
    fmt::print("=== ArchetypeManager Dump ===\n");
    fmt::print("Archetypes: {}\n", m_archetypes.size());
    for (const auto& [id, arch] : m_archetypes) {
        fmt::print("  Archetype {:08x}: {} entities, {} components\n",
                   id.hash, arch->size(), arch->componentTypes().size());
    }
    fmt::print("=============================\n");
}

} // namespace seed::ecs
