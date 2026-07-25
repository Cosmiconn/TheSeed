#include "core/jobs/job_system.h"
#include "core/jobs/work_stealing_queue.h"
#include "core/memory/block_allocator.h"
#include "core/memory/pool_allocator.h"
#include "core/profiling/tracy_seed.h"

#include <atomic>
#include <condition_variable>
#include <deque>
#include <mutex>
#include <vector>

namespace seed::jobs {

namespace {
// "Kein Worker" - Sentinel fuer t_currentWorkerId, siehe Impl::submitToQueue().
constexpr uint32_t kNoWorker = UINT32_MAX;

// thread_local: jeder Worker-Thread setzt dies zu Beginn seiner loop() auf
// die eigene Worker-ID. Aufrufe von aussen (z.B. Haupt-Thread ruft
// schedule() auf) sehen den Default kNoWorker.
thread_local uint32_t t_currentWorkerId = kNoWorker;
} // namespace

// ---------------------------------------------------------------------------
// Worker
// ---------------------------------------------------------------------------
struct JobSystem::Impl {
public:
    explicit Impl(const Config& cfg)
        : config(resolveConfig(cfg))
        , taskPool(&blockAllocator) {
        workers.reserve(config.numWorkers);
        for (uint32_t i = 0; i < config.numWorkers; ++i) {
            workers.push_back(std::make_unique<WorkerThread>(*this, i, config.queueCapacity));
        }
        for (auto& w : workers) {
            w->start();
        }
    }

    ~Impl() {
        shutdownFlag.store(true, std::memory_order_release);
        for (auto& w : workers) {
            w->join();
        }
        // Hinweis: Tasks, die zum Zeitpunkt des Shutdowns noch Pending/Ready
        // in einer Queue lagen (nie ausgefuehrt), werden NICHT mehr explizit
        // destruiert - ihr Speicher wird beim Zerfall von taskPool/
        // blockAllocator gleich mit freigegeben (siehe pool_allocator.h:
        // "Pages are owned by BlockAllocator"). Das ist ein bewusst
        // akzeptierter Phase-0-Kompromiss (nie in den getesteten Ablaeufen
        // relevant, da diese immer vorher waitForAll() aufrufen).
    }

    static Config resolveConfig(const Config& cfg) {
        Config resolved = cfg;
        if (resolved.numWorkers == 0) {
            resolved.numWorkers = std::thread::hardware_concurrency();
        }
        if (resolved.numWorkers == 0) {
            resolved.numWorkers = 1; // hardware_concurrency() kann 0 liefern
        }
        if (resolved.queueCapacity == 0) {
            resolved.queueCapacity = 256;
        }
        // Auf naechste Zweierpotenz aufrunden (Chase-Lev Bitmaske erfordert das)
        uint32_t cap = resolved.queueCapacity;
        cap--;
        cap |= cap >> 1;
        cap |= cap >> 2;
        cap |= cap >> 4;
        cap |= cap >> 8;
        cap |= cap >> 16;
        cap++;
        resolved.queueCapacity = cap;
        return resolved;
    }

    // Reicht einen bereits als Ready markierten Task ein. Laeuft dieser
    // Aufruf auf einem Worker-Thread (t_currentWorkerId != kNoWorker), geht
    // der Task bevorzugt in dessen EIGENE lokale Queue (Cache-Lokalitaet).
    // Von aussen (z.B. Haupt-Thread) oder bei voller lokaler Queue geht der
    // Task in die globale, mutex-geschuetzte Injector-Queue.
    void submitToQueue(Task* task) {
        if (t_currentWorkerId != kNoWorker) {
            if (workers[t_currentWorkerId]->queue.push(task)) {
                return;
            }
            // Lokale Queue voll -> Fallback unten.
        }
        std::lock_guard<std::mutex> lock(injectorMutex);
        injectorQueue.push_back(task);
    }

    Task* tryPopInjector() {
        std::lock_guard<std::mutex> lock(injectorMutex);
        if (injectorQueue.empty()) return nullptr;
        Task* t = injectorQueue.front();
        injectorQueue.pop_front();
        return t;
    }

    // Fuehrt einen Task aus und loest anschliessend seine Abhaengigkeits-
    // Buchhaltung auf: Dependents, deren letzte offene Abhaengigkeit dieser
    // Task war, werden automatisch eingereiht.
    void execute(Task* task) {
        task->state.store(TaskState::Running, std::memory_order_relaxed);
        {
            SEED_ZONE(task->name);
            task->work();
        }
        task->state.store(TaskState::Completed, std::memory_order_release);

        // Dependents AUSSERHALB des Locks einreihen (submitToQueue kann
        // seinerseits einen Mutex nehmen - Locks nicht ineinander verschachteln).
        std::vector<Task*> readyDeps;
        {
            std::lock_guard<std::mutex> lock(task->dependentsMutex);
            for (Task* dep : task->dependents) {
                if (dep->unfinishedDependencies.fetch_sub(1, std::memory_order_acq_rel) == 1) {
                    // Letzte offene Abhaengigkeit dieses Dependenten wurde
                    // soeben aufgeloest. CAS-Guard gegen einen gleichzeitigen
                    // manuellen submit()-Aufruf (siehe task.h: alreadySubmitted).
                    bool expected = false;
                    if (dep->alreadySubmitted.compare_exchange_strong(
                            expected, true, std::memory_order_acq_rel)) {
                        dep->state.store(TaskState::Ready, std::memory_order_relaxed);
                        readyDeps.push_back(dep);
                    }
                }
            }
        }
        for (Task* dep : readyDeps) {
            submitToQueue(dep);
        }

        taskPool.destroy(task);

        if (pendingTasks.fetch_sub(1, std::memory_order_acq_rel) == 1) {
            std::lock_guard<std::mutex> lock(waitMutex);
            waitCv.notify_all();
        }
    }

    Config config;
    seed::memory::BlockAllocator blockAllocator;
    seed::memory::PoolAllocator<Task> taskPool;

    std::atomic<bool> shutdownFlag{false};
    std::atomic<uint32_t> pendingTasks{0};

    std::mutex waitMutex;
    std::condition_variable waitCv;

    std::mutex injectorMutex;
    std::deque<Task*> injectorQueue;

    struct WorkerThread {
        WorkerThread(Impl& sys, uint32_t idx, size_t queueCap)
            : system(sys), id(idx), queue(queueCap) {}

        void start() {
            thread = std::thread([this] { loop(); });
        }

        void join() {
            if (thread.joinable()) thread.join();
        }

        void loop() {
            t_currentWorkerId = id;
            SEED_ZONE("JobSystem::Worker");
            const uint32_t numWorkers = static_cast<uint32_t>(system.workers.size());

            while (!system.shutdownFlag.load(std::memory_order_acquire)) {
                Task* task = queue.pop();

                if (!task) {
                    task = system.tryPopInjector();
                }

                if (!task) {
                    // Stehlen ab einem zufaelligen Startpunkt (hier: der
                    // naechste Worker in Ringreihenfolge ab der eigenen ID) -
                    // vermeidet, dass alle Worker synchron denselben ersten
                    // Nachbarn ueberlasten.
                    for (uint32_t i = 1; i < numWorkers && !task; ++i) {
                        const uint32_t victim = (id + i) % numWorkers;
                        task = system.workers[victim]->queue.steal();
                    }
                }

                if (task) {
                    system.execute(task);
                } else {
                    std::this_thread::yield();
                }
            }
        }

        Impl& system;
        uint32_t id;
        WorkStealingQueue queue;
        std::thread thread;
    };

    std::vector<std::unique_ptr<WorkerThread>> workers;
};

JobSystem::JobSystem(const Config& config) : pImpl(std::make_unique<Impl>(config)) {}
JobSystem::~JobSystem() = default;

uint32_t JobSystem::workerCount() const noexcept {
    return pImpl->config.numWorkers;
}

uint32_t JobSystem::currentWorkerId() const noexcept {
    return t_currentWorkerId;
}

void JobSystem::scheduleRaw(std::function<void()> work, const char* name) {
    schedule(std::move(work), name);
}

void JobSystem::schedule(std::function<void()> work, const char* name) {
    Task* task = pImpl->taskPool.construct(std::move(work), name);
    task->alreadySubmitted.store(true, std::memory_order_relaxed);
    task->state.store(TaskState::Ready, std::memory_order_relaxed);
    pImpl->pendingTasks.fetch_add(1, std::memory_order_relaxed);
    pImpl->submitToQueue(task);
}

TaskHandle JobSystem::createTask(std::function<void()> work, const char* name) {
    Task* task = pImpl->taskPool.construct(std::move(work), name);
    // Zaehlt schon ab Erzeugung als "pending", nicht erst ab dem Einreihen in
    // eine Queue - sonst koennte waitForAll() zurueckkehren, waehrend ein
    // erzeugter, aber wegen offener Abhaengigkeiten noch nicht eingereihter
    // Task auf seine Ausfuehrung wartet (klassisches TOCTOU-Race bei
    // Zaehler-basierten Wait-Konstrukten).
    pImpl->pendingTasks.fetch_add(1, std::memory_order_relaxed);
    return TaskHandle{task};
}

void JobSystem::addDependency(TaskHandle before, TaskHandle after) {
    std::lock_guard<std::mutex> lock(before.ptr->dependentsMutex);
    before.ptr->dependents.push_back(after.ptr);
    after.ptr->unfinishedDependencies.fetch_add(1, std::memory_order_relaxed);
}

void JobSystem::submit(TaskHandle task) {
    // Nur einreihen, wenn aktuell keine offenen Abhaengigkeiten bestehen -
    // sonst geschieht das automatisch in Impl::execute(), sobald die letzte
    // Abhaengigkeit abgeschlossen ist. submit() darf daher fuer JEDEN Task
    // im Graphen aufgerufen werden, unabhaengig von der Aufrufreihenfolge
    // relativ zu addDependency().
    if (task.ptr->unfinishedDependencies.load(std::memory_order_acquire) == 0) {
        bool expected = false;
        if (task.ptr->alreadySubmitted.compare_exchange_strong(
                expected, true, std::memory_order_acq_rel)) {
            task.ptr->state.store(TaskState::Ready, std::memory_order_relaxed);
            pImpl->submitToQueue(task.ptr);
        }
    }
}

void JobSystem::barrier() {
    // P0: Semantisch identisch zu waitForAll(). In spaeteren Phasen wird hier
    // ein echter Graphen-Barrier eingefuehrt, der nur auf einen definierten
    // Sub-Graphen wartet (z.B. "Physics-System + Collision-System fertig").
    waitForAll();
}

void JobSystem::waitForAll() {
    std::unique_lock<std::mutex> lock(pImpl->waitMutex);
    pImpl->waitCv.wait(lock, [this] {
        return pImpl->pendingTasks.load(std::memory_order_acquire) == 0;
    });
}

} // namespace seed::jobs
