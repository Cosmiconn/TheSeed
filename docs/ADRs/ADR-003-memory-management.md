# ADR-003: Hierarchical Memory Management

## Status
Accepted

## Context
Game engines allocate and deallocate millions of small objects per frame. Standard `malloc`/`free` is too slow and causes fragmentation. We need a custom allocation strategy that is:
- Fast (< 100ns per allocation)
- Fragmentation-free for bulk operations
- Trackable for memory budgets and leaks
- Compatible with Tracy profiling

## Decision
Implement a **hierarchical allocator system**:

```
[OS: VirtualAlloc / mmap]
    |
[BlockAllocator: 64MB aligned pages]
    |
    |-- [PoolAllocator<T>]: Fixed-size, lock-free, thread-local cache
    |-- [ArenaAllocator]: Linear, bulk-free, scope-based
    |-- [StackAllocator]: LIFO, scope-based
    `-- [MallocFallback]: Tracked fallback for rare cases
```

### Key Design Points

1. **PoolAllocator**: Lock-free stack per thread-local cache, bulk-free to global list. No fragmentation for fixed-size objects.
2. **ArenaAllocator**: Single pointer bump. Reset in O(1). Perfect for frame-scoped allocations.
3. **StackAllocator**: LIFO only. Matches C++ RAII scopes.
4. **MemoryTracker**: Per-category budgets with alarm callbacks.
5. **Tracy Integration**: Every allocation/deallocation is a Tracy zone event.

## Consequences

### Positive
- 1M+ allocations/sec per thread
- Zero fragmentation for ECS components (PoolAllocator)
- Frame budget enforcement prevents OOM
- Tracy heatmap shows allocation hotspots

### Negative
- Custom allocators require discipline (no raw `new`/`delete`)
- PoolAllocator wastes memory if object sizes vary wildly
- Arena reset invalidates all pointers (must be documented)

## Mitigations
- `SEED_ASSERT` on raw `new`/`delete` in `src/core/`
- `MemoryTracker` alarms warn before OOM
- Clear documentation on Arena lifetime
