#pragma once

#include "core/profiling/tracy_seed.h"
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <mutex>
#include <string>
#include <unordered_map>

namespace seed::memory {

// ---------------------------------------------------------------------------
// MemoryTracker
// ---------------------------------------------------------------------------
// Budget tracking with alarm callback. Thread-safe via atomics.
// ---------------------------------------------------------------------------
struct MemoryStats {
    std::atomic<size_t> totalAllocated{0};
    std::atomic<size_t> totalUsed{0};
    std::atomic<size_t> peakUsed{0};
    std::atomic<uint32_t> activeAllocations{0};
};

class MemoryTracker {
public:
    using AlarmCallback = std::function<void(const std::string& category,
                                              size_t used,
                                              size_t budget)>;

    MemoryTracker() = default;

    // Set budget for a category (0 = unlimited)
    void setBudget(const std::string& category, size_t bytes);

    // Track allocation
    void trackAllocation(const std::string& category, size_t size);

    // Track deallocation
    void trackDeallocation(const std::string& category, size_t size);

    // Check if category is over budget
    bool checkBudget(const std::string& category) const;

    // Set alarm callback
    void setAlarmCallback(AlarmCallback cb) { m_alarmCallback = std::move(cb); }

    // Query stats
    const MemoryStats* getStats(const std::string& category) const;
    size_t getBudget(const std::string& category) const;

private:
    struct CategoryData {
        MemoryStats stats;
        size_t budget = 0; // 0 = unlimited
    };

    mutable std::mutex m_mutex;
    std::unordered_map<std::string, CategoryData> m_categories;
    AlarmCallback m_alarmCallback;
};

} // namespace seed::memory
