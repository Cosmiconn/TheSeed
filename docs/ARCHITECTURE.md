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

## Submodule: seed-network

### Subsystems

| Module | Path | Status |
|--------|------|--------|
| Socket | `src/network/socket.h/.cpp` | ✅ Complete (M7) |
| Packet Header | `src/network/packet_header.h/.cpp` | ✅ Complete (M7) |
| ReliableChannel | `src/network/reliable_channel.h/.cpp` | ✅ Complete (M7) |
| Fragmenter | `src/network/fragmenter.h/.cpp` | ✅ Complete (M7) |
| Transport | `src/network/transport.h/.cpp` | ✅ Complete (M7) |
| ReplicationSystem | `src/network/replication_system.h/.cpp` | ⏳ Planned (M8) |
| InterestManagement | `src/network/interest_management.h/.cpp` | ⏳ Planned (M8) |
| PredictionSystem | `src/network/prediction_system.h/.cpp` | ⏳ Planned (M9) |
| LagCompensation | `src/network/lag_compensation.h/.cpp` | ⏳ Planned (M10) |
| GameServer | `src/network/game_server.h/.cpp` | ⏳ Planned (M11) |
| SecurityManager | `src/network/security_manager.h/.cpp` | ⏳ Planned (M12) |

### Architecture

```
[Application]
     |
     v
[Transport] --reliable/unreliable--> [UDPSocket] --> UDP
     |
     +-- [ReliableChannel] --ACK-based--> ordered delivery
     +-- [Fragmenter] --MTU 1200--> reassembly
```

### Public API

```cpp
#include <seed/network/transport.h>        // Transport, TransportConfig
#include <seed/network/socket.h>           // UDPSocket, SocketAddress
#include <seed/network/reliable_channel.h> // ReliableChannel
#include <seed/network/fragmenter.h>       // Fragmenter
#include <seed/network/packet_header.h>    // PacketHeader
```

### Threading Model

- **Network Thread:** Non-blocking `recvfrom()` loop (100us sleep), ACK processing, incoming queueing
- **Main/Game Thread:** `update(deltaTime)` – outgoing send, resend, heartbeat, `receive()` – dequeue

See `docs/NETWORK.md` for full protocol specification.

---

## Design Principles

1. **No raw new/delete** in game code – always use seed allocators
2. **Deterministic** – same seed = same behavior (critical for networking)
3. **Data-oriented** – ECS with SoA storage, cache-friendly iteration
4. **Zero-overhead abstractions** – templates where possible, virtuals only at boundaries
5. **Testable** – every subsystem independently testable with mock allocators
