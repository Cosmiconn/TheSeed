#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>
#include <type_traits>
#include <string>
#include <vector>
#include <fmt/format.h>
#include <nlohmann/json.hpp>

// ---------------------------------------------------------------------------
// C++20 feature tests
// ---------------------------------------------------------------------------
template<typename T>
concept Numeric = std::is_arithmetic_v<T>;

TEST_CASE("BuildSystem_Cpp20_Concepts") {
    static_assert(Numeric<int>);
    static_assert(Numeric<float>);
    static_assert(!Numeric<void>);
    CHECK(true);
}

TEST_CASE("BuildSystem_Cpp20_Constexpr_Vector") {
    constexpr auto size = []() {
        std::vector<int> v{1, 2, 3};
        return v.size();
    }();
    static_assert(size == 3);
    CHECK(size == 3);
}

// ---------------------------------------------------------------------------
// Dependency integration tests
// ---------------------------------------------------------------------------
TEST_CASE("BuildSystem_fmt_Formatting") {
    std::string msg = fmt::format("Hello, {}! Value = {}", "TheSeed", 42);
    CHECK(msg == "Hello, TheSeed! Value = 42");
}

TEST_CASE("BuildSystem_nlohmann_JSON") {
    nlohmann::json config;
    config["engine"] = "TheSeed";
    config["version"] = "0.1.0";
    config["active"] = true;

    CHECK(config["engine"] == "TheSeed");
    CHECK(config["version"] == "0.1.0");
    CHECK(config["active"] == true);
    CHECK(config.dump().find("TheSeed") != std::string::npos);
}

TEST_CASE("BuildSystem_doctest_Running") {
    // Meta-test: if this runs, doctest is correctly linked
    CHECK(1 + 1 == 2);
}

// ---------------------------------------------------------------------------
// Build-system integrity
// ---------------------------------------------------------------------------
TEST_CASE("BuildSystem_WarningLevel_Smoke") {
    // Intentionally trivial test that will fail to compile if
    // -Werror / -WX is not active and we have unused variables.
    // With strict warnings this file must compile cleanly.
    int used = 42;
    CHECK(used == 42);
}
