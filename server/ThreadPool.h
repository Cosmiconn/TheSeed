#pragma once
// =============================================================================
// server/ThreadPool.h — Lock-Free Work-Stealing Thread Pool (P4-FIX)
// =============================================================================
// KORREKTUR P4:
// • Lock-Free Work-Stealing Queue (atomare Operationen statt Mutex)
// • SIMD-freundliche Task-Verteilung (64-Byte Alignment)
// • Performance-Monitoring mit atomaren Zählern
// • Parallele ECS-System-Ausführung mit korrekten Read-Write-Locks
// =============================================================================
#include "../core/World.h"
#include "../core/ECS.h"
#include "../core/Log.h"
#include "../ecs/ecs_EcsWorld.h"

#include <thread>
#include <vector>
#include <queue>
#include <mutex>
#include <condition_variable>
#include <atomic>
#include <functional>
#include <future>
#include <memory>
#include <random>
#include <chrono>
#include <cstdint>

// =============================================================================
// LOCK-FREE WORK-STEALING QUEUE (P4-FIX)
// =============================================================================
// Verwendet atomare Operationen statt Mutex für maximale Performance.
// Jeder Worker hat eine lokale Queue. Andere Worker können von hinten stehlen.
// =============================================================================
template<typename T>
class LockFreeWorkStealingQueue {
private:
    static constexpr size_t CAPACITY = 1024;

    struct alignas(64) Node {
        std::atomic<size_t> top{0};
        std::atomic<size_t> bottom{0};
        std::array<T, CAPACITY> buffer;
    };

    std::unique_ptr<Node> node;

public:
    LockFreeWorkStealingQueue() : node(std::make_unique<Node>()) {
        node->top.store(0, std::memory_order_relaxed);
        node->bottom.store(0, std::memory_order_relaxed);
    }

    // Push: Nur der Eigentümer-Thread darf push
    void Push(T item) {
        size_t b = node->bottom.load(std::memory_order_relaxed);
        size_t t = node->top.load(std::memory_order_acquire);

        if (b - t >= CAPACITY) {
            // Queue voll: Verwerfen (sollte bei CAPACITY=1024 selten sein)
            return;
        }

        node->buffer[b % CAPACITY] = std::move(item);
        node->bottom.store(b + 1, std::memory_order_release);
    }

    // Pop: Nur der Eigentümer-Thread darf pop
    [[nodiscard]] bool TryPop(T& item) {
        size_t b = node->bottom.load(std::memory_order_relaxed) - 1;
        node->bottom.store(b, std::memory_order_relaxed);

        std::atomic_thread_fence(std::memory_order_seq_cst);

        size_t t = node->top.load(std::memory_order_relaxed);

        if (t <= b) {
            item = std::move(node->buffer[b % CAPACITY]);
            if (t == b) {
                // Letztes Element: Race mit Steal
                if (!node->top.compare_exchange_strong(t, t + 1,
                        std::memory_order_seq_cst, std::memory_order_relaxed)) {
                    node->bottom.store(b + 1, std::memory_order_relaxed);
                    return false;
                }
                node->bottom.store(b + 1, std::memory_order_relaxed);
            }
            return true;
        } else {
            node->bottom.store(b + 1, std::memory_order_relaxed);
            return false;
        }
    }

    // Steal: Andere Worker-Threads stehlen von hinten
    [[nodiscard]] bool TrySteal(T& item) {
        size_t t = node->top.load(std::memory_order_acquire);
        std::atomic_thread_fence(std::memory_order_seq_cst);
        size_t b = node->bottom.load(std::memory_order_acquire);

        if (t < b) {
            item = std::move(node->buffer[t % CAPACITY]);
            if (node->top.compare_exchange_strong(t, t + 1,
                    std::memory_order_seq_cst, std::memory_order_relaxed)) {
                return true;
            }
        }
        return false;
    }

    [[nodiscard]] bool IsEmpty() const {
        size_t t = node->top.load(std::memory_order_acquire);
        size_t b = node->bottom.load(std::memory_order_acquire);
        return t >= b;
    }

    [[nodiscard]] size_t Size() const {
        size_t t = node->top.load(std::memory_order_acquire);
        size_t b = node->bottom.load(std::memory_order_acquire);
        return (b > t) ? (b - t) : 0;
    }
};

// =============================================================================
// WORK ITEM (64-Byte aligned für Cache-Performance)
// =============================================================================
struct alignas(64) WorkItem {
    std::function<void()> task;
    std::chrono::steady_clock::time_point enqueueTime;
    uint32_t priority = 0; // 0 = höchste Priorität

    WorkItem() = default;
    explicit WorkItem(std::function<void()> t, uint32_t p = 0)
        : task(std::move(t)), enqueueTime(std::chrono::steady_clock::now()), priority(p) {}
};

// =============================================================================
// THREAD POOL
// =============================================================================
class ThreadPool {
private:
    std::vector<std::thread> workers;
    std::vector<std::unique_ptr<LockFreeWorkStealingQueue<WorkItem>>> localQueues;
    std::queue<WorkItem> globalQueue;
    std::mutex globalMutex;
    std::condition_variable condition;
    std::atomic<bool> stopFlag{false};
    std::atomic<uint32_t> pendingTasks{0};
    std::atomic<uint64_t> tasksExecuted{0};
    std::atomic<uint64_t> tasksDropped{0};

    // Performance-Monitoring (atomar, lock-free)
    alignas(64) std::atomic<uint64_t> totalTaskTimeUs{0};
    alignas(64) std::atomic<uint64_t> maxTaskTimeUs{0};

    // P4: ECS-System-Barrier für parallele Ausführung
    std::atomic<uint32_t> ecsSystemsRunning{0};

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
    // ECS-Integration (P4-FIX: Parallele Ausführung mit Barriere)
    // ===================================================================
    void ExecuteEcsSystems(ecs::EcsWorld& world, float deltaTime);

    // ===================================================================
    // Warte auf Leerlauf
    // ===================================================================
    void WaitForAll();

    // ===================================================================
    // Statistiken
    // ===================================================================
    [[nodiscard]] size_t GetPendingCount() const { return pendingTasks.load(std::memory_order_relaxed); }
    [[nodiscard]] uint64_t GetExecutedCount() const { return tasksExecuted.load(std::memory_order_relaxed); }
    [[nodiscard]] uint64_t GetDroppedCount() const { return tasksDropped.load(std::memory_order_relaxed); }
    [[nodiscard]] double GetAverageTaskTimeUs() const;
    [[nodiscard]] uint64_t GetMaxTaskTimeUs() const { return maxTaskTimeUs.load(std::memory_order_relaxed); }

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
