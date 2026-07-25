#include "core/memory/block_allocator.h"
#include <chrono>
#include <cstdio>
#include <vector>

using namespace seed::memory;

int main() {
    constexpr size_t N = 1000;
    constexpr size_t ITERATIONS = 100;

    BlockAllocator alloc;
    std::vector<void*> ptrs;
    ptrs.reserve(N);

    // Warm-up
    for (size_t i = 0; i < 10; ++i) {
        void* p = alloc.allocate(4096);
        alloc.deallocate(p);
    }

    auto start = std::chrono::high_resolution_clock::now();

    for (size_t iter = 0; iter < ITERATIONS; ++iter) {
        for (size_t i = 0; i < N; ++i) {
            ptrs.push_back(alloc.allocate(4096));
        }
        for (void* p : ptrs) {
            alloc.deallocate(p);
        }
        ptrs.clear();
    }

    auto elapsed = std::chrono::high_resolution_clock::now() - start;
    auto ms = static_cast<double>(
        std::chrono::duration_cast<std::chrono::milliseconds>(elapsed).count()
    );

    std::printf("BlockAllocator Benchmark\n");
    std::printf("  %zu allocations x %zu iterations = %zu total ops\n",
                N, ITERATIONS, N * ITERATIONS * 2);
    std::printf("  Total time: %.0f ms\n", ms);
    std::printf("  Ops/sec:    %.0f\n",
                (N * ITERATIONS * 2.0 * 1000.0) / (ms > 0.0 ? ms : 1.0));
    std::printf("  Blocks:     %zu\n", alloc.numBlocks());

    return 0;
}
