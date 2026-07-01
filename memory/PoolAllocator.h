#pragma once
// =============================================================================
// memory/PoolAllocator.h — Fixed-size object pool (P4-FIX)
// =============================================================================
// KORREKTUR P4:
// • 64-Byte Alignment für Cache-Line + AVX-512
// • SIMD-freundliche Speicheranordnung
// • Prefetching-Unterstützung
// =============================================================================
#include <memory>
#include <cstddef>
#include <cstdint>
#include <cassert>

#ifdef _WIN32
    #include <malloc.h>
#else
    #include <stdlib.h>
#endif

namespace memory {

class PoolAllocator {
public:
    // P4-FIX: Default Alignment auf 64 Bytes (Cache-Line)
    PoolAllocator(size_t objectSize, size_t objectCount, size_t alignment = 64);
    ~PoolAllocator();

    PoolAllocator(const PoolAllocator&) = delete;
    PoolAllocator& operator=(const PoolAllocator&) = delete;

    [[nodiscard]] void* Allocate();
    void Free(void* ptr);
    void Reset();

    // P4: Prefetching für nächste Allokation
    void PrefetchNext() const;

    [[nodiscard]] size_t GetFreeCount() const { return freeCount; }
    [[nodiscard]] size_t GetTotalCount() const { return totalCount; }
    [[nodiscard]] size_t GetAlignment() const { return alignment; }

private:
    struct alignas(64) FreeNode {
        FreeNode* next = nullptr;
        // Padding auf 64 Bytes für Cache-Line-Alignment
        uint8_t padding[64 - sizeof(FreeNode*)];
    };

    size_t objectSize = 0;
    size_t totalCount = 0;
    size_t freeCount = 0;
    size_t alignment = 64;

    // P4-FIX: Aligned Memory Block
    void* rawMemory = nullptr;
    void* alignedMemory = nullptr;
    size_t allocatedSize = 0;

    FreeNode* freeList = nullptr;
};

} // namespace memory
