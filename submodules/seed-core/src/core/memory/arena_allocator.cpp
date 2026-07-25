#include "core/memory/arena_allocator.h"

namespace seed::memory {

// ---------------------------------------------------------------------------
// ArenaAllocator implementation
// ---------------------------------------------------------------------------
ArenaAllocator::ArenaAllocator(BlockAllocator* blockAlloc, size_t arenaSize)
    : m_blockAlloc(blockAlloc)
    , m_arenaSize(arenaSize)
    , m_current(nullptr)
    , m_totalUsed(0)
    , m_totalCapacity(0)
{}

ArenaAllocator::~ArenaAllocator() {
    SEED_ZONE("ArenaAllocator::dtor");
    Arena* arena = m_current;
    while (arena) {
        Arena* next = arena->next;
        // Return to block allocator (or leak if block alloc doesn't support partial free)
        // For now we just leave it – BlockAllocator owns the memory
        arena = next;
    }
}

ArenaAllocator::Arena* ArenaAllocator::allocateArena(size_t minSize) {
    SEED_ZONE("ArenaAllocator::allocateArena");

    size_t allocSize = (minSize > m_arenaSize) ? minSize : m_arenaSize;
    allocSize = (allocSize + 63) & ~static_cast<size_t>(63); // align up to 64 bytes

    void* mem = m_blockAlloc->allocate(allocSize, 64);
    if (!mem) return nullptr;

    Arena* arena = new (mem) Arena();
    arena->base = static_cast<uint8_t*>(mem) + sizeof(Arena);
    arena->size = allocSize - sizeof(Arena);
    arena->used = 0;
    arena->next = nullptr;
    return arena;
}

void* ArenaAllocator::allocate(size_t size, size_t alignment) {
    SEED_ZONE("ArenaAllocator::allocate");

    // Phase 0: alignment must be a power of two
    if ((alignment & (alignment - 1)) != 0) {
        return nullptr;
    }

    size_t alignedSize = (size + alignment - 1) & ~(alignment - 1);

    // Ensure we have a current arena
    if (!m_current) {
        m_current = allocateArena(alignedSize + sizeof(Arena));
        if (!m_current) return nullptr;
        m_totalCapacity += m_current->size;
    }

    // Try current arena
    size_t alignedUsed = (m_current->used + alignment - 1) & ~(alignment - 1);
    if (alignedUsed + alignedSize <= m_current->size) {
        void* ptr = m_current->base + alignedUsed;
        m_current->used = alignedUsed + alignedSize;
        m_totalUsed += alignedSize;
        SEED_ALLOC(ptr, alignedSize);
        return ptr;
    }

    // Current arena full – allocate new one
    Arena* newArena = allocateArena(alignedSize + sizeof(Arena));
    if (!newArena) return nullptr;

    newArena->next = m_current;
    m_current = newArena;
    m_totalCapacity += newArena->size;

    // Allocate from fresh arena
    void* ptr = m_current->base;
    m_current->used = alignedSize;
    m_totalUsed += alignedSize;
    SEED_ALLOC(ptr, alignedSize);
    return ptr;
}

void ArenaAllocator::reset() {
    SEED_ZONE("ArenaAllocator::reset");

    Arena* arena = m_current;
    while (arena) {
        arena->used = 0;
        arena = arena->next;
    }
    m_totalUsed = 0;
}

size_t ArenaAllocator::currentArenaUsed() const {
    return m_current ? m_current->used : 0;
}

} // namespace seed::memory
