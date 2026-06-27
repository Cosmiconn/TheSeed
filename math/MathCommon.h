#pragma once
#include <cstdint>
#include <cmath>

#if defined(__AVX2__)
    #define MATH_AVX2
    #include <immintrin.h>
#elif defined(__SSE4_2__)
    #define MATH_SSE4
    #include <nmmintrin.h>
#elif defined(__ARM_NEON)
    #define MATH_NEON
    #include <arm_neon.h>
#else
    #define MATH_SCALAR
#endif

namespace math {
inline constexpr float PI = 3.14159265358979323846f;
inline constexpr float DEG2RAD = PI / 180.0f;
inline constexpr float RAD2DEG = 180.0f / PI;
inline constexpr float EPSILON = 1e-6f;

[[nodiscard]] inline bool IsNearlyEqual(float a, float b, float tolerance = EPSILON) {
    return std::abs(a - b) <= tolerance;
}
}
