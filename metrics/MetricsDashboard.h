#pragma once
// =============================================================================
// metrics/MetricsDashboard.h — Zentrales Metrics Dashboard (AP-91)
// =============================================================================
// Einheitliche Sammlung und Anzeige aller Engine-Metriken:
// • MemoryProfiler (AP-80) — Speicher-Allokationen, Leaks, ECS-Speicher
// • NetworkProfiler (AP-81) — RTT, Paketverlust, Bandbreite, Verbindungsqualitaet
// • ThreadPool (P4-FIX) — Task-Statistiken, Performance-Monitoring
// • ECS-World — Entity-Count, Archetypes, Chunks
// • Renderer — FPS, Frame-Zeit, Draw Calls
//
// ImGui-basiertes Dashboard mit Echtzeit-Graphen, Historie, Alerts.
// Singleton-Pattern fuer globalen Zugriff.
// =============================================================================
#include <cstdint>
#include <string>
#include <vector>
#include <deque>
#include <chrono>
#include <mutex>
#include <atomic>
#include <array>

// Forward Declarations (vermeidet zirkulaere Includes)
namespace memory { class MemoryProfiler; struct AllocatorStats; struct EcsMemoryStats; }
namespace net { class NetworkProfiler; struct ConnectionMetrics; struct GlobalNetworkStats; }
class ThreadPool;

namespace metrics {

// =============================================================================
// METRIC-KATEGORIE — Enum fuer Dashboard-Tabs
// =============================================================================
enum class MetricCategory : uint8_t {
    Overview = 0,       // Uebersicht aller Metriken
    Memory = 1,         // Speicher-Metriken
    Network = 2,        // Netzwerk-Metriken
    ThreadPool = 3,     // ThreadPool-Metriken
    ECS = 4,            // ECS-Metriken
    Renderer = 5,       // Renderer-Metriken
    Alerts = 6,         // Warnungen und Alerts
    COUNT = 7
};

// =============================================================================
// ALERT-TYP — Schweregrad
// =============================================================================
enum class AlertSeverity : uint8_t {
    Info = 0,
    Warning = 1,
    Error = 2,
    Critical = 3
};

// =============================================================================
// ALERT — Einzelne Warnmeldung
// =============================================================================
struct Alert {
    std::chrono::steady_clock::time_point timestamp;
    AlertSeverity severity;
    MetricCategory category;
    std::string message;
    std::string source;
    float value = 0.0f;
    float threshold = 0.0f;
    bool acknowledged = false;
};

// =============================================================================
// GESCHICHTLICHER DATENPUNKT — Fuer Graphen
// =============================================================================
struct MetricsHistoryPoint {
    std::chrono::steady_clock::time_point timestamp;

    // Memory
    size_t memoryActiveBytes = 0;
    size_t memoryPeakBytes = 0;
    size_t memoryAllocationCount = 0;
    float memoryFragmentation = 0.0f;

    // Network
    uint64_t netBytesSent = 0;
    uint64_t netBytesReceived = 0;
    float netAvgRttMs = 0.0f;
    float netPacketLoss = 0.0f;
    float netQualityScore = 1.0f;
    uint32_t netActiveConnections = 0;

    // ThreadPool
    size_t tpPendingTasks = 0;
    uint64_t tpExecutedTasks = 0;
    float tpAvgTaskTimeUs = 0.0f;
    uint64_t tpMaxTaskTimeUs = 0;

    // ECS
    size_t ecsEntityCount = 0;
    size_t ecsArchetypeCount = 0;
    size_t ecsChunkCount = 0;
    float ecsUtilization = 0.0f;

    // Renderer
    float fps = 0.0f;
    float frameTimeMs = 0.0f;
    uint32_t drawCalls = 0;
};

// =============================================================================
// ALERT-KONFIGURATION — Schwellenwerte
// =============================================================================
struct AlertConfig {
    // Memory
    size_t memoryLeakThresholdBytes = 10 * 1024 * 1024;       // 10 MB
    float memoryFragmentationThreshold = 0.5f;                // 50%
    float memoryLeakAgeSeconds = 300.0f;                       // 5 Minuten

    // Network
    float networkRttThresholdMs = 200.0f;                    // 200ms
    float networkPacketLossThreshold = 0.05f;                // 5%
    float networkQualityThreshold = 0.5f;                      // 50%
    uint32_t networkMaxConnections = 1000;                     // 1000 Clients

    // ThreadPool
    size_t threadPoolQueueThreshold = 1000;                    // 1000 Tasks
    float threadPoolMaxTaskTimeMs = 100.0f;                    // 100ms

    // ECS
    float ecsUtilizationThreshold = 0.3f;                      // 30%
    size_t ecsMaxEntities = 100000;                            // 100k Entities

    // Renderer
    float fpsThreshold = 30.0f;                                // 30 FPS
    float frameTimeThresholdMs = 33.3f;                        // 33.3ms
};

// =============================================================================
// METRICS DASHBOARD — Singleton
// =============================================================================
class MetricsDashboard {
public:
    static MetricsDashboard& GetInstance();

    MetricsDashboard(const MetricsDashboard&) = delete;
    MetricsDashboard& operator=(const MetricsDashboard&) = delete;

    // ===================================================================
    // Lifecycle
    // ===================================================================
    void Initialize();
    void Shutdown();
    [[nodiscard]] bool IsInitialized() const { return initialized.load(); }

    // ===================================================================
    // Update (wird jeden Frame aufgerufen)
    // ===================================================================
    void Update(float deltaTime, float fps, uint32_t drawCalls);

    // ===================================================================
    // ImGui-Rendering
    // ===================================================================
    void Render();
    void RenderWindow(bool* pOpen);

    // ===================================================================
    // Konfiguration
    // ===================================================================
    void SetAlertConfig(const AlertConfig& config) { alertConfig = config; }
    [[nodiscard]] const AlertConfig& GetAlertConfig() const { return alertConfig; }
    void SetHistoryLimit(size_t limit) { maxHistoryPoints = limit; }
    void SetUpdateInterval(float seconds) { updateIntervalSeconds = seconds; }
    void SetActiveCategory(MetricCategory cat) { activeCategory = cat; }

    // ===================================================================
    // Alerts
    // ===================================================================
    [[nodiscard]] std::vector<Alert> GetAlerts() const;
    [[nodiscard]] std::vector<Alert> GetUnacknowledgedAlerts() const;
    void AcknowledgeAlert(size_t index);
    void AcknowledgeAllAlerts();
    void ClearAlerts();
    [[nodiscard]] size_t GetAlertCount() const;
    [[nodiscard]] size_t GetCriticalAlertCount() const;

    // ===================================================================
    // Historische Daten
    // ===================================================================
    [[nodiscard]] std::vector<MetricsHistoryPoint> GetHistory() const;
    void ClearHistory();

    // ===================================================================
    // Export
    // ===================================================================
    [[nodiscard]] std::string GenerateReport() const;
    void PrintReport() const;

    // ===================================================================
    // Performance: Direkte Metrik-Abfragen (fuer schnellen Zugriff)
    // ===================================================================
    [[nodiscard]] float GetCurrentFps() const { return currentFps.load(); }
    [[nodiscard]] float GetCurrentFrameTimeMs() const { return currentFrameTimeMs.load(); }
    [[nodiscard]] size_t GetCurrentMemoryBytes() const { return currentMemoryBytes.load(); }
    [[nodiscard]] float GetCurrentNetworkQuality() const { return currentNetworkQuality.load(); }

private:
    MetricsDashboard() = default;
    ~MetricsDashboard() = default;

    std::atomic<bool> initialized{false};

    // Aktive Kategorie (fuer Tab-Auswahl)
    std::atomic<MetricCategory> activeCategory{MetricCategory::Overview};

    // Update-Timer
    float updateIntervalSeconds = 1.0f;
    float updateAccumulator = 0.0f;

    // Aktuelle Werte (atomar fuer schnellen Zugriff)
    std::atomic<float> currentFps{0.0f};
    std::atomic<float> currentFrameTimeMs{0.0f};
    std::atomic<uint32_t> currentDrawCalls{0};
    std::atomic<size_t> currentMemoryBytes{0};
    std::atomic<float> currentNetworkQuality{1.0f};
    std::atomic<size_t> currentPendingTasks{0};

    // Historische Daten
    mutable std::mutex historyMutex;
    std::deque<MetricsHistoryPoint> history;
    size_t maxHistoryPoints = 300; // 5 Minuten bei 1s Intervall

    // Alerts
    mutable std::mutex alertsMutex;
    std::vector<Alert> alerts;
    size_t maxAlerts = 1000;

    // Konfiguration
    AlertConfig alertConfig;

    // ===================================================================
    // Interne Update-Methoden
    // ===================================================================
    void CollectMetrics(float fps, uint32_t drawCalls);
    void RecordHistoryPoint();
    void CheckAlerts();

    void AddAlert(AlertSeverity severity, MetricCategory category,
                  std::string_view message, std::string_view source,
                  float value, float threshold);

    // ===================================================================
    // Render-Hilfsmethoden
    // ===================================================================
    void RenderOverview();
    void RenderMemoryTab();
    void RenderNetworkTab();
    void RenderThreadPoolTab();
    void RenderECSTab();
    void RenderRendererTab();
    void RenderAlertsTab();

    // Graph-Rendering
    void RenderSparkline(const char* label, const std::vector<float>& values,
                         float minVal, float maxVal, uint32_t color);
    void RenderHistoryGraph(const char* label,
                            const std::vector<MetricsHistoryPoint>& hist,
                            size_t valueOffset, float scale);

    // Farben
    static uint32_t GetSeverityColor(AlertSeverity severity);
    static uint32_t GetQualityColor(float quality);
};

// =============================================================================
// MAKRO FUER KOMFORTABLES DASHBOARD-UPDATE
// =============================================================================
#define METRICS_UPDATE(deltaTime, fps, drawCalls) \
    metrics::MetricsDashboard::GetInstance().Update(deltaTime, fps, drawCalls)

#define METRICS_RENDER_WINDOW(pOpen) \
    metrics::MetricsDashboard::GetInstance().RenderWindow(pOpen)

} // namespace metrics
