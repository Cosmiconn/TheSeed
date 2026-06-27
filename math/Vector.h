#pragma once
#include "MathCommon.h"
#include <array>

namespace math {

struct alignas(16) Vector3 {
    float x = 0.0f, y = 0.0f, z = 0.0f, w = 0.0f;

    Vector3() = default;
    constexpr Vector3(float x_, float y_, float z_, float w_ = 0.0f) 
        : x(x_), y(y_), z(z_), w(w_) {}

    [[nodiscard]] float LengthSq() const { return x*x + y*y + z*z; }
    [[nodiscard]] float Length() const { return std::sqrt(LengthSq()); }
    
    void Normalize() {
        float len = Length();
        if (len > EPSILON) { x /= len; y /= len; z /= len; }
    }
    
    [[nodiscard]] Vector3 Normalized() const {
        Vector3 v = *this; v.Normalize(); return v;
    }

    [[nodiscard]] float Dot(const Vector3& o) const { return x*o.x + y*o.y + z*o.z; }
    
    [[nodiscard]] Vector3 Cross(const Vector3& o) const {
        return Vector3{y * o.z - z * o.y, z * o.x - x * o.z, x * o.y - y * o.x};
    }

#ifdef MATH_SSE4
    [[nodiscard]] Vector3 operator+(const Vector3& o) const {
        __m128 a = _mm_load_ps(&x);
        __m128 b = _mm_load_ps(&o.x);
        __m128 r = _mm_add_ps(a, b);
        Vector3 out; _mm_store_ps(&out.x, r); return out;
    }
    [[nodiscard]] Vector3 operator-(const Vector3& o) const {
        __m128 a = _mm_load_ps(&x);
        __m128 b = _mm_load_ps(&o.x);
        __m128 r = _mm_sub_ps(a, b);
        Vector3 out; _mm_store_ps(&out.x, r); return out;
    }
    [[nodiscard]] Vector3 operator*(float s) const {
        __m128 a = _mm_load_ps(&x);
        __m128 b = _mm_set1_ps(s);
        __m128 r = _mm_mul_ps(a, b);
        Vector3 out; _mm_store_ps(&out.x, r); return out;
    }
#else
    [[nodiscard]] Vector3 operator+(const Vector3& o) const { 
        return Vector3{x + o.x, y + o.y, z + o.z}; 
    }
    [[nodiscard]] Vector3 operator-(const Vector3& o) const { 
        return Vector3{x - o.x, y - o.y, z - o.z}; 
    }
    [[nodiscard]] Vector3 operator*(float s) const { 
        return Vector3{x * s, y * s, z * s}; 
    }
#endif

    [[nodiscard]] Vector3 operator-() const { return Vector3{-x, -y, -z}; }
    Vector3& operator+=(const Vector3& o) { *this = *this + o; return *this; }
    Vector3& operator-=(const Vector3& o) { *this = *this - o; return *this; }
    Vector3& operator*=(float s) { *this = *this * s; return *this; }
};

struct alignas(16) Vector4 {
    float x = 0.0f, y = 0.0f, z = 0.0f, w = 0.0f;
    Vector4() = default;
    constexpr Vector4(float x_, float y_, float z_, float w_) 
        : x(x_), y(y_), z(z_), w(w_) {}
    explicit Vector4(const Vector3& v, float w_ = 0.0f) 
        : x(v.x), y(v.y), z(v.z), w(w_) {}
};

}
