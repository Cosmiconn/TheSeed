// =============================================================================
// tests/Test_MetricsDashboard.cpp — Metrics Dashboard Tests (AP-91)
// =============================================================================
// 10 Tests + 1 Benchmark fuer das MetricsDashboard.
// =============================================================================
#include "TestMain.h"
#include "../metrics/MetricsDashboard.h"
#include "../memory/MemoryProfiler.h"
#include "../network/NetworkProfiler.h"

// =============================================================================
// TEST 1: Grundlegende Initialisierung
// =============================================================================
TEST(MetricsDashboard_Initialization) {
    auto& dashboard = metrics::MetricsDashboard::GetInstance();
    dashboard.Initialize();

    TEST_ASSERT(dashboard.IsInitialized());
    TEST_ASSERT_EQ(dashboard.GetAlertCount(), 0u);
    TEST_ASSERT_EQ(dashboard.GetCriticalAlertCount(), 0u);

    dashboard.Shutdown();
    TEST_ASSERT(!dashboard.IsInitialized());
}

// =============================================================================
// TEST 2: Alert-Generierung (manuell)
// =============================================================================
TEST(MetricsDashboard_AlertGeneration) {
    auto& dashboard = metrics::MetricsDashboard::GetInstance();
    dashboard.Initialize();
    dashboard.ClearAlerts();

    // Simuliere niedrige FPS
    dashboard.Update(0.05f, 15.0f, 100); // 15 FPS, 50ms Frame-Zeit

    // Warte bis Update-Intervall (default 1s) — fuer Test direkt CheckAlerts aufrufen
    // Da Update nur alle 1s laeuft, rufen wir CheckAlerts direkt auf
    // (In echtem Code wuerde man warten)

    // Manuelle Alert-Pruefung durch wiederholtes Update
    for (int i = 0; i < 25; ++i) {
        dashboard.Update(0.05f, 15.0f, 100);
    }

    auto alerts = dashboard.GetAlerts();
    // Es sollte mindestens ein Alert fuer niedrige FPS geben
    TEST_ASSERT(!alerts.empty());

    dashboard.Shutdown();
}

// =============================================================================
// TEST 3: Alert-Acknowledgement
// =============================================================================
TEST(MetricsDashboard_AlertAcknowledgement) {
    auto& dashboard = metrics::MetricsDashboard::GetInstance();
    dashboard.Initialize();
    dashboard.ClearAlerts();

    // Manuelle Alert-Erzeugung durch wiederholtes Update mit niedriger FPS
    for (int i = 0; i < 25; ++i) {
        dashboard.Update(0.05f, 15.0f, 100);
    }

    auto unackBefore = dashboard.GetUnacknowledgedAlerts();
    TEST_ASSERT(!unackBefore.empty());

    dashboard.AcknowledgeAllAlerts();

    auto unackAfter = dashboard.GetUnacknowledgedAlerts();
    TEST_ASSERT(unackAfter.empty());

    dashboard.Shutdown();
}

// =============================================================================
// TEST 4: Historische Daten
// =============================================================================
TEST(MetricsDashboard_History) {
    auto& dashboard = metrics::MetricsDashboard::GetInstance();
    dashboard.Initialize();
    dashboard.ClearHistory();

    // Mehrere Updates fuer Historie
    for (int i = 0; i < 10; ++i) {
        dashboard.Update(0.1f, 60.0f, 100);
    }

    auto history = dashboard.GetHistory();
    TEST_ASSERT(!history.empty());

    dashboard.ClearHistory();
    history = dashboard.GetHistory();
    TEST_ASSERT(history.empty());

    dashboard.Shutdown();
}

// =============================================================================
// TEST 5: Konfiguration
// =============================================================================
TEST(MetricsDashboard_Configuration) {
    auto& dashboard = metrics::MetricsDashboard::GetInstance();
    dashboard.Initialize();

    metrics::AlertConfig config;
    config.fpsThreshold = 45.0f;
    config.memoryLeakThresholdBytes = 5 * 1024 * 1024; // 5 MB
    config.networkRttThresholdMs = 150.0f;

    dashboard.SetAlertConfig(config);

    auto retrieved = dashboard.GetAlertConfig();
    TEST_ASSERT_EQ(retrieved.fpsThreshold, 45.0f);
    TEST_ASSERT_EQ(retrieved.memoryLeakThresholdBytes, 5u * 1024u * 1024u);
    TEST_ASSERT_EQ(retrieved.networkRttThresholdMs, 150.0f);

    dashboard.Shutdown();
}

// =============================================================================
// TEST 6: Aktuelle Werte
// =============================================================================
TEST(MetricsDashboard_CurrentValues) {
    auto& dashboard = metrics::MetricsDashboard::GetInstance();
    dashboard.Initialize();

    dashboard.Update(0.016f, 60.0f, 200);

    TEST_ASSERT(dashboard.GetCurrentFps() > 0.0f);
    TEST_ASSERT(dashboard.GetCurrentFrameTimeMs() > 0.0f);

    dashboard.Shutdown();
}

// =============================================================================
// TEST 7: Berichtserstellung
// =============================================================================
TEST(MetricsDashboard_ReportGeneration) {
    auto& dashboard = metrics::MetricsDashboard::GetInstance();
    dashboard.Initialize();

    std::string report = dashboard.GenerateReport();
    TEST_ASSERT(!report.empty());
    TEST_ASSERT(report.find("METRICS DASHBOARD BERICHT") != std::string::npos);

    dashboard.Shutdown();
}

// =============================================================================
// TEST 8: Alert-Deduplizierung
// =============================================================================
TEST(MetricsDashboard_AlertDeduplication) {
    auto& dashboard = metrics::MetricsDashboard::GetInstance();
    dashboard.Initialize();
    dashboard.ClearAlerts();

    // Gleichen Alert mehrmals ausloesen
    for (int i = 0; i < 50; ++i) {
        dashboard.Update(0.05f, 15.0f, 100);
    }

    auto alerts = dashboard.GetAlerts();
    // Es sollte nicht 50 Alerts geben, sondern dedupliziert
    TEST_ASSERT(alerts.size() < 50u);

    dashboard.Shutdown();
}

// =============================================================================
// TEST 9: Kategorie-Wechsel
// =============================================================================
TEST(MetricsDashboard_CategorySwitch) {
    auto& dashboard = metrics::MetricsDashboard::GetInstance();
    dashboard.Initialize();

    dashboard.SetActiveCategory(metrics::MetricCategory::Memory);
    // Keine direkte Abfrage moeglich, aber kein Crash = Erfolg

    dashboard.SetActiveCategory(metrics::MetricCategory::Network);
    dashboard.SetActiveCategory(metrics::MetricCategory::Alerts);

    dashboard.Shutdown();
}

// =============================================================================
// TEST 10: Shutdown mit aktiven Alerts
// =============================================================================
TEST(MetricsDashboard_ShutdownWithAlerts) {
    auto& dashboard = metrics::MetricsDashboard::GetInstance();
    dashboard.Initialize();
    dashboard.ClearAlerts();

    for (int i = 0; i < 25; ++i) {
        dashboard.Update(0.05f, 15.0f, 100);
    }

    TEST_ASSERT(!dashboard.GetAlerts().empty());

    // Shutdown sollte sauber durchlaufen
    dashboard.Shutdown();
    TEST_ASSERT(!dashboard.IsInitialized());
}

// =============================================================================
// BENCHMARK: Massen-Update
// =============================================================================
BENCHMARK(MetricsDashboard_MassUpdate, 1000) {
    auto& dashboard = metrics::MetricsDashboard::GetInstance();
    dashboard.Initialize();
    dashboard.ClearHistory();
    dashboard.ClearAlerts();

    for (int i = 0; i < 1000; ++i) {
        float fps = 30.0f + static_cast<float>(i % 30);
        dashboard.Update(0.016f, fps, 100);
    }

    auto history = dashboard.GetHistory();
    TEST_ASSERT(!history.empty());

    dashboard.Shutdown();
}
