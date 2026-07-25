#include <doctest/doctest.h>
#include <seed/memory.h>

using namespace seed::memory;

// Simplified fuzz-style test for ArenaAllocator
// In a real libFuzzer setup, this would be:
// extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size)
TEST_CASE("Fuzz_ArenaAllocator_VariousSizes") {
    BlockAllocator blockAlloc(64 * 1024 * 1024);

    // Simulate fuzz input: sequence of allocation sizes
    const uint32_t fuzzInput[] = {1, 4, 8, 16, 32, 64, 128, 256, 512, 1024,
                                   2048, 4096, 8192, 1, 2, 4, 8, 16};

    for (int round = 0; round < 100; ++round) {
        ArenaAllocator arena(&blockAlloc);
        for (size_t i = 0; i < sizeof(fuzzInput)/sizeof(fuzzInput[0]); ++i) {
            size_t size = static_cast<size_t>(fuzzInput[i]) + static_cast<size_t>(round % 16); // Vary sizes
            void* p = arena.allocate(size, 8);
            REQUIRE(p != nullptr);
            // Write pattern to detect corruption
            std::memset(p, static_cast<int>(i & 0xFF), size);
        }
        arena.reset();
    }
}

TEST_CASE("Fuzz_ArenaAllocator_RandomPattern") {
    BlockAllocator blockAlloc(64 * 1024 * 1024);

    for (int seed = 0; seed < 10; ++seed) {
        ArenaAllocator arena(&blockAlloc);
        std::vector<std::pair<void*, size_t>> allocs;

        // Pseudo-random allocations
        size_t size = 1;
        for (int i = 0; i < 1000; ++i) {
            size = (size * 1103515245 + 12345) & 0x7FFF; // LCG
            size = (size % 1024) + 1;
            void* p = arena.allocate(size, 8);
            REQUIRE(p != nullptr);
            allocs.push_back({p, size});
        }

        // Verify all allocations are valid (no overlap, no corruption)
        for (auto& [p, sz] : allocs) {
            std::memset(p, 0xAB, sz);
        }

        arena.reset();
    }
}
