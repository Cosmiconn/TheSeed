#pragma once
// =============================================================================
// ecs/Chunk.h — SOA Memory Storage for Homogeneous Entities
// AP-20: Archetype Storage
// =============================================================================
#include "Types.h"
#include "ComponentTraits.h"
#include <memory>
#include <cstring>
#include <algorithm>
#include <cassert>

namespace ecs {

// A Chunk stores entities of the same archetype in SOA layout
// Memory layout: [entityHandles][component0_array][component1_array]...[componentN_array]
class Chunk {
public:
    static constexpr size_t CAPACITY = 1024; // Entities per chunk

private:
    struct ComponentLayout {
        ComponentTypeId typeId;
        ComponentOffset offset;  // From start of component data area
        ComponentSize size;
        ComponentAlign alignment;
    };

    std::unique_ptr<std::byte[]> memory;
    size_t memorySize = 0;

    std::vector<ComponentLayout> layouts;
    size_t entitySize = 0;  // Total bytes per entity (all components)

    uint32_t entityCount = 0;

    // Offsets into memory block
    size_t entityHandleOffset = 0;  // Where EntityHandle[] starts
    size_t componentDataOffset = 0; // Where component arrays start

public:
    Chunk() = default;

    explicit Chunk(const std::vector<ComponentMeta>& componentMetas) {
        Initialize(componentMetas);
    }

    void Initialize(const std::vector<ComponentMeta>& componentMetas) {
        layouts.clear();

        // Calculate offsets for each component array
        // Layout: [EntityHandle[CAPACITY]] [align] [comp0[CAPACITY]] [align] [comp1[CAPACITY]] ...
        entityHandleOffset = 0;
        componentDataOffset = sizeof(EntityHandle) * CAPACITY;
        // Align component data to 64 bytes for cache efficiency
        componentDataOffset = (componentDataOffset + 63) & ~63;

        size_t currentOffset = componentDataOffset;

        for (const auto& meta : componentMetas) {
            // Align to component's alignment requirement
            size_t alignedOffset = (currentOffset + meta.alignment - 1) & ~(meta.alignment - 1);

            layouts.push_back({
                .typeId = meta.typeId,
                .offset = alignedOffset,
                .size = meta.size,
                .alignment = meta.alignment
            });

            currentOffset = alignedOffset + meta.size * CAPACITY;
        }

        memorySize = currentOffset;
        memory = std::make_unique<std::byte[]>(memorySize);

        // Zero-initialize
        std::memset(memory.get(), 0, memorySize);
    }

    [[nodiscard]] bool IsFull() const noexcept { return entityCount >= CAPACITY; }
    [[nodiscard]] bool IsEmpty() const noexcept { return entityCount == 0; }
    [[nodiscard]] uint32_t Count() const noexcept { return entityCount; }
    [[nodiscard]] uint32_t Capacity() const noexcept { return CAPACITY; }

    // Allocate a new entity slot, returns dense index
    [[nodiscard]] ArchetypeIndex AllocateEntity(EntityHandle handle) {
        assert(entityCount < CAPACITY && "Chunk is full");
        ArchetypeIndex idx = entityCount++;
        GetEntityHandle(idx) = handle;
        return idx;
    }

    // Remove entity by swapping with last (O(1))
    // Returns the handle that was swapped into this slot (or INVALID if no swap)
    [[nodiscard]] EntityHandle RemoveEntity(ArchetypeIndex idx) {
        assert(idx < entityCount && "Invalid entity index");

        EntityHandle movedHandle = INVALID_ENTITY;

        if (idx != entityCount - 1) {
            // Swap with last entity
            ArchetypeIndex lastIdx = entityCount - 1;
            movedHandle = GetEntityHandle(lastIdx);

            // Swap entity handle
            GetEntityHandle(idx) = movedHandle;

            // Swap all component data
            for (const auto& layout : layouts) {
                std::byte* dst = GetComponentPtr(idx, layout);
                std::byte* src = GetComponentPtr(lastIdx, layout);
                std::memcpy(dst, src, layout.size);
            }
        }

        entityCount--;
        return movedHandle;
    }

    // Get entity handle at dense index
    [[nodiscard]] EntityHandle& GetEntityHandle(ArchetypeIndex idx) {
        assert(idx < entityCount && "Invalid entity index");
        auto* handles = reinterpret_cast<EntityHandle*>(memory.get() + entityHandleOffset);
        return handles[idx];
    }

    [[nodiscard]] const EntityHandle& GetEntityHandle(ArchetypeIndex idx) const {
        assert(idx < entityCount && "Invalid entity index");
        auto* handles = reinterpret_cast<const EntityHandle*>(memory.get() + entityHandleOffset);
        return handles[idx];
    }

    // Get component pointer for an entity (raw bytes)
    [[nodiscard]] std::byte* GetComponentPtr(ArchetypeIndex idx, const ComponentLayout& layout) {
        return memory.get() + layout.offset + (idx * layout.size);
    }

    [[nodiscard]] const std::byte* GetComponentPtr(ArchetypeIndex idx, const ComponentLayout& layout) const {
        return memory.get() + layout.offset + (idx * layout.size);
    }

    // Type-safe component access
    template<typename T>
    [[nodiscard]] T* GetComponent(ArchetypeIndex idx, ComponentTypeId typeId) {
        auto it = std::ranges::find_if(layouts, 
            [typeId](const auto& l){ return l.typeId == typeId; });
        if (it == layouts.end()) return nullptr;
        return reinterpret_cast<T*>(GetComponentPtr(idx, *it));
    }

    template<typename T>
    [[nodiscard]] const T* GetComponent(ArchetypeIndex idx, ComponentTypeId typeId) const {
        auto it = std::ranges::find_if(layouts,
            [typeId](const auto& l){ return l.typeId == typeId; });
        if (it == layouts.end()) return nullptr;
        return reinterpret_cast<const T*>(GetComponentPtr(idx, *it));
    }

    // Move entity from another chunk (used during archetype transitions)
    void MoveEntityFrom(ArchetypeIndex dstIdx, const Chunk& srcChunk, ArchetypeIndex srcIdx,
                        const std::vector<ComponentTypeId>& sharedComponents) {
        GetEntityHandle(dstIdx) = srcChunk.GetEntityHandle(srcIdx);

        for (ComponentTypeId typeId : sharedComponents) {
            auto dstLayout = std::ranges::find_if(layouts,
                [typeId](const auto& l){ return l.typeId == typeId; });
            auto srcLayout = std::ranges::find_if(srcChunk.layouts,
                [typeId](const auto& l){ return l.typeId == typeId; });

            if (dstLayout != layouts.end() && srcLayout != srcChunk.layouts.end()) {
                std::byte* dst = GetComponentPtr(dstIdx, *dstLayout);
                const std::byte* src = srcChunk.GetComponentPtr(srcIdx, *srcLayout);
                std::memcpy(dst, src, dstLayout->size);
            }
        }
    }

    // Iterate over all entities in chunk (for system processing)
    template<typename Func>
    void ForEachEntity(Func&& func) {
        for (uint32_t i = 0; i < entityCount; ++i) {
            func(GetEntityHandle(i), i);
        }
    }
};

} // namespace ecs
