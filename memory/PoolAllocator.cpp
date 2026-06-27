#include "PoolAllocator.h"
#include <algorithm>

namespace memory {

PoolAllocator::PoolAllocator(size_t objSize, size_t objCount, size_t align) 
    : objectSize(std::max(objSize, sizeof(FreeNode))), totalCount(objCount), freeCount(objCount), alignment(align) 
{
    size_t alignedSize = (objectSize + alignment - 1) & ~(alignment - 1);
    size_t blockSize = alignedSize * totalCount + alignment;
    
    memoryBlock = std::make_unique<std::byte[]>(blockSize);
    
    uintptr_t raw = reinterpret_cast<uintptr_t>(memoryBlock.get());
    uintptr_t aligned = (raw + alignment - 1) & ~(alignment - 1);
    
    freeList = nullptr;
    for (size_t i = 0; i < totalCount; ++i) {
        FreeNode* node = reinterpret_cast<FreeNode*>(aligned + i * alignedSize);
        node->next = freeList;
        freeList = node;
    }
}

PoolAllocator::~PoolAllocator() = default;

void* PoolAllocator::Allocate() {
    assert(freeList && "PoolAllocator out of memory");
    FreeNode* node = freeList;
    freeList = freeList->next;
    --freeCount;
    return node;
}

void PoolAllocator::Free(void* ptr) {
    if (!ptr) return;
    FreeNode* node = static_cast<FreeNode*>(ptr);
    node->next = freeList;
    freeList = node;
    ++freeCount;
}

void PoolAllocator::Reset() {
    freeCount = totalCount;
    freeList = nullptr;
    
    size_t alignedSize = (objectSize + alignment - 1) & ~(alignment - 1);
    uintptr_t raw = reinterpret_cast<uintptr_t>(memoryBlock.get());
    uintptr_t aligned = (raw + alignment - 1) & ~(alignment - 1);
    
    for (size_t i = 0; i < totalCount; ++i) {
        FreeNode* node = reinterpret_cast<FreeNode*>(aligned + i * alignedSize);
        node->next = freeList;
        freeList = node;
    }
}

} // namespace memory
