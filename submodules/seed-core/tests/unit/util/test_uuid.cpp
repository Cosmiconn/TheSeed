#include <doctest/doctest.h>
#include "core/util/uuid.h"
#include <unordered_set>

using namespace seed::util;

TEST_CASE("UUID_Generate") {
    UUID u = UUID::generate();
    REQUIRE(u.toString().size() == 36);
}

TEST_CASE("UUID_Unique") {
    std::unordered_set<std::string> uuids;
    for (int i = 0; i < 1000; ++i) {
        auto s = UUID::generate().toString();
        REQUIRE(uuids.find(s) == uuids.end());
        uuids.insert(s);
    }
}

TEST_CASE("UUID_Equality") {
    UUID a = UUID::generate();
    UUID b = a;
    REQUIRE(a == b);
    UUID c = UUID::generate();
    REQUIRE(a != c);
}

TEST_CASE("UUID_Format") {
    UUID u = UUID::generate();
    std::string s = u.toString();
    REQUIRE(s[8] == '-');
    REQUIRE(s[13] == '-');
    REQUIRE(s[18] == '-');
    REQUIRE(s[23] == '-');
}
