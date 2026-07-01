// =============================================================================
// server/ai/AIParallelSystem.cpp — KI-Parallelisierung Implementierung (P2-FIX)
// =============================================================================
// KORREKTUR P2:
// • Verwendet ecs::AIComponent und ecs::PositionComponent statt
//   nicht-existenten game::AIState und game::Transform
// • ProcessChunk verwendet korrekte ECS-API
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

    auto chunks = BuildChunks(world, deltaTime);

    if (chunks.empty()) {
        return AIUpdateStats{};
    }

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
// Build Chunks
// =============================================================================
std::vector<AIWorkChunk> AIParallelSystem::BuildChunks(ecs::EcsWorld& world, float deltaTime) {
    std::vector<AIWorkChunk> chunks;

    // FIX P2: Verwendet ecs::AIComponent statt game::AIState
    auto query = world.QueryEntities<ecs::AIComponent>();

    AIWorkChunk currentChunk;
    currentChunk.deltaTime = deltaTime;
    currentChunk.world = &world;
    currentChunk.aiSystem = &aiSystem;

    for (auto [handle, ai] : query) {
        (void)ai;
        currentChunk.entities.push_back(handle);

        if (currentChunk.entities.size() >= ENTITIES_PER_CHUNK) {
            chunks.push_back(std::move(currentChunk));
            currentChunk = AIWorkChunk{};
            currentChunk.deltaTime = deltaTime;
            currentChunk.world = &world;
            currentChunk.aiSystem = &aiSystem;
        }
    }

    if (!currentChunk.entities.empty()) {
        chunks.push_back(std::move(currentChunk));
    }

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
// Process Chunk
// =============================================================================
void AIParallelSystem::ProcessChunk(const AIWorkChunk& chunk) {
    if (!chunk.world || !chunk.aiSystem) return;

    for (const auto& entityHandle : chunk.entities) {
        auto* aiComp = chunk.world->GetComponent<ecs::AIComponent>(entityHandle);
        if (!aiComp) continue;

        auto* context = chunk.aiSystem->GetContext(entityHandle);
        if (!context) continue;

        auto& mutableContext = const_cast<AIContext&>(*context);
        mutableContext.timeInState += chunk.deltaTime;
        mutableContext.world = chunk.world;
        mutableContext.self = entityHandle;

        // FIX P2: Verwendet ecs::PositionComponent statt game::Transform
        auto* pos = chunk.world->GetComponent<ecs::PositionComponent>(entityHandle);
        if (pos && mutableContext.targetEntity != 0) {
            auto* targetPos = chunk.world->GetComponent<ecs::PositionComponent>(
                ecs::EntityHandle(mutableContext.targetEntity));
            if (targetPos) {
                float dx = targetPos->x - pos->x;
                float dz = targetPos->z - pos->z;
                float dist = std::sqrt(dx*dx + dz*dz);
                if (dist > 0.1f) {
                    pos->x += (dx / dist) * 2.0f * chunk.deltaTime;
                    pos->z += (dz / dist) * 2.0f * chunk.deltaTime;
                }
            }
        }
    }

    entitiesProcessed.fetch_add(static_cast<uint32_t>(chunk.entities.size()));
    chunksProcessed.fetch_add(1);
}

} // namespace ai
