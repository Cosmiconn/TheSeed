// =============================================================================
// server/ai/AIParallelSystem.h — KI-Parallelisierung im ThreadPool (AP-47 Fix)
// =============================================================================
// KORREKTUR: AI-Update wird über den Work-Stealing Thread Pool parallel ausgeführt.
// Entities werden in Chunks aufgeteilt, jeder Worker verarbeitet einen Chunk.
// Thread-safes durch ECS-Archetype-Design (keine shared mutable state).
// =============================================================================
#pragma once
#include "AIBehavior.h"
#include "../ThreadPool.h"
#include "../../ecs/ecs_EcsWorld.h"
#include "../../ecs/Components.h"

#include <vector>
#include <atomic>
#include <mutex>
#include <barrier>

namespace ai {

// =============================================================================
// AI Work Chunk — Ein Block von Entities für einen Worker
// =============================================================================
struct AIWorkChunk {
    std::vector<ecs::EntityHandle> entities;
    float deltaTime = 0.0f;
    ecs::EcsWorld* world = nullptr;
    AISystem* aiSystem = nullptr;
};

// =============================================================================
// AI Parallel Update Result
// =============================================================================
struct AIUpdateStats {
    uint32_t entitiesProcessed = 0;
    uint32_t chunksProcessed = 0;
    float totalTimeMs = 0.0f;
};

// =============================================================================
// AIParallelSystem — Thread-Pool-basierte KI-Ausführung
// =============================================================================
class AIParallelSystem {
    ThreadPool& threadPool;
    AISystem& aiSystem;

    // Chunking config
    static constexpr size_t ENTITIES_PER_CHUNK = 64;
    static constexpr size_t MIN_ENTITIES_FOR_PARALLEL = 32;

    // Stats
    std::atomic<uint32_t> entitiesProcessed{0};
    std::atomic<uint32_t> chunksProcessed{0};

public:
    AIParallelSystem(ThreadPool& pool, AISystem& ai)
        : threadPool(pool), aiSystem(ai) {}

    ~AIParallelSystem() = default;

    AIParallelSystem(const AIParallelSystem&) = delete;
    AIParallelSystem& operator=(const AIParallelSystem&) = delete;

    // ===================================================================
    // Parallel Update — Haupt-Entry Point
    // ===================================================================
    [[nodiscard]] AIUpdateStats UpdateParallel(ecs::EcsWorld& world, float deltaTime);

    // ===================================================================
    // Stats
    // ===================================================================
    [[nodiscard]] uint32_t GetEntitiesProcessed() const { return entitiesProcessed.load(); }
    [[nodiscard]] uint32_t GetChunksProcessed() const { return chunksProcessed.load(); }

private:
    [[nodiscard]] std::vector<AIWorkChunk> BuildChunks(ecs::EcsWorld& world, float deltaTime);
    void ProcessChunk(const AIWorkChunk& chunk);
};

} // namespace ai
