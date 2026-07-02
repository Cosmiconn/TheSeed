// =============================================================================
// tests/Test_NetworkProfiler.cpp — Network Profiler Tests (AP-81)
// =============================================================================
// 10 Tests + 1 Benchmark fuer den NetworkProfiler.
// Analog zu Test_MemoryProfiler.cpp (AP-80).
// =============================================================================
#include "TestMain.h"
#include "../network/NetworkProfiler.h"

// =============================================================================
// TEST 1: Grundlegendes Verbindungs-Tracking
// =============================================================================
TEST(NetworkProfiler_BasicConnection) {
    auto& profiler = net::NetworkProfiler::GetInstance();
    profiler.Initialize();
    profiler.ClearHistory();

    profiler.RegisterConnection(1, "127.0.0.1", 54001);

    auto metrics = profiler.GetConnectionMetrics(1);
    TEST_ASSERT_EQ(metrics.clientId, 1u);
    TEST_ASSERT_EQ(metrics.clientAddress, "127.0.0.1");
    TEST_ASSERT_EQ(metrics.clientPort, 54001u);

    profiler.UnregisterConnection(1);
    auto empty = profiler.GetConnectionMetrics(1);
    TEST_ASSERT_EQ(empty.clientId, 0u);

    profiler.Shutdown();
}

// =============================================================================
// TEST 2: Paket-Tracking (Sent/Received)
// =============================================================================
TEST(NetworkProfiler_PacketTracking) {
    auto& profiler = net::NetworkProfiler::GetInstance();
    profiler.Initialize();
    profiler.ClearHistory();

    profiler.RegisterConnection(2, "192.168.1.1", 54002);

    profiler.TrackPacketSent(2, 1500);
    profiler.TrackPacketSent(2, 500);
    profiler.TrackPacketReceived(2, 200);
    profiler.TrackPacketReceived(2, 300);

    auto metrics = profiler.GetConnectionMetrics(2);
    TEST_ASSERT_EQ(metrics.packetsSent, 2u);
    TEST_ASSERT_EQ(metrics.packetsReceived, 2u);
    TEST_ASSERT_EQ(metrics.bytesSent, 2000u);
    TEST_ASSERT_EQ(metrics.bytesReceived, 500u);

    auto stats = profiler.GetGlobalStats();
    TEST_ASSERT_EQ(stats.totalPacketsSent, 2u);
    TEST_ASSERT_EQ(stats.totalPacketsReceived, 2u);
    TEST_ASSERT_EQ(stats.totalBytesSent, 2000u);
    TEST_ASSERT_EQ(stats.totalBytesReceived, 500u);

    profiler.Shutdown();
}

// =============================================================================
// TEST 3: RTT-Tracking und Jitter-Berechnung
// =============================================================================
TEST(NetworkProfiler_RttTracking) {
    auto& profiler = net::NetworkProfiler::GetInstance();
    profiler.Initialize();
    profiler.ClearHistory();

    profiler.RegisterConnection(3, "10.0.0.1", 54003);

    // Simuliere RTT-Messungen
    profiler.TrackRtt(3, 50.0f);
    profiler.TrackRtt(3, 55.0f);
    profiler.TrackRtt(3, 48.0f);
    profiler.TrackRtt(3, 52.0f);
    profiler.TrackRtt(3, 100.0f); // Ausreisser

    auto metrics = profiler.GetConnectionMetrics(3);
    TEST_ASSERT(metrics.avgRttMs > 0.0f);
    TEST_ASSERT(metrics.maxRttMs >= 100.0f);
    TEST_ASSERT(metrics.minRttMs <= 48.0f);
    TEST_ASSERT(metrics.jitterMs >= 0.0f);

    profiler.Shutdown();
}

// =============================================================================
// TEST 4: Retransmission-Tracking
// =============================================================================
TEST(NetworkProfiler_RetransmissionTracking) {
    auto& profiler = net::NetworkProfiler::GetInstance();
    profiler.Initialize();
    profiler.ClearHistory();

    profiler.RegisterConnection(4, "10.0.0.2", 54004);

    profiler.TrackRetransmission(4);
    profiler.TrackRetransmission(4);
    profiler.TrackRetransmission(4);

    auto metrics = profiler.GetConnectionMetrics(4);
    TEST_ASSERT_EQ(metrics.packetsRetransmitted, 3u);

    auto stats = profiler.GetGlobalStats();
    TEST_ASSERT_EQ(stats.totalRetransmissions, 3u);

    profiler.Shutdown();
}

// =============================================================================
// TEST 5: Fragment-Tracking
// =============================================================================
TEST(NetworkProfiler_FragmentTracking) {
    auto& profiler = net::NetworkProfiler::GetInstance();
    profiler.Initialize();
    profiler.ClearHistory();

    profiler.RegisterConnection(5, "10.0.0.3", 54005);

    profiler.TrackFragmentation(5, 4);
    profiler.TrackFragmentation(5, 2);

    profiler.TrackFragmentReceived(5);
    profiler.TrackFragmentReceived(5);
    profiler.TrackFragmentReassembled(5);

    auto metrics = profiler.GetConnectionMetrics(5);
    TEST_ASSERT_EQ(metrics.packetsFragmented, 2u);
    TEST_ASSERT_EQ(metrics.fragmentsReceived, 2u);
    TEST_ASSERT_EQ(metrics.fragmentsReassembled, 1u);

    auto stats = profiler.GetGlobalStats();
    TEST_ASSERT_EQ(stats.totalFragments, 6u); // 4 + 2

    profiler.Shutdown();
}

// =============================================================================
// TEST 6: Snapshot-Tracking
// =============================================================================
TEST(NetworkProfiler_SnapshotTracking) {
    auto& profiler = net::NetworkProfiler::GetInstance();
    profiler.Initialize();
    profiler.ClearHistory();

    profiler.RegisterConnection(6, "10.0.0.4", 54006);

    profiler.TrackSnapshotSent(6, 1500, true);   // Delta-komprimiert
    profiler.TrackSnapshotSent(6, 2000, true);   // Delta-komprimiert
    profiler.TrackSnapshotSent(6, 5000, false);  // Full-State

    profiler.TrackSnapshotBytesSaved(6, 1000);
    profiler.TrackSnapshotBytesSaved(6, 1500);

    auto metrics = profiler.GetConnectionMetrics(6);
    TEST_ASSERT_EQ(metrics.snapshotsSent, 3u);
    TEST_ASSERT_EQ(metrics.snapshotsDeltaCompressed, 2u);
    TEST_ASSERT_EQ(metrics.snapshotsFullState, 1u);
    TEST_ASSERT_EQ(metrics.snapshotBytesSaved, 2500u);

    profiler.Shutdown();
}

// =============================================================================
// TEST 7: Verbindungsqualitaet
// =============================================================================
TEST(NetworkProfiler_QualityScore) {
    auto& profiler = net::NetworkProfiler::GetInstance();
    profiler.Initialize();
    profiler.ClearHistory();

    profiler.RegisterConnection(7, "10.0.0.5", 54007);

    // Gute Verbindung: niedrige RTT, kein Verlust
    for (int i = 0; i < 10; ++i) {
        profiler.TrackRtt(7, 20.0f + static_cast<float>(i));
        profiler.TrackPacketSent(7, 100);
        profiler.TrackPacketAcked(7, 100);
    }

    auto metrics = profiler.GetConnectionMetrics(7);
    TEST_ASSERT(metrics.qualityScore > 0.8f); // Gute Qualitaet erwartet

    // Schlechte Verbindung: hohe RTT, Paketverlust
    profiler.RegisterConnection(8, "10.0.0.6", 54008);
    for (int i = 0; i < 10; ++i) {
        profiler.TrackRtt(8, 400.0f);
        profiler.TrackPacketSent(8, 100);
        if (i % 2 == 0) {
            profiler.TrackPacketDropped(8, 100);
        }
    }

    auto metricsBad = profiler.GetConnectionMetrics(8);
    TEST_ASSERT(metricsBad.qualityScore < 0.5f); // Schlechte Qualitaet erwartet

    auto poor = profiler.GetPoorQualityConnections();
    TEST_ASSERT(!poor.empty());
    TEST_ASSERT_EQ(poor[0], 8u);

    profiler.Shutdown();
}

// =============================================================================
// TEST 8: Historische Daten
// =============================================================================
TEST(NetworkProfiler_History) {
    auto& profiler = net::NetworkProfiler::GetInstance();
    profiler.Initialize();
    profiler.ClearHistory();

    profiler.RegisterConnection(9, "10.0.0.7", 54009);

    for (int i = 0; i < 5; ++i) {
        profiler.TrackPacketSent(9, 100);
        profiler.RecordSnapshot();
    }

    auto history = profiler.GetHistory();
    TEST_ASSERT_EQ(history.size(), 5u);

    profiler.ClearHistory();
    history = profiler.GetHistory();
    TEST_ASSERT(history.empty());

    profiler.Shutdown();
}

// =============================================================================
// TEST 9: Globale Statistiken
// =============================================================================
TEST(NetworkProfiler_GlobalStats) {
    auto& profiler = net::NetworkProfiler::GetInstance();
    profiler.Initialize();
    profiler.ClearHistory();

    profiler.RegisterConnection(10, "10.0.0.8", 54010);
    profiler.RegisterConnection(11, "10.0.0.9", 54011);

    profiler.TrackPacketSent(10, 1000);
    profiler.TrackPacketSent(11, 2000);
    profiler.TrackPacketReceived(10, 500);
    profiler.TrackPacketReceived(11, 1000);

    auto stats = profiler.GetGlobalStats();
    TEST_ASSERT_EQ(stats.activeConnections, 2u);
    TEST_ASSERT_EQ(stats.totalConnections, 2u);
    TEST_ASSERT_EQ(stats.totalPacketsSent, 2u);
    TEST_ASSERT_EQ(stats.totalPacketsReceived, 2u);
    TEST_ASSERT_EQ(stats.totalBytesSent, 3000u);
    TEST_ASSERT_EQ(stats.totalBytesReceived, 1500u);

    profiler.Shutdown();
}

// =============================================================================
// TEST 10: Berichtserstellung
// =============================================================================
TEST(NetworkProfiler_ReportGeneration) {
    auto& profiler = net::NetworkProfiler::GetInstance();
    profiler.Initialize();
    profiler.ClearHistory();

    profiler.RegisterConnection(12, "10.0.0.10", 54012);
    profiler.TrackPacketSent(12, 1000);
    profiler.TrackPacketReceived(12, 500);
    profiler.TrackRtt(12, 30.0f);

    std::string report = profiler.GenerateReport();
    TEST_ASSERT(!report.empty());
    TEST_ASSERT(report.find("NETWORK PROFILER BERICHT") != std::string::npos);
    TEST_ASSERT(report.find("Client 12") != std::string::npos);

    profiler.Shutdown();
}

// =============================================================================
// BENCHMARK: Massen-Paket-Tracking
// =============================================================================
BENCHMARK(NetworkProfiler_MassTracking, 1000) {
    auto& profiler = net::NetworkProfiler::GetInstance();
    profiler.Initialize();

    profiler.RegisterConnection(99, "127.0.0.1", 54999);

    for (int i = 0; i < 1000; ++i) {
        profiler.TrackPacketSent(99, 1400);
        profiler.TrackPacketReceived(99, 200);
        if (i % 100 == 0) {
            profiler.TrackRtt(99, 50.0f);
        }
    }

    auto metrics = profiler.GetConnectionMetrics(99);
    TEST_ASSERT_EQ(metrics.packetsSent, 1000u);
    TEST_ASSERT_EQ(metrics.packetsReceived, 1000u);

    profiler.Shutdown();
}
