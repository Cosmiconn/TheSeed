#pragma once
// =============================================================================
// memory/SlabAllocator.h — Multiple pool sizes (power-of-2)
// AP-14: Memory-Allocators
// =============================================================================
#include "PoolAllocator.h"
#include <array>
#include <memory>

namespace memory {

class SlabAllocator {
public:
    SlabAllocator(size_t minSize = 16, size_t maxSize = 4096, size_t objectsPerSlab = 1024);
    ~SlabAllocator() = default;

    [[nodiscard]] void* Allocate(size_t size);
    void Free(void* ptr, size_t size);
    void Reset();

private:
    size_t minSize = 16;
    size_t maxSize = 4096;
    size_t objectsPerSlab = 1024;
    
    std::vector<std::unique_ptr<PoolAllocator>> slabs;
    
    [[nodiscard]] size_t GetSlabIndex(size_t size) const;
};

} // namespace memory
