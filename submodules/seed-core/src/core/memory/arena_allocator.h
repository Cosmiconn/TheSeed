#pragma once

#include "core/memory/allocator.h"
#include "core/memory/block_allocator.h"
#include "core/profiling/tracy_seed.h"
#include <cstring>

namespace seed::memory {

// ---------------------------------------------------------------------------
// ArenaAllocator
// ---------------------------------------------------------------------------
// Linear bump allocator.  Zero fragmentation.  Bulk-free via reset().
// NOT thread-safe – intended for single-threaded frame-scoped use.
// ---------------------------------------------------------------------------
class ArenaAllocator : public Allocator {
public:
    static constexpr size_t DEFAULT_ARENA_SIZE = 64 * 1024; // 64 KiB

    explicit ArenaAllocator(BlockAllocator* blockAlloc,
                            size_t arenaSize = DEFAULT_ARENA_SIZE);
    ~ArenaAllocator() override;

    ArenaAllocator(const ArenaAllocator&) = delete;
    ArenaAllocator& operator=(const ArenaAllocator&) = delete;

    // Allocator interface
    void* allocate(size_t size, size_t alignment = alignof(std::max_align_t)) override;
    void  deallocate(void* /*ptr*/, size_t /*size*/ = 0) override {
        // No-op: individual dealloc not supported
    }

    // Bulk-free all memory
    void reset();

    // Stats
    size_t totalUsed()      const { return m_totalUsed; }
    size_t totalCapacity()  const { return m_totalCapacity; }
    size_t currentArenaUsed() const;

private:
    struct Arena {
        uint8_t* base;
        size_t size;
        size_t used;
        Arena* next;
    };

    BlockAllocator* m_blockAlloc;
    size_t m_arenaSize;
    Arena* m_current = nullptr;
    size_t m_totalUsed = 0;
    size_t m_totalCapacity = 0;

    Arena* allocateArena(size_t minSize);
};

} // namespace seed::memory
