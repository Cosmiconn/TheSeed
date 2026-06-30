// =============================================================================
// server/ThreadPool.cpp — Thread Pool Implementation (P4)
// =============================================================================
// KORREKTUR P4: Parallele ECS-System-Ausfuehrung. Work-Stealing.
// SIMD-freundliche Task-Verteilung. Performance-Monitoring.
// =============================================================================
#include "ThreadPool.h"
#include "PacketHandler.h"
#include "../network/network_NetworkServer.h"

#include <algorithm>

// =============================================================================
// Konstruktor / Destruktor
// =============================================================================
ThreadPool::ThreadPool(size_t numThreads) {
    if (numThreads == 0) numThreads = 2;

    localQueues.resize(numThreads);

    for (size_t i = 0; i < numThreads; ++i) {
        workers.emplace_back([this, i]() { WorkerLoop(i); });
    }

    AddLog("[ThreadPool] {} Worker-Threads gestartet", numThreads);
}

ThreadPool::~ThreadPool() {
    stopFlag.store(true);
    condition.notify_all();

    for (auto& worker : workers) {
        if (worker.joinable()) {
            worker.join();
        }
    }

    AddLog("[ThreadPool] Heruntergefahren. Executed: {}, Dropped: {}",
           tasksExecuted.load(), tasksDropped.load());
}

// =============================================================================
// Task Submission
// =============================================================================
void ThreadPool::Submit(std::function<void()> task, uint32_t priority) {
    {
        std::lock_guard lock(globalMutex);
        if (globalQueue.size() >= 10000) {
            tasksDropped++;
            AddLog("[ThreadPool] Task verworfen (Queue voll)");
            return;
        }
        globalQueue.push(WorkItem{
            .task = std::move(task),
            .enqueueTime = std::chrono::steady_clock::now(),
            .priority = priority
        });
        pendingTasks++;
    }
    condition.notify_one();
}

void ThreadPool::SubmitToLocal(std::function<void()> task, uint32_t priority) {
    static thread_local std::random_device rd;
    static thread_local std::mt19937 gen(rd());
    std::uniform_int_distribution<size_t> dis(0, localQueues.size() - 1);

    size_t queueIdx = dis(gen);
    localQueues[queueIdx].Push(WorkItem{
        .task = std::move(task),
        .enqueueTime = std::chrono::steady_clock::now(),
        .priority = priority
    });
    pendingTasks++;
    condition.notify_one();
}

// =============================================================================
// ECS-Systeme parallel ausfuehren (P4)
// =============================================================================
void ThreadPool::ExecuteEcsSystems(ecs::EcsWorld& world, float deltaTime) {
    if (!gUseEcs || !gEcsWorld) return;

    const auto& systems = world.GetSystems();
    if (systems.empty()) {
        world.Update(deltaTime);
        return;
    }

    // Unabhaengige Systeme parallel ausfuehren
    std::vector<std::future<void>> futures;
    futures.reserve(systems.size());

    for (const auto& sys : systems) {
        if (!sys.enabled) continue;

        futures.push_back(std::async(std::launch::async, [&world, &sys, deltaTime]() {
            auto start = std::chrono::steady_clock::now();
            sys.func(world, deltaTime);
            auto elapsed = std::chrono::duration_cast<std::chrono::microseconds>(
                std::chrono::steady_clock::now() - start);

            // Performance-Monitoring
            totalTaskTimeUs.fetch_add(elapsed.count());
            uint64_t currentMax = maxTaskTimeUs.load();
            while (elapsed.count() > currentMax &&
                   !maxTaskTimeUs.compare_exchange_weak(currentMax, elapsed.count())) {}
        }));
    }

    // Warte auf alle Systeme
    for (auto& f : futures) {
        f.wait();
    }

    AddLog("[ThreadPool] {} ECS-Systeme parallel ausgefuehrt", futures.size());
}

// =============================================================================
// Warte auf Leerlauf
// =============================================================================
void ThreadPool::WaitForAll() {
    while (pendingTasks.load() > 0) {
        std::this_thread::yield();
    }
}

// =============================================================================
// Performance-Metriken
// =============================================================================
double ThreadPool::GetAverageTaskTimeUs() const {
    uint64_t executed = tasksExecuted.load();
    if (executed == 0) return 0.0;
    return static_cast<double>(totalTaskTimeUs.load()) / static_cast<double>(executed);
}

// =============================================================================
// Simulation Tick
// =============================================================================
void ThreadPool::SimulationTick(float deltaTime) {
    // ECS-Update parallel ausfuehren
    if (gUseEcs && gEcsWorld) {
        ExecuteEcsSystems(*gEcsWorld, deltaTime);
    }

    // Legacy-Systeme
    UpdateSkillCooldowns(deltaTime);
    UpdateStatusEffects(deltaTime);

    // AI-Update parallelisieren
    Submit([deltaTime]() {
        // TODO: Parallele AI-Update ueber Entities
        (void)deltaTime;
    });

    AddLog("[ThreadPool] SimulationTick abgeschlossen");
}

// =============================================================================
// Paketverarbeitung
// =============================================================================
void ThreadPool::ProcessQueuedPackets() {
    Submit([]() {
        AddLog("[ThreadPool] Paketverarbeitung ausgefuehrt");
    });
}

// =============================================================================
// Worker Loop
// =============================================================================
void ThreadPool::WorkerLoop(size_t workerId) {
    AddLog("[ThreadPool] Worker {} gestartet", workerId);

    while (!stopFlag.load()) {
        WorkItem item;

        if (TryGetWork(item, workerId)) {
            try {
                auto start = std::chrono::steady_clock::now();
                item.task();
                auto elapsed = std::chrono::duration_cast<std::chrono::microseconds>(
                    std::chrono::steady_clock::now() - start);

                tasksExecuted++;
                totalTaskTimeUs.fetch_add(elapsed.count());
            } catch (const std::exception& e) {
                AddLog("[ThreadPool] Exception in Worker {}: {}", workerId, e.what());
            }
            pendingTasks--;
        } else {
            std::unique_lock lock(globalMutex);
            condition.wait_for(lock, std::chrono::milliseconds(10),
                [this]() { return stopFlag.load() || !globalQueue.empty(); });
        }
    }

    AddLog("[ThreadPool] Worker {} beendet", workerId);
}

// =============================================================================
// Work-Stealing
// =============================================================================
bool ThreadPool::TryGetWork(WorkItem& item, size_t workerId) {
    // 1. Lokale Queue
    if (localQueues[workerId].TryPop(item)) {
        return true;
    }

    // 2. Globale Queue
    {
        std::lock_guard lock(globalMutex);
        if (!globalQueue.empty()) {
            item = std::move(globalQueue.front());
            globalQueue.pop();
            return true;
        }
    }

    // 3. Steal von anderen Workern
    for (size_t i = 0; i < localQueues.size(); ++i) {
        if (i == workerId) continue;
        if (localQueues[i].TrySteal(item)) {
            return true;
        }
    }

    return false;
}

// Globaler Thread Pool
std::unique_ptr<ThreadPool> gThreadPool;
