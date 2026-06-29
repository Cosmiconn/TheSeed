#pragma once
// =============================================================================
// server/ThreadPool.h — Lock-Free Work-Stealing Thread Pool (AP-42)
// =============================================================================
#include <cstdint>
#include <thread>
#include <vector>
#include <queue>
#include <mutex>
#include <condition_variable>
#include <atomic>
#include <functional>
#include <future>
#include <memory>

namespace server {

// =============================================================================
// Lock-Free Task Queue (per thread)
// =============================================================================
class TaskQueue {
    struct Node {
        std::function<void()> task;
        std::atomic<Node*> next{nullptr};
    };

    std::atomic<Node*> head{nullptr};
    std::atomic<Node*> tail{nullptr};
    std::atomic<size_t> size{0};

public:
    TaskQueue();
    ~TaskQueue();

    void Push(std::function<void()> task);
    [[nodiscard]] std::function<void()> Pop();
    [[nodiscard]] std::function<void()> Steal();
    [[nodiscard]] bool Empty() const { return size.load(std::memory_order_relaxed) == 0; }
    [[nodiscard]] size_t Size() const { return size.load(std::memory_order_relaxed); }
};

// =============================================================================
// Thread Pool
// =============================================================================
class ThreadPool {
    std::vector<std::unique_ptr<TaskQueue>> localQueues;
    std::vector<std::thread> threads;
    std::atomic<bool> running{false};
    std::atomic<uint32_t> nextQueue{0};

    size_t threadCount = 0;

public:
    explicit ThreadPool(size_t numThreads = std::thread::hardware_concurrency());
    ~ThreadPool() { Shutdown(); }

    ThreadPool(const ThreadPool&) = delete;
    ThreadPool& operator=(const ThreadPool&) = delete;

    void Startup();
    void Shutdown();
    [[nodiscard]] bool IsRunning() const { return running.load(); }

    // Submit work (returns future for result retrieval)
    template<typename F, typename... Args>
    auto Submit(F&& f, Args&&... args) -> std::future<decltype(f(args...))> {
        using ReturnType = decltype(f(args...));

        auto task = std::make_shared<std::packaged_task<ReturnType()>>(
            std::bind(std::forward<F>(f), std::forward<Args>(args)...)
        );

        std::future<ReturnType> result = task->get_future();

        // Round-robin distribution
        uint32_t idx = nextQueue.fetch_add(1, std::memory_order_relaxed) % threadCount;
        localQueues[idx]->Push([task]() { (*task)(); });

        return result;
    }

    // Submit fire-and-forget
    void SubmitVoid(std::function<void()> task);

    [[nodiscard]] size_t GetThreadCount() const { return threadCount; }
    [[nodiscard]] size_t GetPendingTasks() const;

private:
    void WorkerLoop(size_t threadIndex);
};

// =============================================================================
// Multi-Threaded Server (AP-42)
// =============================================================================
class MultiThreadedServer {
    std::unique_ptr<ThreadPool> threadPool;

    // Thread-local simulation state
    struct SimState {
        float accumulator = 0.0f;
        float fixedDelta = 1.0f / 60.0f; // 60Hz physics
        uint64_t tickCount = 0;
    };

    SimState simState;
    std::atomic<bool> running{false};

    // Lock-free message queues between threads
    struct NetworkToSimQueue {
        std::atomic<size_t> writeIdx{0};
        std::atomic<size_t> readIdx{0};
        static constexpr size_t SIZE = 4096;
        std::array<std::vector<uint8_t>, SIZE> buffer;
        std::array<std::atomic<bool>, SIZE> ready;
    };

    std::unique_ptr<NetworkToSimQueue> netToSimQueue;

public:
    MultiThreadedServer();
    ~MultiThreadedServer() { Shutdown(); }

    void Startup(size_t workerThreads = 4);
    void Shutdown();

    // Main loop: Network thread calls this
    void NetworkTick(float deltaTime);

    // Simulation thread calls this
    void SimulationTick(float deltaTime);

    // Queue packet from network thread to sim thread
    void QueuePacketForSimulation(std::vector<uint8_t> packet);

    [[nodiscard]] bool IsRunning() const { return running.load(); }
    [[nodiscard]] uint64_t GetTickCount() const { return simState.tickCount; }
};

} // namespace server
