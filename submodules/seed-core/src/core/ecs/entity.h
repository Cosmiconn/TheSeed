#pragma once

#include <cstdint>

namespace seed::ecs {

// ---------------------------------------------------------------------------
// Entity
// ---------------------------------------------------------------------------
// 32-bit handle: [8-bit version | 24-bit index]
//  - Index: 0..16,777,215 (16.7 M entities)
//  - Version: 0..255 (recycling safety)
// ---------------------------------------------------------------------------
using Entity = uint32_t;
constexpr Entity INVALID_ENTITY = 0xFFFFFFFF;

inline constexpr uint32_t entityIndex(Entity e) noexcept {
    return e & 0x00FFFFFFu;
}

inline constexpr uint8_t entityVersion(Entity e) noexcept {
    return static_cast<uint8_t>((e >> 24) & 0xFFu);
}

inline constexpr Entity makeEntity(uint32_t index, uint8_t version) noexcept {
    return (static_cast<uint32_t>(version) << 24) | (index & 0x00FFFFFFu);
}

inline constexpr bool isValid(Entity e) noexcept {
    return e != INVALID_ENTITY;
}

} // namespace seed::ecs
