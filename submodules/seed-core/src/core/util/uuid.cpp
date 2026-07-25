#include "uuid.h"
#include "random.h"
#include <cstdio>
#include <ctime>

namespace seed::util {

UUID UUID::generate() {
    UUID uuid;
    static uint64_t counter = 0;
    uint64_t seed = static_cast<uint64_t>(std::time(nullptr)) ^ counter++;
    Pcg32Random rng(seed);
    for (size_t i = 0; i < 16; ++i) {
        uuid.data[i] = static_cast<uint8_t>(rng.next() & 0xFF);
    }
    // Set version (4) and variant (10)
    uuid.data[6] = (uuid.data[6] & 0x0F) | 0x40;
    uuid.data[8] = (uuid.data[8] & 0x3F) | 0x80;
    return uuid;
}

bool UUID::operator==(const UUID& other) const {
    return data == other.data;
}

bool UUID::operator!=(const UUID& other) const {
    return !(*this == other);
}

std::string UUID::toString() const {
    char buf[37];
    std::snprintf(buf, sizeof(buf),
        "%02x%02x%02x%02x-%02x%02x-%02x%02x-%02x%02x-%02x%02x%02x%02x%02x%02x",
        data[0], data[1], data[2], data[3],
        data[4], data[5], data[6], data[7],
        data[8], data[9], data[10], data[11],
        data[12], data[13], data[14], data[15]);
    return std::string(buf);
}

} // namespace seed::util
