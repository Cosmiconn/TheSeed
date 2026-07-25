#pragma once

#include <array>
#include <string>

namespace seed::util {

struct UUID {
    std::array<uint8_t, 16> data{};

    static UUID generate();
    bool operator==(const UUID& other) const;
    bool operator!=(const UUID& other) const;
    std::string toString() const;
};

} // namespace seed::util
