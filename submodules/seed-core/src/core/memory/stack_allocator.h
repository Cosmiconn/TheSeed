#pragma once

#include "core/memory/allocator.h"
#include "core/memory/block_allocator.h"
#include "core/profiling/tracy_seed.h"
#include <cstdint>

namespace seed::memory {

// ---------------------------------------------------------------------------
// StackAllocator
// ---------------------------------------------------------------------------
// LIFO bump allocator with scope markers.  NOT thread-safe.
// ---------------------------------------------------------------------------
class StackAllocator : public Allocator {
public:
    explicit StackAllocator(BlockAllocator* blockAlloc, size_t size);
    ~StackAllocator() override;

    StackAllocator(const StackAllocator&) = delete;
    StackAllocator& operator=(const StackAllocator&) = delete;

    struct Marker {
        size_t used;
    };

    Marker getMarker() const;
    void freeToMarker(Marker marker);

    // Reset the entire stack (free all allocations)
    void reset();

    void* allocate(size_t size, size_t alignment = alignof(std::max_align_t)) override;
    void deallocate(void* ptr, size_t size = 0) override;

    size_t totalUsed() const { return m_used; }

private:
    BlockAllocator* m_blockAlloc;
    uint8_t* m_base = nullptr;
    size_t m_size = 0;
    size_t m_used = 0;
};

} // namespace seed::memory
