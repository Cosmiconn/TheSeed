#pragma once
// =============================================================================
// memory/PoolAllocator.h — Fixed-size object pool
// AP-14: Memory-Allocators
// =============================================================================
#include <cstdint>
#include <vector>
#include <memory>
#include <cassert>

namespace memory {

class PoolAllocator {
public:
    PoolAllocator(size_t objectSize, size_t objectCount, size_t alignment = 64);
    ~PoolAllocator();

    PoolAllocator(const PoolAllocator&) = delete;
    PoolAllocator& operator=(const PoolAllocator&) = delete;

    [[nodiscard]] void* Allocate();
    void Free(void* ptr);
    void Reset();

    [[nodiscard]] size_t GetFreeCount() const { return freeCount; }
    [[nodiscard]] size_t GetTotalCount() const { return totalCount; }

private:
    struct FreeNode {
        FreeNode* next = nullptr;
    };

    size_t objectSize = 0;
    size_t totalCount = 0;
    size_t freeCount = 0;
    size_t alignment = 64;
    
    std::unique_ptr<std::byte[]> memoryBlock;
    FreeNode* freeList = nullptr;
};

} // namespace memory
