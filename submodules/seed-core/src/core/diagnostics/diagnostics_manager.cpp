#include "core/diagnostics/diagnostics_manager.h"
#include <fmt/format.h>

namespace seed::diagnostics {

DiagnosticsManager& DiagnosticsManager::instance() noexcept {
    static DiagnosticsManager s_instance;
    return s_instance;
}

void DiagnosticsManager::initialize() {
    m_initialized = true;
    globalTimeline().clear();
    m_health.setScore(HealthScore::Module::ECS, 100);
    m_health.setScore(HealthScore::Module::Memory, 100);
}

void DiagnosticsManager::update() {
    if (!m_initialized) return;
    // Periodic health checks could go here
}

void DiagnosticsManager::shutdown() {
    m_initialized = false;
}

bool DiagnosticsManager::isHealthy() const noexcept {
    return m_health.isHealthy();
}

std::string DiagnosticsManager::fullReport() const {
    std::string out;
    out += m_health.report();
    out += "\n";
    out += "=== Event Timeline (last 50) ===\n";
    out += globalTimeline().dump();
    return out;
}

void DiagnosticsManager::snapshot(const seed::ecs::World& world, const char* reason) {
    SnapshotDump dump;
    dump.capture(world);
    dump.errors.push_back(fmt::format("Manual snapshot: {}", reason));
    // Write to file or log
    fmt::print("{}", dump.toString());
}

} // namespace seed::diagnostics
