#include <doctest/doctest.h>
#include "core/math/quat.h"
#include "core/math/vec3.h"

using namespace seed::math;

TEST_CASE("Math_Quat_DefaultConstructor") {
    Quat q;
    REQUIRE(q.x == 0.0f);
    REQUIRE(q.y == 0.0f);
    REQUIRE(q.z == 0.0f);
    REQUIRE(q.w == 1.0f);
}

TEST_CASE("Math_Quat_AxisAngle90Deg") {
    Quat q = fromAxisAngle(Vec3(0.0f, 1.0f, 0.0f), 3.14159265f / 2.0f);
    REQUIRE(q.w == doctest::Approx(0.7071f).epsilon(0.01f));
    REQUIRE(q.y == doctest::Approx(0.7071f).epsilon(0.01f));
}
