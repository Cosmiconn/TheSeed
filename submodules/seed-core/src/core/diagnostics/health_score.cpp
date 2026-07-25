#include "core/diagnostics/health_score.h"
#include <array>
#include <fmt/format.h>
#include <algorithm>
#include <iterator>

namespace seed::diagnostics {

std::array<std::atomic<uint8_t>, static_cast<size_t>(HealthScore::Module::Count)>&
HealthScore::scores() noexcept {
    static std::array<std::atomic<uint8_t>, static_cast<size_t>(Module::Count)> s_scores{};
    // Initialize to 100
    static bool initialized = []() {
        for (auto& s : s_scores) s.store(100, std::memory_order_relaxed);
        return true;
    }();
    (void)initialized;
    return s_scores;
}

void HealthScore::setScore(Module module, uint8_t score) noexcept {
    if (static_cast<size_t>(module) >= static_cast<size_t>(Module::Count)) return;
    scores()[static_cast<size_t>(module)].store(
        (score > 100) ? 100 : score, std::memory_order_relaxed);
}

uint8_t HealthScore::getScore(Module module) noexcept {
    if (static_cast<size_t>(module) >= static_cast<size_t>(Module::Count)) return 0;
    return scores()[static_cast<size_t>(module)].load(std::memory_order_relaxed);
}

bool HealthScore::isHealthy() noexcept {
    for (size_t i = 0; i < static_cast<size_t>(Module::Count); ++i) {
        if (scores()[i].load(std::memory_order_relaxed) < 80) return false;
    }
    return true;
}

bool HealthScore::isCritical() noexcept {
    for (size_t i = 0; i < static_cast<size_t>(Module::Count); ++i) {
        if (scores()[i].load(std::memory_order_relaxed) <= 50) return true;
    }
    return false;
}

HealthScore::Module HealthScore::worstModule() noexcept {
    uint8_t minScore = 101;
    size_t minIdx = 0;
    for (size_t i = 0; i < static_cast<size_t>(Module::Count); ++i) {
        uint8_t s = scores()[i].load(std::memory_order_relaxed);
        if (s < minScore) {
            minScore = s;
            minIdx = i;
        }
    }
    return static_cast<Module>(minIdx);
}

std::string HealthScore::report() {
    std::string out;
    out.reserve(256);
    fmt::format_to(std::back_inserter(out), "=== Health Score Report ===\n");
    for (size_t i = 0; i < static_cast<size_t>(Module::Count); ++i) {
        auto mod = static_cast<Module>(i);
        uint8_t s = getScore(mod);
        const char* status = (s >= 80) ? "OK" : (s >= 50) ? "WARN" : "CRIT";
        fmt::format_to(std::back_inserter(out), "  {:>16} {:3}/100 [{}]\n",
                       moduleName(mod), s, status);
    }
    fmt::format_to(std::back_inserter(out), "=========================\n");
    return out;
}

} // namespace seed::diagnostics
