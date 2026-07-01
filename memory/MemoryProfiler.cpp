// =============================================================================
// memory/MemoryProfiler.cpp — Memory Profiler Implementation (AP-80)
// =============================================================================
// Thread-sicheres Speicher-Tracking mit minimaler Overhead.
// Atomare Counter fuer globale Statistiken, Mutexe fuer detaillierte Daten.
// =============================================================================
#include "MemoryProfiler.h"
#include "../core/Log.h"

#include <sstream>
#include <iomanip>
#include <algorithm>

namespace memory {

// =============================================================================
// SINGLETON
// =============================================================================
MemoryProfiler& MemoryProfiler::GetInstance() {
    static MemoryProfiler instance;
    return instance;
}

// =============================================================================
// LIFECYCLE
// =============================================================================
void MemoryProfiler::Initialize() {
    if (initialized.exchange(true)) return;

    allocations.clear();
    allocatorStats.clear();
    history.clear();
    ecsStats = EcsMemoryStats{};

    totalAllocatedBytes.store(0);
    totalActiveBytes.store(0);
    peakAllocatedBytes.store(0);
    totalAllocationCount.store(0);
    activeAllocationCount.store(0);

    AddLog("[MemoryProfiler] Initialisiert — Tracking aktiv");
}

void MemoryProfiler::Shutdown() {
    if (!initialized.exchange(false)) return;

    // Speicherleck-Warnung ausgeben
    auto leaks = FindLeaks();
    if (!leaks.empty()) {
        AddLog("[MemoryProfiler] WARNUNG: {} Speicherlecks bei Shutdown erkannt!", leaks.size());
        for (const auto& leak : leaks) {
            AddLog("  Leak: {} Bytes in '{}' ({}:{}) seit {:.1f}s",
                   leak.size, leak.allocatorName, leak.sourceFile, leak.sourceLine,
                   std::chrono::duration<float>(
                       std::chrono::steady_clock::now() - leak.allocationTime).count());
        }
    }

    AddLog("[MemoryProfiler] Heruntergefahren — Peak: {} Bytes, Total Allokationen: {}",
           peakAllocatedBytes.load(), totalAllocationCount.load());
}

// =============================================================================
// ALLOKATIONS-TRACKING
// =============================================================================
void MemoryProfiler::TrackAllocation(void* address, size_t size, size_t alignment,
                                      std::string_view allocatorName,
                                      std::string_view sourceFile, uint32_t sourceLine) {
    if (!initialized || !address) return;

    AllocationRecord record;
    record.address = address;
    record.size = size;
    record.alignment = alignment;
    record.allocatorName = std::string(allocatorName);
    record.sourceFile = std::string(sourceFile);
    record.sourceLine = sourceLine;
    record.allocationTime = std::chrono::steady_clock::now();
    record.isFreed = false;

    {
        std::lock_guard lock(allocationsMutex);
        allocations[address] = record;
    }

    // Allokator-Statistiken aktualisieren
    {
        std::lock_guard lock(statsMutex);
        auto& stats = allocatorStats[std::string(allocatorName)];
        stats.name = allocatorName;
        stats.totalAllocatedBytes += size;
        stats.currentAllocatedBytes += size;
        stats.totalAllocations++;
        stats.activeAllocations++;
        if (stats.currentAllocatedBytes > stats.peakAllocatedBytes) {
            stats.peakAllocatedBytes = stats.currentAllocatedBytes;
        }
    }

    // Globale Statistiken (atomar)
    totalAllocatedBytes.fetch_add(size);
    totalActiveBytes.fetch_add(size);
    totalAllocationCount.fetch_add(1);
    activeAllocationCount.fetch_add(1);

    // Peak aktualisieren
    size_t current = totalActiveBytes.load();
    size_t peak = peakAllocatedBytes.load();
    while (current > peak && !peakAllocatedBytes.compare_exchange_weak(peak, current)) {
        // Retry
    }
}

void MemoryProfiler::TrackFree(void* address, std::string_view allocatorName) {
    if (!initialized || !address) return;

    AllocationRecord record;
    bool found = false;

    {
        std::lock_guard lock(allocationsMutex);
        auto it = allocations.find(address);
        if (it != allocations.end()) {
            record = it->second;
            record.isFreed = true;
            record.freeTime = std::chrono::steady_clock::now();
            allocations.erase(it);
            found = true;
        }
    }

    if (!found) return;

    // Allokator-Statistiken aktualisieren
    {
        std::lock_guard lock(statsMutex);
        auto& stats = allocatorStats[std::string(allocatorName)];
        stats.currentAllocatedBytes -= record.size;
        stats.totalFrees++;
        stats.activeAllocations--;
    }

    // Globale Statistiken (atomar)
    totalActiveBytes.fetch_sub(record.size);
    activeAllocationCount.fetch_sub(1);
}

// =============================================================================
// ECS-SPEICHER-TRACKING
// =============================================================================
void MemoryProfiler::TrackEcsMemory(size_t totalChunkMemory, size_t usedChunkMemory,
                                     size_t entityCount, size_t chunkCount,
                                     size_t archetypeCount, size_t componentTypeCount) {
    if (!initialized) return;

    std::lock_guard lock(ecsMutex);
    ecsStats.totalChunkMemory = totalChunkMemory;
    ecsStats.usedChunkMemory = usedChunkMemory;
    ecsStats.entityCount = entityCount;
    ecsStats.chunkCount = chunkCount;
    ecsStats.archetypeCount = archetypeCount;
    ecsStats.componentTypeCount = componentTypeCount;

    if (totalChunkMemory > 0) {
        ecsStats.utilizationRatio = static_cast<float>(usedChunkMemory) / static_cast<float>(totalChunkMemory);
    } else {
        ecsStats.utilizationRatio = 0.0f;
    }
}

// =============================================================================
// STATISTIKEN-ABFRAGEN
// =============================================================================
AllocatorStats MemoryProfiler::GetAllocatorStats(std::string_view name) const {
    std::lock_guard lock(statsMutex);
    auto it = allocatorStats.find(std::string(name));
    if (it != allocatorStats.end()) {
        return it->second;
    }
    return AllocatorStats{.name = std::string(name)};
}

std::vector<AllocatorStats> MemoryProfiler::GetAllAllocatorStats() const {
    std::lock_guard lock(statsMutex);
    std::vector<AllocatorStats> result;
    result.reserve(allocatorStats.size());
    for (const auto& [name, stats] : allocatorStats) {
        result.push_back(stats);
    }
    return result;
}

EcsMemoryStats MemoryProfiler::GetEcsStats() const {
    std::lock_guard lock(ecsMutex);
    return ecsStats;
}

size_t MemoryProfiler::GetTotalAllocatedBytes() const {
    return totalAllocatedBytes.load();
}

size_t MemoryProfiler::GetTotalActiveBytes() const {
    return totalActiveBytes.load();
}

size_t MemoryProfiler::GetPeakAllocatedBytes() const {
    return peakAllocatedBytes.load();
}

size_t MemoryProfiler::GetTotalAllocationCount() const {
    return totalAllocationCount.load();
}

size_t MemoryProfiler::GetActiveAllocationCount() const {
    return activeAllocationCount.load();
}

// =============================================================================
// SPEICHERLECK-ERKENNUNG
// =============================================================================
std::vector<AllocationRecord> MemoryProfiler::FindLeaks() const {
    std::lock_guard lock(allocationsMutex);
    std::vector<AllocationRecord> leaks;
    leaks.reserve(allocations.size());

    for (const auto& [addr, record] : allocations) {
        if (!record.isFreed) {
            leaks.push_back(record);
        }
    }

    return leaks;
}

std::vector<AllocationRecord> MemoryProfiler::FindLeaksOlderThan(float seconds) const {
    auto now = std::chrono::steady_clock::now();
    auto threshold = std::chrono::duration<float>(seconds);

    std::lock_guard lock(allocationsMutex);
    std::vector<AllocationRecord> leaks;

    for (const auto& [addr, record] : allocations) {
        if (!record.isFreed) {
            auto age = std::chrono::duration<float>(now - record.allocationTime);
            if (age > threshold) {
                leaks.push_back(record);
            }
        }
    }

    return leaks;
}

// =============================================================================
// HISTORISCHE DATEN
// =============================================================================
void MemoryProfiler::RecordSnapshot() {
    if (!initialized) return;

    MemoryHistoryPoint point;
    point.timestamp = std::chrono::steady_clock::now();
    point.totalBytes = totalAllocatedBytes.load();
    point.activeBytes = totalActiveBytes.load();
    point.allocationCount = activeAllocationCount.load();

    std::lock_guard lock(historyMutex);
    history.push_back(point);

    // Limit einhalten
    if (history.size() > maxHistoryPoints) {
        history.erase(history.begin(), history.begin() + (history.size() - maxHistoryPoints));
    }
}

std::vector<MemoryHistoryPoint> MemoryProfiler::GetHistory() const {
    std::lock_guard lock(historyMutex);
    return history;
}

void MemoryProfiler::ClearHistory() {
    std::lock_guard lock(historyMutex);
    history.clear();
}

// =============================================================================
// BERICHTSERSTELLUNG
// =============================================================================
std::string MemoryProfiler::GenerateReport() const {
    std::ostringstream oss;
    auto now = std::chrono::steady_clock::now();

    oss << "================================================================================\n";
    oss << "  MEMORY PROFILER BERICHT\n";
    oss << "================================================================================\n";
    oss << "Zeitpunkt: " << std::chrono::duration<float>(now.time_since_epoch()).count() << "s\n";
    oss << "--------------------------------------------------------------------------------\n";
    oss << "  GLOBALE STATISTIKEN\n";
    oss << "--------------------------------------------------------------------------------\n";
    oss << "  Total allokiert:     " << std::setw(12) << GetTotalAllocatedBytes() << " Bytes\n";
    oss << "  Aktiv belegt:        " << std::setw(12) << GetTotalActiveBytes() << " Bytes\n";
    oss << "  Peak belegt:         " << std::setw(12) << GetPeakAllocatedBytes() << " Bytes\n";
    oss << "  Total Allokationen:  " << std::setw(12) << GetTotalAllocationCount() << "\n";
    oss << "  Aktive Allokationen: " << std::setw(12) << GetActiveAllocationCount() << "\n";
    oss << "--------------------------------------------------------------------------------\n";

    // ECS-Statistiken
    auto ecs = GetEcsStats();
    oss << "  ECS-SPEICHER\n";
    oss << "--------------------------------------------------------------------------------\n";
    oss << "  Chunks:              " << std::setw(12) << ecs.chunkCount << "\n";
    oss << "  Entities:            " << std::setw(12) << ecs.entityCount << "\n";
    oss << "  Archetypes:          " << std::setw(12) << ecs.archetypeCount << "\n";
    oss << "  Chunk-Speicher:      " << std::setw(12) << ecs.totalChunkMemory << " Bytes\n";
    oss << "  Genutzt:             " << std::setw(12) << ecs.usedChunkMemory << " Bytes\n";
    oss << "  Nutzungsgrad:        " << std::setw(12) << std::fixed << std::setprecision(2)
        << (ecs.utilizationRatio * 100.0f) << "%\n";
    oss << "--------------------------------------------------------------------------------\n";

    // Allokator-Statistiken
    auto allStats = GetAllAllocatorStats();
    if (!allStats.empty()) {
        oss << "  ALLOKATOR-STATISTIKEN\n";
        oss << "--------------------------------------------------------------------------------\n";
        for (const auto& stats : allStats) {
            oss << "  " << stats.name << ":\n";
            oss << "    Aktiv:     " << std::setw(10) << stats.currentAllocatedBytes << " Bytes\n";
            oss << "    Peak:      " << std::setw(10) << stats.peakAllocatedBytes << " Bytes\n";
            oss << "    Allok:     " << std::setw(10) << stats.totalAllocations << "\n";
            oss << "    Frei:      " << std::setw(10) << stats.totalFrees << "\n";
            oss << "    Offen:     " << std::setw(10) << stats.activeAllocations << "\n";
            if (stats.totalCapacity > 0) {
                float usage = static_cast<float>(stats.currentAllocatedBytes) / static_cast<float>(stats.totalCapacity);
                oss << "    Kapazitaet:" << std::setw(10) << stats.totalCapacity << " Bytes ("
                    << std::fixed << std::setprecision(1) << (usage * 100.0f) << "%)\n";
            }
            oss << "\n";
        }
    }

    // Speicherlecks
    auto leaks = FindLeaks();
    oss << "--------------------------------------------------------------------------------\n";
    oss << "  SPEICHERLECKS: " << leaks.size() << "\n";
    oss << "--------------------------------------------------------------------------------\n";
    if (!leaks.empty()) {
        for (const auto& leak : leaks) {
            auto age = std::chrono::duration<float>(now - leak.allocationTime).count();
            oss << "  " << leak.size << " Bytes in '" << leak.allocatorName << "'"
                << " (" << leak.sourceFile << ":" << leak.sourceLine << ")"
                << " seit " << std::fixed << std::setprecision(1) << age << "s\n";
        }
    } else {
        oss << "  Keine Speicherlecks erkannt.\n";
    }

    // Historische Daten
    auto history = GetHistory();
    if (!history.empty()) {
        oss << "--------------------------------------------------------------------------------\n";
        oss << "  HISTORIE (letzte " << history.size() << " Punkte)\n";
        oss << "--------------------------------------------------------------------------------\n";
        size_t step = std::max(size_t(1), history.size() / 10);
        for (size_t i = 0; i < history.size(); i += step) {
            auto elapsed = std::chrono::duration<float>(now - history[i].timestamp).count();
            oss << "  T-" << std::setw(6) << std::fixed << std::setprecision(1) << elapsed << "s: "
                << std::setw(12) << history[i].activeBytes << " Bytes ("
                << history[i].allocationCount << " Allokationen)\n";
        }
    }

    oss << "================================================================================\n";
    return oss.str();
}

void MemoryProfiler::PrintReport() const {
    AddLog("{}", GenerateReport());
}

// =============================================================================
// SCOPED ALLOCATION TRACKER
// =============================================================================
ScopedAllocationTracker::ScopedAllocationTracker(void* addr, size_t size, size_t alignment,
                                                   std::string_view name,
                                                   std::string_view file, uint32_t line)
    : address(addr), allocatorName(name) {
    MemoryProfiler::GetInstance().TrackAllocation(addr, size, alignment, name, file, line);
}

ScopedAllocationTracker::~ScopedAllocationTracker() {
    MemoryProfiler::GetInstance().TrackFree(address, allocatorName);
}

} // namespace memory
