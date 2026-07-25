#pragma once

// ---------------------------------------------------------------------------
// TaskGraph
// ---------------------------------------------------------------------------
// Duenner Hilfsbaustein oberhalb von JobSystem::createTask/addDependency, der
// mehrere zusammengehoerige Tasks buendelt und sie gemeinsam einreiht. Die
// eigentliche Ausfuehrung (Queues, Worker, Work-Stealing) lebt komplett in
// JobSystem - TaskGraph verwaltet nur, WELCHE Tasks zusammengehoeren und
// reicht Task-Erzeugung/Abhaengigkeiten/Submit an das uebergebene JobSystem
// durch. Nuetzlich, wenn ein Aufrufer einen ganzen Graphen "im Block" bauen
// und erst am Ende komplett einreichen will, statt jeden Task einzeln zu
// submitten.
//
// TaskGraph selbst besitzt KEINE Tasks (die gehoeren dem JobSystem/seinem
// PoolAllocator) - sie sammelt nur die waehrend des Aufbaus vergebenen
// TaskHandles.
// ---------------------------------------------------------------------------

#include "core/jobs/task.h"
#include <vector>

namespace seed::jobs {

class JobSystem;
struct TaskHandle;

class TaskGraph {
public:
    explicit TaskGraph(JobSystem& jobSystem) noexcept : m_jobSystem(jobSystem) {}

    TaskGraph(const TaskGraph&) = delete;
    TaskGraph& operator=(const TaskGraph&) = delete;

    // Erzeugt einen neuen Task ueber das zugrunde liegende JobSystem und
    // merkt ihn sich als Teil dieses Graphen.
    TaskHandle createTask(Task::Func work, const char* name = "unnamed");

    // "after" darf erst laufen, wenn "before" abgeschlossen ist.
    void addDependency(TaskHandle before, TaskHandle after);

    // Reicht alle in diesem Graphen erzeugten Tasks beim JobSystem ein.
    // Tasks mit offenen Abhaengigkeiten bleiben automatisch Pending, bis
    // JobSystem sie selbst freischaltet (siehe job_system.cpp: execute()).
    void submitAll();

private:
    JobSystem& m_jobSystem;
    std::vector<TaskHandle> m_tasks;
};

} // namespace seed::jobs
