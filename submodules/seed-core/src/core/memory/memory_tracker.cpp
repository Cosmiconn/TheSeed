#include "core/memory/memory_tracker.h"
#include "core/diagnostics/event_timeline.h"
#include "core/diagnostics/diagnostics_config.h"

namespace seed::memory {

void MemoryTracker::setBudget(const std::string& category, size_t bytes) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_categories[category].budget = bytes;
}

void MemoryTracker::trackAllocation(const std::string& category, size_t size) {
    SEED_ZONE("MemoryTracker::trackAllocation");
    std::lock_guard<std::mutex> lock(m_mutex);
    auto& data = m_categories[category];

    data.stats.totalAllocated.fetch_add(size, std::memory_order_relaxed);

    size_t newUsed = data.stats.totalUsed.fetch_add(size, std::memory_order_relaxed) + size;
    data.stats.activeAllocations.fetch_add(1, std::memory_order_relaxed);

    // Update peak
    size_t expected = data.stats.peakUsed.load(std::memory_order_relaxed);
    while (newUsed > expected &&
           !data.stats.peakUsed.compare_exchange_weak(
               expected, newUsed,
               std::memory_order_relaxed,
               std::memory_order_relaxed)) {
        // retry
    }

    // Budget alarm
    if (data.budget > 0 && newUsed > data.budget && m_alarmCallback) {
        m_alarmCallback(category, newUsed, data.budget);
    }
}

void MemoryTracker::trackDeallocation(const std::string& category, size_t size) {
    SEED_ZONE("MemoryTracker::trackDeallocation");
    std::lock_guard<std::mutex> lock(m_mutex);
    auto& data = m_categories[category];

    data.stats.totalUsed.fetch_sub(size, std::memory_order_relaxed);
    data.stats.activeAllocations.fetch_sub(1, std::memory_order_relaxed);
}

bool MemoryTracker::checkBudget(const std::string& category) const {
    std::lock_guard<std::mutex> lock(m_mutex);
    auto it = m_categories.find(category);
    if (it == m_categories.end() || it->second.budget == 0) {
        return false; // No budget set or unlimited
    }
    return it->second.stats.totalUsed.load(std::memory_order_relaxed) > it->second.budget;
}

const MemoryStats* MemoryTracker::getStats(const std::string& category) const {
    std::lock_guard<std::mutex> lock(m_mutex);
    auto it = m_categories.find(category);
    if (it == m_categories.end()) return nullptr;
    return &it->second.stats;
}

size_t MemoryTracker::getBudget(const std::string& category) const {
    std::lock_guard<std::mutex> lock(m_mutex);
    auto it = m_categories.find(category);
    if (it == m_categories.end()) return 0;
    return it->second.budget;
}

} // namespace seed::memory
