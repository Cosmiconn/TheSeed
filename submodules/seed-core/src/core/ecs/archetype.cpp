#include "core/ecs/archetype.h"
#include "core/profiling/seed_assert.h"
#include "core/profiling/tracy_seed.h"

namespace seed::ecs {

Archetype::Archetype(ArchetypeId id,
                     std::vector<ComponentType> componentTypes,
                     std::vector<std::unique_ptr<IComponentArray>> columns,
                     seed::memory::Allocator* allocator)
    : m_id(id)
    , m_componentTypes(std::move(componentTypes))
    , m_columns(std::move(columns))
    , m_allocator(allocator)
{
    SEED_ZONE("Archetype::ctor");
}

Archetype::~Archetype() {
    SEED_ZONE("Archetype::dtor");
}

bool Archetype::hasComponent(ComponentType type) const noexcept {
    return std::binary_search(m_componentTypes.begin(), m_componentTypes.end(), type);
}

size_t Archetype::addEntity(Entity e) {
    SEED_ZONE("Archetype::addEntity");
    size_t index = m_entityCount;

    if (index >= m_entities.size()) {
        m_entities.push_back(e);
    } else {
        m_entities[index] = e;
    }

    for (auto& col : m_columns) {
        col->reserve(index + 1);
        col->defaultConstruct(index);
    }

    ++m_entityCount;
    return index;
}

void Archetype::removeEntityByIndex(size_t index) {
    SEED_ASSERT(index < m_entityCount, "removeEntityByIndex out of bounds");
    SEED_ZONE("Archetype::removeEntityByIndex");

    // FIX: Use col->remove(index) which correctly decrements ComponentArray::m_size.
    // The old code called move()/destructAt() which left m_size unchanged,
    // causing double-free in clear() when the archetype was destroyed.
    for (auto& col : m_columns) {
        col->remove(index);
    }

    if (index != m_entityCount - 1) {
        m_entities[index] = m_entities[m_entityCount - 1];
    }
    --m_entityCount;
}

Entity Archetype::entityAt(size_t index) const {
    SEED_ASSERT(index < m_entityCount, "entityAt out of bounds");
    return m_entities[index];
}

size_t Archetype::findEntityIndex(Entity e) const {
    for (size_t i = 0; i < m_entityCount; ++i) {
        if (m_entities[i] == e) return i;
    }
    return static_cast<size_t>(-1);
}

void* Archetype::getComponent(size_t index, ComponentType type) {
    return const_cast<void*>(static_cast<const Archetype*>(this)->getComponent(index, type));
}

const void* Archetype::getComponent(size_t index, ComponentType type) const {
    SEED_ASSERT(index < m_entityCount, "getComponent index out of bounds");
    for (size_t i = 0; i < m_componentTypes.size(); ++i) {
        if (m_componentTypes[i] == type) {
            return m_columns[i]->get(index);
        }
    }
    SEED_ASSERT(false, "Component type not found in archetype");
    return nullptr;
}

void Archetype::setComponent(size_t index, ComponentType type, const void* data) {
    SEED_ASSERT(index < m_entityCount, "setComponent index out of bounds");
    for (size_t i = 0; i < m_componentTypes.size(); ++i) {
        if (m_componentTypes[i] == type) {
            m_columns[i]->copy(index, data);
            return;
        }
    }
    SEED_ASSERT(false, "Component type not found in archetype");
}

IComponentArray* Archetype::getColumn(ComponentType type) {
    for (size_t i = 0; i < m_componentTypes.size(); ++i) {
        if (m_componentTypes[i] == type) {
            return m_columns[i].get();
        }
    }
    return nullptr;
}

const IComponentArray* Archetype::getColumn(ComponentType type) const {
    for (size_t i = 0; i < m_componentTypes.size(); ++i) {
        if (m_componentTypes[i] == type) {
            return m_columns[i].get();
        }
    }
    return nullptr;
}

void Archetype::moveComponent(size_t dstIndex, ComponentType type, IComponentArray* src, size_t srcIndex) {
    SEED_ASSERT(dstIndex < m_entityCount, "moveComponent dstIndex out of bounds");
    SEED_ASSERT(src != nullptr, "moveComponent src is null");
    for (size_t i = 0; i < m_componentTypes.size(); ++i) {
        if (m_componentTypes[i] == type) {
            m_columns[i]->moveFrom(dstIndex, src, srcIndex);
            return;
        }
    }
    SEED_ASSERT(false, "Component type not found in archetype for moveComponent");
}
void Archetype::destructComponentAt(size_t index, ComponentType type) {
    for (size_t i = 0; i < m_componentTypes.size(); ++i) {
        if (m_componentTypes[i] == type) {
            m_columns[i]->destructAt(index);
            return;
        }
    }
    SEED_ASSERT(false, "Component type not found in archetype for destructComponentAt");
}


size_t Archetype::capacity() const {
    return m_columns.empty() ? 0 : m_columns[0]->capacity();
}

void Archetype::reserve(size_t n) {
    m_entities.reserve(n);
    for (auto& col : m_columns) {
        col->reserve(n);
    }
}

} // namespace seed::ecs
