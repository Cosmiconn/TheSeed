#include "hash.h"

namespace seed::util {

uint32_t fnv1a32(const void* data, size_t size) {
    const uint8_t* bytes = static_cast<const uint8_t*>(data);
    uint32_t hash = 0x811c9dc5u;
    for (size_t i = 0; i < size; ++i) {
        hash ^= bytes[i];
        hash *= 0x01000193u;
    }
    return hash;
}

uint64_t fnv1a64(const void* data, size_t size) {
    const uint8_t* bytes = static_cast<const uint8_t*>(data);
    uint64_t hash = 0xcbf29ce484222325ull;
    for (size_t i = 0; i < size; ++i) {
        hash ^= bytes[i];
        hash *= 0x00000100000001b3ull;
    }
    return hash;
}

} // namespace seed::util
