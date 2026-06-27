#pragma once
#include "Vector.h"

namespace math {

struct Quaternion {
    float x = 0.0f, y = 0.0f, z = 0.0f, w = 1.0f;

    Quaternion() = default;
    constexpr Quaternion(float x_, float y_, float z_, float w_) 
        : x(x_), y(y_), z(z_), w(w_) {}

    static Quaternion FromAxisAngle(const Vector3& axis, float angle) {
        float half = angle * 0.5f;
        float s = std::sin(half);
        Vector3 n = axis.Normalized();
        return Quaternion{n.x * s, n.y * s, n.z * s, std::cos(half)};
    }

    [[nodiscard]] Quaternion operator*(const Quaternion& o) const {
        return Quaternion{
            w*o.x + x*o.w + y*o.z - z*o.y,
            w*o.y - x*o.z + y*o.w + z*o.x,
            w*o.z + x*o.y - y*o.x + z*o.w,
            w*o.w - x*o.x - y*o.y - z*o.z
        };
    }

    [[nodiscard]] Vector3 RotateVector(const Vector3& v) const {
        Quaternion qv{v.x, v.y, v.z, 0.0f};
        Quaternion conj{-x, -y, -z, w};
        Quaternion t = *this * qv * conj;
        return Vector3{t.x, t.y, t.z};
    }

    [[nodiscard]] Matrix4x4 ToMatrix() const {
        Matrix4x4 out = Matrix4x4::Identity();
        float xx = x*x, yy = y*y, zz = z*z;
        float xy = x*y, xz = x*z, yz = y*z;
        float wx = w*x, wy = w*y, wz = w*z;
        out.m[0][0] = 1.0f - 2.0f*(yy + zz);
        out.m[0][1] = 2.0f*(xy + wz);
        out.m[0][2] = 2.0f*(xz - wy);
        out.m[1][0] = 2.0f*(xy - wz);
        out.m[1][1] = 1.0f - 2.0f*(xx + zz);
        out.m[1][2] = 2.0f*(yz + wx);
        out.m[2][0] = 2.0f*(xz + wy);
        out.m[2][1] = 2.0f*(yz - wx);
        out.m[2][2] = 1.0f - 2.0f*(xx + yy);
        return out;
    }
};

}
