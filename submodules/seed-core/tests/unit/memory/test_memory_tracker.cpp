#include <doctest/doctest.h>
#include "core/memory/memory_tracker.h"

using namespace seed::memory;

TEST_CASE("MemoryTracker_BasicTracking") {
    MemoryTracker tracker;

    tracker.trackAllocation("ecs", 1024);
    tracker.trackAllocation("ecs", 2048);

    const auto* stats = tracker.getStats("ecs");
    REQUIRE(stats != nullptr);
    CHECK(stats->totalAllocated.load() == 3072);
    CHECK(stats->totalUsed.load() == 3072);
    CHECK(stats->activeAllocations.load() == 2);

    tracker.trackDeallocation("ecs", 1024);
    CHECK(stats->totalUsed.load() == 2048);
    CHECK(stats->activeAllocations.load() == 1);
}

TEST_CASE("MemoryTracker_BudgetAlarm") {
    MemoryTracker tracker;

    bool alarmFired = false;
    std::string alarmCategory;
    size_t alarmUsed = 0;
    size_t alarmBudget = 0;

    tracker.setAlarmCallback([&](const std::string& cat, size_t used, size_t budget) {
        alarmFired = true;
        alarmCategory = cat;
        alarmUsed = used;
        alarmBudget = budget;
    });

    tracker.setBudget("render", 1000);
    tracker.trackAllocation("render", 500);
    CHECK(!alarmFired); // Under budget

    tracker.trackAllocation("render", 600);
    CHECK(alarmFired);
    CHECK(alarmCategory == "render");
    CHECK(alarmUsed == 1100);
    CHECK(alarmBudget == 1000);
}

TEST_CASE("MemoryTracker_CheckBudget") {
    MemoryTracker tracker;

    tracker.setBudget("audio", 100);
    CHECK(!tracker.checkBudget("audio"));

    tracker.trackAllocation("audio", 50);
    CHECK(!tracker.checkBudget("audio"));

    tracker.trackAllocation("audio", 60);
    CHECK(tracker.checkBudget("audio"));
}

TEST_CASE("MemoryTracker_PeakTracking") {
    MemoryTracker tracker;

    tracker.trackAllocation("physics", 100);
    tracker.trackAllocation("physics", 50);
    tracker.trackDeallocation("physics", 100);

    const auto* stats = tracker.getStats("physics");
    REQUIRE(stats != nullptr);
    CHECK(stats->peakUsed.load() == 150);
    CHECK(stats->totalUsed.load() == 50);
}
