# TheSeed Engine Diagnostics Framework (TEDF)
## Integration Guide – P0-M3 ECS Extension

### Overview

The Diagnostic Framework provides comprehensive observability for TheSeed Engine,
following the 12-layer architecture defined in `TheSeed_Engine_Diagnostics_Framework.md`.

### Layers Implemented

| Layer | Component | Status | Files |
|-------|-----------|--------|-------|
| 1 | Contracts | ✅ | `diagnostics_config.h` (SEED_REQUIRES, SEED_ENSURES, SEED_INVARIANT) |
| 2 | Runtime Validation | ✅ | `ecs_validator.h/.cpp`, `memory_validator.h/.cpp` |
| 3 | Event Timeline | ✅ | `event_timeline.h/.cpp` |
| 6 | Snapshot System | ✅ | `snapshot_dump.h/.cpp` |
| 9 | Health Score | ✅ | `health_score.h/.cpp` |
| Orchestrator | DiagnosticsManager | ✅ | `diagnostics_manager.h/.cpp` |

### Quick Start

```cpp
#include "core/diagnostics/diagnostics_manager.h"

// Initialize
auto& diag = seed::diagnostics::DiagnosticsManager::instance();
diag.initialize();

// Use in your code
seed::diagnostics::SEED_DIAG_EVENT(
    seed::diagnostics::EventType::EntityCreate,
    entity, archetypeHash, componentType, index,
    "description", __FILE__, __LINE__
);

// Validate
seed::diagnostics::EcsValidationResult result;
if (!seed::diagnostics::EcsValidator::validateWorld(world, &result)) {
    // Handle validation failure
}

// Check health
if (!seed::diagnostics::HealthScore::isHealthy()) {
    auto worst = seed::diagnostics::HealthScore::worstModule();
}

// Shutdown
diag.shutdown();
```

### Configuration

Compile-time switches in `diagnostics_config.h`:

```cpp
#define SEED_DIAGNOSTICS_ENABLED 1           // Master switch
#define SEED_DIAGNOSTICS_ECS_VALIDATION 1    // ECS validation
#define SEED_DIAGNOSTICS_MEMORY_VALIDATION 1 // Memory validation
#define SEED_DIAGNOSTICS_EVENT_TIMELINE 1    // Event logging
#define SEED_DIAGNOSTICS_HEALTH_SCORE 1      // Health scoring
#define SEED_DIAGNOSTICS_SNAPSHOT_ON_FAILURE 1 // Auto-snapshot on assert
```

### ECS Integration Points

The following ECS operations are instrumented:

- `World::createEntity()` → `EventType::EntityCreate`
- `World::destroyEntity()` → `EventType::EntityDestroy`
- `World::addComponent<T>()` → `EventType::ComponentAdd`
- `World::removeComponent<T>()` → `EventType::ComponentRemove`
- `World::moveEntity()` → `EventType::ComponentMove`
- `World::validateInvariants()` → `EventType::WorldValidate`
- `Archetype::addEntity()` → `EventType::EntityCreate`
- `Archetype::removeEntityByIndex()` → `EventType::EntityDestroy`
- `ComponentArray::defaultConstruct()` → `EventType::ComponentDefaultConstruct`
- `ComponentArray::destructAt()` → `EventType::ComponentDestruct`

### Memory Integration Points

- `BlockAllocator::allocate()` → `EventType::MemoryAllocate`
- `BlockAllocator::deallocate()` → `EventType::MemoryDeallocate`
- `MemoryTracker::trackAllocation()` → `EventType::MemoryAllocate`
- `MemoryTracker::trackDeallocation()` → `EventType::MemoryDeallocate`

### Validation Checks

#### ECS Validator
- Entity ↔ Record consistency
- Entity ↔ Archetype row mapping
- ComponentArray size consistency
- Version integrity
- Archetype signature ↔ column consistency
- No duplicate components per entity
- Memory integrity (no nullptr columns)

#### Memory Validator
- BlockAllocator alignment
- MemoryTracker budget compliance
- Leak detection (via ASan/LSan integration)
- Pointer validity

### Testing

```bash
# Build with diagnostics enabled
cmake --preset linux-debug -DSEED_ENABLE_SANITIZERS=ON

# Run diagnostics tests
ctest --test-dir build/linux-debug -R Diagnostics

# Run all tests
ctest --test-dir build/linux-debug --output-on-failure
```

### Crash Recovery

On assertion failure, the framework automatically:
1. Captures the event timeline
2. Records health scores
3. Dumps ECS world state
4. Writes build information
5. Saves everything to `seed_crash_dump.txt`

### Future Extensions (P1+)

- Layer 4: Live Diagnostics Graph (Editor integration)
- Layer 5: GPU Marker integration (Renderer)
- Layer 7: Replay system
- Layer 8: Diagnostics Dashboard
- Layer 10: Automatic rule checking
- Layer 11: AI Debug Package generation
- Layer 12: CI integration with automatic health scoring
