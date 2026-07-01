#pragma once
// =============================================================================
// memory/MemoryProfiler.h — Memory Profiler (AP-80)
// =============================================================================
// Echtzeit-Tracking von Speicherallokationen fuer alle Allokatoren und das
// ECS. Erfasst: Allokationsgroessen, Fragmentierung, Leak-Erkennung,
// Peak-Nutzung, historische Daten. Thread-sicher, minimale Overhead.
// =============================================================================
#include <cstdint>
#include <cstddef>
#include <string>
#include <vector>
#include <unordered_map>
#include <mutex>
#include <memory>
#include <chrono>
#include <atomic>

namespace memory {

// =============================================================================
// ALLOKATIONS-RECORD — Einzelne Allokation
// =============================================================================
struct AllocationRecord {
    void* address = nullptr;
    size_t size = 0;
    size_t alignment = 0;
    std::string allocatorName;
    std::string sourceFile;
    uint32_t sourceLine = 0;
    std::chrono::steady_clock::time_point allocationTime;
    bool isFreed = false;
    std::chrono::steady_clock::time_point freeTime;
};

// =============================================================================
// ALLOKATOR-STATISTIKEN — Pro Allokator
// =============================================================================
struct AllocatorStats {
    std::string name;
    size_t totalAllocatedBytes = 0;      // Summe aller Allokationen
    size_t currentAllocatedBytes = 0;    // Aktuell belegt
    size_t peakAllocatedBytes = 0;       // Maximal belegt
    size_t totalAllocations = 0;         // Anzahl Allokationen
    size_t totalFrees = 0;               // Anzahl Freigaben
    size_t activeAllocations = 0;        // Aktuell nicht freigegeben
    size_t totalCapacity = 0;            // Gesamtkapazitaet (falls bekannt)
    float fragmentationRatio = 0.0f;     // Fragmentierung (0.0 - 1.0)
};

// =============================================================================
// ECS-SPEICHER-STATISTIKEN
// =============================================================================
struct EcsMemoryStats {
    size_t totalChunkMemory = 0;         // Gesamter Chunk-Speicher
    size_t usedChunkMemory = 0;          // Belegter Chunk-Speicher
    size_t entityCount = 0;              // Anzahl Entities
    size_t chunkCount = 0;               // Anzahl Chunks
    size_t archetypeCount = 0;           // Anzahl Archetypes
    size_t componentTypeCount = 0;       // Anzahl Komponenten-Typen
    float utilizationRatio = 0.0f;       // Nutzungsgrad (0.0 - 1.0)
};

// =============================================================================
// HISTORISCHER DATENPUNKT — Fuer Zeitverlauf
// =============================================================================
struct MemoryHistoryPoint {
    std::chrono::steady_clock::time_point timestamp;
    size_t totalBytes = 0;
    size_t activeBytes = 0;
    size_t allocationCount = 0;
};

// =============================================================================
// MEMORY PROFILER — Singleton
// =============================================================================
class MemoryProfiler {
public:
    static MemoryProfiler& GetInstance();

    MemoryProfiler(const MemoryProfiler&) = delete;
    MemoryProfiler& operator=(const MemoryProfiler&) = delete;

    // ===================================================================
    // Lifecycle
    // ===================================================================
    void Initialize();
    void Shutdown();
    [[nodiscard]] bool IsInitialized() const { return initialized; }

    // ===================================================================
    // Allokations-Tracking (von Allokatoren aufgerufen)
    // ===================================================================
    void TrackAllocation(void* address, size_t size, size_t alignment,
                         std::string_view allocatorName,
                         std::string_view sourceFile = "", uint32_t sourceLine = 0);
    void TrackFree(void* address, std::string_view allocatorName);

    // ===================================================================
    // ECS-Speicher-Tracking
    // ===================================================================
    void TrackEcsMemory(size_t totalChunkMemory, size_t usedChunkMemory,
                        size_t entityCount, size_t chunkCount,
                        size_t archetypeCount, size_t componentTypeCount);

    // ===================================================================
    // Allokator-Statistiken
    // ===================================================================
    [[nodiscard]] AllocatorStats GetAllocatorStats(std::string_view name) const;
    [[nodiscard]] std::vector<AllocatorStats> GetAllAllocatorStats() const;

    // ===================================================================
    // ECS-Statistiken
    // ===================================================================
    [[nodiscard]] EcsMemoryStats GetEcsStats() const;

    // ===================================================================
    // Globale Statistiken
    // ===================================================================
    [[nodiscard]] size_t GetTotalAllocatedBytes() const;
    [[nodiscard]] size_t GetTotalActiveBytes() const;
    [[nodiscard]] size_t GetPeakAllocatedBytes() const;
    [[nodiscard]] size_t GetTotalAllocationCount() const;
    [[nodiscard]] size_t GetActiveAllocationCount() const;

    // ===================================================================
    // Speicherleck-Erkennung
    // ===================================================================
    [[nodiscard]] std::vector<AllocationRecord> FindLeaks() const;
    [[nodiscard]] std::vector<AllocationRecord> FindLeaksOlderThan(float seconds) const;

    // ===================================================================
    // Historische Daten
    // ===================================================================
    void RecordSnapshot();
    [[nodiscard]] std::vector<MemoryHistoryPoint> GetHistory() const;
    void ClearHistory();

    // ===================================================================
    // Berichtserstellung
    // ===================================================================
    std::string GenerateReport() const;
    void PrintReport() const;

    // ===================================================================
    // Konfiguration
    // ===================================================================
    void SetHistoryLimit(size_t limit) { maxHistoryPoints = limit; }
    void SetLeakThresholdSeconds(float seconds) { leakThresholdSeconds = seconds; }

private:
    MemoryProfiler() = default;
    ~MemoryProfiler() = default;

    std::atomic<bool> initialized{false};

    mutable std::mutex allocationsMutex;
    std::unordered_map<void*, AllocationRecord> allocations;

    mutable std::mutex statsMutex;
    std::unordered_map<std::string, AllocatorStats> allocatorStats;

    mutable std::mutex ecsMutex;
    EcsMemoryStats ecsStats;

    mutable std::mutex historyMutex;
    std::vector<MemoryHistoryPoint> history;
    size_t maxHistoryPoints = 1000;

    float leakThresholdSeconds = 300.0f; // 5 Minuten

    std::atomic<size_t> totalAllocatedBytes{0};
    std::atomic<size_t> totalActiveBytes{0};
    std::atomic<size_t> peakAllocatedBytes{0};
    std::atomic<size_t> totalAllocationCount{0};
    std::atomic<size_t> activeAllocationCount{0};
};

// =============================================================================
// RAII-Allokations-Tracker — Automatisches Tracking im Scope
// =============================================================================
class ScopedAllocationTracker {
    void* address = nullptr;
    std::string allocatorName;
public:
    ScopedAllocationTracker(void* addr, size_t size, size_t alignment,
                            std::string_view name,
                            std::string_view file = "", uint32_t line = 0);
    ~ScopedAllocationTracker();
};

// =============================================================================
// MAKROS FUER KOMFORTABLES TRACKING
// =============================================================================
#define MEMPROFILE_TRACK_ALLOC(addr, size, align, name) \
    memory::MemoryProfiler::GetInstance().TrackAllocation(addr, size, align, name, __FILE__, __LINE__)

#define MEMPROFILE_TRACK_FREE(addr, name) \
    memory::MemoryProfiler::GetInstance().TrackFree(addr, name)

#define MEMPROFILE_RECORD_SNAPSHOT() \
    memory::MemoryProfiler::GetInstance().RecordSnapshot()

} // namespace memory
