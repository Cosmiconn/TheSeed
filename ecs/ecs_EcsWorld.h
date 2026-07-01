#pragma once
// =============================================================================
// ecs/ecs_EcsWorld.h — Haupt-ECS-Interface (AP-20, P4, AP-80)
// =============================================================================
// KORREKTUR P4: Chunk-Index wird korrekt berechnet. System-Getter hinzugefuegt.
// Archetype-Lookup optimiert mit unordered_map. Entity-Count gecached.
// KORREKTUR AP-80: Memory Profiler Integration fuer ECS-Speicher-Tracking.
// =============================================================================
#include "ecs_Types.h"
#include "ecs_Archetype.h"
#include "ecs_Query.h"
#include "ecs_EntityManager.h"
#include "Components.h"
#include "../core/Log.h"
#include "../memory/MemoryProfilerIntegration.h"

#include <functional>
#include <vector>
#include <unordered_map>
#include <string>
#include <memory>
#include <mutex>
#include <atomic>

namespace ecs {

// =============================================================================
// ECS WORLD
// =============================================================================
class EcsWorld {
public:
    using SystemFunc = std::function<void(EcsWorld&, float)>;

    struct NamedSystem {
        std::string name;
        SystemFunc func;
        bool enabled = true;
    };

private:
    // FIX P1-2: Component-Level Read-Write Locks
    mutable std::shared_mutex componentMutex;

    EntityManager entityManager;
    std::vector<std::unique_ptr<Archetype>> archetypes;
    std::unordered_map<ComponentMask, Archetype*> archetypeMap;
    std::vector<NamedSystem> systems;
    std::atomic<size_t> entityCount{0};
    std::mutex archetypeMutex;
    bool initialized = false;

    // AP-80: Profiling-Timer fuer periodisches ECS-Tracking
    std::chrono::steady_clock::time_point lastProfileUpdate;

public:
    EcsWorld() = default;
    ~EcsWorld() = default;

    EcsWorld(const EcsWorld&) = delete;
    EcsWorld& operator=(const EcsWorld&) = delete;

    // ===================================================================
    // Lifecycle
    // ===================================================================
    [[nodiscard]] bool Initialize();
    void Shutdown();
    [[nodiscard]] bool IsInitialized() const { return initialized; }

    // ===================================================================
    // Entity Management
    // ===================================================================
    [[nodiscard]] EntityHandle CreateEntity();
    void DestroyEntity(EntityHandle entity);
    [[nodiscard]] bool IsAlive(EntityHandle entity) const;
    [[nodiscard]] size_t GetEntityCount() const { return entityCount.load(); }

    // ===================================================================
    // Component Management
    // ===================================================================
    template<typename T>
    void AddComponent(EntityHandle entity, const T& component);

    template<typename T>
    void RemoveComponent(EntityHandle entity);

    template<typename T>
    [[nodiscard]] T* GetComponent(EntityHandle entity);

    template<typename T>
    [[nodiscard]] bool HasComponent(EntityHandle entity) const;

    // ===================================================================
    // Query
    // ===================================================================
    template<typename... Components>
    [[nodiscard]] Query<Components...> QueryEntities();

    // ===================================================================
    // System Management
    // ===================================================================
    void RegisterSystem(std::string_view name, SystemFunc func);
    void EnableSystem(std::string_view name);
    void DisableSystem(std::string_view name);
    [[nodiscard]] size_t GetSystemCount() const { return systems.size(); }
    [[nodiscard]] const std::vector<NamedSystem>& GetSystems() const { return systems; }

    // ===================================================================
    // Update
    // ===================================================================
    void Update(float deltaTime);

    // ===================================================================
    // Performance-Metriken
    // ===================================================================
    [[nodiscard]] size_t GetArchetypeCount() const { return archetypes.size(); }
    [[nodiscard]] size_t GetTotalChunkCount() const;
    [[nodiscard]] size_t GetTotalMemoryUsage() const;

    // AP-80: Profiling-Update
    void UpdateMemoryProfile();

private:
    [[nodiscard]] Archetype* FindOrCreateArchetype(const ComponentMask& mask);
    [[nodiscard]] std::pair<Archetype*, size_t> FindEntityLocation(EntityHandle entity) const;
    void MoveEntity(EntityHandle entity, Archetype* from, size_t fromIdx,
                    Archetype* to, size_t toIdx);
};

// =============================================================================
// TEMPLATE-IMPLEMENTIERUNGEN
// =============================================================================

template<typename T>
void EcsWorld::AddComponent(EntityHandle entity, const T& component) {
    std::unique_lock lock(componentMutex); // FIX P1-2: Write-Lock
    auto record = entityManager.GetRecord(entity);
    if (!record.chunk) {
        AddLog("[ECS] Ungueltige Entity {} fuer AddComponent", entity);
        return;
    }

    ComponentMask newMask = record.chunk->GetComponentMask();
    newMask.Set(ComponentTraits<T>::GetId());

    Archetype* newArchetype = FindOrCreateArchetype(newMask);
    if (newArchetype == record.chunk->GetArchetype()) {
        record.chunk->SetComponent<T>(record.denseIndex, component);
        return;
    }

    // KORREKTUR P4: Chunk-Index korrekt berechnen
    size_t newIndex = newArchetype->AllocateEntity(entity);
    auto [newChunk, newDenseIdx] = newArchetype->FindEntity(entity);
    if (newChunk) {
        newChunk->TransferComponents(*record.chunk, record.denseIndex, newDenseIdx);
        newChunk->SetComponent<T>(newDenseIdx, component);
    }

    // Alten Eintrag entfernen
    record.chunk->RemoveEntity(record.denseIndex);

    // Record updaten mit korrektem Chunk
    auto [updatedChunk, updatedIdx] = newArchetype->FindEntity(entity);
    if (updatedChunk) {
        entityManager.UpdateRecord(entity, updatedChunk, updatedIdx);
    }

    // AP-80: Speicher-Profil aktualisieren
    UpdateMemoryProfile();
}

template<typename T>
void EcsWorld::RemoveComponent(EntityHandle entity) {
    std::unique_lock lock(componentMutex); // FIX P1-2: Write-Lock
    auto record = entityManager.GetRecord(entity);
    if (!record.chunk) return;

    ComponentMask newMask = record.chunk->GetComponentMask();
    newMask.Clear(ComponentTraits<T>::GetId());

    Archetype* newArchetype = FindOrCreateArchetype(newMask);
    if (newArchetype == record.chunk->GetArchetype()) return;

    size_t newIndex = newArchetype->AllocateEntity(entity);
    auto [newChunk, newDenseIdx] = newArchetype->FindEntity(entity);
    if (newChunk) {
        newChunk->TransferComponents(*record.chunk, record.denseIndex, newDenseIdx);
    }

    record.chunk->RemoveEntity(record.denseIndex);

    auto [updatedChunk, updatedIdx] = newArchetype->FindEntity(entity);
    if (updatedChunk) {
        entityManager.UpdateRecord(entity, updatedChunk, updatedIdx);
    }

    // AP-80: Speicher-Profil aktualisieren
    UpdateMemoryProfile();
}

template<typename T>
T* EcsWorld::GetComponent(EntityHandle entity) {
    std::shared_lock lock(componentMutex); // FIX P1-2: Read-Lock
    auto record = entityManager.GetRecord(entity);
    if (!record.chunk) return nullptr;
    return record.chunk->GetComponent<T>(record.denseIndex);
}

template<typename T>
bool EcsWorld::HasComponent(EntityHandle entity) const {
    auto record = entityManager.GetRecord(entity);
    if (!record.chunk) return false;
    return record.chunk->GetComponentMask().Test(ComponentTraits<T>::GetId());
}

template<typename... Components>
Query<Components...> EcsWorld::QueryEntities() {
    std::shared_lock lock(componentMutex); // FIX P1-2: Read-Lock
    ComponentMask queryMask;
    (queryMask.Set(ComponentTraits<Components>::GetId()), ...);

    std::vector<Chunk*> matchingChunks;
    matchingChunks.reserve(archetypes.size() * 2); // P4: Reserve

    std::lock_guard lockArchetype(archetypeMutex);
    for (const auto& archetype : archetypes) {
        if (archetype->Matches(queryMask)) {
            for (size_t i = 0; i < archetype->GetChunkCount(); ++i) {
                auto* chunk = archetype->GetChunk(i);
                if (chunk && chunk->GetEntityCount() > 0) {
                    matchingChunks.push_back(chunk);
                }
            }
        }
    }

    return Query<Components...>(std::move(matchingChunks));
}

} // namespace ecs
