// TheSeed Engine – Meta-Level Property Tests: Random (rapidcheck + doctest)
// ---------------------------------------------------------------------------
// Cross-submodule property tests for random number generation.
// ---------------------------------------------------------------------------
#include <doctest/doctest.h>
#include <rapidcheck.h>
#include <seed/util.h>
#include <unordered_set>

using namespace seed::util;

TEST_CASE("Meta_Property_Random_SameSeedSameSequence") {
    rc::check("Same seed -> identical sequence", []() {
        uint64_t seed = *rc::gen::arbitrary<uint64_t>();
        Pcg32Random rng1(seed);
        Pcg32Random rng2(seed);
        for (int i = 0; i < 1000; ++i) {
            RC_ASSERT(rng1.next() == rng2.next());
        }
    });
}

TEST_CASE("Meta_Property_Random_FloatRange") {
    rc::check("nextFloat in [0, 1)", []() {
        Pcg32Random rng;
        for (int i = 0; i < 1000; ++i) {
            float f = rng.nextFloat();
            RC_ASSERT(f >= 0.0f);
            RC_ASSERT(f < 1.0f);
        }
    });
}

TEST_CASE("Meta_Property_Random_IntRange") {
    rc::check("nextInt in [min, max)", []() {
        Pcg32Random rng;
        int min = *rc::gen::inRange(-10000, 10000);
        int max = *rc::gen::inRange(min + 1, min + 20000);
        for (int i = 0; i < 1000; ++i) {
            int v = rng.nextInt(min, max);
            RC_ASSERT(v >= min);
            RC_ASSERT(v < max);
        }
    });
}

TEST_CASE("Meta_Property_UUID_Unique") {
    rc::check("10k UUIDs are unique", []() {
        std::unordered_set<std::string> uuids;
        for (int i = 0; i < 10000; ++i) {
            auto s = UUID::generate().toString();
            RC_ASSERT(uuids.find(s) == uuids.end());
            uuids.insert(s);
        }
    });
}
