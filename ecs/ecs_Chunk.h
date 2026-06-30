#pragma once
// =============================================================================
// ecs/ecs_Chunk.h — SOA-Speicher-Chunk (P4)
// =============================================================================
// KORREKTUR P4: GetMemorySize() hinzugefuegt. Prefetching fuer Iterationen.
// SIMD-freundliche Alignment (64 Bytes). Null-Initialisierung optimiert.
// =============================================================================
#include "ecs_Types.h"
#include "ecs_ComponentTraits.h"
#include "../core/Log.h"

#include <cstdint>
#include <cstddef>
#include <cstring>
#include <memory>
#include <vector>
#include <algorithm>

#ifdef _WIN32
    #include <intrin.h>
#else
    #include <immintrin.h>
#endif

namespace ecs {

// =============================================================================
// CHUNK
// =============================================================================
class Chunk {
public:
    static constexpr size_t MAX_ENTITIES = 1024;
    static constexpr size_t ALIGNMENT = 64; // Cache-line + AVX-512

private:
    ComponentMask componentMask;
    std::unique_ptr<uint8_t[]> memory;
    size_t memorySize = 0;
    size_t entityCount = 0;
    size_t entityCapacity = 0;

    struct ComponentInfo {
        ComponentTypeId typeId;
        size_t offset;
        size_t size;
        size_t alignment;
    };
    std::vector<ComponentInfo> componentInfos;
    EntityHandle* entityHandles = nullptr;

public:
    explicit Chunk(const ComponentMask& mask) : componentMask(mask) {}

    // ===================================================================
    // Initialisierung
    // ===================================================================
    template<typename... Components>
    void Initialize() {
        memorySize = 0;
        componentInfos.clear();

        // Entity-Handles Array
        size_t handlesSize = MAX_ENTITIES * sizeof(EntityHandle);
        memorySize += handlesSize;

        size_t currentOffset = handlesSize;

        auto addComponent = [&]<typename T>() {
            if (!componentMask.Test(ComponentTraits<T>::GetId())) return;

            size_t align = ComponentTraits<T>::Align();
            size_t alignedOffset = (currentOffset + align - 1) & ~(align - 1);

            size_t componentSize = ComponentTraits<T>::Size() * MAX_ENTITIES;
            if (alignedOffset + componentSize > 1024 * 1024) {
                AddLog("[ECS] WARNUNG: Chunk wuerde 1MB ueberschreiten!");
            }

            componentInfos.push_back({
                ComponentTraits<T>::GetId(),
                alignedOffset,
                ComponentTraits<T>::Size(),
                align
            });

            currentOffset = alignedOffset + componentSize;
        };

        (addComponent.template operator()<Components>(), ...);

        memorySize = (currentOffset + ALIGNMENT - 1) & ~(ALIGNMENT - 1);

        #ifdef _WIN32
        memory.reset(static_cast<uint8_t*>(_aligned_malloc(memorySize, ALIGNMENT)));
        #else
        memory.reset(static_cast<uint8_t*>(aligned_alloc(ALIGNMENT, memorySize)));
        #endif

        if (!memory) {
            AddLog("[ECS] FEHLER: Chunk-Speicherallokation fehlgeschlagen!");
            return;
        }

        // Null-Initialisierung (SIMD-optimiert)
        std::memset(memory.get(), 0, memorySize);

        entityHandles = reinterpret_cast<EntityHandle*>(memory.get());
        entityCapacity = MAX_ENTITIES;

        AddLog("[ECS] Chunk initialisiert: {} Bytes, {} Komponenten, Kapazitaet {}",
               memorySize, componentInfos.size(), entityCapacity);
    }

    // ===================================================================
    // Entity Management
    // ===================================================================
    [[nodiscard]] size_t AllocateEntity(EntityHandle entity) {
        if (entityCount >= entityCapacity) {
            AddLog("[ECS] Chunk voll! Entity {} kann nicht allokiert werden.", entity);
            return SIZE_MAX;
        }
        entityHandles[entityCount] = entity;
        return entityCount++;
    }

    void RemoveEntity(size_t index) {
        if (index >= entityCount) return;

        if (index != entityCount - 1) {
            EntityHandle lastEntity = entityHandles[entityCount - 1];
            entityHandles[index] = lastEntity;

            for (const auto& info : componentInfos) {
                uint8_t* src = memory.get() + info.offset + (entityCount - 1) * info.size;
                uint8_t* dst = memory.get() + info.offset + index * info.size;
                std::memcpy(dst, src, info.size);
            }
        }

        entityCount--;
    }

    [[nodiscard]] EntityHandle GetEntity(size_t index) const {
        if (index >= entityCount) return INVALID_ENTITY;
        return entityHandles[index];
    }

    [[nodiscard]] size_t GetEntityCount() const { return entityCount; }
    [[nodiscard]] size_t GetCapacity() const { return entityCapacity; }
    [[nodiscard]] bool IsFull() const { return entityCount >= entityCapacity; }
    [[nodiscard]] size_t GetMemorySize() const { return memorySize; }

    // ===================================================================
    // Component Access
    // ===================================================================
    template<typename T>
    [[nodiscard]] T* GetComponent(size_t index) {
        ComponentTypeId id = ComponentTraits<T>::GetId();
        for (const auto& info : componentInfos) {
            if (info.typeId == id) {
                return reinterpret_cast<T*>(memory.get() + info.offset + index * info.size);
            }
        }
        return nullptr;
    }

    template<typename T>
    void SetComponent(size_t index, const T& value) {
        T* ptr = GetComponent<T>(index);
        if (ptr) *ptr = value;
    }

    // ===================================================================
    // Prefetching (P4: Performance)
    // ===================================================================
    void PrefetchComponents(size_t index) const {
        if (index >= entityCount) return;
        for (const auto& info : componentInfos) {
            const void* addr = memory.get() + info.offset + index * info.size;
            #ifdef _WIN32
            _mm_prefetch(static_cast<const char*>(addr), _MM_HINT_T0);
            #else
            __builtin_prefetch(addr, 1, 3);
            #endif
        }
    }

    // ===================================================================
    // Transfer
    // ===================================================================
    void TransferComponents(const Chunk& source, size_t sourceIndex, size_t destIndex) {
        for (const auto& info : componentInfos) {
            const uint8_t* src = source.GetRawComponentPtr(info.typeId, sourceIndex);
            if (src) {
                uint8_t* dst = memory.get() + info.offset + destIndex * info.size;
                std::memcpy(dst, src, info.size);
            }
        }
    }

    // ===================================================================
    // Queries
    // ===================================================================
    [[nodiscard]] const ComponentMask& GetComponentMask() const { return componentMask; }
    [[nodiscard]] bool Matches(const ComponentMask& queryMask) const {
        return componentMask.Contains(queryMask);
    }

private:
    [[nodiscard]] const uint8_t* GetRawComponentPtr(ComponentTypeId id, size_t index) const {
        for (const auto& info : componentInfos) {
            if (info.typeId == id) {
                return memory.get() + info.offset + index * info.size;
            }
        }
        return nullptr;
    }
};

} // namespace ecs
