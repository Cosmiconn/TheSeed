// =============================================================================
// server/ai/AIParallelSystem.cpp — KI-Parallelisierung Implementierung
// =============================================================================
#include "AIParallelSystem.h"
#include "../../core/Log.h"
#include <chrono>

namespace ai {

// =============================================================================
// Parallel Update
// =============================================================================
AIUpdateStats AIParallelSystem::UpdateParallel(ecs::EcsWorld& world, float deltaTime) {
    auto start = std::chrono::steady_clock::now();

    entitiesProcessed.store(0);
    chunksProcessed.store(0);

    // Build work chunks
    auto chunks = BuildChunks(world, deltaTime);

    if (chunks.empty()) {
        return AIUpdateStats{};
    }

    // Single-threaded fallback for small entity counts
    if (chunks.size() == 1) {
        ProcessChunk(chunks[0]);

        auto end = std::chrono::steady_clock::now();
        float elapsed = std::chrono::duration<float, std::milli>(end - start).count();

        return AIUpdateStats{
            .entitiesProcessed = entitiesProcessed.load(),
            .chunksProcessed = chunksProcessed.load(),
            .totalTimeMs = elapsed
        };
    }

    // Parallel execution via ThreadPool
    std::atomic<size_t> completedChunks{0};
    std::mutex completionMutex;
    std::condition_variable completionCV;

    for (auto& chunk : chunks) {
        threadPool.Submit([this, &chunk, &completedChunks, &completionMutex, &completionCV]() {
            ProcessChunk(chunk);
            completedChunks++;
            completionCV.notify_one();
        });
    }

    // Wait for all chunks to complete
    {
        std::unique_lock lock(completionMutex);
        completionCV.wait(lock, [&completedChunks, &chunks]() {
            return completedChunks.load() >= chunks.size();
        });
    }

    auto end = std::chrono::steady_clock::now();
    float elapsed = std::chrono::duration<float, std::milli>(end - start).count();

    AddLog("[AI] Parallel update: {} entities in {} chunks, {:.2f}ms",
           entitiesProcessed.load(), chunksProcessed.load(), elapsed);

    return AIUpdateStats{
        .entitiesProcessed = entitiesProcessed.load(),
        .chunksProcessed = chunksProcessed.load(),
        .totalTimeMs = elapsed
    };
}

// =============================================================================
// Build Chunks — Aufteilung in parallele Arbeitseinheiten
// =============================================================================
std::vector<AIWorkChunk> AIParallelSystem::BuildChunks(ecs::EcsWorld& world, float deltaTime) {
    std::vector<AIWorkChunk> chunks;

    // Query all entities with AIState
    auto query = world.Query<game::AIState>();

    AIWorkChunk currentChunk;
    currentChunk.deltaTime = deltaTime;
    currentChunk.world = &world;
    currentChunk.aiSystem = &aiSystem;

    for (auto [handle] : query) {
        currentChunk.entities.push_back(handle);

        if (currentChunk.entities.size() >= ENTITIES_PER_CHUNK) {
            chunks.push_back(std::move(currentChunk));
            currentChunk = AIWorkChunk{};
            currentChunk.deltaTime = deltaTime;
            currentChunk.world = &world;
            currentChunk.aiSystem = &aiSystem;
        }
    }

    // Add remaining entities
    if (!currentChunk.entities.empty()) {
        chunks.push_back(std::move(currentChunk));
    }

    // If total entities < MIN_ENTITIES_FOR_PARALLEL, merge into single chunk
    size_t totalEntities = 0;
    for (const auto& chunk : chunks) {
        totalEntities += chunk.entities.size();
    }

    if (totalEntities < MIN_ENTITIES_FOR_PARALLEL && chunks.size() > 1) {
        AIWorkChunk merged;
        merged.deltaTime = deltaTime;
        merged.world = &world;
        merged.aiSystem = &aiSystem;
        for (auto& chunk : chunks) {
            merged.entities.insert(merged.entities.end(),
                chunk.entities.begin(), chunk.entities.end());
        }
        chunks.clear();
        chunks.push_back(std::move(merged));
    }

    return chunks;
}

// =============================================================================
// Process Chunk — Ein Worker verarbeitet seine Entities
// =============================================================================
void AIParallelSystem::ProcessChunk(const AIWorkChunk& chunk) {
    if (!chunk.world || !chunk.aiSystem) return;

    for (const auto& entityHandle : chunk.entities) {
        auto* aiState = chunk.world->GetComponent<game::AIState>(entityHandle);
        if (!aiState) continue;

        // Get or create AI context for this entity
        auto* context = chunk.aiSystem->GetContext(entityHandle);
        if (!context) {
            // Entity not registered with AI system yet
            continue;
        }

        // Update AI context
        auto& mutableContext = const_cast<AIContext&>(*context);
        mutableContext.timeInState += chunk.deltaTime;
        mutableContext.world = chunk.world;
        mutableContext.self = entityHandle;

        // Execute behavior tree
        // Note: In a full implementation, we'd look up the behavior tree
        // for this entity and execute it. For now, we update the context.

        // Update entity state based on AI
        auto* transform = chunk.world->GetComponent<game::Transform>(entityHandle);
        if (transform && mutableContext.targetEntity != 0) {
            auto* targetTransform = chunk.world->GetComponent<game::Transform>(
                ecs::EntityHandle(mutableContext.targetEntity));
            if (targetTransform) {
                // Simple chase logic (would be replaced by behavior tree)
                float dx = targetTransform->x - transform->x;
                float dz = targetTransform->z - transform->z;
                float dist = std::sqrt(dx*dx + dz*dz);
                if (dist > 0.1f) {
                    transform->x += (dx / dist) * 2.0f * chunk.deltaTime;
                    transform->z += (dz / dist) * 2.0f * chunk.deltaTime;
                }
            }
        }
    }

    entitiesProcessed.fetch_add(static_cast<uint32_t>(chunk.entities.size()));
    chunksProcessed.fetch_add(1);
}

} // namespace ai
