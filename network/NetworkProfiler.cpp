// =============================================================================
// network/NetworkProfiler.cpp — Network Profiler Implementation (AP-81)
// =============================================================================
// Thread-sicheres Netzwerk-Tracking mit minimaler Overhead.
// Atomare Counter fuer globale Statistiken, Mutexe fuer detaillierte Daten.
// =============================================================================
#include "NetworkProfiler.h"
#include "../core/Log.h"

#include <sstream>
#include <iomanip>
#include <cmath>
#include <algorithm>

namespace net {

// =============================================================================
// CONNECTION METRICS — Hilfsmethoden
// =============================================================================
float ConnectionMetrics::GetPacketLossRate() const {
    uint64_t total = packetsSent + packetsRetransmitted;
    if (total == 0) return 0.0f;
    return static_cast<float>(packetsDropped) / static_cast<float>(total);
}

float ConnectionMetrics::GetSendBandwidthBps() const {
    auto now = std::chrono::steady_clock::now();
    auto elapsedSec = std::chrono::duration<float>(now - connectedTime).count();
    if (elapsedSec <= 0.0f) return 0.0f;
    return static_cast<float>(bytesSent) / elapsedSec;
}

float ConnectionMetrics::GetReceiveBandwidthBps() const {
    auto now = std::chrono::steady_clock::now();
    auto elapsedSec = std::chrono::duration<float>(now - connectedTime).count();
    if (elapsedSec <= 0.0f) return 0.0f;
    return static_cast<float>(bytesReceived) / elapsedSec;
}

void ConnectionMetrics::UpdateQualityScore() {
    // Qualitaet basiert auf: RTT, Paketverlust, Jitter
    // Gewichtung: RTT 40%, Loss 40%, Jitter 20%
    float rttScore = 1.0f;
    if (avgRttMs > 0.0f) {
        rttScore = std::clamp(1.0f - (avgRttMs / 500.0f), 0.0f, 1.0f); // 500ms = 0%
    }

    float lossScore = 1.0f - GetPacketLossRate();
    lossScore = std::clamp(lossScore, 0.0f, 1.0f);

    float jitterScore = 1.0f;
    if (jitterMs > 0.0f) {
        jitterScore = std::clamp(1.0f - (jitterMs / 100.0f), 0.0f, 1.0f); // 100ms Jitter = 0%
    }

    qualityScore = (rttScore * 0.4f) + (lossScore * 0.4f) + (jitterScore * 0.2f);
    qualityScore = std::clamp(qualityScore, 0.0f, 1.0f);
}

// =============================================================================
// SINGLETON
// =============================================================================
NetworkProfiler& NetworkProfiler::GetInstance() {
    static NetworkProfiler instance;
    return instance;
}

// =============================================================================
// LIFECYCLE
// =============================================================================
void NetworkProfiler::Initialize() {
    if (initialized.exchange(true)) return;

    connections.clear();
    rttWindows.clear();
    history.clear();

    totalConnections.store(0);
    totalPacketsSent.store(0);
    totalPacketsReceived.store(0);
    totalBytesSent.store(0);
    totalBytesReceived.store(0);
    totalRetransmissions.store(0);
    totalFragments.store(0);

    AddLog("[NetworkProfiler] Initialisiert — Netzwerk-Tracking aktiv");
}

void NetworkProfiler::Shutdown() {
    if (!initialized.exchange(false)) return;

    // Verbindungs-Report ausgeben
    auto allMetrics = GetAllConnectionMetrics();
    if (!allMetrics.empty()) {
        AddLog("[NetworkProfiler] {} Verbindungen bei Shutdown:", allMetrics.size());
        for (const auto& m : allMetrics) {
            AddLog("  Client {}: {} pkts sent, {} pkts recv, {:.2f}ms avg RTT, {:.1f}% quality",
                     m.clientId, m.packetsSent, m.packetsReceived, m.avgRttMs, m.qualityScore * 100.0f);
        }
    }

    AddLog("[NetworkProfiler] Heruntergefahren — Total: {} pkts sent, {} pkts recv, {} retrans",
             totalPacketsSent.load(), totalPacketsReceived.load(), totalRetransmissions.load());
}

// =============================================================================
// VERBINDUNGS-MANAGEMENT
// =============================================================================
void NetworkProfiler::RegisterConnection(uint32_t clientId, std::string_view address, uint16_t port) {
    if (!initialized) return;

    std::lock_guard lock(connectionsMutex);
    auto& metrics = connections[clientId];
    metrics.clientId = clientId;
    metrics.clientAddress = std::string(address);
    metrics.clientPort = port;
    metrics.connectedTime = std::chrono::steady_clock::now();
    metrics.lastActivity = metrics.connectedTime;

    totalConnections.fetch_add(1);

    AddLog("[NetworkProfiler] Verbindung registriert: {}:{} (Client {})",
             address, port, clientId);
}

void NetworkProfiler::UnregisterConnection(uint32_t clientId) {
    if (!initialized) return;

    std::lock_guard lock(connectionsMutex);
    auto it = connections.find(clientId);
    if (it != connections.end()) {
        AddLog("[NetworkProfiler] Verbindung entfernt: {} ({} pkts sent, {} pkts recv)",
                 clientId, it->second.packetsSent, it->second.packetsReceived);
        connections.erase(it);
    }

    {
        std::lock_guard rttLock(rttWindowsMutex);
        rttWindows.erase(clientId);
    }
}

void NetworkProfiler::UpdateConnectionActivity(uint32_t clientId) {
    if (!initialized) return;

    std::lock_guard lock(connectionsMutex);
    auto it = connections.find(clientId);
    if (it != connections.end()) {
        it->second.lastActivity = std::chrono::steady_clock::now();
    }
}

// =============================================================================
// PAKET-TRACKING
// =============================================================================
void NetworkProfiler::TrackPacketSent(uint32_t clientId, size_t bytes) {
    if (!initialized) return;

    {
        std::lock_guard lock(connectionsMutex);
        auto it = connections.find(clientId);
        if (it != connections.end()) {
            it->second.packetsSent++;
            it->second.bytesSent += bytes;
            it->second.lastActivity = std::chrono::steady_clock::now();
        }
    }

    totalPacketsSent.fetch_add(1);
    totalBytesSent.fetch_add(bytes);
}

void NetworkProfiler::TrackPacketReceived(uint32_t clientId, size_t bytes) {
    if (!initialized) return;

    {
        std::lock_guard lock(connectionsMutex);
        auto it = connections.find(clientId);
        if (it != connections.end()) {
            it->second.packetsReceived++;
            it->second.bytesReceived += bytes;
            it->second.lastActivity = std::chrono::steady_clock::now();
        }
    }

    totalPacketsReceived.fetch_add(1);
    totalBytesReceived.fetch_add(bytes);
}

void NetworkProfiler::TrackPacketAcked(uint32_t clientId, size_t bytes) {
    if (!initialized) return;

    std::lock_guard lock(connectionsMutex);
    auto it = connections.find(clientId);
    if (it != connections.end()) {
        it->second.packetsAcked++;
        it->second.bytesAcked += bytes;
    }
}

void NetworkProfiler::TrackPacketDropped(uint32_t clientId, size_t bytes) {
    if (!initialized) return;

    std::lock_guard lock(connectionsMutex);
    auto it = connections.find(clientId);
    if (it != connections.end()) {
        it->second.packetsDropped++;
    }

    (void)bytes; // Bytes bei Drops nicht von Total abziehen (wurden bereits gezaehlt)
}

void NetworkProfiler::TrackRetransmission(uint32_t clientId) {
    if (!initialized) return;

    {
        std::lock_guard lock(connectionsMutex);
        auto it = connections.find(clientId);
        if (it != connections.end()) {
            it->second.packetsRetransmitted++;
        }
    }

    totalRetransmissions.fetch_add(1);
}

void NetworkProfiler::TrackFragmentation(uint32_t clientId, uint16_t fragmentCount) {
    if (!initialized) return;

    std::lock_guard lock(connectionsMutex);
    auto it = connections.find(clientId);
    if (it != connections.end()) {
        it->second.packetsFragmented++;
    }

    totalFragments.fetch_add(fragmentCount);
}

void NetworkProfiler::TrackFragmentReceived(uint32_t clientId) {
    if (!initialized) return;

    std::lock_guard lock(connectionsMutex);
    auto it = connections.find(clientId);
    if (it != connections.end()) {
        it->second.fragmentsReceived++;
    }
}

void NetworkProfiler::TrackFragmentReassembled(uint32_t clientId) {
    if (!initialized) return;

    std::lock_guard lock(connectionsMutex);
    auto it = connections.find(clientId);
    if (it != connections.end()) {
        it->second.fragmentsReassembled++;
    }
}

// =============================================================================
// RTT-TRACKING
// =============================================================================
void NetworkProfiler::TrackRtt(uint32_t clientId, float rttMs) {
    if (!initialized || rttMs <= 0.0f) return;

    std::lock_guard lock(connectionsMutex);
    auto it = connections.find(clientId);
    if (it == connections.end()) return;

    auto& metrics = it->second;
    metrics.lastRttMs = rttMs;

    // Min/Max aktualisieren
    if (metrics.minRttMs == 0.0f || rttMs < metrics.minRttMs) {
        metrics.minRttMs = rttMs;
    }
    if (rttMs > metrics.maxRttMs) {
        metrics.maxRttMs = rttMs;
    }

    // Gleitender Durchschnitt (EWMA, alpha=0.125)
    if (metrics.avgRttMs < 0.001f) {
        metrics.avgRttMs = rttMs;
    } else {
        metrics.avgRttMs = metrics.avgRttMs * 0.875f + rttMs * 0.125f;
    }

    // RTT-Fenster fuer Jitter-Berechnung
    {
        std::lock_guard rttLock(rttWindowsMutex);
        auto& window = rttWindows[clientId];
        window.push_back(rttMs);
        if (window.size() > rttWindowSize) {
            window.pop_front();
        }
        metrics.jitterMs = CalculateJitter(window);
    }

    // Qualitaet aktualisieren
    metrics.UpdateQualityScore();
}

float NetworkProfiler::CalculateJitter(const std::deque<float>& rttWindow) const {
    if (rttWindow.size() < 2) return 0.0f;

    // Berechne Standardabweichung der RTT-Werte
    float sum = 0.0f;
    for (float rtt : rttWindow) {
        sum += rtt;
    }
    float mean = sum / static_cast<float>(rttWindow.size());

    float variance = 0.0f;
    for (float rtt : rttWindow) {
        float diff = rtt - mean;
        variance += diff * diff;
    }
    variance /= static_cast<float>(rttWindow.size());

    return std::sqrt(variance);
}

// =============================================================================
// SNAPSHOT-TRACKING
// =============================================================================
void NetworkProfiler::TrackSnapshotSent(uint32_t clientId, size_t bytes, bool deltaCompressed) {
    if (!initialized) return;

    std::lock_guard lock(connectionsMutex);
    auto it = connections.find(clientId);
    if (it != connections.end()) {
        it->second.snapshotsSent++;
        if (deltaCompressed) {
            it->second.snapshotsDeltaCompressed++;
        }
    }
}

void NetworkProfiler::TrackSnapshotFullState(uint32_t clientId, size_t bytes) {
    if (!initialized) return;

    std::lock_guard lock(connectionsMutex);
    auto it = connections.find(clientId);
    if (it != connections.end()) {
        it->second.snapshotsFullState++;
    }

    (void)bytes;
}

void NetworkProfiler::TrackSnapshotBytesSaved(uint32_t clientId, size_t savedBytes) {
    if (!initialized) return;

    std::lock_guard lock(connectionsMutex);
    auto it = connections.find(clientId);
    if (it != connections.end()) {
        it->second.snapshotBytesSaved += savedBytes;
    }
}

// =============================================================================
// STATISTIKEN-ABFRAGEN
// =============================================================================
ConnectionMetrics NetworkProfiler::GetConnectionMetrics(uint32_t clientId) const {
    std::lock_guard lock(connectionsMutex);
    auto it = connections.find(clientId);
    if (it != connections.end()) {
        return it->second;
    }
    return ConnectionMetrics{};
}

std::vector<ConnectionMetrics> NetworkProfiler::GetAllConnectionMetrics() const {
    std::lock_guard lock(connectionsMutex);
    std::vector<ConnectionMetrics> result;
    result.reserve(connections.size());
    for (const auto& [id, metrics] : connections) {
        result.push_back(metrics);
    }
    return result;
}

GlobalNetworkStats NetworkProfiler::GetGlobalStats() const {
    GlobalNetworkStats stats;
    stats.totalConnections = totalConnections.load();
    stats.totalPacketsSent = totalPacketsSent.load();
    stats.totalPacketsReceived = totalPacketsReceived.load();
    stats.totalBytesSent = totalBytesSent.load();
    stats.totalBytesReceived = totalBytesReceived.load();
    stats.totalRetransmissions = totalRetransmissions.load();
    stats.totalFragments = totalFragments.load();

    // Berechne globale Durchschnittswerte
    std::lock_guard lock(connectionsMutex);
    stats.activeConnections = connections.size();

    if (!connections.empty()) {
        float totalRtt = 0.0f;
        float totalLoss = 0.0f;
        float totalQuality = 0.0f;
        for (const auto& [id, m] : connections) {
            totalRtt += m.avgRttMs;
            totalLoss += m.GetPacketLossRate();
            totalQuality += m.qualityScore;
        }
        stats.globalAvgRttMs = totalRtt / static_cast<float>(connections.size());
        stats.globalPacketLossRate = totalLoss / static_cast<float>(connections.size());
        stats.globalQualityScore = totalQuality / static_cast<float>(connections.size());
    }

    return stats;
}

// =============================================================================
// HISTORISCHE DATEN
// =============================================================================
void NetworkProfiler::RecordSnapshot() {
    if (!initialized) return;

    NetworkHistoryPoint point;
    point.timestamp = std::chrono::steady_clock::now();
    point.totalBytesSent = totalBytesSent.load();
    point.totalBytesReceived = totalBytesReceived.load();
    point.totalPacketsSent = totalPacketsSent.load();
    point.totalPacketsReceived = totalPacketsReceived.load();

    auto stats = GetGlobalStats();
    point.avgRttMs = stats.globalAvgRttMs;
    point.packetLossRate = stats.globalPacketLossRate;
    point.activeConnections = static_cast<uint32_t>(stats.activeConnections);

    std::lock_guard lock(historyMutex);
    history.push_back(point);

    if (history.size() > maxHistoryPoints) {
        history.erase(history.begin(), history.begin() + (history.size() - maxHistoryPoints));
    }
}

std::vector<NetworkHistoryPoint> NetworkProfiler::GetHistory() const {
    std::lock_guard lock(historyMutex);
    return history;
}

void NetworkProfiler::ClearHistory() {
    std::lock_guard lock(historyMutex);
    history.clear();
}

// =============================================================================
// VERBINDUNGSQUALITAET
// =============================================================================
float NetworkProfiler::GetConnectionQuality(uint32_t clientId) const {
    std::lock_guard lock(connectionsMutex);
    auto it = connections.find(clientId);
    if (it != connections.end()) {
        return it->second.qualityScore;
    }
    return 0.0f;
}

std::vector<uint32_t> NetworkProfiler::GetPoorQualityConnections() const {
    std::vector<uint32_t> result;
    std::lock_guard lock(connectionsMutex);
    for (const auto& [id, metrics] : connections) {
        if (metrics.qualityScore < 0.5f) {
            result.push_back(id);
        }
    }
    return result;
}

// =============================================================================
// BERICHTSERSTELLUNG
// =============================================================================
std::string NetworkProfiler::GenerateReport() const {
    std::ostringstream oss;
    auto now = std::chrono::steady_clock::now();
    auto stats = GetGlobalStats();

    oss << "================================================================================\n";
    oss << " NETWORK PROFILER BERICHT\n";
    oss << "================================================================================\n";
    oss << "Zeitpunkt: " << std::chrono::duration<float>(now.time_since_epoch()).count() << "s\n";
    oss << "--------------------------------------------------------------------------------\n";
    oss << " GLOBALE STATISTIKEN\n";
    oss << "--------------------------------------------------------------------------------\n";
    oss << " Aktive Verbindungen:   " << std::setw(10) << stats.activeConnections << "\n";
    oss << " Total Verbindungen:   " << std::setw(10) << stats.totalConnections << "\n";
    oss << " Pakete gesendet:      " << std::setw(10) << stats.totalPacketsSent << "\n";
    oss << " Pakete empfangen:     " << std::setw(10) << stats.totalPacketsReceived << "\n";
    oss << " Bytes gesendet:       " << std::setw(10) << stats.totalBytesSent << " Bytes\n";
    oss << " Bytes empfangen:      " << std::setw(10) << stats.totalBytesReceived << " Bytes\n";
    oss << " Retransmissions:      " << std::setw(10) << stats.totalRetransmissions << "\n";
    oss << " Fragmente:            " << std::setw(10) << stats.totalFragments << "\n";
    oss << " Durchschn. RTT:       " << std::setw(10) << std::fixed << std::setprecision(2)
        << stats.globalAvgRttMs << " ms\n";
    oss << " Paketverlust:         " << std::setw(10) << std::fixed << std::setprecision(2)
        << (stats.globalPacketLossRate * 100.0f) << "%\n";
    oss << " Globale Qualitaet:    " << std::setw(10) << std::fixed << std::setprecision(1)
        << (stats.globalQualityScore * 100.0f) << "%\n";
    oss << "--------------------------------------------------------------------------------\n";

    // Verbindungs-Details
    auto allMetrics = GetAllConnectionMetrics();
    if (!allMetrics.empty()) {
        oss << " VERBINDUNGS-DETAILS\n";
        oss << "--------------------------------------------------------------------------------\n";
        for (const auto& m : allMetrics) {
            auto elapsed = std::chrono::duration<float>(now - m.connectedTime).count();
            oss << " Client " << m.clientId << " (" << m.clientAddress << ":" << m.clientPort << "):\n";
            oss << "   Dauer:              " << std::setw(8) << std::fixed << std::setprecision(1)
                << elapsed << " s\n";
            oss << "   RTT (min/avg/max):  " << std::setw(8) << m.minRttMs << " / "
                << m.avgRttMs << " / " << m.maxRttMs << " ms\n";
            oss << "   Jitter:             " << std::setw(8) << m.jitterMs << " ms\n";
            oss << "   Pakete (S/R/A):     " << std::setw(8) << m.packetsSent << " / "
                << m.packetsReceived << " / " << m.packetsAcked << "\n";
            oss << "   Retransmissions:    " << std::setw(8) << m.packetsRetransmitted << "\n";
            oss << "   Verlust:            " << std::setw(8) << std::fixed << std::setprecision(2)
                << (m.GetPacketLossRate() * 100.0f) << "%\n";
            oss << "   Bandbreite (Out):   " << std::setw(8) << std::fixed << std::setprecision(0)
                << (m.GetSendBandwidthBps() / 1024.0f) << " KB/s\n";
            oss << "   Bandbreite (In):    " << std::setw(8) << std::fixed << std::setprecision(0)
                << (m.GetReceiveBandwidthBps() / 1024.0f) << " KB/s\n";
            oss << "   Snapshots:          " << std::setw(8) << m.snapshotsSent
                << " (" << m.snapshotsDeltaCompressed << " delta, " << m.snapshotsFullState << " full)\n";
            oss << "   Bytes gespart:      " << std::setw(8) << m.snapshotBytesSaved << " Bytes\n";
            oss << "   Qualitaet:          " << std::setw(8) << std::fixed << std::setprecision(1)
                << (m.qualityScore * 100.0f) << "%\n";
            oss << "\n";
        }
    }

    // Schlechte Verbindungen
    auto poor = GetPoorQualityConnections();
    if (!poor.empty()) {
        oss << "--------------------------------------------------------------------------------\n";
        oss << " SCHLECHTE VERBINDUNGEN (< 50% Qualitaet): " << poor.size() << "\n";
        oss << "--------------------------------------------------------------------------------\n";
        for (uint32_t id : poor) {
            auto m = GetConnectionMetrics(id);
            oss << "   Client " << id << ": " << m.avgRttMs << "ms RTT, "
                << (m.GetPacketLossRate() * 100.0f) << "% Verlust\n";
        }
    }

    // Historische Daten
    auto history = GetHistory();
    if (!history.empty()) {
        oss << "--------------------------------------------------------------------------------\n";
        oss << " HISTORIE (letzte " << history.size() << " Punkte)\n";
        oss << "--------------------------------------------------------------------------------\n";
        size_t step = std::max(size_t(1), history.size() / 10);
        for (size_t i = 0; i < history.size(); i += step) {
            auto elapsed = std::chrono::duration<float>(now - history[i].timestamp).count();
            oss << " T-" << std::setw(6) << std::fixed << std::setprecision(1) << elapsed << "s: "
                << std::setw(8) << history[i].totalBytesSent << " B out, "
                << std::setw(8) << history[i].totalBytesReceived << " B in, "
                << std::setw(4) << history[i].activeConnections << " conns, "
                << std::setw(6) << history[i].avgRttMs << "ms RTT\n";
        }
    }

    oss << "================================================================================\n";
    return oss.str();
}

void NetworkProfiler::PrintReport() const {
    AddLog("{}", GenerateReport());
}

// =============================================================================
// SCOPED CONNECTION TRACKER
// =============================================================================
ScopedConnectionTracker::ScopedConnectionTracker(uint32_t id, std::string_view address, uint16_t port)
    : clientId(id) {
    NetworkProfiler::GetInstance().RegisterConnection(id, address, port);
}

ScopedConnectionTracker::~ScopedConnectionTracker() {
    NetworkProfiler::GetInstance().UnregisterConnection(clientId);
}

} // namespace net
