// =============================================================================
// ecs/ecs_EcsWorld.cpp — EcsWorld Implementation (P4, AP-80)
// =============================================================================
// KORREKTUR P4: Chunk-Index korrekt berechnet. Performance-Metriken hinzugefuegt.
// Archetype-Lookup mit Mutex geschuetzt. Entity-Count atomar.
// KORREKTUR AP-80: Memory Profiler Integration fuer ECS-Speicher-Tracking.
// =============================================================================
#include "ecs_EcsWorld.h"
#include "ecs_Chunk.h"
#include "ecs_ComponentTraits.h"

namespace ecs {

// =============================================================================
// Lifecycle
// =============================================================================
bool EcsWorld::Initialize() {
    ComponentTraits<PositionComponent>::Register(0);
    ComponentTraits<RotationComponent>::Register(1);
    ComponentTraits<ScaleComponent>::Register(2);
    ComponentTraits<VelocityComponent>::Register(3);
    ComponentTraits<HealthComponent>::Register(4);
    ComponentTraits<NameComponent>::Register(5);
    ComponentTraits<RenderComponentECS>::Register(6);
    ComponentTraits<AIComponent>::Register(7);
    ComponentTraits<PlayerTag>::Register(8);
    ComponentTraits<LegacyIdComponent>::Register(9);
    ComponentTraits<StatusEffectComponent>::Register(10);
    ComponentTraits<CombatComponent>::Register(11);

    initialized = true;
    lastProfileUpdate = std::chrono::steady_clock::now();

    AddLog("[ECS] EcsWorld initialisiert mit {} Komponenten", 12);
    return true;
}

void EcsWorld::Shutdown() {
    std::lock_guard lock(archetypeMutex);
    archetypes.clear();
    archetypeMap.clear();
    systems.clear();
    entityManager.Clear();
    entityCount.store(0);
    initialized = false;

    // AP-80: ECS-Speicher-Tracking zuruecksetzen
    memory::TrackEcsChunkMemory(0, 0, 0, 0, 0, 0);

    AddLog("[ECS] EcsWorld heruntergefahren");
}

// =============================================================================
// Entity Management
// =============================================================================
EntityHandle EcsWorld::CreateEntity() {
    EntityHandle entity = entityManager.CreateEntity();

    ComponentMask emptyMask;
    Archetype* archetype = FindOrCreateArchetype(emptyMask);
    size_t index = archetype->AllocateEntity(entity);
    auto [chunk, denseIdx] = archetype->FindEntity(entity);

    if (chunk) {
        entityManager.UpdateRecord(entity, chunk, denseIdx);
    }

    entityCount.fetch_add(1);

    // AP-80: Periodisches Speicher-Profiling
    auto now = std::chrono::steady_clock::now();
    if (std::chrono::duration<float>(now - lastProfileUpdate).count() >= 1.0f) {
        UpdateMemoryProfile();
        lastProfileUpdate = now;
    }

    return entity;
}

void EcsWorld::DestroyEntity(EntityHandle entity) {
    if (!entityManager.IsAlive(entity)) return;

    auto record = entityManager.GetRecord(entity);
    if (record.chunk) {
        record.chunk->RemoveEntity(record.denseIndex);
    }

    entityManager.DestroyEntity(entity);
    entityCount.fetch_sub(1);

    // AP-80: Periodisches Speicher-Profiling
    auto now = std::chrono::steady_clock::now();
    if (std::chrono::duration<float>(now - lastProfileUpdate).count() >= 1.0f) {
        UpdateMemoryProfile();
        lastProfileUpdate = now;
    }
}

bool EcsWorld::IsAlive(EntityHandle entity) const {
    return entityManager.IsAlive(entity);
}

// =============================================================================
// System Management
// =============================================================================
void EcsWorld::RegisterSystem(std::string_view name, SystemFunc func) {
    systems.push_back(NamedSystem{
        .name = std::string(name),
        .func = std::move(func),
        .enabled = true
    });
    AddLog("[ECS] System '{}' registriert", name);
}

void EcsWorld::EnableSystem(std::string_view name) {
    for (auto& sys : systems) {
        if (sys.name == name) {
            sys.enabled = true;
            AddLog("[ECS] System '{}' aktiviert", name);
            return;
        }
    }
}

void EcsWorld::DisableSystem(std::string_view name) {
    for (auto& sys : systems) {
        if (sys.name == name) {
            sys.enabled = false;
            AddLog("[ECS] System '{}' deaktiviert", name);
            return;
        }
    }
}

// =============================================================================
// Update
// =============================================================================
void EcsWorld::Update(float deltaTime) {
    if (!initialized) return;

    for (auto& sys : systems) {
        if (sys.enabled && sys.func) {
            sys.func(*this, deltaTime);
        }
    }

    // AP-80: Periodisches Speicher-Profiling waehrend Update
    auto now = std::chrono::steady_clock::now();
    if (std::chrono::duration<float>(now - lastProfileUpdate).count() >= 5.0f) {
        UpdateMemoryProfile();
        lastProfileUpdate = now;
    }
}

// =============================================================================
// Performance-Metriken
// =============================================================================
size_t EcsWorld::GetTotalChunkCount() const {
    std::lock_guard lock(archetypeMutex);
    size_t count = 0;
    for (const auto& archetype : archetypes) {
        count += archetype->GetChunkCount();
    }
    return count;
}

size_t EcsWorld::GetTotalMemoryUsage() const {
    std::lock_guard lock(archetypeMutex);
    size_t bytes = 0;
    for (const auto& archetype : archetypes) {
        bytes += archetype->GetMemoryUsage();
    }
    return bytes;
}

// =============================================================================
// AP-80: Memory Profiler Update
// =============================================================================
void EcsWorld::UpdateMemoryProfile() {
    size_t totalMemory = GetTotalMemoryUsage();
    size_t chunkCount = GetTotalChunkCount();
    size_t archetypeCount = GetArchetypeCount();
    size_t entityCount = this->GetEntityCount();

    // Berechne genutzten Speicher (Entities * durchschnittliche Komponenten-Groesse)
    // Vereinfacht: Annahme ~200 Bytes pro Entity durchschnittlich
    size_t usedMemory = entityCount * 200;
    if (usedMemory > totalMemory) usedMemory = totalMemory;

    memory::TrackEcsChunkMemory(
        totalMemory,
        usedMemory,
        entityCount,
        chunkCount,
        archetypeCount,
        12 // Komponenten-Typen
    );

    // AP-80: Snapshot aufzeichnen
    memory::MemoryProfiler::GetInstance().RecordSnapshot();
}

// =============================================================================
// Interne Hilfsmethoden
// =============================================================================
Archetype* EcsWorld::FindOrCreateArchetype(const ComponentMask& mask) {
    std::lock_guard lock(archetypeMutex);

    auto it = archetypeMap.find(mask);
    if (it != archetypeMap.end()) {
        return it->second;
    }

    auto archetype = std::make_unique<Archetype>(mask);
    Archetype* ptr = archetype.get();
    archetypes.push_back(std::move(archetype));
    archetypeMap[mask] = ptr;
    return ptr;
}

std::pair<Archetype*, size_t> EcsWorld::FindEntityLocation(EntityHandle entity) const {
    std::lock_guard lock(archetypeMutex);
    for (const auto& archetype : archetypes) {
        auto [chunk, idx] = archetype->FindEntity(entity);
        if (chunk) {
            return {archetype.get(), idx};
        }
    }
    return {nullptr, SIZE_MAX};
}

void EcsWorld::MoveEntity(EntityHandle entity, Archetype* from, size_t fromIdx,
                          Archetype* to, size_t toIdx) {
    (void)entity; (void)from; (void)fromIdx; (void)to; (void)toIdx;
    // Wird von AddComponent/RemoveComponent aufgerufen
}

} // namespace ecs

// =============================================================================
// EXPLIZITE TEMPLATE-INSTANZIIERUNG (FIX: Verhindert Linker-Fehler)
// =============================================================================
template class ecs::EcsWorld::Query<ecs::PositionComponent>;
template class ecs::EcsWorld::Query<ecs::PositionComponent, ecs::VelocityComponent>;
template class ecs::EcsWorld::Query<ecs::PositionComponent, ecs::HealthComponent>;
template class ecs::EcsWorld::Query<ecs::PositionComponent, ecs::VelocityComponent, ecs::HealthComponent>;
template class ecs::EcsWorld::Query<ecs::PositionComponent, ecs::NameComponent>;
template class ecs::EcsWorld::Query<ecs::PositionComponent, ecs::VelocityComponent, ecs::HealthComponent, ecs::NameComponent>;
