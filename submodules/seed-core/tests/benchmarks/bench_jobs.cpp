#include "core/jobs/job_system.h"
#include <atomic>
#include <chrono>
#include <iostream>
#include <thread>
#include <vector>

using namespace seed::jobs;

namespace {

template<typename Func>
long long timeMs(Func&& func) {
    auto start = std::chrono::high_resolution_clock::now();
    func();
    auto elapsed = std::chrono::high_resolution_clock::now() - start;
    return std::chrono::duration_cast<std::chrono::milliseconds>(elapsed).count();
}

} // namespace

int main() {
    const uint32_t hwThreads = std::thread::hardware_concurrency();
    std::cout << "hardware_concurrency(): " << hwThreads << "\n\n";

    // --- 1M fire-and-forget Tasks: Spec-Ziel <1s ---------------------------
    {
        JobSystem js({.numWorkers = hwThreads});
        constexpr int N = 1'000'000;
        std::atomic<long long> counter{0};

        long long ms = timeMs([&] {
            for (int i = 0; i < N; ++i) {
                js.schedule([&] { counter.fetch_add(1, std::memory_order_relaxed); });
            }
            js.waitForAll();
        });

        const double perSec = ms > 0
            ? (static_cast<double>(N) / (static_cast<double>(ms) / 1000.0))
            : 0.0;
        std::cout << N << " Tasks (schedule + waitForAll): " << ms << " ms"
                   << " (" << static_cast<long long>(perSec) << " Tasks/s)"
                   << " counter=" << counter.load() << "\n";
    }

    // --- parallelFor ueber 10M Elemente --------------------------------------
    {
        JobSystem js({.numWorkers = hwThreads});
        constexpr size_t N = 10'000'000;
        std::vector<float> data(N, 1.0f);

        long long ms = timeMs([&] {
            js.parallelFor(N, [&](size_t i) { data[i] *= 2.0f; });
        });
        std::cout << "parallelFor ueber " << N << " Elemente: " << ms << " ms\n";
    }

    // --- DAG-Durchsatz: viele kleine, voneinander abhaengige Task-Ketten ----
    {
        JobSystem js({.numWorkers = hwThreads});
        constexpr int Chains = 10000;
        std::atomic<int> completed{0};

        long long ms = timeMs([&] {
            for (int i = 0; i < Chains; ++i) {
                auto A = js.createTask([] {}, "A");
                auto B = js.createTask([] {}, "B");
                auto C = js.createTask([&] { completed.fetch_add(1); }, "C");
                js.addDependency(A, B);
                js.addDependency(B, C);
                js.submit(A);
                js.submit(B);
                js.submit(C);
            }
            js.waitForAll();
        });
        std::cout << Chains << " A->B->C Ketten: " << ms << " ms"
                   << " (completed=" << completed.load() << ")\n";
    }

    // --- Work-Stealing unter ungleichmaessiger Last -------------------------
    {
        JobSystem js({.numWorkers = hwThreads});
        std::atomic<long long> totalWork{0};

        long long ms = timeMs([&] {
            // Ein einzelner "schwerer" Batch plus viele leichte Tasks -
            // Work-Stealing sollte verhindern, dass Worker, die die leichten
            // Tasks schnell abarbeiten, danach idle bleiben, waehrend ein
            // anderer Worker noch am schweren Batch sitzt.
            js.schedule([&] {
                for (int i = 0; i < 5'000'000; ++i) {
                    totalWork.fetch_add(1, std::memory_order_relaxed);
                }
            }, "heavy");
            for (int i = 0; i < 10000; ++i) {
                js.schedule([&] { totalWork.fetch_add(1, std::memory_order_relaxed); }, "light");
            }
            js.waitForAll();
        });
        std::cout << "Ungleichmaessige Last (1 schwerer + 10000 leichte Tasks): "
                   << ms << " ms (totalWork=" << totalWork.load() << ")\n";
    }


    // --- Work-Stealing Lastverteilung (P0-M4 Review Punkt 4) ---------------
    {
        JobSystem js({.numWorkers = hwThreads});
        constexpr int N = 1'000'000;
        std::vector<std::atomic<long long>> perWorker(hwThreads);
        for (auto& p : perWorker) p.store(0);

        long long ms = timeMs([&] {
            for (int i = 0; i < N; ++i) {
                js.schedule([&]() {
                    uint32_t wid = js.currentWorkerId();
                    if (wid < hwThreads) {
                        perWorker[wid].fetch_add(1, std::memory_order_relaxed);
                    }
                });
            }
            js.waitForAll();
        });

        long long maxVal = 0, minVal = N;
        for (uint32_t i = 0; i < hwThreads; ++i) {
            long long v = perWorker[i].load();
            if (v > maxVal) maxVal = v;
            if (v < minVal) minVal = v;
        }

        double imbalance = (maxVal > 0) ? (100.0 * static_cast<double>(maxVal - minVal) / static_cast<double>(maxVal)) : 0.0;
        std::cout << "Lastverteilung (1M Tasks, " << hwThreads << " Worker):\n";
        for (uint32_t i = 0; i < hwThreads; ++i) {
            std::cout << "  Worker " << i << ": " << perWorker[i].load() << "\n";
        }
        std::cout << "  Imbalance: " << imbalance << "% (max=" << maxVal
                  << ", min=" << minVal << ")\n";
        std::cout << "  Zeit: " << ms << " ms\n\n";
    }

    return 0;
}

