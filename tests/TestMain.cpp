// =============================================================================
// tests/TestMain.cpp — Test Framework Implementation (P5-FIX)
// =============================================================================
#include "TestMain.h"

namespace tests {

std::vector<std::pair<std::string, std::function<void()>>> TestRegistry::tests;
std::vector<TestResult> TestRegistry::results;

void TestRegistry::Register(const std::string& name, std::function<void()> test) {
    tests.push_back({name, test});
}

int TestRegistry::RunAll() {
    results.clear();
    std::cout << "========================================" << std::endl;
    std::cout << " TheSeed Engine — Unit Tests" << std::endl;
    std::cout << "========================================" << std::endl;

    for (const auto& [name, test] : tests) {
        TestResult result;
        result.name = name;

        auto start = std::chrono::steady_clock::now();
        try {
            test();
            result.passed = true;
        } catch (const std::exception& e) {
            result.passed = false;
            result.errorMessage = e.what();
        }
        auto end = std::chrono::steady_clock::now();
        result.duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);

        results.push_back(result);

        std::cout << "[" << (result.passed ? "PASS" : "FAIL") << "] "
                  << name << " (" << result.duration.count() << " us)";
        if (!result.passed) {
            std::cout << " — " << result.errorMessage;
        }
        std::cout << std::endl;
    }

    PrintSummary();
    return static_cast<int>(GetFailedCount());
}

void TestRegistry::PrintSummary() {
    size_t passed = GetPassedCount();
    size_t failed = GetFailedCount();
    size_t total = results.size();

    std::cout << "========================================" << std::endl;
    std::cout << " Ergebnis: " << passed << "/" << total << " bestanden"
              << " (" << failed << " fehlgeschlagen)" << std::endl;
    std::cout << "========================================" << std::endl;
}

size_t TestRegistry::GetPassedCount() {
    return std::count_if(results.begin(), results.end(),
        [](const TestResult& r) { return r.passed; });
}

size_t TestRegistry::GetFailedCount() {
    return std::count_if(results.begin(), results.end(),
        [](const TestResult& r) { return !r.passed; });
}

} // namespace tests

// =============================================================================
// MAIN ENTRY POINT FUER TESTS
// =============================================================================
int main() {
    return tests::TestRegistry::RunAll();
}
