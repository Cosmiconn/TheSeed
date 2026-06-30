#pragma once
// =============================================================================
// ecs/ecs_ComponentTraits.h — Komponenten-Typ-Registrierung (AP-20)
// =============================================================================
// KORREKTUR: Register()-Methode hinzugefügt für explizite ID-Zuweisung.
// =============================================================================
#include "ecs_Types.h"
#include <type_traits>
#include <cstdint>

namespace ecs {

// =============================================================================
// Komponenten-ID-Zuweisung
// =============================================================================
template<typename T>
class ComponentTraits {
    static inline ComponentTypeId s_id = INVALID_COMPONENT_TYPE;
    static inline bool s_registered = false;

public:
    static void Register(ComponentTypeId id) {
        s_id = id;
        s_registered = true;
    }

    [[nodiscard]] static ComponentTypeId GetId() {
        return s_id;
    }

    [[nodiscard]] static bool IsRegistered() {
        return s_registered;
    }

    [[nodiscard]] static constexpr size_t Size() {
        return sizeof(T);
    }

    [[nodiscard]] static constexpr size_t Align() {
        return alignof(T);
    }
};

} // namespace ecs
