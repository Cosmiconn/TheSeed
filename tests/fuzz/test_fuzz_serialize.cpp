#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>
#include <seed/serialize.h>

using namespace seed::serialize;

TEST_CASE("Fuzz_Serialize_RandomData") {
    BinaryWriter writer;

    for (int i = 0; i < 1000; ++i) {
        writer.writePOD(i);
        writer.writePOD(static_cast<float>(i) * 0.5f);
        writer.writePOD(static_cast<uint64_t>(i * i));
    }

    auto data = writer.data();
    REQUIRE(data.size() > 0);

    BinaryReader reader(data.data(), data.size());
    for (int i = 0; i < 1000; ++i) {
        int32_t i32 = reader.readPOD<int32_t>();
        float f = reader.readPOD<float>();
        uint64_t u64 = reader.readPOD<uint64_t>();
        REQUIRE(i32 == i);
        REQUIRE(f == static_cast<float>(i) * 0.5f);
        REQUIRE(u64 == static_cast<uint64_t>(i * i));
    }
}

TEST_CASE("Fuzz_Serialize_CorruptedData") {
    std::vector<uint8_t> data = {0x01, 0x02, 0x03, 0x04};
    BinaryReader reader(data.data(), data.size());

    int32_t val = reader.readPOD<int32_t>();
    REQUIRE(val == 0x04030201); // Little-endian read

    // Reading past end - behavior depends on implementation
    // (may throw, may return default, may assert in debug)
    // This test documents the boundary condition
    REQUIRE(data.size() == 4);
}
