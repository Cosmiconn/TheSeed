#include "core/memory/arena_allocator.h"
#include "core/memory/block_allocator.h"
#include <chrono>
#include <cstdio>

using namespace seed::memory;

int main() {
    BlockAllocator blockAlloc;
    ArenaAllocator arena(&blockAlloc);

    constexpr size_t N = 100'000;
    constexpr size_t OBJ_SIZE = 32;

    auto start = std::chrono::high_resolution_clock::now();
    for (size_t i = 0; i < N; ++i) {
        arena.allocate(OBJ_SIZE, 8);
    }
    auto elapsed = std::chrono::high_resolution_clock::now() - start;
    auto ns = static_cast<double>(
        std::chrono::duration_cast<std::chrono::nanoseconds>(elapsed).count()
    );

    std::printf("ArenaAllocator: %zu x %zu-byte allocs in %.0f ns (%.2f ns/op)\n",
                N, OBJ_SIZE, ns, ns / N);
    std::printf("  Total used: %zu bytes\n", arena.totalUsed());
    std::printf("  Capacity:   %zu bytes\n", arena.totalCapacity());

    return 0;
}
