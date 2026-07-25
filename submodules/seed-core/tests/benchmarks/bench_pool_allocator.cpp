#include "core/memory/pool_allocator.h"
#include "core/memory/block_allocator.h"
#include <chrono>
#include <cstdio>
#include <thread>
#include <vector>

using namespace seed::memory;

static void bench_single_thread() {
    BlockAllocator blockAlloc;
    PoolAllocator<uint64_t> pool(&blockAlloc);

    constexpr size_t N = 1'000'000;

    auto start = std::chrono::high_resolution_clock::now();
    for (size_t i = 0; i < N; ++i) {
        auto* p = pool.construct(i);
        pool.destroy(p);
    }
    auto elapsed = std::chrono::high_resolution_clock::now() - start;
    auto ms = static_cast<double>(
        std::chrono::duration_cast<std::chrono::milliseconds>(elapsed).count()
    );

    std::printf("Single-thread: %zu ops in %.2f ms (%.0f ops/sec)\n",
                N, ms, (static_cast<double>(N) * 1000.0) / (ms > 0.0 ? ms : 1.0));
}

static void bench_multi_thread(size_t threads, size_t opsPerThread) {
    BlockAllocator blockAlloc;
    PoolAllocator<uint64_t> pool(&blockAlloc);

    std::vector<std::thread> ts;
    auto start = std::chrono::high_resolution_clock::now();

    for (size_t t = 0; t < threads; ++t) {
        ts.emplace_back([&]() {
            for (size_t i = 0; i < opsPerThread; ++i) {
                auto* p = pool.construct(i);
                if (p) pool.destroy(p);
            }
        });
    }

    for (auto& t : ts) t.join();

    auto elapsed = std::chrono::high_resolution_clock::now() - start;
    auto ms = static_cast<double>(
        std::chrono::duration_cast<std::chrono::milliseconds>(elapsed).count()
    );

    size_t totalOps = threads * opsPerThread;
    std::printf("Multi-thread (%zu threads): %zu ops in %.2f ms (%.0f ops/sec)\n",
                threads, totalOps, ms,
                (static_cast<double>(totalOps) * 1000.0) / (ms > 0.0 ? ms : 1.0));
}

int main() {
    std::printf("=== PoolAllocator Benchmark ===\n\n");

    bench_single_thread();
    std::printf("\n");
    bench_multi_thread(4, 250'000);
    std::printf("\n");
    bench_multi_thread(8, 125'000);

    return 0;
}
