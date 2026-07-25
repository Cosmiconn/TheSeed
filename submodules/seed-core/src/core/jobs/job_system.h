#pragma once

// ---------------------------------------------------------------------------
// JobSystem
// ---------------------------------------------------------------------------
// Work-Stealing Job-System: N Worker-Threads, jeder mit eigener
// WorkStealingQueue. Ein Worker ohne eigene Arbeit stiehlt von zufaellig
// anderen Workern (siehe job_system.cpp: Worker::loop()).
//
// Zwei Nutzungsarten:
//   1. Fire-and-forget:  schedule(fn)            - keine Abhaengigkeiten
//   2. Abhaengigkeits-Graph: createTask/addDependency/submit - DAG-faehig
//
// Task-Speicher wird intern ueber PoolAllocator<Task> (P0-M2) verwaltet -
// die JobSystem-API selbst verlangt dafuer KEINEN Allocator-Parameter vom
// Aufrufer (JobSystem ist bewusst self-contained, analog zu einer eigenen
// kleinen "Arena" fuer Tasks). Ein TaskHandle ist nur bis zum Abschluss des
// zugehoerigen Tasks gueltig (siehe task.h).
//
// Thread-Safety: alle public-Methoden sind von jedem Thread aus aufrufbar.
// ---------------------------------------------------------------------------

#include "core/jobs/task.h"
#include <algorithm>
#include <atomic>
#include <condition_variable>
#include <cstdint>
#include <functional>
#include <memory>
#include <mutex>
#include <thread>

namespace seed::jobs {

// Opakes Handle auf einen Task. Nur gueltig zwischen createTask() und dem
// Abschluss des Tasks (siehe task.h - Lebensdauer-Hinweis).
struct TaskHandle {
    Task* ptr = nullptr;

    explicit operator bool() const noexcept { return ptr != nullptr; }
};

// Konfiguration fuer JobSystem. Bewusst NICHT als verschachtelte Klasse
// (JobSystem::Config), da ein Default-Argument im JobSystem-Ctor sonst auf
// Default-Member-Initializer eines noch nicht vollstaendigen verschachtelten
// Typs zugreifen wuerde (Compiler-Fehler bei manchen Toolchains).
struct JobSystemConfig {
    // 0 bedeutet "hardware_concurrency() verwenden"; wird im Ctor
    // aufgeloest und mindestens auf 1 angehoben (Systeme, auf denen
    // hardware_concurrency() 0 liefert, z.B. manche Container-Limits).
    uint32_t numWorkers = 0;

    // Kapazitaet der WorkStealingQueue pro Worker. Muss eine Zweierpotenz
    // sein (wird im Konstruktor automatisch auf die naechste Zweierpotenz
    // aufgerundet, falls noetig). Default 256.
    uint32_t queueCapacity = 256;
};

class JobSystem {
public:
    using Config = JobSystemConfig; // Bequemlichkeits-Alias: JobSystem::Config

    explicit JobSystem(const JobSystemConfig& config = JobSystemConfig{});
    ~JobSystem();

    JobSystem(const JobSystem&) = delete;
    JobSystem& operator=(const JobSystem&) = delete;

    // --- Fire-and-forget ---------------------------------------------------
    void schedule(std::function<void()> work, const char* name = "unnamed");

    // --- Abhaengigkeits-Graph ------------------------------------------------
    TaskHandle createTask(std::function<void()> work, const char* name = "unnamed");
    void addDependency(TaskHandle before, TaskHandle after);
    void submit(TaskHandle task);

    // --- Daten-Parallelitaet -------------------------------------------------
    // Teilt [0, count) in numWorkers-viele Chunks auf und verteilt sie als
    // Tasks. Blockiert, bis ALLE Chunks fertig sind.
    //
    // WICHTIG: DARF NUR vom Haupt-Thread (oder einem sonstigen Nicht-Worker-
    // Thread) aufgerufen werden. Der Aufruf aus einem Worker-Thread heraus
    // fuehrt zu einem Deadlock, weil der Worker blockiert, statt Tasks zu
    // stehlen. Dies wird zur Laufzeit via assert erzwungen.
    template<typename Func>
    void parallelFor(size_t count, Func&& func);

    // Globaler Synchronisationspunkt: wartet auf ALLE aktuell im System
    // befindlichen Tasks (nicht nur die eines bestimmten Aufrufs).
    //
    // P0-Status: Semantisch identisch zu waitForAll(). In spaeteren Phasen
    // wird hier ein echter Graphen-Barrier implementiert, der nur auf einen
    // definierten Sub-Graphen wartet (z.B. "Physics-System fertig").
    //
    // WICHTIG: NICHT von innerhalb eines auf einem Worker laufenden Tasks
    // aufrufen (auch nicht transitiv ueber parallelFor()).
    void barrier();

    // Blockiert den aufrufenden Thread, bis alle aktuell im System
    // befindlichen Tasks abgeschlossen sind. Dieselbe Einschraenkung wie
    // barrier() gilt.
    void waitForAll();

    uint32_t workerCount() const noexcept;

    // --- Diagnose / Benchmarks ---------------------------------------------
    // Gibt die ID des aktuell laufenden Workers zurueck, oder UINT32_MAX
    // wenn der aufrufende Thread kein Worker ist (z.B. Haupt-Thread).
    // Nuetzlich fuer Lastverteilungs-Messungen in Benchmarks.
    uint32_t currentWorkerId() const noexcept;

private:
    struct Impl;
    std::unique_ptr<Impl> pImpl;

    void scheduleRaw(std::function<void()> work, const char* name);
};

template<typename Func>
void JobSystem::parallelFor(size_t count, Func&& func) {
    if (count == 0) return;

    const uint32_t workers = std::max<uint32_t>(1, workerCount());
    const size_t chunkSize = (count + workers - 1) / workers;

    size_t chunkCount = 0;
    for (size_t start = 0; start < count; start += chunkSize) ++chunkCount;

    // Stack-allokierte Synchronisationsprimitiven – sicher, weil parallelFor
    // nur vom Main-Thread aufgerufen werden darf (assert unten). Keine
    // Heap-Allokationen im Hot-Path mehr (Fix fuer P0-M4 Review-Punkt 5).
    std::atomic<size_t> remaining{chunkCount};
    std::mutex mutex;
    std::condition_variable cv;

    for (size_t start = 0; start < count; start += chunkSize) {
        const size_t end = std::min(start + chunkSize, count);
        scheduleRaw(
            [&func, start, end, &remaining, &mutex, &cv]() {
                for (size_t i = start; i < end; ++i) {
                    func(i);
                }
                if (remaining.fetch_sub(1, std::memory_order_acq_rel) == 1) {
                    std::lock_guard<std::mutex> lock(mutex);
                    cv.notify_all();
                }
            },
            "parallelFor_chunk");
    }

    std::unique_lock<std::mutex> lock(mutex);
    cv.wait(lock, [&] { return remaining.load(std::memory_order_acquire) == 0; });
}

} // namespace seed::jobs
