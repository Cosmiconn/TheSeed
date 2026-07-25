# ADR-004: Archetype-Based ECS (EnTT-Style)

## Status
Accepted

## Context
The engine needs to support 100k+ entities with complex component combinations. Traditional OOP inheritance is too slow and cache-unfriendly. We need an ECS that is:
- Cache-friendly (SoA storage)
- Deterministic (for network replication)
- Fast iteration (no virtual dispatch during queries)
- Memory-efficient (< 50MB for 100k entities)

## Decision
Use an **archetype-based ECS** inspired by EnTT:

```
[EntityManager] <--> [ArchetypeRegistry]
      |
[Archetype A] -> [ComponentArray<Pos>][ComponentArray<Vel>]
[Archetype B] -> [ComponentArray<Pos>][ComponentArray<Vel>][ComponentArray<Health>]
```

### Key Design Points

1. **Entity Handle**: 32-bit packed (24-bit index + 8-bit version). Version prevents zombie handle reuse bugs.
2. **Archetype ID**: Deterministic hash over sorted component type IDs. Same signature = same archetype always.
3. **ComponentArray**: SoA storage using PoolAllocator. Dense, cache-friendly iteration.
4. **Swap-and-Pop**: Entity removal is O(1) by swapping with last element and updating the moved entity's record.
5. **Query Engine**: Archetype-level filtering first, then dense iteration over matching archetypes.

## Consequences

### Positive
- 100k entities in < 100MB
- Query iteration is cache-friendly (sequential memory access)
- Archetype changes are O(1) amortized
- Deterministic: same operations always produce same memory layout

### Negative
- Component addition/removal may trigger archetype migration (memory copy)
- Fragmentation across archetypes (mitigated by PoolAllocator)
- Query filtering has overhead for many archetypes

## Mitigations
- Batch component additions when possible
- PoolAllocator per component type eliminates fragmentation
- Archetype registry caches frequent combinations
