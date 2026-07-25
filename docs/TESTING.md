# Testing Strategy – TheSeed Engine

## Overview

TheSeed uses a multi-layered testing approach to ensure correctness, performance, and reliability across all submodules.

## Test Categories

| Type | Framework | Purpose | Location |
|------|-----------|---------|----------|
| Unit | doctest | Individual component correctness | `submodules/seed-core/tests/unit/` |
| Integration | doctest | Cross-component interaction | `tests/integration/` |
| Property | rapidcheck | Invariant-based testing | `submodules/seed-core/tests/unit/` |
| Fuzz | libFuzzer | Edge case discovery | `submodules/seed-core/tests/fuzz/` |
| Benchmark | nanobench / custom | Performance regression | `submodules/seed-core/tests/benchmarks/` |
| E2E | doctest | Full system simulation | `tests/e2e/` |

## Running Tests

```bash
# All tests
./scripts/test.sh

# Filtered tests
./scripts/test.sh ECS

# With sanitizers
cmake --preset linux-debug
ctest --test-dir build/linux-debug --output-on-failure
```

## CI Test Matrix

| Job | Trigger | Tests |
|-----|---------|-------|
| CI (ci.yml) | Push/PR | Build + Unit + Integration |
| Nightly (nightly.yml) | Daily 02:00 UTC | + Sanitizers + Stress x20 + Valgrind |
| Gate (gate.yml) | Manual | + Performance benchmarks + Build time check |

## Coverage Target

- Unit tests: >= 80%
- Integration tests: >= 60%
- Critical paths (ECS, Memory): >= 90%
