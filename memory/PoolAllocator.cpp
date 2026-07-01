// =============================================================================
// memory/PoolAllocator.cpp — Aligned Pool Allocator (P4-FIX)
// =============================================================================
// KORREKTUR P4:
// • 64-Byte Alignment mit aligned_alloc / _aligned_malloc
// • SIMD-freundliche Speicheranordnung
// • Prefetching für nächste Allokation
// • Korrekte Freigabe mit aligned_free
// =============================================================================
#include "PoolAllocator.h"
#include <cstring>

#ifdef _WIN32
    #include <malloc.h>
#endif

namespace memory {

// =============================================================================
// Konstruktor
// =============================================================================
PoolAllocator::PoolAllocator(size_t objSize, size_t objCount, size_t align)
    : objectSize(std::max(objSize, sizeof(FreeNode))),
      totalCount(objCount),
      freeCount(objCount),
      alignment(align)
{
    // P4-FIX: Berechne aligned size
    size_t alignedObjSize = (objectSize + alignment - 1) & ~(alignment - 1);
    size_t blockSize = alignedObjSize * totalCount + alignment;

    // P4-FIX: Aligned Allocation
#ifdef _WIN32
    rawMemory = _aligned_malloc(blockSize, alignment);
    alignedMemory = rawMemory;
#else
    rawMemory = std::malloc(blockSize);
    if (rawMemory) {
        uintptr_t raw = reinterpret_cast<uintptr_t>(rawMemory);
        uintptr_t aligned = (raw + alignment - 1) & ~(alignment - 1);
        alignedMemory = reinterpret_cast<void*>(aligned);
    }
#endif

    if (!rawMemory) {
        totalCount = 0;
        freeCount = 0;
        return;
    }

    allocatedSize = blockSize;

    // P4-FIX: Null-Initialisierung (SIMD-optimiert)
    std::memset(alignedMemory, 0, alignedObjSize * totalCount);

    // Freie Liste aufbauen
    freeList = nullptr;
    for (size_t i = 0; i < totalCount; ++i) {
        FreeNode* node = reinterpret_cast<FreeNode*>(
            reinterpret_cast<uintptr_t>(alignedMemory) + i * alignedObjSize);
        node->next = freeList;
        freeList = node;
    }
}

// =============================================================================
// Destruktor
// =============================================================================
PoolAllocator::~PoolAllocator() {
    if (rawMemory) {
#ifdef _WIN32
        _aligned_free(rawMemory);
#else
        std::free(rawMemory);
#endif
    }
}

// =============================================================================
// Allocate
// =============================================================================
void* PoolAllocator::Allocate() {
    assert(freeList && "PoolAllocator out of memory");
    if (!freeList) return nullptr;

    FreeNode* node = freeList;
    freeList = freeList->next;
    --freeCount;
    return node;
}

// =============================================================================
// Free
// =============================================================================
void PoolAllocator::Free(void* ptr) {
    if (!ptr) return;
    FreeNode* node = static_cast<FreeNode*>(ptr);
    node->next = freeList;
    freeList = node;
    ++freeCount;
}

// =============================================================================
// Reset
// =============================================================================
void PoolAllocator::Reset() {
    freeCount = totalCount;
    freeList = nullptr;

    size_t alignedObjSize = (objectSize + alignment - 1) & ~(alignment - 1);

    // Null-Initialisierung
    std::memset(alignedMemory, 0, alignedObjSize * totalCount);

    // Freie Liste neu aufbauen
    for (size_t i = 0; i < totalCount; ++i) {
        FreeNode* node = reinterpret_cast<FreeNode*>(
            reinterpret_cast<uintptr_t>(alignedMemory) + i * alignedObjSize);
        node->next = freeList;
        freeList = node;
    }
}

// =============================================================================
// Prefetching (P4)
// =============================================================================
void PoolAllocator::PrefetchNext() const {
    if (!freeList) return;

#ifdef _WIN32
    _mm_prefetch(reinterpret_cast<const char*>(freeList), _MM_HINT_T0);
#else
    __builtin_prefetch(freeList, 1, 3);
#endif
}

} // namespace memory
