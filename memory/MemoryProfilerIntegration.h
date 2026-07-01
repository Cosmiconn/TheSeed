#pragma once
// =============================================================================
// memory/MemoryProfilerIntegration.h — Integration des Profilers in Allokatoren
// =============================================================================
// Diese Header-Datei ermoeglicht das einfache Tracking von Allokationen
// in bestehenden Allokatoren. Wird in jedem Allokator-Header eingebunden.
// =============================================================================
#include "MemoryProfiler.h"

namespace memory {

// =============================================================================
// INTEGRATIONS-HILFSFUNKTIONEN
// =============================================================================

// Tracking fuer PoolAllocator
inline void TrackPoolAllocation(void* ptr, size_t size, std::string_view poolName) {
    MemoryProfiler::GetInstance().TrackAllocation(ptr, size, 64, poolName, "PoolAllocator", 0);
}

inline void TrackPoolFree(void* ptr, std::string_view poolName) {
    MemoryProfiler::GetInstance().TrackFree(ptr, poolName);
}

// Tracking fuer StackAllocator
inline void TrackStackAllocation(void* ptr, size_t size, size_t marker, std::string_view stackName) {
    MemoryProfiler::GetInstance().TrackAllocation(ptr, size, 64, stackName, "StackAllocator", 0);
}

// Tracking fuer FreelistAllocator
inline void TrackFreelistAllocation(void* ptr, size_t size, std::string_view freelistName) {
    MemoryProfiler::GetInstance().TrackAllocation(ptr, size, 64, freelistName, "FreelistAllocator", 0);
}

inline void TrackFreelistFree(void* ptr, std::string_view freelistName) {
    MemoryProfiler::GetInstance().TrackFree(ptr, freelistName);
}

// Tracking fuer SlabAllocator
inline void TrackSlabAllocation(void* ptr, size_t size, size_t slabIndex, std::string_view slabName) {
    std::string name = std::string(slabName) + "_slab" + std::to_string(slabIndex);
    MemoryProfiler::GetInstance().TrackAllocation(ptr, size, 64, name, "SlabAllocator", 0);
}

inline void TrackSlabFree(void* ptr, size_t slabIndex, std::string_view slabName) {
    std::string name = std::string(slabName) + "_slab" + std::to_string(slabIndex);
    MemoryProfiler::GetInstance().TrackFree(ptr, name);
}

// =============================================================================
// ECS-SPEICHER-TRACKING
// =============================================================================
inline void TrackEcsChunkMemory(size_t totalMemory, size_t usedMemory,
                                 size_t entityCount, size_t chunkCount,
                                 size_t archetypeCount, size_t componentTypes) {
    MemoryProfiler::GetInstance().TrackEcsMemory(
        totalMemory, usedMemory, entityCount, chunkCount, archetypeCount, componentTypes);
}

} // namespace memory
