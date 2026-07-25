#include <doctest/doctest.h>
#include "core/util/config.h"
#include <fstream>

using namespace seed::util;

TEST_CASE("Config_LoadAndGetInt") {
    std::ofstream f("test_config.json");
    f << "{\"test.value\": 42, \"test.float\": 3.14, \"test.string\": \"hello\"}";
    f.close();

    Config cfg("test_config.json");
    REQUIRE(cfg.load());
    REQUIRE(cfg.getInt("test.value", 0) == 42);
    REQUIRE(cfg.getFloat("test.float", 0.0f) == doctest::Approx(3.14f));
    REQUIRE(cfg.getString("test.string", "") == "hello");

    std::remove("test_config.json");
}

TEST_CASE("Config_DefaultValues") {
    Config cfg("nonexistent.json");
    REQUIRE(!cfg.load());
    REQUIRE(cfg.getInt("missing", 99) == 99);
    REQUIRE(cfg.getFloat("missing", 1.5f) == doctest::Approx(1.5f));
    REQUIRE(cfg.getString("missing", "default") == "default");
}
