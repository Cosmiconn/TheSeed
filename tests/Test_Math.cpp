// =============================================================================
// tests/Test_Math.cpp — Mathematik Unit Tests (P5-FIX)
// =============================================================================
#include "TestMain.h"
#include "../math/Vector.h"
#include "../math/Matrix.h"
#include "../math/Quaternion.h"

namespace tests {

// =============================================================================
// TEST: Vector3 Basics
// =============================================================================
TEST(Vector3Basics) {
    math::Vector3 a(1.0f, 2.0f, 3.0f);
    math::Vector3 b(4.0f, 5.0f, 6.0f);

    // Addition
    auto c = a + b;
    TEST_ASSERT_EQ(5.0f, c.x);
    TEST_ASSERT_EQ(7.0f, c.y);
    TEST_ASSERT_EQ(9.0f, c.z);

    // Subtraction
    auto d = b - a;
    TEST_ASSERT_EQ(3.0f, d.x);
    TEST_ASSERT_EQ(3.0f, d.y);
    TEST_ASSERT_EQ(3.0f, d.z);

    // Scalar multiplication
    auto e = a * 2.0f;
    TEST_ASSERT_EQ(2.0f, e.x);
    TEST_ASSERT_EQ(4.0f, e.y);
    TEST_ASSERT_EQ(6.0f, e.z);

    // Dot product
    float dot = a.Dot(b);
    TEST_ASSERT_EQ(32.0f, dot); // 1*4 + 2*5 + 3*6 = 32

    // Cross product
    auto cross = a.Cross(b);
    TEST_ASSERT_EQ(-3.0f, cross.x); // 2*6 - 3*5 = -3
    TEST_ASSERT_EQ(6.0f, cross.y);  // 3*4 - 1*6 = 6
    TEST_ASSERT_EQ(-3.0f, cross.z); // 1*5 - 2*4 = -3

    // Length
    float len = a.Length();
    TEST_ASSERT_NEAR(3.7417f, len, 0.001f); // sqrt(14)

    // Normalization
    auto norm = a.Normalized();
    TEST_ASSERT_NEAR(1.0f, norm.Length(), 0.0001f);
}

// =============================================================================
// TEST: Vector3 Zero/One
// =============================================================================
TEST(Vector3Constants) {
    auto zero = math::Vector3::Zero();
    TEST_ASSERT_EQ(0.0f, zero.x);
    TEST_ASSERT_EQ(0.0f, zero.y);
    TEST_ASSERT_EQ(0.0f, zero.z);

    auto one = math::Vector3::One();
    TEST_ASSERT_EQ(1.0f, one.x);
    TEST_ASSERT_EQ(1.0f, one.y);
    TEST_ASSERT_EQ(1.0f, one.z);
}

// =============================================================================
// TEST: Matrix4x4 Identity
// =============================================================================
TEST(Matrix4x4Identity) {
    math::Matrix4x4 identity = math::Matrix4x4::Identity();

    // Diagonal should be 1
    for (int i = 0; i < 4; ++i) {
        for (int j = 0; j < 4; ++j) {
            float expected = (i == j) ? 1.0f : 0.0f;
            TEST_ASSERT_EQ(expected, identity.m[i][j]);
        }
    }
}

// =============================================================================
// TEST: Matrix4x4 Translation
// =============================================================================
TEST(Matrix4x4Translation) {
    math::Matrix4x4 mat = math::Matrix4x4::Translation(1.0f, 2.0f, 3.0f);

    // Last column should be translation
    TEST_ASSERT_EQ(1.0f, mat.m[0][3]);
    TEST_ASSERT_EQ(2.0f, mat.m[1][3]);
    TEST_ASSERT_EQ(3.0f, mat.m[2][3]);
}

// =============================================================================
// TEST: Matrix4x4 Multiplication
// =============================================================================
TEST(Matrix4x4Multiplication) {
    math::Matrix4x4 a = math::Matrix4x4::Identity();
    math::Matrix4x4 b = math::Matrix4x4::Identity();

    auto c = a * b;

    // Identity * Identity = Identity
    for (int i = 0; i < 4; ++i) {
        for (int j = 0; j < 4; ++j) {
            float expected = (i == j) ? 1.0f : 0.0f;
            TEST_ASSERT_EQ(expected, c.m[i][j]);
        }
    }
}

// =============================================================================
// TEST: Quaternion Basics
// =============================================================================
TEST(QuaternionBasics) {
    math::Quaternion q(0.0f, 0.0f, 0.0f, 1.0f); // Identity quaternion

    // Identity quaternion should represent no rotation
    auto axis = q.GetAxis();
    TEST_ASSERT_NEAR(0.0f, axis.x, 0.0001f);
    TEST_ASSERT_NEAR(0.0f, axis.y, 0.0001f);
    TEST_ASSERT_NEAR(0.0f, axis.z, 0.0001f);

    // Length should be 1
    TEST_ASSERT_NEAR(1.0f, q.Length(), 0.0001f);
}

// =============================================================================
// TEST: Quaternion Rotation
// =============================================================================
TEST(QuaternionRotation) {
    // 90 degree rotation around Y axis
    math::Quaternion q = math::Quaternion::FromAxisAngle(
        math::Vector3(0.0f, 1.0f, 0.0f), 3.14159265f / 2.0f);

    // Rotate (1, 0, 0) by 90 degrees around Y → should be (0, 0, -1)
    math::Vector3 v(1.0f, 0.0f, 0.0f);
    auto rotated = q.Rotate(v);

    TEST_ASSERT_NEAR(0.0f, rotated.x, 0.01f);
    TEST_ASSERT_NEAR(0.0f, rotated.y, 0.01f);
    TEST_ASSERT_NEAR(-1.0f, rotated.z, 0.01f);
}

// =============================================================================
// TEST: Quaternion Slerp
// =============================================================================
TEST(QuaternionSlerp) {
    math::Quaternion a(0.0f, 0.0f, 0.0f, 1.0f);
    math::Quaternion b(0.0f, 1.0f, 0.0f, 0.0f);

    auto mid = math::Quaternion::Slerp(a, b, 0.5f);

    // Midpoint should have length 1
    TEST_ASSERT_NEAR(1.0f, mid.Length(), 0.0001f);
}

// =============================================================================
// BENCHMARK: Vector3 Operations
// =============================================================================
BENCHMARK(Vector3Operations, 1000000) {
    math::Vector3 a(1.0f, 2.0f, 3.0f);
    math::Vector3 b(4.0f, 5.0f, 6.0f);

    for (int i = 0; i < 1000000; ++i) {
        auto c = a + b;
        auto d = a.Cross(b);
        auto e = c.Normalized();
        (void)e;
        (void)d;
    }
}

// =============================================================================
// BENCHMARK: Matrix4x4 Multiplication
// =============================================================================
BENCHMARK(Matrix4x4Multiply, 100000) {
    math::Matrix4x4 a = math::Matrix4x4::Identity();
    math::Matrix4x4 b = math::Matrix4x4::Translation(1.0f, 2.0f, 3.0f);

    for (int i = 0; i < 100000; ++i) {
        auto c = a * b;
        (void)c;
    }
}

} // namespace tests
