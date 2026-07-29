// TheSeed Engine – Meta-Level Property Tests: Math (rapidcheck + doctest)
// ---------------------------------------------------------------------------
// Cross-submodule property tests for math correctness.
// ---------------------------------------------------------------------------
#include <doctest/doctest.h>
#include <rapidcheck.h>
#include <seed/math.h>
#include <cmath>

using namespace seed::math;

// ---------------------------------------------------------------------------
// Helper: near-equal comparison for floats
// ---------------------------------------------------------------------------
inline bool nearEqual(float a, float b, float eps = 1e-5f) {
    return std::abs(a - b) < eps;
}

inline bool nearZero(float a, float eps = 1e-5f) {
    return std::abs(a) < eps;
}

TEST_CASE("Meta_Property_Math_Vec3AdditionCommutative") {
    rc::check("a + b == b + a", []() {
        Vec3 a(*rc::gen::arbitrary<float>(), *rc::gen::arbitrary<float>(), *rc::gen::arbitrary<float>());
        Vec3 b(*rc::gen::arbitrary<float>(), *rc::gen::arbitrary<float>(), *rc::gen::arbitrary<float>());
        Vec3 s1 = a + b;
        Vec3 s2 = b + a;
        RC_ASSERT(nearEqual(s1.x, s2.x));
        RC_ASSERT(nearEqual(s1.y, s2.y));
        RC_ASSERT(nearEqual(s1.z, s2.z));
    });
}

TEST_CASE("Meta_Property_Math_Vec3ScalarDistributive") {
    rc::check("s * (a + b) == s*a + s*b", []() {
        Vec3 a(*rc::gen::arbitrary<float>(), *rc::gen::arbitrary<float>(), *rc::gen::arbitrary<float>());
        Vec3 b(*rc::gen::arbitrary<float>(), *rc::gen::arbitrary<float>(), *rc::gen::arbitrary<float>());
        float s = *rc::gen::arbitrary<float>();
        Vec3 left = (a + b) * s;
        Vec3 right = a * s + b * s;
        RC_ASSERT(nearEqual(left.x, right.x));
        RC_ASSERT(nearEqual(left.y, right.y));
        RC_ASSERT(nearEqual(left.z, right.z));
    });
}

TEST_CASE("Meta_Property_Math_Vec3NormalizeLengthOne") {
    rc::check("length(normalize(v)) == 1", []() {
        Vec3 v(*rc::gen::arbitrary<float>(), *rc::gen::arbitrary<float>(), *rc::gen::arbitrary<float>());
        RC_PRE(!nearZero(lengthSq(v)));
        Vec3 n = normalize(v);
        RC_ASSERT(nearEqual(length(n), 1.0f));
    });
}

TEST_CASE("Meta_Property_Math_Mat4Identity") {
    rc::check("M * I == M", []() {
        Mat4 m;
        for (int i = 0; i < 16; ++i) m.m[i] = *rc::gen::arbitrary<float>();
        Mat4 i = Mat4::identity();
        Mat4 r = m * i;
        for (int j = 0; j < 16; ++j) {
            RC_ASSERT(nearEqual(r.m[j], m.m[j]));
        }
    });
}
