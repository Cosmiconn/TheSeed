#pragma once

#include "vec3.h"
#include "quat.h"
#include <cmath>

namespace seed::math {

struct Mat4 {
    float m[16] = {
        1,0,0,0,
        0,1,0,0,
        0,0,1,0,
        0,0,0,1
    };

    constexpr Mat4() = default;

    static Mat4 identity() { return Mat4(); }

    float& operator()(int row, int col) { return m[row * 4 + col]; }
    const float& operator()(int row, int col) const { return m[row * 4 + col]; }
};

inline Mat4 operator*(const Mat4& a, const Mat4& b) {
    Mat4 r;
    for (int i = 0; i < 4; ++i) {
        for (int j = 0; j < 4; ++j) {
            r(i,j) = 0.0f;
            for (int k = 0; k < 4; ++k) {
                r(i,j) += a(i,k) * b(k,j);
            }
        }
    }
    return r;
}

inline Mat4 perspective(float fovY, float aspect, float nearPlane, float farPlane) {
    Mat4 r;
    float tanHalfFov = std::tan(fovY * 0.5f);
    r(0,0) = 1.0f / (aspect * tanHalfFov);
    r(1,1) = 1.0f / tanHalfFov;
    r(2,2) = -(farPlane + nearPlane) / (farPlane - nearPlane);
    r(2,3) = -1.0f;
    r(3,2) = -(2.0f * farPlane * nearPlane) / (farPlane - nearPlane);
    r(3,3) = 0.0f;
    return r;
}

inline Mat4 lookAt(const Vec3& eye, const Vec3& center, const Vec3& up) {
    Vec3 f = normalize(center - eye);
    Vec3 s = normalize(cross(f, up));
    Vec3 u = cross(s, f);

    Mat4 r;
    r(0,0) = s.x; r(0,1) = s.y; r(0,2) = s.z; r(0,3) = 0.0f;
    r(1,0) = u.x; r(1,1) = u.y; r(1,2) = u.z; r(1,3) = 0.0f;
    r(2,0) = -f.x; r(2,1) = -f.y; r(2,2) = -f.z; r(2,3) = 0.0f;
    r(3,0) = -dot(s, eye); r(3,1) = -dot(u, eye); r(3,2) = dot(f, eye); r(3,3) = 1.0f;
    return r;
}

inline Mat4 toMatrix(const Quat& q) {
    Mat4 r;
    float xx = q.x * q.x, yy = q.y * q.y, zz = q.z * q.z;
    float xy = q.x * q.y, xz = q.x * q.z, yz = q.y * q.z;
    float wx = q.w * q.x, wy = q.w * q.y, wz = q.w * q.z;

    r(0,0) = 1.0f - 2.0f * (yy + zz);
    r(0,1) = 2.0f * (xy - wz);
    r(0,2) = 2.0f * (xz + wy);
    r(1,0) = 2.0f * (xy + wz);
    r(1,1) = 1.0f - 2.0f * (xx + zz);
    r(1,2) = 2.0f * (yz - wx);
    r(2,0) = 2.0f * (xz - wy);
    r(2,1) = 2.0f * (yz + wx);
    r(2,2) = 1.0f - 2.0f * (xx + yy);
    return r;
}

} // namespace seed::math
