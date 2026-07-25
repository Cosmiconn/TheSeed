#include <doctest/doctest.h>
#include "core/util/random.h"

using namespace seed::util;

TEST_CASE("Random_PCG32_Deterministic") {
    Pcg32Random rng1(42);
    Pcg32Random rng2(42);
    for (int i = 0; i < 1000; ++i) {
        REQUIRE(rng1.next() == rng2.next());
    }
}

TEST_CASE("Random_PCG32_DifferentSeeds") {
    Pcg32Random rng1(1);
    Pcg32Random rng2(2);
    bool different = false;
    for (int i = 0; i < 10; ++i) {
        if (rng1.next() != rng2.next()) different = true;
    }
    REQUIRE(different);
}

TEST_CASE("Random_PCG32_FloatRange") {
    Pcg32Random rng;
    for (int i = 0; i < 100; ++i) {
        float f = rng.nextFloat();
        REQUIRE(f >= 0.0f);
        REQUIRE(f <= 1.0f);
    }
}

TEST_CASE("Random_PCG32_IntRange") {
    Pcg32Random rng;
    for (int i = 0; i < 100; ++i) {
        int v = rng.nextInt(10, 20);
        REQUIRE(v >= 10);
        REQUIRE(v < 20);
    }
}
