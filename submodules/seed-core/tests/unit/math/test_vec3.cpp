#include <doctest/doctest.h>
#include "core/math/vec3.h"

using namespace seed::math;

TEST_CASE("Math_Vec3_DefaultConstructor") {
    Vec3 v;
    REQUIRE(v.x == 0.0f);
    REQUIRE(v.y == 0.0f);
    REQUIRE(v.z == 0.0f);
}

TEST_CASE("Math_Vec3_Constructor") {
    Vec3 v(1.0f, 2.0f, 3.0f);
    REQUIRE(v.x == 1.0f);
    REQUIRE(v.y == 2.0f);
    REQUIRE(v.z == 3.0f);
}

TEST_CASE("Math_Vec3_Addition") {
    Vec3 a(1.0f, 2.0f, 3.0f);
    Vec3 b(4.0f, 5.0f, 6.0f);
    Vec3 c = a + b;
    REQUIRE(c.x == 5.0f);
    REQUIRE(c.y == 7.0f);
    REQUIRE(c.z == 9.0f);
}

TEST_CASE("Math_Vec3_Subtraction") {
    Vec3 a(5.0f, 5.0f, 5.0f);
    Vec3 b(1.0f, 2.0f, 3.0f);
    Vec3 c = a - b;
    REQUIRE(c.x == 4.0f);
    REQUIRE(c.y == 3.0f);
    REQUIRE(c.z == 2.0f);
}

TEST_CASE("Math_Vec3_ScalarMultiply") {
    Vec3 v(1.0f, 2.0f, 3.0f);
    Vec3 c = v * 2.0f;
    REQUIRE(c.x == 2.0f);
    REQUIRE(c.y == 4.0f);
    REQUIRE(c.z == 6.0f);
}

TEST_CASE("Math_Vec3_ScalarMultiplyReverse") {
    Vec3 v(1.0f, 2.0f, 3.0f);
    Vec3 c = 2.0f * v;
    REQUIRE(c.x == 2.0f);
    REQUIRE(c.y == 4.0f);
    REQUIRE(c.z == 6.0f);
}

TEST_CASE("Math_Vec3_Dot") {
    Vec3 a(1.0f, 2.0f, 3.0f);
    Vec3 b(4.0f, 5.0f, 6.0f);
    float d = dot(a, b);
    REQUIRE(d == 32.0f);
}

TEST_CASE("Math_Vec3_Cross") {
    Vec3 a(1.0f, 0.0f, 0.0f);
    Vec3 b(0.0f, 1.0f, 0.0f);
    Vec3 c = cross(a, b);
    REQUIRE(c.x == 0.0f);
    REQUIRE(c.y == 0.0f);
    REQUIRE(c.z == 1.0f);
}

TEST_CASE("Math_Vec3_Length") {
    Vec3 v(3.0f, 4.0f, 0.0f);
    REQUIRE(length(v) == 5.0f);
}

TEST_CASE("Math_Vec3_Normalize") {
    Vec3 v(3.0f, 4.0f, 0.0f);
    Vec3 n = normalize(v);
    REQUIRE(n.x == doctest::Approx(0.6f));
    REQUIRE(n.y == doctest::Approx(0.8f));
    REQUIRE(n.z == doctest::Approx(0.0f));
}

TEST_CASE("Math_Vec3_Equality") {
    Vec3 a(1.0f, 2.0f, 3.0f);
    Vec3 b(1.0f, 2.0f, 3.0f);
    Vec3 c(4.0f, 5.0f, 6.0f);
    REQUIRE(a == b);
    REQUIRE(a != c);
}
