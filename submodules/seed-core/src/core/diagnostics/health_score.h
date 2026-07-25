#pragma once

#include "core/diagnostics/diagnostics_config.h"
#include <array>
#include <atomic>
#include <cstdint>
#include <string>

namespace seed::diagnostics {

// ---------------------------------------------------------------------------
// HealthScore – 0-100 score per module (Layer 9 of TEDF)
// ---------------------------------------------------------------------------
// 100 = perfect health
//  80 = warning threshold
//  50 = critical threshold
//   0 = module failure / shutdown required
// ---------------------------------------------------------------------------
class HealthScore {
public:
    enum class Module : uint8_t {
        Memory,
        ECS,
        Renderer,
        Jobs,
        Networking,
        Serialization,
        Count
    };

    static constexpr const char* moduleName(Module m) noexcept {
        switch (m) {
            case Module::Memory:      return "Memory";
            case Module::ECS:         return "ECS";
            case Module::Renderer:    return "Renderer";
            case Module::Jobs:        return "Jobs";
            case Module::Networking:  return "Networking";
            case Module::Serialization: return "Serialization";
            case Module::Count:       return "Invalid";
        }
        return "Unknown";
    }

    // Update score for a module (0-100)
    static void setScore(Module module, uint8_t score) noexcept;
    static uint8_t getScore(Module module) noexcept;

    // Check if any module is below threshold
    static bool isHealthy() noexcept;  // all >= 80
    static bool isCritical() noexcept; // any <= 50

    // Get the worst-performing module
    static Module worstModule() noexcept;

    // Formatted report
    static std::string report();

private:
    static std::array<std::atomic<uint8_t>, static_cast<size_t>(Module::Count)>& scores() noexcept;
};

// ---------------------------------------------------------------------------
// Convenience macros
// ---------------------------------------------------------------------------
#if SEED_DIAGNOSTICS_HEALTH_SCORE
#  define SEED_HEALTH_SET(mod, score) ::seed::diagnostics::HealthScore::setScore((mod), (score))
#  define SEED_HEALTH_CHECK()         ::seed::diagnostics::HealthScore::isHealthy()
#else
#  define SEED_HEALTH_SET(mod, score) ((void)0)
#  define SEED_HEALTH_CHECK()         (true)
#endif

} // namespace seed::diagnostics
