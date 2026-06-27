#pragma once
// =============================================================================
// ecs/Types.h — ECS Core Type System
// AP-20: Archetype Storage
// =============================================================================
#include <cstdint>
#include <typeindex>
#include <vector>
#include <bitset>
#include <array>

namespace ecs {

// Maximum components per archetype
inline constexpr size_t MAX_COMPONENTS = 64;

// Component type identifier using type_index hashing
using ComponentTypeId = size_t;

// Entity identifier: index + generation for safe recycling
struct EntityHandle {
    uint32_t index = UINT32_MAX;
    uint32_t generation = 0;

    [[nodiscard]] bool IsValid() const noexcept { return index != UINT32_MAX; }
    [[nodiscard]] bool operator==(const EntityHandle& o) const noexcept {
        return index == o.index && generation == o.generation;
    }
    [[nodiscard]] bool operator!=(const EntityHandle& o) const noexcept {
        return !(*this == o);
    }
};

inline constexpr EntityHandle INVALID_ENTITY{UINT32_MAX, 0};

// Archetype signature: bitset of component types
using ArchetypeSignature = std::bitset<MAX_COMPONENTS>;

// Component offset within a chunk (in bytes)
using ComponentOffset = size_t;

// Component size (in bytes)
using ComponentSize = size_t;

// Component alignment requirement
using ComponentAlign = size_t;

// Component metadata for archetype construction
struct ComponentMeta {
    ComponentTypeId typeId;
    ComponentSize size;
    ComponentAlign alignment;
    std::type_index typeIndex;
};

// Dense index into archetype's entity array (0..N, no gaps)
using ArchetypeIndex = uint32_t;

// Sparse index into global entity array (may have gaps/recycled slots)
using SparseIndex = uint32_t;

} // namespace ecs
