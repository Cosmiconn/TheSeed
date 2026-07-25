#include "core/profiling/seed_assert.h"
#include "core/ecs/type_registry.h"

namespace seed::ecs {

TypeRegistry& TypeRegistry::instance() {
    static TypeRegistry s_instance;
    return s_instance;
}

std::unique_ptr<IComponentArray> TypeRegistry::createArray(ComponentType type,
                                                            seed::memory::Allocator* alloc) const {
    auto it = m_factories.find(type);
    if (it != m_factories.end()) {
        return it->second(alloc);
    }
    return nullptr;
}

bool TypeRegistry::isRegistered(ComponentType type) const {
    return m_factories.find(type) != m_factories.end();
}

const ComponentMeta& TypeRegistry::getMeta(ComponentType type) const {
    static const ComponentMeta s_emptyMeta{};
    auto it = m_metas.find(type);
    if (it != m_metas.end()) {
        return it->second;
    }
    return s_emptyMeta;
}

} // namespace seed::ecs
