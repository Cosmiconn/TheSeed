#include "core/memory/memory_system.h"
#include "core/memory/pool_allocator.h"
#include "core/memory/arena_allocator.h"
#include <chrono>
#include <cstdio>
#include <vector>

using namespace seed::memory;

int main() {
    std::printf("=== M2 Integration Benchmark ===\n\n");

    BlockAllocator blockAlloc;

    // 1. PoolAllocator: 1M ops
    {
        PoolAllocator<uint64_t> pool(&blockAlloc);
        constexpr size_t N = 1'000'000;

        auto start = std::chrono::high_resolution_clock::now();
        for (size_t i = 0; i < N; ++i) {
            auto* p = pool.construct(i);
            pool.destroy(p);
        }
        auto ms = static_cast<double>(
            std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::high_resolution_clock::now() - start
            ).count()
        );

        std::printf("PoolAllocator:   %zu ops in %.2f ms (%.0f ops/sec)\n",
                    N, ms, (N * 1000.0) / (ms > 0.0 ? ms : 1.0));
    }

    // 2. ArenaAllocator: 100k allocs + reset
    {
        ArenaAllocator arena(&blockAlloc);
        constexpr size_t N = 100'000;

        auto start = std::chrono::high_resolution_clock::now();
        for (size_t i = 0; i < N; ++i) {
            arena.allocate(32, 8);
        }
        auto ms = static_cast<double>(
            std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::high_resolution_clock::now() - start
            ).count()
        );

        std::printf("ArenaAllocator:  %zu allocs in %.2f ms (%.0f allocs/sec)\n",
                    N, ms, (N * 1000.0) / (ms > 0.0 ? ms : 1.0));
        std::printf("  Total used: %zu bytes\n", arena.totalUsed());
    }

    // 3. Combined: simulate 1000 frames
    {
        ArenaAllocator frameArena(&blockAlloc);
        PoolAllocator<uint64_t> entityPool(&blockAlloc);
        std::vector<uint64_t*> entities;

        auto start = std::chrono::high_resolution_clock::now();
        for (int frame = 0; frame < 1000; ++frame) {
            // Spawn entities
            for (int i = 0; i < 100; ++i) {
                entities.push_back(entityPool.construct(static_cast<uint64_t>(frame * 100 + i)));
            }

            // Temp frame data
            frameArena.allocate(1024, 16);
            frameArena.allocate(2048, 32);

            // End frame
            frameArena.reset();

            // Despawn half
            size_t toRemove = entities.size() / 2;
            for (size_t i = 0; i < toRemove; ++i) {
                entityPool.destroy(entities.back());
                entities.pop_back();
            }
        }

        // Cleanup
        for (auto* e : entities) {
            entityPool.destroy(e);
        }

        auto ms = static_cast<double>(
            std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::high_resolution_clock::now() - start
            ).count()
        );

        std::printf("\n1000 frames (100 spawns/frame): %.2f ms (%.2f ms/frame)\n",
                    ms, ms / 1000.0);
    }

    return 0;
}
