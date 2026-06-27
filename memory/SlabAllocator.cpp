#include "SlabAllocator.h"
#include <cmath>

namespace memory {

SlabAllocator::SlabAllocator(size_t min, size_t max, size_t count) 
    : minSize(min), maxSize(max), objectsPerSlab(count) 
{
    size_t numSlabs = static_cast<size_t>(std::log2(maxSize / minSize)) + 1;
    for (size_t i = 0; i < numSlabs; ++i) {
        size_t slabSize = minSize * (1 << i);
        slabs.push_back(std::make_unique<PoolAllocator>(slabSize, objectsPerSlab));
    }
}

void* SlabAllocator::Allocate(size_t size) {
    if (size > maxSize) return nullptr;
    size_t idx = GetSlabIndex(size);
    return slabs[idx]->Allocate();
}

void SlabAllocator::Free(void* ptr, size_t size) {
    if (!ptr || size > maxSize) return;
    size_t idx = GetSlabIndex(size);
    slabs[idx]->Free(ptr);
}

void SlabAllocator::Reset() {
    for (auto& slab : slabs) slab->Reset();
}

size_t SlabAllocator::GetSlabIndex(size_t size) const {
    if (size <= minSize) return 0;
    size_t idx = static_cast<size_t>(std::ceil(std::log2(static_cast<double>(size) / minSize)));
    return std::min(idx, slabs.size() - 1);
}

} // namespace memory
