#pragma once
#include "Vector.h"

namespace math {

struct alignas(64) Matrix4x4 {
    std::array<std::array<float, 4>, 4> m{};

    Matrix4x4() = default;

    static Matrix4x4 Identity() {
        Matrix4x4 out;
        out.m[0][0] = out.m[1][1] = out.m[2][2] = out.m[3][3] = 1.0f;
        return out;
    }

    static Matrix4x4 Translation(const Vector3& t) {
        Matrix4x4 out = Identity();
        out.m[3][0] = t.x; out.m[3][1] = t.y; out.m[3][2] = t.z;
        return out;
    }

    static Matrix4x4 Scale(const Vector3& s) {
        Matrix4x4 out;
        out.m[0][0] = s.x; out.m[1][1] = s.y; out.m[2][2] = s.z; out.m[3][3] = 1.0f;
        return out;
    }

    static Matrix4x4 RotationY(float radians) {
        Matrix4x4 out = Identity();
        float c = std::cos(radians), s = std::sin(radians);
        out.m[0][0] = c;  out.m[0][2] = s;
        out.m[2][0] = -s; out.m[2][2] = c;
        return out;
    }

    [[nodiscard]] Matrix4x4 operator*(const Matrix4x4& o) const {
        Matrix4x4 out;
        for (int i = 0; i < 4; ++i)
            for (int j = 0; j < 4; ++j)
                out.m[i][j] = m[i][0]*o.m[0][j] + m[i][1]*o.m[1][j] 
                            + m[i][2]*o.m[2][j] + m[i][3]*o.m[3][j];
        return out;
    }

    [[nodiscard]] Vector3 TransformPoint(const Vector3& p) const {
        return Vector3{
            p.x*m[0][0] + p.y*m[1][0] + p.z*m[2][0] + 1.0f*m[3][0],
            p.x*m[0][1] + p.y*m[1][1] + p.z*m[2][1] + 1.0f*m[3][1],
            p.x*m[0][2] + p.y*m[1][2] + p.z*m[2][2] + 1.0f*m[3][2]
        };
    }

    [[nodiscard]] Vector3 TransformVector(const Vector3& v) const {
        return Vector3{
            v.x*m[0][0] + v.y*m[1][0] + v.z*m[2][0],
            v.x*m[0][1] + v.y*m[1][1] + v.z*m[2][1],
            v.x*m[0][2] + v.y*m[1][2] + v.z*m[2][2]
        };
    }

    static Matrix4x4 PerspectiveFovLH(float fovY, float aspect, float zn, float zf) {
        Matrix4x4 out{};
        float h = 1.0f / std::tan(fovY * 0.5f);
        float w = h / aspect;
        out.m[0][0] = w;
        out.m[1][1] = h;
        out.m[2][2] = zf / (zf - zn);
        out.m[2][3] = 1.0f;
        out.m[3][2] = -zn * zf / (zf - zn);
        return out;
    }

    static Matrix4x4 LookAtLH(const Vector3& eye, const Vector3& at, const Vector3& up) {
        Vector3 z = (at - eye).Normalized();
        Vector3 x = up.Cross(z).Normalized();
        Vector3 y = z.Cross(x);
        Matrix4x4 out = Identity();
        out.m[0][0] = x.x; out.m[0][1] = x.y; out.m[0][2] = x.z;
        out.m[1][0] = y.x; out.m[1][1] = y.y; out.m[1][2] = y.z;
        out.m[2][0] = z.x; out.m[2][1] = z.y; out.m[2][2] = z.z;
        out.m[3][0] = -x.Dot(eye);
        out.m[3][1] = -y.Dot(eye);
        out.m[3][2] = -z.Dot(eye);
        return out;
    }
};

}
