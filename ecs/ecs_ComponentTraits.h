#pragma once
// =============================================================================
// ecs/ComponentTraits.h — Type-safe Component Registration
// AP-20: Archetype Storage
// =============================================================================
#include "Types.h"
#include <type_traits>
#include <mutex>
#include <unordered_map>

namespace ecs {

class ComponentRegistry {
    std::mutex mutex;
    std::unordered_map<std::type_index, ComponentTypeId> typeToId;
    std::vector<ComponentMeta> metas;
    ComponentTypeId nextId = 0;

public:
    static ComponentRegistry& Instance() {
        static ComponentRegistry inst;
        return inst;
    }

    template<typename T>
    [[nodiscard]] ComponentTypeId Register() {
        static_assert(std::is_trivially_copyable_v<T>, 
                      "ECS components must be trivially copyable");

        std::lock_guard lock(mutex);
        std::type_index ti(typeid(T));

        auto it = typeToId.find(ti);
        if (it != typeToId.end()) return it->second;

        ComponentTypeId id = nextId++;
        if (id >= MAX_COMPONENTS) {
            throw std::runtime_error("Exceeded MAX_COMPONENTS limit");
        }

        typeToId[ti] = id;
        metas.push_back({
            .typeId = id,
            .size = sizeof(T),
            .alignment = alignof(T),
            .typeIndex = ti
        });

        return id;
    }

    template<typename T>
    [[nodiscard]] ComponentTypeId GetId() const {
        std::type_index ti(typeid(T));
        auto it = typeToId.find(ti);
        if (it == typeToId.end()) {
            throw std::runtime_error("Component type not registered");
        }
        return it->second;
    }

    [[nodiscard]] const ComponentMeta& GetMeta(ComponentTypeId id) const {
        return metas[id];
    }

    [[nodiscard]] bool IsRegistered(ComponentTypeId id) const {
        return id < metas.size();
    }
};

// Helper to get component ID at compile time (lazy registration)
template<typename T>
[[nodiscard]] inline ComponentTypeId ComponentId() {
    static ComponentTypeId id = ComponentRegistry::Instance().Register<T>();
    return id;
}

} // namespace ecs
