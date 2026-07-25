#pragma once

#include "vec3.h"
#include <cmath>

namespace seed::math {

struct Quat {
    float x = 0.0f, y = 0.0f, z = 0.0f, w = 1.0f;

    constexpr Quat() = default;
    constexpr Quat(float x_, float y_, float z_, float w_) : x(x_), y(y_), z(z_), w(w_) {}

    constexpr bool operator==(const Quat& o) const {
        return x == o.x && y == o.y && z == o.z && w == o.w;
    }
    constexpr bool operator!=(const Quat& o) const { return !(*this == o); }
};

inline Quat fromAxisAngle(const Vec3& axis, float angle) {
    float half = angle * 0.5f;
    float s = std::sin(half);
    float c = std::cos(half);
    Vec3 n = normalize(axis);
    return Quat(n.x * s, n.y * s, n.z * s, c);
}

} // namespace seed::math
