#pragma once
// =============================================================================
// server/ThreadPool.h — Lock-Free Work-Stealing Thread Pool (P4)
// =============================================================================
// KORREKTUR P4: Parallele ECS-System-Ausfuehrung. Work-Stealing-Queue.
// SIMD-freundliche Task-Verteilung. Performance-Monitoring.
// =============================================================================
#include "../core/World.h"
#include "../core/ECS.h"
#include "../core/Log.h"
#include "../ecs/ecs_EcsWorld.h"

#include <vector>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <queue>
#include <functional>
#include <atomic>
#include <chrono>
#include <deque>
#include <random>

// =============================================================================
// WORK-STEALING QUEUE
// =============================================================================
template<typename T>
class WorkStealingQueue {
private:
    std::deque<T> queue;
    mutable std::mutex mutex;

public:
    void Push(T item) {
        std::lock_guard lock(mutex);
        queue.push_back(std::move(item));
    }

    [[nodiscard]] bool TryPop(T& item) {
        std::lock_guard lock(mutex);
        if (queue.empty()) return false;
        item = std::move(queue.front());
        queue.pop_front();
        return true;
    }

    [[nodiscard]] bool TrySteal(T& item) {
        std::lock_guard lock(mutex);
        if (queue.empty()) return false;
        item = std::move(queue.back());
        queue.pop_back();
        return true;
    }

    [[nodiscard]] bool IsEmpty() const {
        std::lock_guard lock(mutex);
        return queue.empty();
    }

    [[nodiscard]] size_t Size() const {
        std::lock_guard lock(mutex);
        return queue.size();
    }
};

// =============================================================================
// WORK ITEM
// =============================================================================
struct WorkItem {
    std::function<void()> task;
    std::chrono::steady_clock::time_point enqueueTime;
    uint32_t priority = 0; // 0 = hoechste Prioritaet
};

// =============================================================================
// THREAD POOL
// =============================================================================
class ThreadPool {
private:
    std::vector<std::thread> workers;
    std::vector<WorkStealingQueue<WorkItem>> localQueues;
    std::queue<WorkItem> globalQueue;
    std::mutex globalMutex;
    std::condition_variable condition;
    std::atomic<bool> stopFlag{false};
    std::atomic<size_t> pendingTasks{0};
    std::atomic<uint64_t> tasksExecuted{0};
    std::atomic<uint64_t> tasksDropped{0};

    // Performance-Monitoring
    std::atomic<uint64_t> totalTaskTimeUs{0};
    std::atomic<uint64_t> maxTaskTimeUs{0};

public:
    explicit ThreadPool(size_t numThreads = std::thread::hardware_concurrency());
    ~ThreadPool();

    ThreadPool(const ThreadPool&) = delete;
    ThreadPool& operator=(const ThreadPool&) = delete;

    // ===================================================================
    // Task Submission
    // ===================================================================
    void Submit(std::function<void()> task, uint32_t priority = 0);
    void SubmitToLocal(std::function<void()> task, uint32_t priority = 0);

    // ===================================================================
    // ECS-Integration (P4)
    // ===================================================================
    void ExecuteEcsSystems(ecs::EcsWorld& world, float deltaTime);

    // ===================================================================
    // Warte auf Leerlauf
    // ===================================================================
    void WaitForAll();

    // ===================================================================
    // Statistiken
    // ===================================================================
    [[nodiscard]] size_t GetPendingCount() const { return pendingTasks.load(); }
    [[nodiscard]] uint64_t GetExecutedCount() const { return tasksExecuted.load(); }
    [[nodiscard]] uint64_t GetDroppedCount() const { return tasksDropped.load(); }
    [[nodiscard]] double GetAverageTaskTimeUs() const;
    [[nodiscard]] uint64_t GetMaxTaskTimeUs() const { return maxTaskTimeUs.load(); }

    // ===================================================================
    // Server Tick
    // ===================================================================
    void SimulationTick(float deltaTime);
    void ProcessQueuedPackets();

private:
    void WorkerLoop(size_t workerId);
    [[nodiscard]] bool TryGetWork(WorkItem& item, size_t workerId);
};

// Globaler Thread Pool
extern std::unique_ptr<ThreadPool> gThreadPool;
