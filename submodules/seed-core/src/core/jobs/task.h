#pragma once

// ---------------------------------------------------------------------------
// Task
// ---------------------------------------------------------------------------
// Ein einzelnes Arbeitspaket im Job-System. Traegt seine eigene Abhaengigkeits-
// Buchhaltung: 'unfinishedDependencies' zaehlt, wie viele Vorgaenger-Tasks noch
// nicht fertig sind. Erreicht der Zaehler 0, wird der Task von JobSystem
// automatisch eingereiht (siehe job_system.cpp: execute()).
//
// Lebensdauer: Tasks werden vom JobSystem ueber einen PoolAllocator<Task>
// alloziert (P0-M2) und nach vollstaendiger Ausfuehrung automatisch wieder
// freigegeben. Ein TaskHandle darf daher NICHT mehr verwendet werden, nachdem
// JobSystem::waitForAll() zurueckgekehrt ist und der Task bereits abgeschlossen
// wurde - das Handle ist danach ein "toter" Zeiger (analog zu Entity-Handles
// im ECS nach destroyEntity()).
// ---------------------------------------------------------------------------

#include <atomic>
#include <cstdint>
#include <functional>
#include <mutex>
#include <vector>

namespace seed::jobs {

enum class TaskState : uint8_t {
    Pending,    // Erzeugt, wartet auf offene Abhaengigkeiten
    Ready,      // Keine offenen Abhaengigkeiten mehr, in einer Queue eingereiht
    Running,    // Wird gerade von einem Worker ausgefuehrt
    Completed   // work() ist durchgelaufen
};

struct Task {
    using Func = std::function<void()>;

    Func work;
    const char* name;

    std::atomic<TaskState> state{TaskState::Pending};
    std::atomic<uint32_t> unfinishedDependencies{0};

    // BUGFIX (gefunden via ASan-Repro): Sowohl JobSystem::submit() (manueller
    // Aufruf) als auch Impl::execute() (automatische Freischaltung, sobald
    // die letzte Abhaengigkeit fertig ist) koennen unabhaengig voneinander
    // beobachten, dass unfinishedDependencies == 0 ist - z.B. wenn ein
    // Vorgaenger-Task so schnell durchlaeuft, dass er einen Dependenten schon
    // automatisch einreiht, WAEHREND der Aufrufer gerade noch dabei ist,
    // submit() fuer denselben Task aufzurufen (siehe Spec-Testmuster: submit()
    // wird fuer JEDEN Task im Graphen aufgerufen, unabhaengig von offenen
    // Abhaengigkeiten). Ohne Schutz fuehrt das zu einer doppelten Einreihung
    // desselben Task* in zwei Queues -> doppelte Ausfuehrung -> doppeltes
    // taskPool.destroy() -> korrumpierte PoolAllocator-Freelist (SIGSEGV).
    // Der CAS auf alreadySubmitted stellt sicher, dass GENAU einer der beiden
    // Pfade den Task tatsaechlich einreiht.
    std::atomic<bool> alreadySubmitted{false};

    // Tasks, die auf DIESEN Task warten. Wird nur waehrend addDependency()
    // (Graph-Aufbau, i.d.R. single-threaded) und einmalig in execute() beim
    // Abschluss gelesen - der Mutex schuetzt trotzdem gegen den seltenen Fall,
    // dass addDependency() aus mehreren Threads gleichzeitig aufgerufen wird.
    std::mutex dependentsMutex;
    std::vector<Task*> dependents;

    explicit Task(Func f, const char* n = "unnamed")
        : work(std::move(f)), name(n) {}

    // Tasks werden ausschliesslich ueber Task* referenziert (Pool-Allokation) -
    // Kopieren/Verschieben ist nicht vorgesehen und durch std::mutex ohnehin
    // nicht moeglich.
    Task(const Task&) = delete;
    Task& operator=(const Task&) = delete;
};

} // namespace seed::jobs
