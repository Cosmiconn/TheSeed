// TheSeed Engine – Fuzz Test: BinaryReader (libFuzzer-compatible)
// ---------------------------------------------------------------------------
// Dual-mode: Runs as doctest unit test OR libFuzzer harness.
// Build with doctest: g++ -DSEED_FUZZ_AS_TEST test_fuzz_serialize.cpp ...
// Build with libFuzzer: clang++ -fsanitize=fuzzer,address test_fuzz_serialize.cpp ...
// ---------------------------------------------------------------------------

#ifdef SEED_FUZZ_AS_TEST
    #define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
    #include <doctest/doctest.h>
#endif

#include <seed/serialize.h>
#include <cstdint>
#include <cstddef>

using namespace seed::serialize;

// ---------------------------------------------------------------------------
// Core fuzz logic (shared between test modes)
// ---------------------------------------------------------------------------
static int fuzzSerialize(const uint8_t* data, size_t size) {
    BinaryReader reader(data, size);

    // Try reading various types – should not crash, even with corrupted data
    while (!reader.eof() && reader.remaining() >= 1) {
        uint8_t op = reader.readUInt8();
        switch (op % 8) {
            case 0: if (reader.remaining() >= 4) reader.readUInt32(); break;
            case 1: if (reader.remaining() >= 8) reader.readUInt64(); break;
            case 2: if (reader.remaining() >= 4) reader.readFloat(); break;
            case 3: if (reader.remaining() >= 8) reader.readDouble(); break;
            case 4: if (reader.remaining() >= 2) reader.readUInt16(); break;
            case 5: if (reader.remaining() >= 1) reader.readBool(); break;
            case 6: if (reader.remaining() >= 4) reader.readInt32(); break;
            case 7: if (reader.remaining() >= 4) reader.readInt32(); break;
        }
    }

    return 0;
}

// ---------------------------------------------------------------------------
// libFuzzer entry point
// ---------------------------------------------------------------------------
#ifndef SEED_FUZZ_AS_TEST
extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
    return fuzzSerialize(data, size);
}
#endif

// ---------------------------------------------------------------------------
// doctest entry point
// ---------------------------------------------------------------------------
#ifdef SEED_FUZZ_AS_TEST
TEST_CASE("Fuzz_Serialize_RandomData") {
    const uint8_t fuzzInput[] = {
        0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,
        0x08, 0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F,
        0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17,
        0x18, 0x19, 0x1A, 0x1B, 0x1C, 0x1D, 0x1E, 0x1F
    };
    CHECK(fuzzSerialize(fuzzInput, sizeof(fuzzInput)) == 0);
}

TEST_CASE("Fuzz_Serialize_CorruptedData") {
    const uint8_t fuzzInput[] = {
        0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF
    };
    CHECK(fuzzSerialize(fuzzInput, sizeof(fuzzInput)) == 0);
}

TEST_CASE("Fuzz_Serialize_EmptyData") {
    CHECK(fuzzSerialize(nullptr, 0) == 0);
}
#endif
