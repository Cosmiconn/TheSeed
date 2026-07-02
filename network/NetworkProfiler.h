#pragma once
// =============================================================================
// network/NetworkProfiler.h — Network Profiler (AP-81)
// =============================================================================
// Echtzeit-Tracking von Netzwerk-Metriken fuer alle Verbindungen.
// Erfasst: RTT, Paketverlust, Jitter, Bandbreite, Paket-Statistiken,
// Verbindungsqualitaet, historische Daten. Thread-sicher, minimaler Overhead.
//
// Analog zum MemoryProfiler (AP-80) — Singleton-Pattern, atomare Counter,
// Mutex-geschuetzte detaillierte Daten.
// =============================================================================
#include <cstdint>
#include <string>
#include <vector>
#include <unordered_map>
#include <mutex>
#include <atomic>
#include <chrono>
#include <deque>
#include <span>

namespace net {

// =============================================================================
// VERBINDUNGS-METRIKEN — Pro Client-Verbindung
// =============================================================================
struct ConnectionMetrics {
    uint32_t clientId = 0;
    std::string clientAddress;
    uint16_t clientPort = 0;

    // RTT-Statistiken (in Millisekunden)
    float minRttMs = 0.0f;
    float maxRttMs = 0.0f;
    float avgRttMs = 0.0f;
    float lastRttMs = 0.0f;
    float jitterMs = 0.0f;          // Standardabweichung der RTT

    // Paket-Statistiken
    uint64_t packetsSent = 0;
    uint64_t packetsReceived = 0;
    uint64_t packetsAcked = 0;
    uint64_t packetsRetransmitted = 0;
    uint64_t packetsDropped = 0;
    uint64_t packetsFragmented = 0;
    uint64_t fragmentsReceived = 0;
    uint64_t fragmentsReassembled = 0;

    // Byte-Statistiken
    uint64_t bytesSent = 0;
    uint64_t bytesReceived = 0;
    uint64_t bytesAcked = 0;

    // Verbindungsqualitaet (0.0 - 1.0)
    float qualityScore = 1.0f;

    // Verbindungs-Lebensdauer
    std::chrono::steady_clock::time_point connectedTime;
    std::chrono::steady_clock::time_point lastActivity;

    // Snapshot-Statistiken
    uint64_t snapshotsSent = 0;
    uint64_t snapshotsDeltaCompressed = 0;
    uint64_t snapshotsFullState = 0;
    uint64_t snapshotBytesSaved = 0;     // Durch Delta-Kompression gespart

    // Berechnet Paketverlustrate (0.0 - 1.0)
    [[nodiscard]] float GetPacketLossRate() const;

    // Berechnet Bandbreite in Bytes/Sekunde (gesendet)
    [[nodiscard]] float GetSendBandwidthBps() const;

    // Berechnet Bandbreite in Bytes/Sekunde (empfangen)
    [[nodiscard]] float GetReceiveBandwidthBps() const;

    // Aktualisiert die Verbindungsqualitaet
    void UpdateQualityScore();
};

// =============================================================================
// GLOBALE NETZWERK-STATISTIKEN
// =============================================================================
struct GlobalNetworkStats {
    uint64_t totalConnections = 0;
    uint64_t activeConnections = 0;
    uint64_t totalPacketsSent = 0;
    uint64_t totalPacketsReceived = 0;
    uint64_t totalBytesSent = 0;
    uint64_t totalBytesReceived = 0;
    uint64_t totalRetransmissions = 0;
    uint64_t totalFragments = 0;

    float globalAvgRttMs = 0.0f;
    float globalPacketLossRate = 0.0f;
    float globalQualityScore = 1.0f;
};

// =============================================================================
// HISTORISCHER DATENPUNKT — Fuer Zeitverlauf
// =============================================================================
struct NetworkHistoryPoint {
    std::chrono::steady_clock::time_point timestamp;
    uint64_t totalBytesSent = 0;
    uint64_t totalBytesReceived = 0;
    uint64_t totalPacketsSent = 0;
    uint64_t totalPacketsReceived = 0;
    float avgRttMs = 0.0f;
    float packetLossRate = 0.0f;
    uint32_t activeConnections = 0;
};

// =============================================================================
// NETWORK PROFILER — Singleton
// =============================================================================
class NetworkProfiler {
public:
    static NetworkProfiler& GetInstance();

    NetworkProfiler(const NetworkProfiler&) = delete;
    NetworkProfiler& operator=(const NetworkProfiler&) = delete;

    // ===================================================================
    // Lifecycle
    // ===================================================================
    void Initialize();
    void Shutdown();
    [[nodiscard]] bool IsInitialized() const { return initialized.load(); }

    // ===================================================================
    // Verbindungs-Management
    // ===================================================================
    void RegisterConnection(uint32_t clientId, std::string_view address, uint16_t port);
    void UnregisterConnection(uint32_t clientId);
    void UpdateConnectionActivity(uint32_t clientId);

    // ===================================================================
    // Paket-Tracking (wird von NetworkServer aufgerufen)
    // ===================================================================
    void TrackPacketSent(uint32_t clientId, size_t bytes);
    void TrackPacketReceived(uint32_t clientId, size_t bytes);
    void TrackPacketAcked(uint32_t clientId, size_t bytes);
    void TrackPacketDropped(uint32_t clientId, size_t bytes);
    void TrackRetransmission(uint32_t clientId);
    void TrackFragmentation(uint32_t clientId, uint16_t fragmentCount);
    void TrackFragmentReceived(uint32_t clientId);
    void TrackFragmentReassembled(uint32_t clientId);

    // ===================================================================
    // RTT-Tracking
    // ===================================================================
    void TrackRtt(uint32_t clientId, float rttMs);

    // ===================================================================
    // Snapshot-Tracking
    // ===================================================================
    void TrackSnapshotSent(uint32_t clientId, size_t bytes, bool deltaCompressed);
    void TrackSnapshotFullState(uint32_t clientId, size_t bytes);
    void TrackSnapshotBytesSaved(uint32_t clientId, size_t savedBytes);

    // ===================================================================
    // Verbindungs-Metriken abfragen
    // ===================================================================
    [[nodiscard]] ConnectionMetrics GetConnectionMetrics(uint32_t clientId) const;
    [[nodiscard]] std::vector<ConnectionMetrics> GetAllConnectionMetrics() const;

    // ===================================================================
    // Globale Statistiken
    // ===================================================================
    [[nodiscard]] GlobalNetworkStats GetGlobalStats() const;

    // ===================================================================
    // Historische Daten
    // ===================================================================
    void RecordSnapshot();
    [[nodiscard]] std::vector<NetworkHistoryPoint> GetHistory() const;
    void ClearHistory();

    // ===================================================================
    // Berichtserstellung
    // ===================================================================
    [[nodiscard]] std::string GenerateReport() const;
    void PrintReport() const;

    // ===================================================================
    // Konfiguration
    // ===================================================================
    void SetHistoryLimit(size_t limit) { maxHistoryPoints = limit; }
    void SetRttWindowSize(size_t size) { rttWindowSize = size; }

    // ===================================================================
    // Verbindungsqualitaet
    // ===================================================================
    [[nodiscard]] float GetConnectionQuality(uint32_t clientId) const;
    [[nodiscard]] std::vector<uint32_t> GetPoorQualityConnections() const;

private:
    NetworkProfiler() = default;
    ~NetworkProfiler() = default;

    std::atomic<bool> initialized{false};

    mutable std::mutex connectionsMutex;
    std::unordered_map<uint32_t, ConnectionMetrics> connections;

    // RTT-Fenster fuer Jitter-Berechnung
    mutable std::mutex rttWindowsMutex;
    std::unordered_map<uint32_t, std::deque<float>> rttWindows;
    size_t rttWindowSize = 100;

    // Globale Statistiken (atomar)
    std::atomic<uint64_t> totalConnections{0};
    std::atomic<uint64_t> totalPacketsSent{0};
    std::atomic<uint64_t> totalPacketsReceived{0};
    std::atomic<uint64_t> totalBytesSent{0};
    std::atomic<uint64_t> totalBytesReceived{0};
    std::atomic<uint64_t> totalRetransmissions{0};
    std::atomic<uint64_t> totalFragments{0};

    // Historische Daten
    mutable std::mutex historyMutex;
    std::vector<NetworkHistoryPoint> history;
    size_t maxHistoryPoints = 1000;

    // Berechnet Jitter aus RTT-Fenster
    [[nodiscard]] float CalculateJitter(const std::deque<float>& rttWindow) const;

    // Aktualisiert globale Statistiken
    void UpdateGlobalStats();
};

// =============================================================================
// RAII-Connection-Tracker — Automatisches Tracking im Scope
// =============================================================================
class ScopedConnectionTracker {
    uint32_t clientId = 0;
public:
    ScopedConnectionTracker(uint32_t id, std::string_view address, uint16_t port);
    ~ScopedConnectionTracker();
};

// =============================================================================
// MAKROS FUER KOMFORTABLES TRACKING
// =============================================================================
#define NETPROFILE_TRACK_SENT(clientId, bytes) \
    net::NetworkProfiler::GetInstance().TrackPacketSent(clientId, bytes)

#define NETPROFILE_TRACK_RECEIVED(clientId, bytes) \
    net::NetworkProfiler::GetInstance().TrackPacketReceived(clientId, bytes)

#define NETPROFILE_TRACK_ACKED(clientId, bytes) \
    net::NetworkProfiler::GetInstance().TrackPacketAcked(clientId, bytes)

#define NETPROFILE_TRACK_RTT(clientId, rttMs) \
    net::NetworkProfiler::GetInstance().TrackRtt(clientId, rttMs)

#define NETPROFILE_TRACK_RETRANSMISSION(clientId) \
    net::NetworkProfiler::GetInstance().TrackRetransmission(clientId)

#define NETPROFILE_TRACK_FRAGMENTATION(clientId, count) \
    net::NetworkProfiler::GetInstance().TrackFragmentation(clientId, count)

#define NETPROFILE_RECORD_SNAPSHOT() \
    net::NetworkProfiler::GetInstance().RecordSnapshot()

} // namespace net
