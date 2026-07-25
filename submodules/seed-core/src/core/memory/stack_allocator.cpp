#include "core/memory/stack_allocator.h"

namespace seed::memory {

StackAllocator::StackAllocator(BlockAllocator* blockAlloc, size_t size)
    : m_blockAlloc(blockAlloc)
    , m_size(size)
    , m_used(0)
{
    SEED_ZONE("StackAllocator::ctor");
    m_base = static_cast<uint8_t*>(blockAlloc->allocate(size, 64));
}

StackAllocator::~StackAllocator() {
    SEED_ZONE("StackAllocator::dtor");
    if (m_base) {
        m_blockAlloc->deallocate(m_base, m_size);
    }
}

StackAllocator::Marker StackAllocator::getMarker() const {
    return { m_used };
}

void StackAllocator::freeToMarker(Marker marker) {
    SEED_ZONE("StackAllocator::freeToMarker");
    m_used = marker.used;
}

void StackAllocator::reset() {
    SEED_ZONE("StackAllocator::reset");
    m_used = 0;
}

void* StackAllocator::allocate(size_t size, size_t alignment) {
    SEED_ZONE("StackAllocator::allocate");

    // Phase 0: alignment must be a power of two
    if ((alignment & (alignment - 1)) != 0) {
        return nullptr;
    }

    size_t aligned = (m_used + alignment - 1) & ~(alignment - 1);
    if (aligned + size > m_size) {
        return nullptr; // Overflow
    }
    void* ptr = m_base + aligned;
    m_used = aligned + size;
    SEED_ALLOC(ptr, size);
    return ptr;
}

void StackAllocator::deallocate(void* ptr, size_t size) {
    SEED_ZONE("StackAllocator::deallocate");
    (void)ptr;
    (void)size;
    // LIFO semantics: caller must use freeToMarker() or reset()
}

} // namespace seed::memory
