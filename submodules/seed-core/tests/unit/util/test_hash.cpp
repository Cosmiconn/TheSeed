#include <doctest/doctest.h>
#include "core/util/hash.h"

using namespace seed::util;

TEST_CASE("Hash_FNV1a32_Consistent") {
    const char* data = "hello";
    uint32_t h1 = fnv1a32(data, 5);
    uint32_t h2 = fnv1a32(data, 5);
    REQUIRE(h1 == h2);
}

TEST_CASE("Hash_FNV1a32_DifferentData") {
    const char* a = "hello";
    const char* b = "world";
    uint32_t h1 = fnv1a32(a, 5);
    uint32_t h2 = fnv1a32(b, 5);
    REQUIRE(h1 != h2);
}

TEST_CASE("Hash_FNV1a64_Consistent") {
    const char* data = "hello";
    uint64_t h1 = fnv1a64(data, 5);
    uint64_t h2 = fnv1a64(data, 5);
    REQUIRE(h1 == h2);
}
