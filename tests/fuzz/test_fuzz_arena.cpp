// TheSeed Engine – Fuzz Test: ArenaAllocator (libFuzzer-compatible)
// ---------------------------------------------------------------------------
// Dual-mode: Runs as doctest unit test OR libFuzzer harness.
// Build with doctest: g++ -DSEED_FUZZ_AS_TEST test_fuzz_arena.cpp ...
// Build with libFuzzer: clang++ -fsanitize=fuzzer,address test_fuzz_arena.cpp ...
// ---------------------------------------------------------------------------

#ifdef SEED_FUZZ_AS_TEST
    #define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
    #include <doctest/doctest.h>
#endif

#include <seed/memory.h>
#include <cstdint>
#include <cstddef>
#include <cstring>

using namespace seed::memory;

// ---------------------------------------------------------------------------
// Core fuzz logic (shared between test modes)
// ---------------------------------------------------------------------------
static int fuzzArena(const uint8_t* data, size_t size) {
    BlockAllocator blockAlloc(64 * 1024 * 1024);
    ArenaAllocator arena(&blockAlloc);

    for (size_t i = 0; i + 4 <= size; i += 4) {
        uint32_t req = *reinterpret_cast<const uint32_t*>(data + i);
        size_t allocSize = (req & 0xFFFF) + 1;
        size_t alignment = static_cast<size_t>(1) << ((req >> 16) & 0xF);
        if (alignment == 0) alignment = 1;

        void* p = arena.allocate(allocSize, alignment);
        if (p) {
            std::memset(p, static_cast<int>(i & 0xFF), allocSize);
        }
    }

    arena.reset();
    return 0;
}

// ---------------------------------------------------------------------------
// libFuzzer entry point
// ---------------------------------------------------------------------------
#ifndef SEED_FUZZ_AS_TEST
extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
    return fuzzArena(data, size);
}
#endif

// ---------------------------------------------------------------------------
// doctest entry point
// ---------------------------------------------------------------------------
#ifdef SEED_FUZZ_AS_TEST
TEST_CASE("Fuzz_ArenaAllocator_VariousSizes") {
    const uint8_t fuzzInput[] = {
        0x01, 0x00, 0x00, 0x00, 0x04, 0x00, 0x00, 0x00,
        0x08, 0x00, 0x00, 0x00, 0x10, 0x00, 0x00, 0x00,
        0x20, 0x00, 0x00, 0x00, 0x40, 0x00, 0x00, 0x00,
        0x80, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00,
        0x00, 0x02, 0x00, 0x00, 0x00, 0x04, 0x00, 0x00
    };
    CHECK(fuzzArena(fuzzInput, sizeof(fuzzInput)) == 0);
}

TEST_CASE("Fuzz_ArenaAllocator_RandomPattern") {
    const uint8_t fuzzInput[] = {
        0xAB, 0xCD, 0xEF, 0x12, 0x34, 0x56, 0x78, 0x9A,
        0xBC, 0xDE, 0xF0, 0x11, 0x22, 0x33, 0x44, 0x55,
        0x66, 0x77, 0x88, 0x99, 0xAA, 0xBB, 0xCC, 0xDD
    };
    CHECK(fuzzArena(fuzzInput, sizeof(fuzzInput)) == 0);
}
#endif
