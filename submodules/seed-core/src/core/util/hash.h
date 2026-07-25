#pragma once

#include <cstdint>
#include <cstddef>

namespace seed::util {

uint32_t fnv1a32(const void* data, size_t size);
uint64_t fnv1a64(const void* data, size_t size);

template<typename T>
inline uint32_t hashCombine(uint32_t seed, const T& v) {
    uint32_t hash = fnv1a32(&v, sizeof(T));
    return seed ^ (hash + 0x9e3779b9 + (seed << 6) + (seed >> 2));
}

} // namespace seed::util
