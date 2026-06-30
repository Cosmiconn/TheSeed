#pragma once
// =============================================================================
// ecs/ecs_Types.h — ECS-Kerntypen (AP-20)
// =============================================================================
// KORREKTUR: Vollständige Typ-Definitionen für das ECS.
// =============================================================================
#include <cstdint>
#include <cstddef>
#include <bitset>

namespace ecs {

// =============================================================================
// ENTITY HANDLE
// =============================================================================
using EntityHandle = uint32_t;
constexpr EntityHandle INVALID_ENTITY = 0;
constexpr size_t MAX_COMPONENTS = 64;

// =============================================================================
// KOMPONENTEN-TYP-ID
// =============================================================================
using ComponentTypeId = uint8_t;
constexpr ComponentTypeId INVALID_COMPONENT_TYPE = 255;

// =============================================================================
// COMPONENT MASK
// =============================================================================
class ComponentMask {
private:
    std::bitset<MAX_COMPONENTS> bits;

public:
    void Set(ComponentTypeId id) { bits.set(id); }
    void Clear(ComponentTypeId id) { bits.reset(id); }
    [[nodiscard]] bool Test(ComponentTypeId id) const { return bits.test(id); }
    [[nodiscard]] bool Contains(const ComponentMask& other) const {
        return (bits & other.bits) == other.bits;
    }
    [[nodiscard]] bool Matches(const ComponentMask& other) const {
        return Contains(other);
    }
    [[nodiscard]] bool operator==(const ComponentMask& other) const {
        return bits == other.bits;
    }
    [[nodiscard]] bool operator!=(const ComponentMask& other) const {
        return bits != other.bits;
    }
    [[nodiscard]] auto Hash() const { return std::hash<std::bitset<MAX_COMPONENTS>>{}(bits); }
};

} // namespace ecs

// =============================================================================
// HASH FUER COMPONENTMASK (fuer unordered_map)
// =============================================================================
namespace std {
    template<>
    struct hash<ecs::ComponentMask> {
        [[nodiscard]] size_t operator()(const ecs::ComponentMask& mask) const {
            return mask.Hash();
        }
    };
}
