#pragma once
// =============================================================================
// tests/TestMain.h — Unit-Test Framework (P5-FIX)
// =============================================================================
// Einfaches Header-Only Test-Framework (keine externe Abhaengigkeit).
// Unterstuetzt: ASSERT, EXPECT, TEST-Suites, Benchmarks.
// =============================================================================
#include <string>
#include <vector>
#include <functional>
#include <chrono>
#include <iostream>
#include <sstream>
#include <cmath>

namespace tests {

// =============================================================================
// TEST RESULT
// =============================================================================
struct TestResult {
    std::string name;
    bool passed = false;
    std::string errorMessage;
    std::chrono::microseconds duration{0};
};

// =============================================================================
// TEST REGISTRY
// =============================================================================
class TestRegistry {
    static std::vector<std::pair<std::string, std::function<void()>>> tests;
    static std::vector<TestResult> results;

public:
    static void Register(const std::string& name, std::function<void()> test);
    static int RunAll();
    static void PrintSummary();
    [[nodiscard]] static size_t GetPassedCount();
    [[nodiscard]] static size_t GetFailedCount();
};

// =============================================================================
// ASSERT MACROS
// =============================================================================
#define TEST_ASSERT(condition)     if (!(condition)) {         throw std::runtime_error("ASSERT failed: " #condition " at " __FILE__ ":" + std::to_string(__LINE__));     }

#define TEST_ASSERT_EQ(expected, actual)     if ((expected) != (actual)) {         std::ostringstream oss;         oss << "ASSERT_EQ failed: expected " << (expected) << " but got " << (actual)             << " at " << __FILE__ << ":" << __LINE__;         throw std::runtime_error(oss.str());     }

#define TEST_ASSERT_NEAR(expected, actual, epsilon)     if (std::abs((expected) - (actual)) > (epsilon)) {         std::ostringstream oss;         oss << "ASSERT_NEAR failed: expected " << (expected) << " but got " << (actual)             << " (epsilon=" << (epsilon) << ") at " << __FILE__ << ":" << __LINE__;         throw std::runtime_error(oss.str());     }

#define TEST_ASSERT_TRUE(condition) TEST_ASSERT(condition)
#define TEST_ASSERT_FALSE(condition) TEST_ASSERT(!(condition))

// =============================================================================
// TEST MACRO
// =============================================================================
#define TEST(name)     static void test_##name();     static struct test_##name##_registrar {         test_##name##_registrar() {             tests::TestRegistry::Register(#name, test_##name);         }     } test_##name##_instance;     static void test_##name()

// =============================================================================
// BENCHMARK MACRO
// =============================================================================
#define BENCHMARK(name, iterations)     static void benchmark_##name();     static struct benchmark_##name##_registrar {         benchmark_##name##_registrar() {             tests::TestRegistry::Register("BENCH_" #name "_" #iterations, benchmark_##name);         }     } benchmark_##name##_instance;     static void benchmark_##name()

} // namespace tests
