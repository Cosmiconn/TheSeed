// =============================================================================
// server/ThreadPool.cpp — Lock-Free Work-Stealing Implementation (AP-42)
// =============================================================================
#include "ThreadPool.h"
#include "../core/Log.h"
#include <random>

namespace server {

// =============================================================================
// TaskQueue
// =============================================================================
TaskQueue::TaskQueue() {
    Node* dummy = new Node();
    head.store(dummy, std::memory_order_relaxed);
    tail.store(dummy, std::memory_order_relaxed);
}

TaskQueue::~TaskQueue() {
    while (Node* node = head.load(std::memory_order_relaxed)) {
        head.store(node->next.load(std::memory_order_relaxed), std::memory_order_relaxed);
        delete node;
    }
}

void TaskQueue::Push(std::function<void()> task) {
    Node* newNode = new Node();
    newNode->task = std::move(task);

    Node* oldTail = tail.exchange(newNode, std::memory_order_acq_rel);
    oldTail->next.store(newNode, std::memory_order_release);
    size.fetch_add(1, std::memory_order_relaxed);
}

std::function<void()> TaskQueue::Pop() {
    Node* oldHead = head.load(std::memory_order_relaxed);
    Node* next = oldHead->next.load(std::memory_order_acquire);

    if (next == nullptr) {
        return nullptr; // Empty
    }

    std::function<void()> task = std::move(next->task);
    head.store(next, std::memory_order_release);
    delete oldHead;
    size.fetch_sub(1, std::memory_order_relaxed);

    return task;
}

std::function<void()> TaskQueue::Steal() {
    Node* oldHead = head.load(std::memory_order_acquire);
    Node* next = oldHead->next.load(std::memory_order_acquire);

    if (next == nullptr) {
        return nullptr; // Empty
    }

    std::function<void()> task = std::move(next->task);

    // Try to CAS head
    if (head.compare_exchange_strong(oldHead, next, std::memory_order_release)) {
        delete oldHead;
        size.fetch_sub(1, std::memory_order_relaxed);
        return task;
    }

    return nullptr; // Failed to steal
}

// =============================================================================
// ThreadPool
// =============================================================================
ThreadPool::ThreadPool(size_t numThreads) : threadCount(numThreads) {
    if (threadCount == 0) {
        threadCount = std::thread::hardware_concurrency();
    }
    localQueues.reserve(threadCount);
    for (size_t i = 0; i < threadCount; ++i) {
        localQueues.push_back(std::make_unique<TaskQueue>());
    }
}

void ThreadPool::Startup() {
    if (running.exchange(true)) return; // Already running

    threads.reserve(threadCount);
    for (size_t i = 0; i < threadCount; ++i) {
        threads.emplace_back(&ThreadPool::WorkerLoop, this, i);
    }

    AddLog("[ThreadPool] Started with {} threads", threadCount);
}

void ThreadPool::Shutdown() {
    if (!running.exchange(false)) return; // Already stopped

    // Wake all threads (push dummy tasks)
    for (auto& queue : localQueues) {
        queue->Push(nullptr); // nullptr = shutdown signal
    }

    for (auto& t : threads) {
        if (t.joinable()) t.join();
    }
    threads.clear();

    AddLog("[ThreadPool] Shutdown complete");
}

void ThreadPool::SubmitVoid(std::function<void()> task) {
    uint32_t idx = nextQueue.fetch_add(1, std::memory_order_relaxed) % threadCount;
    localQueues[idx]->Push(std::move(task));
}

size_t ThreadPool::GetPendingTasks() const {
    size_t total = 0;
    for (const auto& queue : localQueues) {
        total += queue->Size();
    }
    return total;
}

void ThreadPool::WorkerLoop(size_t threadIndex) {
    // Pin thread to specific queue
    TaskQueue* myQueue = localQueues[threadIndex].get();

    std::random_device rd;
    std::mt19937 rng(rd());
    std::uniform_int_distribution<size_t> dist(0, threadCount - 1);

    while (running.load(std::memory_order_relaxed)) {
        std::function<void()> task = myQueue->Pop();

        // If empty, try stealing from other queues
        if (!task) {
            for (size_t attempt = 0; attempt < threadCount * 2; ++attempt) {
                size_t victim = dist(rng);
                if (victim == threadIndex) continue;

                task = localQueues[victim]->Steal();
                if (task) break;
            }
        }

        if (task) {
            if (!task) { // nullptr = shutdown signal
                return;
            }
            task();
        } else {
            // No work available, yield
            std::this_thread::yield();
        }
    }
}

// =============================================================================
// MultiThreadedServer
// =============================================================================
MultiThreadedServer::MultiThreadedServer() 
    : threadPool(std::make_unique<ThreadPool>()),
      netToSimQueue(std::make_unique<NetworkToSimQueue>()) {

    for (auto& ready : netToSimQueue->ready) {
        ready.store(false, std::memory_order_relaxed);
    }
}

void MultiThreadedServer::Startup(size_t workerThreads) {
    threadPool = std::make_unique<ThreadPool>(workerThreads);
    threadPool->Startup();
    running.store(true);

    AddLog("[MultiThreadedServer] Started with {} worker threads", workerThreads);
}

void MultiThreadedServer::Shutdown() {
    running.store(false);
    if (threadPool) {
        threadPool->Shutdown();
    }
    AddLog("[MultiThreadedServer] Shutdown complete");
}

void MultiThreadedServer::NetworkTick(float deltaTime) {
    // Network thread: receive packets, parse, queue for sim
    // This runs on the main thread or a dedicated network thread

    // Process any outgoing snapshots from sim thread
    // (would need a SimToNetwork queue, similar to netToSimQueue)
}

void MultiThreadedServer::SimulationTick(float deltaTime) {
    if (!running.load()) return;

    simState.accumulator += deltaTime;

    while (simState.accumulator >= simState.fixedDelta) {
        // Process queued network packets
        ProcessQueuedPackets();

        // Run ECS systems
        // TODO: Call ECS system update here

        simState.accumulator -= simState.fixedDelta;
        simState.tickCount++;
    }
}

void MultiThreadedServer::QueuePacketForSimulation(std::vector<uint8_t> packet) {
    auto& queue = *netToSimQueue;
    size_t idx = queue.writeIdx.fetch_add(1, std::memory_order_relaxed) % NetworkToSimQueue::SIZE;

    // Spin-wait if buffer full (should be rare with 4096 slots)
    while (queue.ready[idx].load(std::memory_order_acquire)) {
        std::this_thread::yield();
    }

    queue.buffer[idx] = std::move(packet);
    queue.ready[idx].store(true, std::memory_order_release);
}

void MultiThreadedServer::ProcessQueuedPackets() {
    auto& queue = *netToSimQueue;
    size_t readIdx = queue.readIdx.load(std::memory_order_relaxed);
    size_t writeIdx = queue.writeIdx.load(std::memory_order_relaxed);

    while (readIdx != writeIdx) {
        size_t idx = readIdx % NetworkToSimQueue::SIZE;

        if (!queue.ready[idx].load(std::memory_order_acquire)) {
            break; // Not ready yet
        }

        // Process packet
        auto& packet = queue.buffer[idx];
        // TODO: Route to packet handler
        (void)packet;

        queue.ready[idx].store(false, std::memory_order_release);
        readIdx++;
    }

    queue.readIdx.store(readIdx, std::memory_order_relaxed);
}

} // namespace server
