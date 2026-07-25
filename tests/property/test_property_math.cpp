#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>
#include <seed/math.h>

using namespace seed::math;

// Property: Vector addition is commutative
TEST_CASE("Property_Math_Vec3_Commutative") {
    Vec3 a(1.0f, 2.0f, 3.0f);
    Vec3 b(4.0f, 5.0f, 6.0f);
    REQUIRE(a + b == b + a);
}

// Property: Scalar multiplication distributes over addition
TEST_CASE("Property_Math_Vec3_Distributive") {
    Vec3 v(1.0f, 2.0f, 3.0f);
    float s = 2.0f;
    REQUIRE((v + v) == v * s);
}

// Property: Dot product is commutative
TEST_CASE("Property_Math_Vec3_DotCommutative") {
    Vec3 a(1.0f, 2.0f, 3.0f);
    Vec3 b(4.0f, 5.0f, 6.0f);
    REQUIRE(dot(a, b) == dot(b, a));
}

// Property: Cross product of parallel vectors is zero
TEST_CASE("Property_Math_Vec3_CrossParallel") {
    Vec3 a(1.0f, 2.0f, 3.0f);
    Vec3 b = a * 2.0f;
    Vec3 c = cross(a, b);
    REQUIRE(c.x == doctest::Approx(0.0f));
    REQUIRE(c.y == doctest::Approx(0.0f));
    REQUIRE(c.z == doctest::Approx(0.0f));
}

// Property: Normalized vector has length 1
TEST_CASE("Property_Math_Vec3_NormalizeLength") {
    Vec3 v(3.0f, 4.0f, 0.0f);
    Vec3 n = normalize(v);
    REQUIRE(length(n) == doctest::Approx(1.0f));
}

// Property: Identity matrix multiplication
TEST_CASE("Property_Math_Mat4_Identity") {
    Mat4 m;
    Mat4 i = Mat4::identity();
    Mat4 r = m * i;
    REQUIRE(r(0,0) == 1.0f);
    REQUIRE(r(1,1) == 1.0f);
    REQUIRE(r(2,2) == 1.0f);
    REQUIRE(r(3,3) == 1.0f);
}
