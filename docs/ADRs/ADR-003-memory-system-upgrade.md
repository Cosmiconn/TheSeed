# ADR 003: Memory System Upgrade – Two-Tier Allocation & Lock-Free Tracking

## Status
Accepted – 2026-07-29

## Context
The CI pipeline on Windows failed with `VirtualAlloc` exhaustion during the
`ECS_100k_Entities_Create` test. Root cause: `ComponentArray` requested
12 KiB chunks directly from `BlockAllocator`, which always allocated 64 MiB
OS blocks. 400 chunks × 64 MiB ≈ 25 GiB virtual address space – exceeding
the GitHub Actions runner limit.

Additionally, three structural issues were identified:
1. **ArenaAllocator leaked arenas** on destruction (no return path).
2. **MemoryTracker serialized all threads** via a global `std::mutex`.
3. **PoolAllocator had an acknowledged ABA risk** in its lock-free stack.

## Decision
We upgrade the memory subsystem with five coordinated changes:

### 1. ChunkAllocator (New)
A two-tier allocator that sits between `BlockAllocator` and fine-grained
consumers (ECS, serialization). It carves 1 MiB blocks from the parent into
a configurable chunk size (default 64 KiB) and manages them in a lock-protected
freelist. Large requests (> chunkSize) are transparently forwarded.

**Impact:** `ECS_100k_Entities_Create` now allocates ~25 MiB actual memory
instead of ~25 GiB virtual space.

### 2. ArenaAllocator – Global Arena Pool
Freed arenas are returned to a process-wide pool (`static vector<Arena*>`)
instead of leaking. New `ArenaAllocator` instances reuse pooled arenas,
eliminating address-space bloat for per-frame scratch allocators.

### 3. MemoryTracker – Lock-Free Fast Path
Replaced the global `std::mutex` with `std::shared_mutex`:
- `trackAllocation()` / `trackDeallocation()` take a **shared lock** (fast,
  lock-free for existing categories).
- New categories take an **exclusive lock** (rare, init-time only).
- Per-category stats remain fully atomic.

**Impact:** High-frequency ECS allocations no longer contend on a single mutex.

### 4. ComponentArray – Lazy Chunk Shrink
`remove()` now returns the last chunk to the allocator when it becomes empty
(`m_size % 1024 == 0`). This prevents unbounded growth after spawn waves.

### 5. PoolAllocator – Tagged Pointers
`m_globalFreeList` changed from `atomic<FreeNode*>` to `atomic<uint64_t>`
with a 16-bit ABA tag in the high bits. The tag increments on every CAS,
making recycled addresses detectable.

## Consequences
- **Positive:** CI stability, lower memory footprint, better multi-threaded
  performance, elimination of theoretical ABA crashes.
- **Negative:** `ChunkAllocator` adds one indirection and a mutex for small
  allocations. This is acceptable because ECS chunk allocation is not on the
  hot path (systems iterate components, they don't allocate them).
- **Migration:** All ECS tests updated to use `ChunkAllocator`. No public API
  changes – `World` still accepts `Allocator*`.

## References
- GitHub Issue: Windows CI `VirtualAlloc` exhaustion
- `submodules/seed-core/src/core/memory/chunk_allocator.h`
- `submodules/seed-core/src/core/memory/arena_allocator.cpp`
- `submodules/seed-core/src/core/memory/memory_tracker.cpp`
- `submodules/seed-core/src/core/ecs/component_array.h`
