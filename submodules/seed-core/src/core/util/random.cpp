#include "random.h"

namespace seed::util {

Pcg32Random::Pcg32Random(uint64_t seed) {
    state = 0;
    inc = (seed << 1u) | 1u;
    next();
    state += seed;
    next();
}

uint32_t Pcg32Random::next() {
    uint64_t oldState = state;
    state = oldState * 6364136223846793005ULL + inc;
    uint32_t xorshifted = static_cast<uint32_t>(((oldState >> 18u) ^ oldState) >> 27u);
    uint32_t rot = static_cast<uint32_t>(oldState >> 59u);
    return (xorshifted >> rot) | (xorshifted << ((-rot) & 31));
}

float Pcg32Random::nextFloat() {
    return static_cast<float>(next()) / static_cast<float>(UINT32_MAX);
}

int32_t Pcg32Random::nextInt(int32_t min, int32_t max) {
    if (min >= max) return min;
    uint32_t range = static_cast<uint32_t>(max - min);
    uint32_t threshold = (UINT32_MAX / range) * range;
    uint32_t value;
    do {
        value = next();
    } while (value >= threshold);
    return min + static_cast<int32_t>(value % range);
}

} // namespace seed::util
