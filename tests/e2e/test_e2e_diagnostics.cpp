#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>

#include <seed/diagnostics.h>
#include <filesystem>

using namespace seed::diagnostics;

// ============================================================================
// E2E TESTS (2) – Phase 0 Month 0
// ============================================================================

TEST_CASE("[e2e] FullCrashToReportPipeline") {
    auto& logger = Logger::instance();
    auto& handler = CrashHandler::instance();
    auto& telemetry = ErrorTelemetryCollector::instance();

    LogConfig cfg;
    cfg.logDirectory = "e2e_logs/";
    cfg.baseFilename = "e2e";
    cfg.asyncMode = false;
    logger.initialize(cfg);
    handler.initialize("e2e_crashes/");
    handler.setBuildInfo("1.0.0-e2e", "RelWithDebInfo");
    telemetry.initialize();

    logger.info("E2E", "System startup");
    logger.warning("E2E", "High memory usage detected");
    
    auto report = handler.simulateCrash([](){});
    telemetry.reportCrash(report);

    CHECK(!report.stackTrace.empty());
    CHECK(telemetry.uniqueErrorCount() >= 1);

    logger.shutdown();
    handler.shutdown();
    telemetry.shutdown();
    std::filesystem::remove_all("e2e_logs/");
    std::filesystem::remove_all("e2e_crashes/");
}

TEST_CASE("[e2e] AssertToTelemetryToConsole") {
    auto& telemetry = ErrorTelemetryCollector::instance();
    auto& console = DiagnosticsConsole::instance();
    auto& logger = Logger::instance();
    
    LogConfig cfg;
    cfg.logDirectory = "e2e_logs/";
    cfg.baseFilename = "e2e_assert";
    cfg.consoleLevel = 6;
    cfg.fileLevel = 6;
    cfg.asyncMode = false;
    logger.initialize(cfg);
    telemetry.initialize();
    console.initialize();

    SEED_VERIFY(1 == 2, "E2E verification failure");
    
    console.print("Error handled", LogLevel::Error);
    console.executeCommand("metrics");

    auto errors = telemetry.getTopErrors(10);
    CHECK(!errors.empty());

    console.shutdown();
    telemetry.shutdown();
    logger.shutdown();
    std::filesystem::remove_all("e2e_logs/");
}
