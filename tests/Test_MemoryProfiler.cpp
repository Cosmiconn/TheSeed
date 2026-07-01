// =============================================================================
// tests/Test_MemoryProfiler.cpp — Memory Profiler Tests (AP-80)
// =============================================================================
// Validiert das Speicher-Tracking, Leak-Erkennung, Statistiken und
// ECS-Integration des Memory Profilers.
// =============================================================================
#include "TestMain.h"
#include "../memory/MemoryProfiler.h"
#include "../memory/PoolAllocator.h"
#include "../memory/StackAllocator.h"
#include "../memory/FreelistAllocator.h"
#include "../memory/SlabAllocator.h"
#include "../ecs/ecs_EcsWorld.h"
#include "../ecs/Components.h"
#include "../core/Log.h"

#include <thread>
#include <chrono>

using namespace memory;

// =============================================================================
// TEST 1: Grundlegendes Allokations-Tracking
// =============================================================================
TEST(MemoryProfiler_BasicTracking) {
    LogInit();
    auto& profiler = MemoryProfiler::GetInstance();
    profiler.Initialize();

    // Allokation tracken
    void* ptr1 = reinterpret_cast<void*>(0x1000);
    profiler.TrackAllocation(ptr1, 1024, 64, "TestAllocator", __FILE__, __LINE__);

    TEST_ASSERT_EQ(profiler.GetTotalAllocatedBytes(), static_cast<size_t>(1024));
    TEST_ASSERT_EQ(profiler.GetTotalActiveBytes(), static_cast<size_t>(1024));
    TEST_ASSERT_EQ(profiler.GetActiveAllocationCount(), static_cast<size_t>(1));

    // Freigabe tracken
    profiler.TrackFree(ptr1, "TestAllocator");

    TEST_ASSERT_EQ(profiler.GetTotalActiveBytes(), static_cast<size_t>(0));
    TEST_ASSERT_EQ(profiler.GetActiveAllocationCount(), static_cast<size_t>(0));
    TEST_ASSERT_EQ(profiler.GetTotalAllocatedBytes(), static_cast<size_t>(1024)); // Total bleibt

    profiler.Shutdown();
    LogShutdown();
}

// =============================================================================
// TEST 2: Peak-Tracking
// =============================================================================
TEST(MemoryProfiler_PeakTracking) {
    LogInit();
    auto& profiler = MemoryProfiler::GetInstance();
    profiler.Initialize();

    void* ptr1 = reinterpret_cast<void*>(0x2000);
    void* ptr2 = reinterpret_cast<void*>(0x3000);
    void* ptr3 = reinterpret_cast<void*>(0x4000);

    profiler.TrackAllocation(ptr1, 1000, 64, "PeakTest");
    TEST_ASSERT_EQ(profiler.GetPeakAllocatedBytes(), static_cast<size_t>(1000));

    profiler.TrackAllocation(ptr2, 2000, 64, "PeakTest");
    TEST_ASSERT_EQ(profiler.GetPeakAllocatedBytes(), static_cast<size_t>(3000));

    profiler.TrackAllocation(ptr3, 500, 64, "PeakTest");
    TEST_ASSERT_EQ(profiler.GetPeakAllocatedBytes(), static_cast<size_t>(3500));

    // Freigabe sollte Peak nicht reduzieren
    profiler.TrackFree(ptr1, "PeakTest");
    profiler.TrackFree(ptr2, "PeakTest");
    profiler.TrackFree(ptr3, "PeakTest");

    TEST_ASSERT_EQ(profiler.GetPeakAllocatedBytes(), static_cast<size_t>(3500));

    profiler.Shutdown();
    LogShutdown();
}

// =============================================================================
// TEST 3: Allokator-Statistiken
// =============================================================================
TEST(MemoryProfiler_AllocatorStats) {
    LogInit();
    auto& profiler = MemoryProfiler::GetInstance();
    profiler.Initialize();

    void* ptr1 = reinterpret_cast<void*>(0x5000);
    void* ptr2 = reinterpret_cast<void*>(0x6000);

    profiler.TrackAllocation(ptr1, 512, 64, "PoolA");
    profiler.TrackAllocation(ptr2, 1024, 64, "PoolB");

    auto statsA = profiler.GetAllocatorStats("PoolA");
    TEST_ASSERT_EQ(statsA.name, "PoolA");
    TEST_ASSERT_EQ(statsA.currentAllocatedBytes, static_cast<size_t>(512));
    TEST_ASSERT_EQ(statsA.totalAllocations, static_cast<size_t>(1));
    TEST_ASSERT_EQ(statsA.activeAllocations, static_cast<size_t>(1));

    auto statsB = profiler.GetAllocatorStats("PoolB");
    TEST_ASSERT_EQ(statsB.currentAllocatedBytes, static_cast<size_t>(1024));

    auto allStats = profiler.GetAllAllocatorStats();
    TEST_ASSERT_EQ(allStats.size(), static_cast<size_t>(2));

    profiler.TrackFree(ptr1, "PoolA");
    profiler.TrackFree(ptr2, "PoolB");

    profiler.Shutdown();
    LogShutdown();
}

// =============================================================================
// TEST 4: Speicherleck-Erkennung
// =============================================================================
TEST(MemoryProfiler_LeakDetection) {
    LogInit();
    auto& profiler = MemoryProfiler::GetInstance();
    profiler.Initialize();

    void* leaked = reinterpret_cast<void*>(0x7000);
    void* notLeaked = reinterpret_cast<void*>(0x8000);

    profiler.TrackAllocation(leaked, 2048, 64, "LeakTest", "test.cpp", 42);
    profiler.TrackAllocation(notLeaked, 1024, 64, "LeakTest", "test.cpp", 43);

    // Eines freigeben
    profiler.TrackFree(notLeaked, "LeakTest");

    // Leaks finden
    auto leaks = profiler.FindLeaks();
    TEST_ASSERT_EQ(leaks.size(), static_cast<size_t>(1));
    TEST_ASSERT_EQ(leaks[0].size, static_cast<size_t>(2048));
    TEST_ASSERT_EQ(leaks[0].sourceLine, static_cast<uint32_t>(42));

    // Leaks aelter als 0 Sekunden (sofort)
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    auto oldLeaks = profiler.FindLeaksOlderThan(0.0f);
    TEST_ASSERT_EQ(oldLeaks.size(), static_cast<size_t>(1));

    profiler.TrackFree(leaked, "LeakTest");
    auto noLeaks = profiler.FindLeaks();
    TEST_ASSERT(noLeaks.empty());

    profiler.Shutdown();
    LogShutdown();
}

// =============================================================================
// TEST 5: Historische Daten
// =============================================================================
TEST(MemoryProfiler_History) {
    LogInit();
    auto& profiler = MemoryProfiler::GetInstance();
    profiler.Initialize();

    profiler.ClearHistory();

    void* ptr = reinterpret_cast<void*>(0x9000);
    profiler.TrackAllocation(ptr, 1000, 64, "HistoryTest");

    // Mehrere Snapshots aufzeichnen
    for (int i = 0; i < 5; ++i) {
        profiler.RecordSnapshot();
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    auto history = profiler.GetHistory();
    TEST_ASSERT_EQ(history.size(), static_cast<size_t>(5));

    // Alle Snapshots sollten die gleiche Allokation enthalten
    for (const auto& point : history) {
        TEST_ASSERT_EQ(point.activeBytes, static_cast<size_t>(1000));
        TEST_ASSERT_EQ(point.allocationCount, static_cast<size_t>(1));
    }

    profiler.TrackFree(ptr, "HistoryTest");
    profiler.RecordSnapshot();

    history = profiler.GetHistory();
    TEST_ASSERT(history.back().activeBytes == 0);

    profiler.ClearHistory();
    TEST_ASSERT(profiler.GetHistory().empty());

    profiler.Shutdown();
    LogShutdown();
}

// =============================================================================
// TEST 6: Berichtserstellung
// =============================================================================
TEST(MemoryProfiler_Report) {
    LogInit();
    auto& profiler = MemoryProfiler::GetInstance();
    profiler.Initialize();

    void* ptr1 = reinterpret_cast<void*>(0xA000);
    void* ptr2 = reinterpret_cast<void*>(0xB000);

    profiler.TrackAllocation(ptr1, 1024, 64, "ReportTest", "test.cpp", 100);
    profiler.TrackAllocation(ptr2, 2048, 64, "ReportTest", "test.cpp", 101);

    auto report = profiler.GenerateReport();
    TEST_ASSERT(!report.empty());
    TEST_ASSERT(report.find("MEMORY PROFILER BERICHT") != std::string::npos);
    TEST_ASSERT(report.find("1024") != std::string::npos);
    TEST_ASSERT(report.find("ReportTest") != std::string::npos);

    profiler.TrackFree(ptr1, "ReportTest");
    profiler.TrackFree(ptr2, "ReportTest");

    profiler.Shutdown();
    LogShutdown();
}

// =============================================================================
// TEST 7: PoolAllocator mit Profiler
// =============================================================================
TEST(MemoryProfiler_PoolAllocatorIntegration) {
    LogInit();
    auto& profiler = MemoryProfiler::GetInstance();
    profiler.Initialize();

    PoolAllocator pool(128, 10, 64, "TestPool");

    void* ptr1 = pool.Allocate();
    TEST_ASSERT(ptr1 != nullptr);

    void* ptr2 = pool.Allocate();
    TEST_ASSERT(ptr2 != nullptr);

    auto stats = profiler.GetAllocatorStats("TestPool");
    TEST_ASSERT(stats.totalAllocations >= 2);

    pool.Free(ptr1);
    pool.Free(ptr2);

    profiler.Shutdown();
    LogShutdown();
}

// =============================================================================
// TEST 8: Thread-Sicherheit
// =============================================================================
TEST(MemoryProfiler_ThreadSafety) {
    LogInit();
    auto& profiler = MemoryProfiler::GetInstance();
    profiler.Initialize();

    std::atomic<int> successCount{0};
    std::vector<std::thread> threads;

    for (int t = 0; t < 4; ++t) {
        threads.emplace_back([&profiler, &successCount, t]() {
            for (int i = 0; i < 100; ++i) {
                void* ptr = reinterpret_cast<void*>(
                    static_cast<uintptr_t>(0x10000 + t * 1000 + i));
                profiler.TrackAllocation(ptr, 64, 64, "ThreadTest");
                profiler.TrackFree(ptr, "ThreadTest");
                successCount.fetch_add(1);
            }
        });
    }

    for (auto& t : threads) {
        t.join();
    }

    TEST_ASSERT_EQ(successCount.load(), 400);
    TEST_ASSERT_EQ(profiler.GetActiveAllocationCount(), static_cast<size_t>(0));

    profiler.Shutdown();
    LogShutdown();
}

// =============================================================================
// TEST 9: ECS-Speicher-Tracking
// =============================================================================
TEST(MemoryProfiler_EcsTracking) {
    LogInit();
    auto& profiler = MemoryProfiler::GetInstance();
    profiler.Initialize();

    // ECS-Speicher tracken
    profiler.TrackEcsMemory(1024 * 1024, 512 * 1024, 100, 5, 3, 12);

    auto ecsStats = profiler.GetEcsStats();
    TEST_ASSERT_EQ(ecsStats.totalChunkMemory, static_cast<size_t>(1024 * 1024));
    TEST_ASSERT_EQ(ecsStats.usedChunkMemory, static_cast<size_t>(512 * 1024));
    TEST_ASSERT_EQ(ecsStats.entityCount, static_cast<size_t>(100));
    TEST_ASSERT_EQ(ecsStats.chunkCount, static_cast<size_t>(5));
    TEST_ASSERT_EQ(ecsStats.archetypeCount, static_cast<size_t>(3));
    TEST_ASSERT_EQ(ecsStats.componentTypeCount, static_cast<size_t>(12));
    TEST_ASSERT_NEAR(ecsStats.utilizationRatio, 0.5f, 0.01f);

    // Report sollte ECS-Daten enthalten
    auto report = profiler.GenerateReport();
    TEST_ASSERT(report.find("ECS-SPEICHER") != std::string::npos);

    profiler.Shutdown();
    LogShutdown();
}

// =============================================================================
// TEST 10: EcsWorld mit automatischem Profiling
// =============================================================================
TEST(MemoryProfiler_EcsWorldIntegration) {
    LogInit();
    auto& profiler = MemoryProfiler::GetInstance();
    profiler.Initialize();

    ecs::EcsWorld world;
    TEST_ASSERT(world.Initialize());

    // Erstelle mehrere Entities
    for (int i = 0; i < 50; ++i) {
        ecs::EntityHandle e = world.CreateEntity();
        world.AddComponent(e, ecs::PositionComponent{static_cast<float>(i), 0.0f, 0.0f});
        world.AddComponent(e, ecs::HealthComponent{100, 100, true});
    }

    // Update ausfuehren (triggert periodisches Profiling)
    world.Update(1.0f / 60.0f);

    // Manuelles Profiling
    world.UpdateMemoryProfile();

    auto ecsStats = profiler.GetEcsStats();
    TEST_ASSERT(ecsStats.entityCount >= 50);
    TEST_ASSERT(ecsStats.chunkCount > 0);

    // Entity entfernen und erneut profilen
    ecs::EntityHandle toRemove = 1;
    if (world.IsAlive(toRemove)) {
        world.DestroyEntity(toRemove);
    }

    world.UpdateMemoryProfile();
    auto updatedStats = profiler.GetEcsStats();
    TEST_ASSERT(updatedStats.entityCount >= 49);

    world.Shutdown();
    profiler.Shutdown();
    LogShutdown();
}

// =============================================================================
// BENCHMARK: Massen-Allokationen
// =============================================================================
BENCHMARK(MemoryProfiler_MassAllocations, 1000) {
    LogInit();
    auto& profiler = MemoryProfiler::GetInstance();
    profiler.Initialize();

    for (int i = 0; i < 1000; ++i) {
        void* ptr = reinterpret_cast<void*>(static_cast<uintptr_t>(0x100000 + i * 64));
        profiler.TrackAllocation(ptr, 64, 64, "BenchTest");
    }

    for (int i = 0; i < 1000; ++i) {
        void* ptr = reinterpret_cast<void*>(static_cast<uintptr_t>(0x100000 + i * 64));
        profiler.TrackFree(ptr, "BenchTest");
    }

    profiler.Shutdown();
    LogShutdown();
}
