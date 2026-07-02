// =============================================================================
// metrics/MetricsDashboard.cpp — Metrics Dashboard Implementation (AP-91)
// =============================================================================
// Zentrale Sammlung und ImGui-Anzeige aller Engine-Metriken.
// Integriert MemoryProfiler (AP-80), NetworkProfiler (AP-81), ThreadPool (P4).
// =============================================================================
#include "MetricsDashboard.h"
#include "../memory/MemoryProfiler.h"
#include "../network/NetworkProfiler.h"
#include "../server/ThreadPool.h"
#include "../ecs/ecs_EcsWorld.h"
#include "../core/Log.h"

#include <imgui.h>
#include <sstream>
#include <iomanip>
#include <algorithm>
#include <cmath>

// Externe globale Variablen (aus main.cpp)
extern std::unique_ptr<ecs::EcsWorld> gEcsWorld;
extern std::unique_ptr<ThreadPool> gThreadPool;

namespace metrics {

// =============================================================================
// SINGLETON
// =============================================================================
MetricsDashboard& MetricsDashboard::GetInstance() {
    static MetricsDashboard instance;
    return instance;
}

// =============================================================================
// LIFECYCLE
// =============================================================================
void MetricsDashboard::Initialize() {
    if (initialized.exchange(true)) return;

    history.clear();
    alerts.clear();

    currentFps.store(0.0f);
    currentFrameTimeMs.store(0.0f);
    currentDrawCalls.store(0);
    currentMemoryBytes.store(0);
    currentNetworkQuality.store(1.0f);
    currentPendingTasks.store(0);

    AddLog("[MetricsDashboard] Initialisiert — Dashboard aktiv");
}

void MetricsDashboard::Shutdown() {
    if (!initialized.exchange(false)) return;

    AddLog("[MetricsDashboard] Heruntergefahren — {} Alerts, {} History-Punkte",
             alerts.size(), history.size());
}

// =============================================================================
// UPDATE — Wird jeden Frame aufgerufen
// =============================================================================
void MetricsDashboard::Update(float deltaTime, float fps, uint32_t drawCalls) {
    if (!initialized) return;

    updateAccumulator += deltaTime;
    if (updateAccumulator < updateIntervalSeconds) return;
    updateAccumulator -= updateIntervalSeconds;

    // Aktuelle Werte aktualisieren
    currentFps.store(fps);
    currentFrameTimeMs.store(deltaTime * 1000.0f);
    currentDrawCalls.store(drawCalls);

    CollectMetrics(fps, drawCalls);
    RecordHistoryPoint();
    CheckAlerts();
}

// =============================================================================
// METRIKEN SAMMELN
// =============================================================================
void MetricsDashboard::CollectMetrics(float fps, uint32_t drawCalls) {
    // Memory
    auto& memProfiler = memory::MemoryProfiler::GetInstance();
    if (memProfiler.IsInitialized()) {
        currentMemoryBytes.store(memProfiler.GetTotalActiveBytes());
    }

    // Network
    auto& netProfiler = net::NetworkProfiler::GetInstance();
    if (netProfiler.IsInitialized()) {
        auto stats = netProfiler.GetGlobalStats();
        currentNetworkQuality.store(stats.globalQualityScore);
    }

    // ThreadPool
    if (gThreadPool) {
        currentPendingTasks.store(gThreadPool->GetPendingCount());
    }

    (void)fps;
    (void)drawCalls;
}

// =============================================================================
// HISTORIE AUFZEICHNEN
// =============================================================================
void MetricsDashboard::RecordHistoryPoint() {
    MetricsHistoryPoint point;
    point.timestamp = std::chrono::steady_clock::now();

    // Memory
    auto& memProfiler = memory::MemoryProfiler::GetInstance();
    if (memProfiler.IsInitialized()) {
        point.memoryActiveBytes = memProfiler.GetTotalActiveBytes();
        point.memoryPeakBytes = memProfiler.GetPeakAllocatedBytes();
        point.memoryAllocationCount = memProfiler.GetActiveAllocationCount();
        auto ecsStats = memProfiler.GetEcsStats();
        point.memoryFragmentation = ecsStats.utilizationRatio;
    }

    // Network
    auto& netProfiler = net::NetworkProfiler::GetInstance();
    if (netProfiler.IsInitialized()) {
        auto stats = netProfiler.GetGlobalStats();
        point.netBytesSent = stats.totalBytesSent;
        point.netBytesReceived = stats.totalBytesReceived;
        point.netAvgRttMs = stats.globalAvgRttMs;
        point.netPacketLoss = stats.globalPacketLossRate;
        point.netQualityScore = stats.globalQualityScore;
        point.netActiveConnections = static_cast<uint32_t>(stats.activeConnections);
    }

    // ThreadPool
    if (gThreadPool) {
        point.tpPendingTasks = gThreadPool->GetPendingCount();
        point.tpExecutedTasks = gThreadPool->GetExecutedCount();
        point.tpAvgTaskTimeUs = static_cast<float>(gThreadPool->GetAverageTaskTimeUs());
        point.tpMaxTaskTimeUs = gThreadPool->GetMaxTaskTimeUs();
    }

    // ECS
    if (gEcsWorld) {
        point.ecsEntityCount = gEcsWorld->GetEntityCount();
        point.ecsArchetypeCount = gEcsWorld->GetArchetypeCount();
        point.ecsChunkCount = gEcsWorld->GetTotalChunkCount();
        auto ecsMem = memProfiler.GetEcsStats();
        point.ecsUtilization = ecsMem.utilizationRatio;
    }

    // Renderer
    point.fps = currentFps.load();
    point.frameTimeMs = currentFrameTimeMs.load();
    point.drawCalls = currentDrawCalls.load();

    std::lock_guard lock(historyMutex);
    history.push_back(point);
    if (history.size() > maxHistoryPoints) {
        history.pop_front();
    }
}

// =============================================================================
// ALERTS PRUEFEN
// =============================================================================
void MetricsDashboard::CheckAlerts() {
    auto& memProfiler = memory::MemoryProfiler::GetInstance();
    auto& netProfiler = net::NetworkProfiler::GetInstance();

    // Memory-Leak-Check
    if (memProfiler.IsInitialized()) {
        auto leaks = memProfiler.FindLeaksOlderThan(alertConfig.memoryLeakAgeSeconds);
        size_t leakBytes = 0;
        for (const auto& leak : leaks) {
            leakBytes += leak.size;
        }
        if (leakBytes > alertConfig.memoryLeakThresholdBytes) {
            AddAlert(AlertSeverity::Warning, MetricCategory::Memory,
                     "Speicherleck erkannt", "MemoryProfiler",
                     static_cast<float>(leakBytes),
                     static_cast<float>(alertConfig.memoryLeakThresholdBytes));
        }

        // Fragmentierung
        auto ecsStats = memProfiler.GetEcsStats();
        if (ecsStats.utilizationRatio > 0.0f &&
            ecsStats.utilizationRatio < alertConfig.memoryFragmentationThreshold) {
            AddAlert(AlertSeverity::Info, MetricCategory::Memory,
                     "Hohe Speicherfragmentierung", "MemoryProfiler",
                     ecsStats.utilizationRatio * 100.0f,
                     alertConfig.memoryFragmentationThreshold * 100.0f);
        }
    }

    // Network-Checks
    if (netProfiler.IsInitialized()) {
        auto stats = netProfiler.GetGlobalStats();

        if (stats.globalAvgRttMs > alertConfig.networkRttThresholdMs) {
            AddAlert(AlertSeverity::Warning, MetricCategory::Network,
                     "Hohe Netzwerk-Latenz", "NetworkProfiler",
                     stats.globalAvgRttMs, alertConfig.networkRttThresholdMs);
        }

        if (stats.globalPacketLossRate > alertConfig.networkPacketLossThreshold) {
            AddAlert(AlertSeverity::Error, MetricCategory::Network,
                     "Hoher Paketverlust", "NetworkProfiler",
                     stats.globalPacketLossRate * 100.0f,
                     alertConfig.networkPacketLossThreshold * 100.0f);
        }

        if (stats.globalQualityScore < alertConfig.networkQualityThreshold) {
            AddAlert(AlertSeverity::Warning, MetricCategory::Network,
                     "Niedrige Verbindungsqualitaet", "NetworkProfiler",
                     stats.globalQualityScore * 100.0f,
                     alertConfig.networkQualityThreshold * 100.0f);
        }

        if (stats.activeConnections > alertConfig.networkMaxConnections) {
            AddAlert(AlertSeverity::Critical, MetricCategory::Network,
                     "Maximale Verbindungsanzahl ueberschritten", "NetworkProfiler",
                     static_cast<float>(stats.activeConnections),
                     static_cast<float>(alertConfig.networkMaxConnections));
        }
    }

    // ThreadPool-Checks
    if (gThreadPool) {
        if (gThreadPool->GetPendingCount() > alertConfig.threadPoolQueueThreshold) {
            AddAlert(AlertSeverity::Warning, MetricCategory::ThreadPool,
                     "ThreadPool-Queue ueberlastet", "ThreadPool",
                     static_cast<float>(gThreadPool->GetPendingCount()),
                     static_cast<float>(alertConfig.threadPoolQueueThreshold));
        }

        if (gThreadPool->GetMaxTaskTimeUs() > alertConfig.threadPoolMaxTaskTimeMs * 1000.0f) {
            AddAlert(AlertSeverity::Warning, MetricCategory::ThreadPool,
                     "Langsame Task-Ausfuehrung", "ThreadPool",
                     gThreadPool->GetMaxTaskTimeUs() / 1000.0f,
                     alertConfig.threadPoolMaxTaskTimeMs);
        }
    }

    // ECS-Checks
    if (gEcsWorld) {
        auto ecsMem = memProfiler.GetEcsStats();
        if (ecsMem.utilizationRatio > 0.0f &&
            ecsMem.utilizationRatio < alertConfig.ecsUtilizationThreshold) {
            AddAlert(AlertSeverity::Info, MetricCategory::ECS,
                     "Niedrige ECS-Auslastung", "ECS",
                     ecsMem.utilizationRatio * 100.0f,
                     alertConfig.ecsUtilizationThreshold * 100.0f);
        }

        if (gEcsWorld->GetEntityCount() > alertConfig.ecsMaxEntities) {
            AddAlert(AlertSeverity::Warning, MetricCategory::ECS,
                     "Hohe Entity-Anzahl", "ECS",
                     static_cast<float>(gEcsWorld->GetEntityCount()),
                     static_cast<float>(alertConfig.ecsMaxEntities));
        }
    }

    // Renderer-Checks
    if (currentFps.load() < alertConfig.fpsThreshold && currentFps.load() > 0.0f) {
        AddAlert(AlertSeverity::Warning, MetricCategory::Renderer,
                 "Niedrige FPS", "Renderer",
                 currentFps.load(), alertConfig.fpsThreshold);
    }

    if (currentFrameTimeMs.load() > alertConfig.frameTimeThresholdMs) {
        AddAlert(AlertSeverity::Warning, MetricCategory::Renderer,
                 "Hohe Frame-Zeit", "Renderer",
                 currentFrameTimeMs.load(), alertConfig.frameTimeThresholdMs);
    }
}

// =============================================================================
// ALERT HINZUFUEGEN
// =============================================================================
void MetricsDashboard::AddAlert(AlertSeverity severity, MetricCategory category,
                                std::string_view message, std::string_view source,
                                float value, float threshold) {
    // Pruefe auf Duplikate (gleiche Nachricht innerhalb 60s)
    {
        std::lock_guard lock(alertsMutex);
        auto now = std::chrono::steady_clock::now();
        for (const auto& alert : alerts) {
            auto age = std::chrono::duration<float>(now - alert.timestamp).count();
            if (age < 60.0f && alert.message == message && alert.source == source) {
                return; // Duplikat
            }
        }

        Alert alert;
        alert.timestamp = now;
        alert.severity = severity;
        alert.category = category;
        alert.message = std::string(message);
        alert.source = std::string(source);
        alert.value = value;
        alert.threshold = threshold;
        alert.acknowledged = false;

        alerts.push_back(alert);

        if (alerts.size() > maxAlerts) {
            alerts.erase(alerts.begin());
        }
    }

    // Log-Ausgabe je nach Schweregrad
    switch (severity) {
        case AlertSeverity::Info:
            AddLog("[MetricsDashboard][INFO] {}: {} (Wert: {:.2f}, Grenze: {:.2f})",
                     source, message, value, threshold);
            break;
        case AlertSeverity::Warning:
            AddLog("[MetricsDashboard][WARN] {}: {} (Wert: {:.2f}, Grenze: {:.2f})",
                     source, message, value, threshold);
            break;
        case AlertSeverity::Error:
            AddLog("[MetricsDashboard][ERROR] {}: {} (Wert: {:.2f}, Grenze: {:.2f})",
                     source, message, value, threshold);
            break;
        case AlertSeverity::Critical:
            AddLog("[MetricsDashboard][CRITICAL] {}: {} (Wert: {:.2f}, Grenze: {:.2f})",
                     source, message, value, threshold);
            break;
    }
}

// =============================================================================
// ALERT-MANAGEMENT
// =============================================================================
std::vector<Alert> MetricsDashboard::GetAlerts() const {
    std::lock_guard lock(alertsMutex);
    return alerts;
}

std::vector<Alert> MetricsDashboard::GetUnacknowledgedAlerts() const {
    std::lock_guard lock(alertsMutex);
    std::vector<Alert> result;
    for (const auto& alert : alerts) {
        if (!alert.acknowledged) {
            result.push_back(alert);
        }
    }
    return result;
}

void MetricsDashboard::AcknowledgeAlert(size_t index) {
    std::lock_guard lock(alertsMutex);
    if (index < alerts.size()) {
        alerts[index].acknowledged = true;
    }
}

void MetricsDashboard::AcknowledgeAllAlerts() {
    std::lock_guard lock(alertsMutex);
    for (auto& alert : alerts) {
        alert.acknowledged = true;
    }
}

void MetricsDashboard::ClearAlerts() {
    std::lock_guard lock(alertsMutex);
    alerts.clear();
}

size_t MetricsDashboard::GetAlertCount() const {
    std::lock_guard lock(alertsMutex);
    return alerts.size();
}

size_t MetricsDashboard::GetCriticalAlertCount() const {
    std::lock_guard lock(alertsMutex);
    size_t count = 0;
    for (const auto& alert : alerts) {
        if (!alert.acknowledged && alert.severity == AlertSeverity::Critical) {
            count++;
        }
    }
    return count;
}

// =============================================================================
// HISTORIE
// =============================================================================
std::vector<MetricsHistoryPoint> MetricsDashboard::GetHistory() const {
    std::lock_guard lock(historyMutex);
    return std::vector<MetricsHistoryPoint>(history.begin(), history.end());
}

void MetricsDashboard::ClearHistory() {
    std::lock_guard lock(historyMutex);
    history.clear();
}

// =============================================================================
// BERICHT
// =============================================================================
std::string MetricsDashboard::GenerateReport() const {
    std::ostringstream oss;
    auto now = std::chrono::steady_clock::now();

    oss << "================================================================================\n";
    oss << " METRICS DASHBOARD BERICHT\n";
    oss << "================================================================================\n";
    oss << "Zeitpunkt: " << std::chrono::duration<float>(now.time_since_epoch()).count() << "s\n";
    oss << "--------------------------------------------------------------------------------\n";

    // Aktuelle Werte
    oss << " AKTUELLE METRIKEN\n";
    oss << "--------------------------------------------------------------------------------\n";
    oss << " FPS:                  " << std::setw(10) << std::fixed << std::setprecision(1)
        << currentFps.load() << "\n";
    oss << " Frame-Zeit:           " << std::setw(10) << std::fixed << std::setprecision(2)
        << currentFrameTimeMs.load() << " ms\n";
    oss << " Draw Calls:           " << std::setw(10) << currentDrawCalls.load() << "\n";
    oss << " Speicher (aktiv):     " << std::setw(10) << currentMemoryBytes.load() << " Bytes\n";
    oss << " Netzwerk-Qualitaet:   " << std::setw(10) << std::fixed << std::setprecision(1)
        << (currentNetworkQuality.load() * 100.0f) << "%\n";
    oss << " Pending Tasks:        " << std::setw(10) << currentPendingTasks.load() << "\n";
    oss << "--------------------------------------------------------------------------------\n";

    // Alerts
    auto allAlerts = GetAlerts();
    oss << " ALERTS (" << allAlerts.size() << " total)\n";
    oss << "--------------------------------------------------------------------------------\n";
    if (!allAlerts.empty()) {
        for (const auto& alert : allAlerts) {
            auto age = std::chrono::duration<float>(now - alert.timestamp).count();
            const char* sevStr = "?";
            switch (alert.severity) {
                case AlertSeverity::Info: sevStr = "INFO"; break;
                case AlertSeverity::Warning: sevStr = "WARN"; break;
                case AlertSeverity::Error: sevStr = "ERROR"; break;
                case AlertSeverity::Critical: sevStr = "CRIT"; break;
            }
            oss << " [" << sevStr << "] " << alert.source << ": " << alert.message;
            oss << " (Wert: " << alert.value << ", Grenze: " << alert.threshold << ")";
            if (alert.acknowledged) oss << " [ACK]";
            oss << " — vor " << std::fixed << std::setprecision(1) << age << "s\n";
        }
    } else {
        oss << " Keine Alerts.\n";
    }

    // Historie
    auto hist = GetHistory();
    if (!hist.empty()) {
        oss << "--------------------------------------------------------------------------------\n";
        oss << " HISTORIE (letzte " << hist.size() << " Punkte)\n";
        oss << "--------------------------------------------------------------------------------\n";
        size_t step = std::max(size_t(1), hist.size() / 10);
        for (size_t i = 0; i < hist.size(); i += step) {
            auto elapsed = std::chrono::duration<float>(now - hist[i].timestamp).count();
            oss << " T-" << std::setw(6) << std::fixed << std::setprecision(1) << elapsed << "s: "
                << "FPS=" << std::setw(5) << std::fixed << std::setprecision(1) << hist[i].fps
                << " Mem=" << std::setw(8) << hist[i].memoryActiveBytes << "B"
                << " Net=" << std::setw(4) << hist[i].netActiveConnections << "c"
                << " RTT=" << std::setw(6) << hist[i].netAvgRttMs << "ms\n";
        }
    }

    oss << "================================================================================\n";
    return oss.str();
}

void MetricsDashboard::PrintReport() const {
    AddLog("{}", GenerateReport());
}

// =============================================================================
// FARBE-HILFSMETHODEN
// =============================================================================
uint32_t MetricsDashboard::GetSeverityColor(AlertSeverity severity) {
    switch (severity) {
        case AlertSeverity::Info:     return IM_COL32(100, 200, 255, 255);
        case AlertSeverity::Warning:  return IM_COL32(255, 200, 50, 255);
        case AlertSeverity::Error:    return IM_COL32(255, 100, 50, 255);
        case AlertSeverity::Critical: return IM_COL32(255, 50, 50, 255);
    }
    return IM_COL32(255, 255, 255, 255);
}

uint32_t MetricsDashboard::GetQualityColor(float quality) {
    if (quality >= 0.8f) return IM_COL32(50, 255, 50, 255);
    if (quality >= 0.5f) return IM_COL32(255, 200, 50, 255);
    return IM_COL32(255, 50, 50, 255);
}

// =============================================================================
// SPARKLINE-RENDERING
// =============================================================================
void MetricsDashboard::RenderSparkline(const char* label, const std::vector<float>& values,
                                       float minVal, float maxVal, uint32_t color) {
    if (values.empty()) return;

    ImVec2 canvasPos = ImGui::GetCursorScreenPos();
    ImVec2 canvasSize = ImVec2(ImGui::GetContentRegionAvail().x, 40.0f);

    ImDrawList* drawList = ImGui::GetWindowDrawList();
    drawList->AddRectFilled(canvasPos, ImVec2(canvasPos.x + canvasSize.x, canvasPos.y + canvasSize.y),
                            IM_COL32(30, 30, 30, 255));

    float range = maxVal - minVal;
    if (range < 0.001f) range = 1.0f;

    float stepX = canvasSize.x / static_cast<float>(values.size() - 1);

    for (size_t i = 1; i < values.size(); ++i) {
        float x1 = canvasPos.x + (i - 1) * stepX;
        float x2 = canvasPos.x + i * stepX;
        float y1 = canvasPos.y + canvasSize.y - ((values[i - 1] - minVal) / range) * canvasSize.y;
        float y2 = canvasPos.y + canvasSize.y - ((values[i] - minVal) / range) * canvasSize.y;
        drawList->AddLine(ImVec2(x1, y1), ImVec2(x2, y2), color, 2.0f);
    }

    ImGui::Dummy(canvasSize);
    ImGui::Text("%s: %.2f (min: %.2f, max: %.2f)", label, values.back(), minVal, maxVal);
}

// =============================================================================
// IMGUI-RENDERING — HAUPTFENSTER
// =============================================================================
void MetricsDashboard::RenderWindow(bool* pOpen) {
    if (!pOpen || !*pOpen) return;

    ImGui::SetNextWindowSize(ImVec2(900, 600), ImGuiCond_FirstUseEver);
    if (ImGui::Begin("Metrics Dashboard (AP-91)", pOpen)) {
        Render();
    }
    ImGui::End();
}

void MetricsDashboard::Render() {
    if (!initialized) {
        ImGui::Text("Dashboard nicht initialisiert.");
        return;
    }

    // Tab-Bar
    const char* tabNames[] = { "Uebersicht", "Speicher", "Netzwerk", "ThreadPool", "ECS", "Renderer", "Alerts" };
    int activeCat = static_cast<int>(activeCategory.load());
    ImGui::TabBar("MetricsTabs");
    for (int i = 0; i < static_cast<int>(MetricCategory::COUNT); ++i) {
        if (ImGui::TabItem(tabNames[i])) {
            activeCategory.store(static_cast<MetricCategory>(i));
            ImGui::EndTabItem();
        }
    }
    ImGui::EndTabBar();

    switch (activeCategory.load()) {
        case MetricCategory::Overview:     RenderOverview(); break;
        case MetricCategory::Memory:       RenderMemoryTab(); break;
        case MetricCategory::Network:      RenderNetworkTab(); break;
        case MetricCategory::ThreadPool:    RenderThreadPoolTab(); break;
        case MetricCategory::ECS:          RenderECSTab(); break;
        case MetricCategory::Renderer:     RenderRendererTab(); break;
        case MetricCategory::Alerts:       RenderAlertsTab(); break;
        default: break;
    }
}

// =============================================================================
// UEBERSICHT-TAB
// =============================================================================
void MetricsDashboard::RenderOverview() {
    ImGui::Text("Engine-Uebersicht — Letztes Update: %.1fs", updateAccumulator);
    ImGui::Separator();

    // Kritische Alerts-Anzeige
    size_t criticalCount = GetCriticalAlertCount();
    size_t totalAlertCount = GetAlertCount();
    if (criticalCount > 0) {
        ImGui::TextColored(ImVec4(1.0f, 0.2f, 0.2f, 1.0f),
                           "KRITISCHE ALERTS: %zu", criticalCount);
    } else if (totalAlertCount > 0) {
        ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.2f, 1.0f),
                           "Alerts: %zu", totalAlertCount);
    } else {
        ImGui::TextColored(ImVec4(0.2f, 1.0f, 0.2f, 1.0f), "System OK — Keine Alerts");
    }

    ImGui::Separator();

    // Schnell-Statistiken in 3 Spalten
    ImGui::Columns(3, "OverviewColumns", false);

    // Spalte 1: Performance
    ImGui::Text("PERFORMANCE");
    ImGui::Text("FPS: %.1f", currentFps.load());
    ImGui::Text("Frame-Zeit: %.2f ms", currentFrameTimeMs.load());
    ImGui::Text("Draw Calls: %u", currentDrawCalls.load());

    float fps = currentFps.load();
    if (fps < 30.0f) {
        ImGui::TextColored(ImVec4(1.0f, 0.3f, 0.3f, 1.0f), "LOW FPS!");
    } else if (fps < 55.0f) {
        ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.2f, 1.0f), "Suboptimal");
    } else {
        ImGui::TextColored(ImVec4(0.3f, 1.0f, 0.3f, 1.0f), "Gut");
    }
    ImGui::NextColumn();

    // Spalte 2: Speicher
    auto& memProfiler = memory::MemoryProfiler::GetInstance();
    if (memProfiler.IsInitialized()) {
        ImGui::Text("SPEICHER");
        ImGui::Text("Aktiv: %.2f MB", memProfiler.GetTotalActiveBytes() / (1024.0f * 1024.0f));
        ImGui::Text("Peak: %.2f MB", memProfiler.GetPeakAllocatedBytes() / (1024.0f * 1024.0f));
        ImGui::Text("Allokationen: %zu", memProfiler.GetActiveAllocationCount());

        auto ecsMem = memProfiler.GetEcsStats();
        ImGui::Text("ECS-Nutzung: %.1f%%", ecsMem.utilizationRatio * 100.0f);
    }
    ImGui::NextColumn();

    // Spalte 3: Netzwerk
    auto& netProfiler = net::NetworkProfiler::GetInstance();
    if (netProfiler.IsInitialized()) {
        auto stats = netProfiler.GetGlobalStats();
        ImGui::Text("NETZWERK");
        ImGui::Text("Verbindungen: %llu", static_cast<unsigned long long>(stats.activeConnections));
        ImGui::Text("RTT: %.2f ms", stats.globalAvgRttMs);
        ImGui::Text("Verlust: %.2f%%", stats.globalPacketLossRate * 100.0f);

        uint32_t color = GetQualityColor(stats.globalQualityScore);
        ImVec4 qCol;
        ImGui::ColorConvertU32ToFloat4(color, &qCol.x);
        ImGui::TextColored(qCol, "Qualitaet: %.1f%%", stats.globalQualityScore * 100.0f);
    }
    ImGui::NextColumn();
    ImGui::Columns(1);

    ImGui::Separator();

    // Historie-Graphen
    auto hist = GetHistory();
    if (hist.size() >= 2) {
        std::vector<float> fpsValues;
        std::vector<float> memValues;
        std::vector<float> rttValues;
        fpsValues.reserve(hist.size());
        memValues.reserve(hist.size());
        rttValues.reserve(hist.size());

        for (const auto& point : hist) {
            fpsValues.push_back(point.fps);
            memValues.push_back(point.memoryActiveBytes / (1024.0f * 1024.0f)); // MB
            rttValues.push_back(point.netAvgRttMs);
        }

        RenderSparkline("FPS", fpsValues, 0.0f, 120.0f, IM_COL32(100, 255, 100, 255));
        RenderSparkline("Speicher (MB)", memValues, 0.0f,
                        *std::max_element(memValues.begin(), memValues.end()) * 1.2f,
                        IM_COL32(100, 150, 255, 255));
        RenderSparkline("RTT (ms)", rttValues, 0.0f,
                        std::max(100.0f, *std::max_element(rttValues.begin(), rttValues.end()) * 1.2f),
                        IM_COL32(255, 150, 100, 255));
    }
}

// =============================================================================
// SPEICHER-TAB
// =============================================================================
void MetricsDashboard::RenderMemoryTab() {
    auto& memProfiler = memory::MemoryProfiler::GetInstance();
    if (!memProfiler.IsInitialized()) {
        ImGui::Text("MemoryProfiler nicht initialisiert.");
        return;
    }

    ImGui::Text("SPEICHER-METRIKEN");
    ImGui::Separator();

    // Globale Statistiken
    ImGui::Text("Total Allokiert: %.2f MB", memProfiler.GetTotalAllocatedBytes() / (1024.0f * 1024.0f));
    ImGui::Text("Aktiv Belegt:    %.2f MB", memProfiler.GetTotalActiveBytes() / (1024.0f * 1024.0f));
    ImGui::Text("Peak Belegt:     %.2f MB", memProfiler.GetPeakAllocatedBytes() / (1024.0f * 1024.0f));
    ImGui::Text("Allokationen:    %zu", memProfiler.GetTotalAllocationCount());
    ImGui::Text("Aktive Allok:    %zu", memProfiler.GetActiveAllocationCount());

    ImGui::Separator();

    // ECS-Speicher
    auto ecsStats = memProfiler.GetEcsStats();
    ImGui::Text("ECS-SPEICHER");
    ImGui::Text("Chunks:      %zu", ecsStats.chunkCount);
    ImGui::Text("Entities:    %zu", ecsStats.entityCount);
    ImGui::Text("Archetypes:  %zu", ecsStats.archetypeCount);
    ImGui::Text("Chunk-Mem:   %.2f MB", ecsStats.totalChunkMemory / (1024.0f * 1024.0f));
    ImGui::Text("Genutzt:     %.2f MB", ecsStats.usedChunkMemory / (1024.0f * 1024.0f));
    ImGui::Text("Nutzungsgrad: %.1f%%", ecsStats.utilizationRatio * 100.0f);

    // Nutzungsgrad-Balken
    float utilization = ecsStats.utilizationRatio;
    ImVec4 uCol = utilization > 0.7f ? ImVec4(0.2f, 1.0f, 0.2f, 1.0f) :
                    (utilization > 0.3f ? ImVec4(1.0f, 0.8f, 0.2f, 1.0f) :
                                          ImVec4(1.0f, 0.3f, 0.3f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_PlotHistogram, uCol);
    ImGui::ProgressBar(utilization, ImVec2(-1, 20), "");
    ImGui::PopStyleColor();

    ImGui::Separator();

    // Allokator-Statistiken
    auto allStats = memProfiler.GetAllAllocatorStats();
    if (!allStats.empty()) {
        ImGui::Text("ALLOKATOREN");
        if (ImGui::BeginTable("AllocatorTable", 6, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg)) {
            ImGui::TableSetupColumn("Name");
            ImGui::TableSetupColumn("Aktiv");
            ImGui::TableSetupColumn("Peak");
            ImGui::TableSetupColumn("Allok");
            ImGui::TableSetupColumn("Frei");
            ImGui::TableSetupColumn("Offen");
            ImGui::TableHeadersRow();

            for (const auto& stats : allStats) {
                ImGui::TableNextRow();
                ImGui::TableSetColumnIndex(0); ImGui::Text("%s", stats.name.c_str());
                ImGui::TableSetColumnIndex(1); ImGui::Text("%.2f KB", stats.currentAllocatedBytes / 1024.0f);
                ImGui::TableSetColumnIndex(2); ImGui::Text("%.2f KB", stats.peakAllocatedBytes / 1024.0f);
                ImGui::TableSetColumnIndex(3); ImGui::Text("%zu", stats.totalAllocations);
                ImGui::TableSetColumnIndex(4); ImGui::Text("%zu", stats.totalFrees);
                ImGui::TableSetColumnIndex(5); ImGui::Text("%zu", stats.activeAllocations);
            }
            ImGui::EndTable();
        }
    }

    // Speicherlecks
    auto leaks = memProfiler.FindLeaks();
    if (!leaks.empty()) {
        ImGui::Separator();
        ImGui::TextColored(ImVec4(1.0f, 0.3f, 0.3f, 1.0f), "SPEICHERLECKS: %zu", leaks.size());
        for (const auto& leak : leaks) {
            ImGui::BulletText("%zu Bytes in '%s' (%s:%u)",
                              leak.size, leak.allocatorName.c_str(),
                              leak.sourceFile.c_str(), leak.sourceLine);
        }
    }
}

// =============================================================================
// NETZWERK-TAB
// =============================================================================
void MetricsDashboard::RenderNetworkTab() {
    auto& netProfiler = net::NetworkProfiler::GetInstance();
    if (!netProfiler.IsInitialized()) {
        ImGui::Text("NetworkProfiler nicht initialisiert.");
        return;
    }

    ImGui::Text("NETZWERK-METRIKEN");
    ImGui::Separator();

    auto stats = netProfiler.GetGlobalStats();

    // Globale Statistiken
    ImGui::Text("Aktive Verbindungen: %llu", static_cast<unsigned long long>(stats.activeConnections));
    ImGui::Text("Total Verbindungen:  %llu", static_cast<unsigned long long>(stats.totalConnections));
    ImGui::Text("Pakete (Out/In):     %llu / %llu",
                static_cast<unsigned long long>(stats.totalPacketsSent),
                static_cast<unsigned long long>(stats.totalPacketsReceived));
    ImGui::Text("Bytes (Out/In):      %.2f KB / %.2f KB",
                stats.totalBytesSent / 1024.0f,
                stats.totalBytesReceived / 1024.0f);
    ImGui::Text("Retransmissions:     %llu", static_cast<unsigned long long>(stats.totalRetransmissions));
    ImGui::Text("Fragmente:           %llu", static_cast<unsigned long long>(stats.totalFragments));

    ImGui::Separator();

    // RTT und Qualitaet
    ImGui::Text("Durchschn. RTT:      %.2f ms", stats.globalAvgRttMs);
    ImGui::Text("Paketverlust:        %.2f%%", stats.globalPacketLossRate * 100.0f);

    uint32_t qColor = GetQualityColor(stats.globalQualityScore);
    ImVec4 qCol;
    ImGui::ColorConvertU32ToFloat4(qColor, &qCol.x);
    ImGui::TextColored(qCol, "Globale Qualitaet:   %.1f%%", stats.globalQualityScore * 100.0f);

    // Qualitaets-Balken
    ImGui::PushStyleColor(ImGuiCol_PlotHistogram, qCol);
    ImGui::ProgressBar(stats.globalQualityScore, ImVec2(-1, 20), "");
    ImGui::PopStyleColor();

    ImGui::Separator();

    // Verbindungs-Details
    auto allMetrics = netProfiler.GetAllConnectionMetrics();
    if (!allMetrics.empty()) {
        ImGui::Text("VERBINDUNGEN (%zu)", allMetrics.size());
        if (ImGui::BeginTable("ConnectionTable", 7, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg)) {
            ImGui::TableSetupColumn("ID");
            ImGui::TableSetupColumn("Adresse");
            ImGui::TableSetupColumn("RTT (ms)");
            ImGui::TableSetupColumn("Verlust");
            ImGui::TableSetupColumn("Bandw. Out");
            ImGui::TableSetupColumn("Snapshots");
            ImGui::TableSetupColumn("Qualitaet");
            ImGui::TableHeadersRow();

            for (const auto& m : allMetrics) {
                ImGui::TableNextRow();
                ImGui::TableSetColumnIndex(0); ImGui::Text("%u", m.clientId);
                ImGui::TableSetColumnIndex(1); ImGui::Text("%s:%u", m.clientAddress.c_str(), m.clientPort);
                ImGui::TableSetColumnIndex(2); ImGui::Text("%.1f", m.avgRttMs);
                ImGui::TableSetColumnIndex(3); ImGui::Text("%.1f%%", m.GetPacketLossRate() * 100.0f);
                ImGui::TableSetColumnIndex(4); ImGui::Text("%.1f KB/s", m.GetSendBandwidthBps() / 1024.0f);
                ImGui::TableSetColumnIndex(5); ImGui::Text("%llu", static_cast<unsigned long long>(m.snapshotsSent));

                ImGui::TableSetColumnIndex(6);
                uint32_t c = GetQualityColor(m.qualityScore);
                ImVec4 col;
                ImGui::ColorConvertU32ToFloat4(c, &col.x);
                ImGui::TextColored(col, "%.0f%%", m.qualityScore * 100.0f);
            }
            ImGui::EndTable();
        }
    }

    // Schlechte Verbindungen
    auto poor = netProfiler.GetPoorQualityConnections();
    if (!poor.empty()) {
        ImGui::Separator();
        ImGui::TextColored(ImVec4(1.0f, 0.3f, 0.3f, 1.0f),
                           "SCHLECHTE VERBINDUNGEN: %zu", poor.size());
        for (uint32_t id : poor) {
            auto m = netProfiler.GetConnectionMetrics(id);
            ImGui::BulletText("Client %u: %.1fms RTT, %.1f%% Verlust",
                              id, m.avgRttMs, m.GetPacketLossRate() * 100.0f);
        }
    }
}

// =============================================================================
// THREADPOOL-TAB
// =============================================================================
void MetricsDashboard::RenderThreadPoolTab() {
    if (!gThreadPool) {
        ImGui::Text("ThreadPool nicht initialisiert.");
        return;
    }

    ImGui::Text("THREADPOOL-METRIKEN");
    ImGui::Separator();

    ImGui::Text("Pending Tasks:     %zu", gThreadPool->GetPendingCount());
    ImGui::Text("Executed Tasks:    %llu", static_cast<unsigned long long>(gThreadPool->GetExecutedCount()));
    ImGui::Text("Dropped Tasks:     %llu", static_cast<unsigned long long>(gThreadPool->GetDroppedCount()));
    ImGui::Text("Avg Task Time:     %.2f us", gThreadPool->GetAverageTaskTimeUs());
    ImGui::Text("Max Task Time:     %llu us", static_cast<unsigned long long>(gThreadPool->GetMaxTaskTimeUs()));

    // Pending-Tasks-Warnung
    if (gThreadPool->GetPendingCount() > 500) {
        ImGui::TextColored(ImVec4(1.0f, 0.3f, 0.3f, 1.0f),
                           "WARNUNG: Hohe Task-Queue!");
    }

    ImGui::Separator();

    // Historie
    auto hist = GetHistory();
    if (hist.size() >= 2) {
        std::vector<float> pendingValues;
        std::vector<float> avgTimeValues;
        for (const auto& point : hist) {
            pendingValues.push_back(static_cast<float>(point.tpPendingTasks));
            avgTimeValues.push_back(point.tpAvgTaskTimeUs);
        }

        RenderSparkline("Pending Tasks", pendingValues, 0.0f,
                        *std::max_element(pendingValues.begin(), pendingValues.end()) * 1.2f,
                        IM_COL32(255, 200, 50, 255));
        RenderSparkline("Avg Task Time (us)", avgTimeValues, 0.0f,
                        std::max(100.0f, *std::max_element(avgTimeValues.begin(), avgTimeValues.end()) * 1.2f),
                        IM_COL32(100, 200, 255, 255));
    }
}

// =============================================================================
// ECS-TAB
// =============================================================================
void MetricsDashboard::RenderECSTab() {
    if (!gEcsWorld) {
        ImGui::Text("ECS-World nicht initialisiert.");
        return;
    }

    ImGui::Text("ECS-METRIKEN");
    ImGui::Separator();

    ImGui::Text("Entities:    %zu", gEcsWorld->GetEntityCount());
    ImGui::Text("Archetypes:  %zu", gEcsWorld->GetArchetypeCount());
    ImGui::Text("Chunks:      %zu", gEcsWorld->GetTotalChunkCount());
    ImGui::Text("Speicher:    %.2f MB", gEcsWorld->GetTotalMemoryUsage() / (1024.0f * 1024.0f));

    auto& memProfiler = memory::MemoryProfiler::GetInstance();
    auto ecsMem = memProfiler.GetEcsStats();
    ImGui::Text("Chunk-Mem:   %.2f MB", ecsMem.totalChunkMemory / (1024.0f * 1024.0f));
    ImGui::Text("Genutzt:     %.2f MB", ecsMem.usedChunkMemory / (1024.0f * 1024.0f));
    ImGui::Text("Nutzungsgrad: %.1f%%", ecsMem.utilizationRatio * 100.0f);

    // Nutzungsgrad-Balken
    ImVec4 uCol = ecsMem.utilizationRatio > 0.7f ? ImVec4(0.2f, 1.0f, 0.2f, 1.0f) :
                    (ecsMem.utilizationRatio > 0.3f ? ImVec4(1.0f, 0.8f, 0.2f, 1.0f) :
                                                        ImVec4(1.0f, 0.3f, 0.3f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_PlotHistogram, uCol);
    ImGui::ProgressBar(ecsMem.utilizationRatio, ImVec2(-1, 20), "");
    ImGui::PopStyleColor();

    // Entity-Warnung
    if (gEcsWorld->GetEntityCount() > 50000) {
        ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.2f, 1.0f),
                           "Hohe Entity-Anzahl — Performance beachten");
    }
}

// =============================================================================
// RENDERER-TAB
// =============================================================================
void MetricsDashboard::RenderRendererTab() {
    ImGui::Text("RENDERER-METRIKEN");
    ImGui::Separator();

    ImGui::Text("FPS:         %.1f", currentFps.load());
    ImGui::Text("Frame-Zeit:  %.2f ms", currentFrameTimeMs.load());
    ImGui::Text("Draw Calls:  %u", currentDrawCalls.load());

    // FPS-Farbcodierung
    float fps = currentFps.load();
    if (fps < 30.0f) {
        ImGui::TextColored(ImVec4(1.0f, 0.2f, 0.2f, 1.0f), "KRITISCH: FPS < 30!");
    } else if (fps < 55.0f) {
        ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.2f, 1.0f), "WARNUNG: FPS < 60");
    } else {
        ImGui::TextColored(ImVec4(0.2f, 1.0f, 0.2f, 1.0f), "OK: FPS >= 60");
    }

    // Historie
    auto hist = GetHistory();
    if (hist.size() >= 2) {
        std::vector<float> fpsValues;
        std::vector<float> frameTimeValues;
        for (const auto& point : hist) {
            fpsValues.push_back(point.fps);
            frameTimeValues.push_back(point.frameTimeMs);
        }

        RenderSparkline("FPS-Verlauf", fpsValues, 0.0f, 120.0f, IM_COL32(100, 255, 100, 255));
        RenderSparkline("Frame-Zeit (ms)", frameTimeValues, 0.0f,
                        std::max(50.0f, *std::max_element(frameTimeValues.begin(), frameTimeValues.end()) * 1.2f),
                        IM_COL32(255, 100, 100, 255));
    }
}

// =============================================================================
// ALERTS-TAB
// =============================================================================
void MetricsDashboard::RenderAlertsTab() {
    ImGui::Text("ALERTS");
    ImGui::Separator();

    auto allAlerts = GetAlerts();
    size_t unackCount = GetUnacknowledgedAlerts().size();

    ImGui::Text("Total: %zu | Unbestaetigt: %zu | Kritisch: %zu",
                allAlerts.size(), unackCount, GetCriticalAlertCount());

    if (ImGui::Button("Alle bestaetigen")) {
        AcknowledgeAllAlerts();
    }
    ImGui::SameLine();
    if (ImGui::Button("Alle loeschen")) {
        ClearAlerts();
    }

    ImGui::Separator();

    if (allAlerts.empty()) {
        ImGui::TextColored(ImVec4(0.2f, 1.0f, 0.2f, 1.0f), "Keine Alerts — System OK");
        return;
    }

    if (ImGui::BeginTable("AlertTable", 6, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg |
                           ImGuiTableFlags_Sortable)) {
        ImGui::TableSetupColumn("Zeit");
        ImGui::TableSetupColumn("Schwere");
        ImGui::TableSetupColumn("Kategorie");
        ImGui::TableSetupColumn("Quelle");
        ImGui::TableSetupColumn("Nachricht");
        ImGui::TableSetupColumn("Wert / Grenze");
        ImGui::TableHeadersRow();

        auto now = std::chrono::steady_clock::now();
        for (size_t i = 0; i < allAlerts.size(); ++i) {
            const auto& alert = allAlerts[i];
            auto age = std::chrono::duration<float>(now - alert.timestamp).count();

            ImGui::TableNextRow();

            // Farbcodierung nach Schweregrad
            uint32_t rowColor = GetSeverityColor(alert.severity);
            ImVec4 rc;
            ImGui::ColorConvertU32ToFloat4(rowColor, &rc.x);
            rc.w = 0.15f; // Sehr transparent
            ImGui::TableSetBgColor(ImGuiTableBgTarget_RowBg0, ImGui::ColorConvertFloat4ToU32(rc));

            ImGui::TableSetColumnIndex(0);
            ImGui::Text("%.0fs", age);

            ImGui::TableSetColumnIndex(1);
            const char* sevStr = "?";
            switch (alert.severity) {
                case AlertSeverity::Info: sevStr = "INFO"; break;
                case AlertSeverity::Warning: sevStr = "WARN"; break;
                case AlertSeverity::Error: sevStr = "ERROR"; break;
                case AlertSeverity::Critical: sevStr = "CRIT"; break;
            }
            ImGui::TextColored(rc, "%s", sevStr);

            ImGui::TableSetColumnIndex(2);
            const char* catStr = "?";
            switch (alert.category) {
                case MetricCategory::Memory: catStr = "Speicher"; break;
                case MetricCategory::Network: catStr = "Netzwerk"; break;
                case MetricCategory::ThreadPool: catStr = "ThreadPool"; break;
                case MetricCategory::ECS: catStr = "ECS"; break;
                case MetricCategory::Renderer: catStr = "Renderer"; break;
                default: catStr = "Sonstige"; break;
            }
            ImGui::Text("%s", catStr);

            ImGui::TableSetColumnIndex(3); ImGui::Text("%s", alert.source.c_str());
            ImGui::TableSetColumnIndex(4); ImGui::Text("%s", alert.message.c_str());
            ImGui::TableSetColumnIndex(5); ImGui::Text("%.2f / %.2f", alert.value, alert.threshold);

            // Bestaetigen-Button
            if (!alert.acknowledged) {
                ImGui::TableSetColumnIndex(5);
                ImGui::SameLine();
                if (ImGui::SmallButton(("ACK##" + std::to_string(i)).c_str())) {
                    AcknowledgeAlert(i);
                }
            }
        }
        ImGui::EndTable();
    }
}

} // namespace metrics
