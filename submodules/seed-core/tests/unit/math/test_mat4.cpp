#include <doctest/doctest.h>
#include "core/math/mat4.h"
#include "core/math/vec3.h"
#include "core/math/quat.h"

using namespace seed::math;

TEST_CASE("Math_Mat4_Identity") {
    Mat4 m;
    REQUIRE(m(0,0) == 1.0f);
    REQUIRE(m(1,1) == 1.0f);
    REQUIRE(m(2,2) == 1.0f);
    REQUIRE(m(3,3) == 1.0f);
    REQUIRE(m(0,1) == 0.0f);
}

TEST_CASE("Math_Mat4_LookAt") {
    Mat4 m = lookAt(Vec3(0,0,5), Vec3(0,0,0), Vec3(0,1,0));
    REQUIRE(m(2,2) == doctest::Approx(1.0f));
}

TEST_CASE("Math_Mat4_Perspective") {
    Mat4 m = perspective(3.14159265f / 4.0f, 16.0f / 9.0f, 0.1f, 100.0f);
    REQUIRE(m(0,0) > 0.0f);
    REQUIRE(m(2,3) == -1.0f);
}

TEST_CASE("Math_Mat4_ToMatrixFromQuat") {
    Quat q = fromAxisAngle(Vec3(0,1,0), 3.14159265f / 2.0f);
    Mat4 m = toMatrix(q);
    REQUIRE(m(0,0) == doctest::Approx(0.0f).epsilon(0.01f));
    REQUIRE(m(0,2) == doctest::Approx(1.0f).epsilon(0.01f));
}
