#pragma once

#include "core/ecs/component_array.h"
#include "core/ecs/component_traits.h"
#include "core/memory/allocator.h"
#include "core/profiling/seed_assert.h"
#include <functional>
#include <memory>
#include <typeindex>
#include <unordered_map>
#include <vector>
#include <cstdint>
#include <cstring>

namespace seed::ecs {

using ComponentArrayFactory = std::function<std::unique_ptr<IComponentArray>(seed::memory::Allocator*)>;

class TypeRegistry {
public:
    static TypeRegistry& instance();

    template<typename T>
    void registerComponent();

    // Register optional delta-compression hooks for a component type.
    // compress:  (oldData, newData, size) -> compressed blob
    // decompress: (compressed, oldData, outData, size) -> writes decompressed into outData
    template<typename T>
    void registerComponentCompressor(
        std::vector<uint8_t> (*compress)(const void* oldData, const void* newData, size_t size),
        void (*decompress)(const std::vector<uint8_t>& compressed, const void* oldData, void* outData, size_t size));

    std::unique_ptr<IComponentArray> createArray(ComponentType type, seed::memory::Allocator* alloc) const;
    bool isRegistered(ComponentType type) const;

    // Get component metadata without instantiating an array (no allocator needed)
    const ComponentMeta& getMeta(ComponentType type) const;

private:
    TypeRegistry() = default;
    std::unordered_map<ComponentType, ComponentArrayFactory> m_factories;
    std::unordered_map<ComponentType, ComponentMeta> m_metas;
    // BUGFIX (defense in depth): merkt sich, welcher C++-Typ zuletzt unter
    // einer ComponentType-id registriert wurde. Re-Registrierung DESSELBEN
    // Typs (z. B. am Anfang mehrerer Testfaelle) bleibt erlaubt; registriert
    // aber ein ANDERER typ dieselbe id, ist das eine ID-Kollision, die sonst
    // still die Factory-Tabelle ueberschreibt und zu Type-Confusion in den
    // ComponentArray-Spalten fuehrt (z. B. ein unique_ptr-Feld wird als
    // POD-Struct reinterpretiert -> SIGSEGV). Das fangen wir hier laut ab.
    std::unordered_map<ComponentType, std::type_index> m_registeredTypes;
};

template<typename T>
void TypeRegistry::registerComponent() {
    const ComponentType id = ComponentTraits<T>::id;
    const std::type_index thisType = std::type_index(typeid(T));

    auto it = m_registeredTypes.find(id);
    if (it != m_registeredTypes.end() && it->second != thisType) {
        SEED_ASSERT(false,
            "TypeRegistry: ComponentType id collision - two different "
            "component types share the same id. Check SEED_REGISTER_COMPONENT_WITH_ID "
            "usages for a duplicate/overlapping id.");
        return;
    }
    m_registeredTypes.insert_or_assign(id, thisType);

    m_factories[id] = [](seed::memory::Allocator* a) {
        return std::make_unique<ComponentArray<T>>(a);
    };
    m_metas[id] = getComponentMeta<T>();

    // Bridge to serialize::Reflect<T> if available (Monat 5 Gap Analysis fix)
    if constexpr (seed::serialize::has_reflect_v<T>) {
        using Reflect = seed::serialize::Reflect<T>;
        m_metas[id].schemaVersion = Reflect::version;
        auto& fields = m_metas[id].fields;
        fields.clear();
        std::apply([&fields](auto&&... field) {
            (fields.push_back(field), ...);
        }, Reflect::fields);

        // Gap-Analysis Fix 2.2: floatCount aus Reflection ableiten
        bool allFloats = true;
        for (const auto& f : m_metas[id].fields) {
            const char* tn = f.typeName.c_str();
            if (std::strcmp(tn, "f") != 0 && std::strcmp(tn, "float") != 0) {
                allFloats = false;
                break;
            }
        }
        if (allFloats && !m_metas[id].fields.empty()) {
            m_metas[id].floatCount = sizeof(T) / sizeof(float);
        }
    }
}

template<typename T>
void TypeRegistry::registerComponentCompressor(
    std::vector<uint8_t> (*compress)(const void* oldData, const void* newData, size_t size),
    void (*decompress)(const std::vector<uint8_t>& compressed, const void* oldData, void* outData, size_t size))
{
    const ComponentType id = ComponentTraits<T>::id;
    auto it = m_metas.find(id);
    if (it != m_metas.end()) {
        it->second.compress = compress;
        it->second.decompress = decompress;
    }
}

} // namespace seed::ecs
