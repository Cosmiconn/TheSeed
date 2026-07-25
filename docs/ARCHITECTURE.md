# Architecture – TheSeed Engine

## Meta-Repo Structure

```
TheSeed/
├── CMakeLists.txt              # Super-build orchestrator
├── CMakePresets.json           # Cross-platform build presets
├── vcpkg.json                  # Global dependency manifest
├── vcpkg-configuration.json    # vcpkg registry configuration
│
├── cmake/                      # Shared CMake modules
│   ├── compiler_warnings.cmake
│   ├── sanitizers.cmake
│   ├── test_macros.cmake
│   └── submodule_helpers.cmake
│
├── scripts/                    # Developer tooling
│   ├── build.sh / build.bat
│   ├── test.sh
│   ├── setup.sh
│   ├── format.sh
│   └── lint.sh
│
├── docs/                       # Documentation
│   ├── BUILD.md
│   ├── TESTING.md
│   ├── ARCHITECTURE.md
│   ├── CI_CD.md
│   ├── ECS_INVARIANTS.md
│   ├── ERROR_CLASSIFICATION.md
│   ├── RECOVERY.md
│   └── ADRs/
│
├── tests/                      # Meta-level tests
│   ├── integration/            # Cross-submodule integration
│   └── e2e/                    # End-to-end scenarios
│
└── submodules/                 # Git submodules
    ├── seed-core/              # P0: Memory, ECS, Jobs, Serialize
    ├── seed-network/           # P1: Transport, Replication
    ├── seed-renderer/          # P2: Vulkan, PBR
    ├── seed-editor/            # P3: Editor, Tools
    ├── seed-game/              # P4: Gameplay
    ├── seed-platform/          # P5: API, Scripting
    ├── seed-cloud/             # P6: Docker, K8s
    └── seed-launch/            # P7: Shop, Steam
```

## Submodule: seed-core

### Subsystems

| Module | Path | Status |
|--------|------|--------|
| Memory | `src/core/memory/` | ✅ Complete (M2) |
| ECS | `src/core/ecs/` | ✅ Complete (M3) |
| Job-System | `src/core/jobs/` | ✅ Complete (M4) |
| Serialization | `src/core/serialize/` | ✅ Complete (M5) |
| Diagnostics | `src/core/diagnostics/` | ✅ Complete (M3 ext) |
| Profiling | `src/core/profiling/` | ✅ Complete |

### Public API

```cpp
#include <seed/memory.h>       // Allocators, MemoryTracker
#include <seed/ecs.h>          // World, Entity, Components
#include <seed/jobs.h>           // JobSystem, Task, parallelFor
#include <seed/serialize.h>      // BinaryReader/Writer, Snapshot, Delta
#include <seed/diagnostics.h>    // DiagnosticsManager, EventTimeline
#include <seed/profiling.h>      // SEED_ASSERT, Tracy macros
```

## Design Principles

1. **No raw new/delete** in game code – always use seed allocators
2. **Deterministic** – same seed = same behavior (critical for networking)
3. **Data-oriented** – ECS with SoA storage, cache-friendly iteration
4. **Zero-overhead abstractions** – templates where possible, virtuals only at boundaries
5. **Testable** – every subsystem independently testable with mock allocators
