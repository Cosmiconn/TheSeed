#pragma once
#include "Vector.h"
#include "Matrix.h"

namespace math {

struct Plane {
    Vector3 normal;
    float distance = 0.0f;
    [[nodiscard]] float DistanceToPoint(const Vector3& p) const {
        return normal.Dot(p) + distance;
    }
};

struct Frustum {
    Plane planes[6];

    static Frustum FromMatrix(const Matrix4x4& mvp) {
        Frustum f;
        f.planes[0] = Plane{Vector3{mvp.m[0][3]+mvp.m[0][0], mvp.m[1][3]+mvp.m[1][0], mvp.m[2][3]+mvp.m[2][0]}.Normalized(), mvp.m[3][3]+mvp.m[3][0]};
        f.planes[1] = Plane{Vector3{mvp.m[0][3]-mvp.m[0][0], mvp.m[1][3]-mvp.m[1][0], mvp.m[2][3]-mvp.m[2][0]}.Normalized(), mvp.m[3][3]-mvp.m[3][0]};
        f.planes[2] = Plane{Vector3{mvp.m[0][3]+mvp.m[0][1], mvp.m[1][3]+mvp.m[1][1], mvp.m[2][3]+mvp.m[2][1]}.Normalized(), mvp.m[3][3]+mvp.m[3][1]};
        f.planes[3] = Plane{Vector3{mvp.m[0][3]-mvp.m[0][1], mvp.m[1][3]-mvp.m[1][1], mvp.m[2][3]-mvp.m[2][1]}.Normalized(), mvp.m[3][3]-mvp.m[3][1]};
        f.planes[4] = Plane{Vector3{mvp.m[0][3]+mvp.m[0][2], mvp.m[1][3]+mvp.m[1][2], mvp.m[2][3]+mvp.m[2][2]}.Normalized(), mvp.m[3][3]+mvp.m[3][2]};
        f.planes[5] = Plane{Vector3{mvp.m[0][3]-mvp.m[0][2], mvp.m[1][3]-mvp.m[1][2], mvp.m[2][3]-mvp.m[2][2]}.Normalized(), mvp.m[3][3]-mvp.m[3][2]};
        return f;
    }

    [[nodiscard]] bool ContainsSphere(const Vector3& center, float radius) const {
        for (const auto& p : planes)
            if (p.DistanceToPoint(center) < -radius) return false;
        return true;
    }

    [[nodiscard]] bool ContainsAABB(const Vector3& min, const Vector3& max) const {
        for (const auto& p : planes) {
            Vector3 positive{p.normal.x >= 0 ? max.x : min.x, p.normal.y >= 0 ? max.y : min.y, p.normal.z >= 0 ? max.z : min.z};
            if (p.DistanceToPoint(positive) < 0) return false;
        }
        return true;
    }
};

}
