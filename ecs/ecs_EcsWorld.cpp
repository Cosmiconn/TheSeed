// =============================================================================
// ecs/ecs_EcsWorld.cpp — EcsWorld Implementation (P4)
// =============================================================================
// KORREKTUR P4: Chunk-Index korrekt berechnet. Performance-Metriken hinzugefuegt.
// Archetype-Lookup mit Mutex geschuetzt. Entity-Count atomar.
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
        for (size_t i = 0; i < archetype->GetChunkCount(); ++i) {
            auto* chunk = archetype->GetChunk(i);
            if (chunk) {
                bytes += chunk->GetMemorySize();
            }
        }
    }
    return bytes;
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
template class ecs::EcsWorld::Query<ecs::PositionComponent, ecs::VelocityComponent>;
template class ecs::EcsWorld::Query<ecs::PositionComponent, ecs::HealthComponent>;
template class ecs::EcsWorld::Query<ecs::PositionComponent, ecs::HealthComponent, ecs::NameComponent>;
template class ecs::EcsWorld::Query<ecs::PositionComponent, ecs::RenderComponent>;
template class ecs::EcsWorld::Query<ecs::AIStateComponent>;
template class ecs::EcsWorld::Query<ecs::CombatComponent>;
