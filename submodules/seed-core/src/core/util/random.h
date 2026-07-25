#pragma once

#include <cstdint>

namespace seed::util {

class Pcg32Random {
    uint64_t state = 0x853c49e6748fea9bULL;
    uint64_t inc = 0xda3e39cb94b95bdbULL;
public:
    Pcg32Random() = default;
    explicit Pcg32Random(uint64_t seed);

    uint32_t next();
    float nextFloat(); // [0, 1)
    int32_t nextInt(int32_t min, int32_t max);
};

} // namespace seed::util
