#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>

#include <seed/diagnostics.h>
#include <chrono>
#include <filesystem>

using namespace seed::diagnostics;
using namespace std::chrono;

// ============================================================================
// Gate P0-M0: Diagnostics, Crash Handling, Logging, Telemetry
// ============================================================================
// Diese Tests pruefen die M0-Kriterien G0.1 – G0.6
// ============================================================================

TEST_CASE("gate_p0_crash_handler") {
    auto& handler = CrashHandler::instance();
    handler.initialize("test_crashes_gate/");
    handler.setBuildInfo("1.0.0-gate", "RelWithDebInfo");

    auto report = handler.simulateCrash([](){});

    REQUIRE(report.stackTrace.size() >= 3);
    REQUIRE(!report.stackTrace.empty());

    bool hasSymbolizedFrame = false;
    for (const auto& frame : report.stackTrace) {
        if (!frame.functionName.empty() && frame.functionName != "???") {
            hasSymbolizedFrame = true;
        }
    }
    CHECK(hasSymbolizedFrame);

    handler.shutdown();
    std::filesystem::remove_all("test_crashes_gate/");
}

TEST_CASE("gate_p0_logging") {
    auto& logger = Logger::instance();
    LogConfig cfg;
    cfg.logDirectory = "test_logs_gate/";
    cfg.baseFilename = "seed";
    cfg.maxFileSizeMB = 100;
    cfg.maxFiles = 10;
    cfg.asyncMode = true;
    cfg.asyncQueueSize = 8192;
    logger.initialize(cfg);

    // 1M messages performance test
    auto start = steady_clock::now();
    for (int i = 0; i < 1'000'000; ++i) {
        logger.info("TEST", fmt::format("Message {}", i));
    }
    logger.flush();
    auto elapsed = steady_clock::now() - start;
    REQUIRE(elapsed < seconds(2));

    // Rotation test
    for (int i = 0; i < 100'000; ++i) {
        logger.debug("FILL", std::string(1000, 'X'));
    }
    logger.flush();

    logger.shutdown();
    std::filesystem::remove_all("test_logs_gate/");
}

TEST_CASE("gate_p0_telemetry") {
    auto& telemetry = ErrorTelemetryCollector::instance();
    telemetry.initialize("https://sentry.test.local");

    // Deduplication
    for (int i = 0; i < 100; ++i) {
        telemetry.reportError("test", "Duplicate error message", {});
    }
    auto topErrors = telemetry.getTopErrors(10);
    REQUIRE(topErrors.size() == 1);
    REQUIRE(topErrors[0].occurrenceCount == 100);

    // Top-100
    for (int i = 0; i < 150; ++i) {
        telemetry.reportError("test", fmt::format("Unique error {}", i), {});
    }
    topErrors = telemetry.getTopErrors(200);
    REQUIRE(topErrors.size() == 101);

    // Mark resolved
    REQUIRE(telemetry.markResolved(topErrors[0].errorId, 100));
    auto updated = telemetry.getTopErrors(10);
    REQUIRE(updated[0].isResolved);

    telemetry.shutdown();
}

TEST_CASE("gate_p0_assert") {
    auto& telemetry = ErrorTelemetryCollector::instance();
    telemetry.initialize();

    // SEED_VERIFY should not abort
    SEED_VERIFY(1 == 1, "This should pass");
    SEED_VERIFY(1 == 2, "This should log error");

    AssertContext ctx;
    ctx.expression = "1 == 2";
    ctx.file = "test.cpp";
    ctx.line = 42;
    ctx.function = "test_func";
    ctx.message = "Assertion failed";

    telemetry.reportAssert(ctx);
    auto reports = telemetry.getTopErrors(10);
    bool foundAssert = false;
    for (const auto& r : reports) {
        if (r.category == "assert") foundAssert = true;
    }
    REQUIRE(foundAssert);
    telemetry.shutdown();
}

TEST_CASE("gate_p0_diagnostics_console") {
    auto& console = DiagnosticsConsole::instance();
    REQUIRE(console.initialize());

    console.toggleVisibility();
    console.toggleVisibility();

    console.print("Test message", LogLevel::Info);
    console.print("Warning message", LogLevel::Warning);
    console.print("Error message", LogLevel::Error);

    DiagnosticsSnapshot snap;
    snap.timestamp = steady_clock::now().time_since_epoch().count();
    snap.cpuUsagePercent = 45.5f;
    snap.memoryUsedBytes = 1024 * 1024 * 512;
    snap.memoryAllocatedBytes = 1024 * 1024 * 1024;
    snap.threadCount = 8;
    snap.activeEntityCount = 50000;
    snap.avgFrameTimeMs = 16.0f;
    snap.counters["draw_calls"] = 1000;

    console.updateMetrics(snap);
    console.executeCommand("help");
    console.executeCommand("metrics");

    console.shutdown();
}

TEST_CASE("gate_p0_crash_signal_safe") {
    auto& handler = CrashHandler::instance();
    handler.setBuildInfo("1.0.0-test", "RelWithDebInfo");

    REQUIRE(handler.registerSignalHandlers());

    auto frames = handler.captureStackTrace(64);
    REQUIRE(frames.size() >= 3);

    handler.initialize("test_crashes_safe/");
    REQUIRE(handler.writeMinidump("test_crashes_safe/test.dmp"));
    REQUIRE(std::filesystem::exists("test_crashes_safe/test.dmp"));

    handler.unregisterSignalHandlers();
    handler.shutdown();
    std::filesystem::remove_all("test_crashes_safe/");
}
