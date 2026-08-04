# ADR-002: Multi-Layer Testing Strategy

## Status
Accepted

## Context
Game engines require high reliability. A single bug in ECS or memory management can corrupt the entire game state. We need testing that catches issues at multiple levels without slowing down development.

## Decision
Implement **5 layers of testing**:

1. **Unit Tests** (doctest) – Every public API function
2. **Integration Tests** – Cross-component interaction (ECS + Memory + Jobs)
3. **Property-Based Tests** (rapidcheck) – Invariants over random inputs
4. **Fuzz Tests** (libFuzzer) – Edge cases in serialization/memory
5. **Benchmarks** (custom) – Performance regression detection

## CI Integration
- **PR/Push**: Unit + Integration (fast feedback, <5min)
- **Nightly**: + Sanitizers + Stress x20 + Valgrind
- **Phase Gate**: + Performance benchmarks + Coverage check

## Consequences

### Positive
- Bugs caught early at the right abstraction level
- Performance regressions detected before merge
- Sanitizers catch memory issues without manual testing

### Negative
- Test maintenance overhead
- CI duration increases with more tests

## Mitigations
- Test fixtures shared across test types
- Parallel test execution (`ctest -j$(nproc)`)
- Focused test execution via filters (`./scripts/test.sh ECS`)
