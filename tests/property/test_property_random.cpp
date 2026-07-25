// Main defined in test_property_math.cpp
#include <doctest/doctest.h>
#include <seed/util.h>
#include <unordered_set>

using namespace seed::util;

// Property: Same seed produces same sequence
TEST_CASE("Property_Random_SameSeedSameSequence") {
    Pcg32Random rng1(42);
    Pcg32Random rng2(42);
    for (int i = 0; i < 1000; ++i) {
        REQUIRE(rng1.next() == rng2.next());
    }
}

// Property: Different seeds produce different sequences (with high probability)
TEST_CASE("Property_Random_DifferentSeeds") {
    Pcg32Random rng1(1);
    Pcg32Random rng2(2);
    bool different = false;
    for (int i = 0; i < 10; ++i) {
        if (rng1.next() != rng2.next()) different = true;
    }
    REQUIRE(different);
}

// Property: nextFloat is in [0, 1)
TEST_CASE("Property_Random_FloatRange") {
    Pcg32Random rng;
    for (int i = 0; i < 1000; ++i) {
        float f = rng.nextFloat();
        REQUIRE(f >= 0.0f);
        REQUIRE(f < 1.0f);
    }
}

// Property: nextInt is within range
TEST_CASE("Property_Random_IntRange") {
    Pcg32Random rng;
    for (int i = 0; i < 1000; ++i) {
        int v = rng.nextInt(10, 100);
        REQUIRE(v >= 10);
        REQUIRE(v < 100);
    }
}

// Property: UUIDs are unique
TEST_CASE("Property_UUID_Unique") {
    std::unordered_set<std::string> uuids;
    for (int i = 0; i < 10000; ++i) {
        auto s = UUID::generate().toString();
        REQUIRE(uuids.find(s) == uuids.end());
        uuids.insert(s);
    }
}
