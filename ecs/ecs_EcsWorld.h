#pragma once
// =============================================================================
// ecs/EcsWorld.h — Main ECS World Interface
// AP-20: Archetype Storage
// =============================================================================
#include "Types.h"
#include "ComponentTraits.h"
#include "Chunk.h"
#include "Archetype.h"
#include "EntityManager.h"
#include "Query.h"
#include <vector>
#include <map>
#include <memory>
#include <mutex>
#include <type_traits>
#include <format>

namespace ecs {

class EcsWorld {
    std::map<ArchetypeSignature, std::unique_ptr<Archetype>> archetypes;
    std::mutex archetypeMutex;

    EntityManager entityManager;

    // Component registration cache
    std::vector<ComponentMeta> registeredMetas;

public:
    EcsWorld() = default;
    ~EcsWorld() = default;

    EcsWorld(const EcsWorld&) = delete;
    EcsWorld& operator=(const EcsWorld&) = delete;

    // ===================================================================
    // ENTITY LIFECYCLE
    // ===================================================================

    // Create entity with no components (empty archetype)
    [[nodiscard]] EntityHandle CreateEntity() {
        return entityManager.CreateEntity();
    }

    // Create entity with initial components
    template<typename... Components>
    [[nodiscard]] EntityHandle CreateEntity(Components&&... components) {
        static_assert(sizeof...(Components) > 0, "Use CreateEntity() for empty entities");

        EntityHandle handle = entityManager.CreateEntity();

        // Register components
        (RegisterComponent<Components>(), ...);

        // Get or create archetype
        auto* archetype = GetOrCreateArchetype<Components...>();

        // Allocate in chunk
        auto [chunk, denseIdx] = archetype->AllocateEntity(handle);

        // Set components
        SetComponents(chunk, denseIdx, std::forward<Components>(components)...);

        // Update entity record
        entityManager.UpdateEntityLocation(handle, archetype, chunk, denseIdx);

        return handle;
    }

    // Destroy entity
    void DestroyEntity(EntityHandle handle) {
        auto* record = entityManager.GetRecord(handle);
        if (!record || !record->isAlive) return;

        if (record->chunk) {
            record->archetype->RemoveEntity(record->chunk, record->denseIndex);
        }

        entityManager.DestroyEntity(handle);
    }

    [[nodiscard]] bool IsAlive(EntityHandle handle) const {
        return entityManager.IsAlive(handle);
    }

    // ===================================================================
    // COMPONENT MANAGEMENT
    // ===================================================================

    // Add component to existing entity (archetype transition)
    template<typename T>
    void AddComponent(EntityHandle handle, T component) {
        static_assert(std::is_trivially_copyable_v<T>, "Component must be trivially copyable");

        auto* record = entityManager.GetRecord(handle);
        if (!record || !record->isAlive) return;

        RegisterComponent<T>();
        ComponentTypeId newTypeId = ComponentId<T>();

        Archetype* oldArchetype = record->archetype;
        Archetype* newArchetype = nullptr;

        if (oldArchetype) {
            // Transition: old archetype -> new archetype with added component
            auto oldSig = oldArchetype->GetSignature();
            if (oldSig.test(newTypeId)) return; // Already has component

            auto newSig = oldSig;
            newSig.set(newTypeId);

            newArchetype = GetOrCreateArchetypeBySignature(newSig, oldArchetype, newTypeId);
        } else {
            // Empty entity -> new archetype with single component
            newArchetype = GetOrCreateArchetype<T>();
        }

        // Move entity to new archetype
        MoveEntityToArchetype(handle, record, newArchetype, &component, newTypeId);
    }

    // Remove component from entity (archetype transition)
    template<typename T>
    void RemoveComponent(EntityHandle handle) {
        auto* record = entityManager.GetRecord(handle);
        if (!record || !record->isAlive) return;

        Archetype* oldArchetype = record->archetype;
        if (!oldArchetype) return;

        ComponentTypeId typeId = ComponentId<T>();
        auto oldSig = oldArchetype->GetSignature();
        if (!oldSig.test(typeId)) return; // Doesn't have component

        auto newSig = oldSig;
        newSig.reset(typeId);

        Archetype* newArchetype = GetOrCreateArchetypeBySignature(newSig, oldArchetype, typeId);
        MoveEntityToArchetype(handle, record, newArchetype, nullptr, typeId);
    }

    // Get component pointer (returns nullptr if entity doesn't have component)
    template<typename T>
    [[nodiscard]] T* GetComponent(EntityHandle handle) {
        auto* record = entityManager.GetRecord(handle);
        if (!record || !record->chunk) return nullptr;

        return record->chunk->GetComponent<T>(record->denseIndex, ComponentId<T>());
    }

    template<typename T>
    [[nodiscard]] const T* GetComponent(EntityHandle handle) const {
        auto* record = entityManager.GetRecord(handle);
        if (!record || !record->chunk) return nullptr;

        return record->chunk->GetComponent<T>(record->denseIndex, ComponentId<T>());
    }

    // Check if entity has component
    template<typename T>
    [[nodiscard]] bool HasComponent(EntityHandle handle) const {
        return GetComponent<T>(handle) != nullptr;
    }

    // ===================================================================
    // QUERIES
    // ===================================================================

    // Query for archetypes matching filter
    [[nodiscard]] QueryResult Query(const QueryFilter& filter) {
        std::vector<Archetype*> matches;

        for (auto& [sig, archetype] : archetypes) {
            if (filter.Matches(sig)) {
                matches.push_back(archetype.get());
            }
        }

        return QueryResult(matches);
    }

    // Typed query for specific components
    template<typename... Components>
    [[nodiscard]] TypedQuery<Components...> Query() {
        auto filter = QueryFilter::With<Components...>();
        return TypedQuery<Components...>(Query(filter));
    }

    // Query with exclusion
    template<typename... WithComponents, typename... WithoutComponents>
    [[nodiscard]] auto QueryWithExclusion() {
        auto filter = QueryFilter::With<WithComponents...>().Without<WithoutComponents...>();
        return TypedQuery<WithComponents...>(Query(filter));
    }

    // ===================================================================
    // STATISTICS
    // ===================================================================

    [[nodiscard]] uint32_t GetEntityCount() const { return entityManager.GetAliveCount(); }
    [[nodiscard]] uint32_t GetArchetypeCount() const { return static_cast<uint32_t>(archetypes.size()); }
    [[nodiscard]] uint32_t GetComponentTypeCount() const { return static_cast<uint32_t>(registeredMetas.size()); }

private:
    // Register component type (idempotent)
    template<typename T>
    void RegisterComponent() {
        ComponentRegistry::Instance().Register<T>();

        ComponentTypeId id = ComponentId<T>();
        if (id >= registeredMetas.size()) {
            registeredMetas.resize(id + 1);
        }
        registeredMetas[id] = {
            .typeId = id,
            .size = sizeof(T),
            .alignment = alignof(T),
            .typeIndex = std::type_index(typeid(T))
        };
    }

    // Get or create archetype for component combination
    template<typename... Components>
    [[nodiscard]] Archetype* GetOrCreateArchetype() {
        ArchetypeSignature sig;
        (sig.set(ComponentId<Components>()), ...);

        return GetOrCreateArchetypeBySignature(sig, nullptr, 0);
    }

    // Get or create archetype by signature (with optional parent for transition)
    [[nodiscard]] Archetype* GetOrCreateArchetypeBySignature(
        const ArchetypeSignature& sig, 
        Archetype* parentArchetype,
        ComponentTypeId transitionTypeId) {

        std::lock_guard lock(archetypeMutex);

        auto it = archetypes.find(sig);
        if (it != archetypes.end()) return it->second.get();

        // Build component metas from signature
        std::vector<ComponentMeta> metas;
        for (size_t i = 0; i < MAX_COMPONENTS; ++i) {
            if (sig.test(i) && i < registeredMetas.size()) {
                metas.push_back(registeredMetas[i]);
            }
        }

        auto archetype = std::make_unique<Archetype>(metas);
        auto* ptr = archetype.get();
        archetypes[sig] = std::move(archetype);

        return ptr;
    }

    // Set components on a newly allocated entity
    template<typename T, typename... Rest>
    void SetComponents(Chunk* chunk, ArchetypeIndex idx, T&& first, Rest&&... rest) {
        auto* ptr = chunk->GetComponent<std::remove_reference_t<T>>(idx, ComponentId<std::remove_reference_t<T>>());
        if (ptr) *ptr = std::forward<T>(first);
        if constexpr (sizeof...(Rest) > 0) {
            SetComponents(chunk, idx, std::forward<Rest>(rest)...);
        }
    }

    // Move entity between archetypes (add/remove component)
    void MoveEntityToArchetype(EntityHandle handle, EntityRecord* record, 
                               Archetype* newArchetype, 
                               void* newComponentData,
                               ComponentTypeId newComponentTypeId) {
        if (!record->chunk) {
            // Empty entity -> just allocate in new archetype
            auto [newChunk, newDenseIdx] = newArchetype->AllocateEntity(handle);

            // Set new component if adding
            if (newComponentData && newChunk) {
                auto* meta = newArchetype->GetComponentMeta(newComponentTypeId);
                if (meta) {
                    auto* ptr = newChunk->GetComponent<std::byte>(newDenseIdx, newComponentTypeId);
                    if (ptr) std::memcpy(ptr, newComponentData, meta->size);
                }
            }

            entityManager.UpdateEntityLocation(handle, newArchetype, newChunk, newDenseIdx);
            return;
        }

        // Remove from old archetype
        EntityHandle movedHandle = record->archetype->RemoveEntity(record->chunk, record->denseIndex);

        // Update the entity that was swapped into our old slot
        if (movedHandle.IsValid() && movedHandle != handle) {
            auto* movedRecord = entityManager.GetRecord(movedHandle);
            if (movedRecord) {
                movedRecord->chunk = record->chunk;
                movedRecord->denseIndex = record->denseIndex;
            }
        }

        // Allocate in new archetype
        auto [newChunk, newDenseIdx] = newArchetype->AllocateEntity(handle);

        // Copy shared components
        auto oldSig = record->archetype->GetSignature();
        auto newSig = newArchetype->GetSignature();

        std::vector<ComponentTypeId> sharedComponents;
        for (size_t i = 0; i < MAX_COMPONENTS; ++i) {
            if (oldSig.test(i) && newSig.test(i)) {
                sharedComponents.push_back(i);
            }
        }

        newChunk->MoveEntityFrom(newDenseIdx, *record->chunk, record->denseIndex, sharedComponents);

        // Set new component if adding
        if (newComponentData) {
            auto* meta = newArchetype->GetComponentMeta(newComponentTypeId);
            if (meta) {
                auto* ptr = newChunk->GetComponent<std::byte>(newDenseIdx, newComponentTypeId);
                if (ptr) std::memcpy(ptr, newComponentData, meta->size);
            }
        }

        entityManager.UpdateEntityLocation(handle, newArchetype, newChunk, newDenseIdx);
    }
};

} // namespace ecs
