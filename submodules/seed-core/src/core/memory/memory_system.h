#pragma once

#include "core/memory/block_allocator.h"
#include "core/memory/pool_allocator.h"
#include "core/memory/arena_allocator.h"
#include "core/memory/stack_allocator.h"
#include "core/memory/memory_tracker.h"
#include "core/profiling/tracy_seed.h"

namespace seed::memory {

// ---------------------------------------------------------------------------
// Global allocator instances (initialized in main, accessed everywhere)
// ---------------------------------------------------------------------------
extern BlockAllocator*  g_blockAllocator;
extern MemoryTracker*   g_memoryTracker;

// ---------------------------------------------------------------------------
// Scoped arena for frame-scoped allocations
// ---------------------------------------------------------------------------
extern ArenaAllocator*  g_frameArena;

// ---------------------------------------------------------------------------
// No-new/delete enforcement helpers
// ---------------------------------------------------------------------------
inline void* seed_alloc(size_t size, size_t alignment = alignof(std::max_align_t)) {
    return g_blockAllocator->allocate(size, alignment);
}

inline void seed_free(void* ptr, size_t size = 0) {
    g_blockAllocator->deallocate(ptr, size);
}

} // namespace seed::memory
