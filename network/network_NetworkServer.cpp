// =============================================================================
// network/NetworkServer.cpp — UDP Server Implementation (AP-34/AP-36)
// =============================================================================
#include "NetworkServer.h"
#include "../core/Log.h"
#include <chrono>
#include <format>

namespace net {

// =============================================================================
// Lifecycle
// =============================================================================

bool NetworkServer::Startup(uint16_t port) {
    if (isRunning) return false;

    listenPort = port;

    if (!NetworkInit()) {
        AddLog("[Net] Failed to initialize network subsystem");
        return false;
    }

    if (!reliableUdp.Initialize(port)) {
        AddLog("[Net] Failed to bind UDP socket to port {}", port);
        NetworkShutdown();
        return false;
    }

    // Setup callbacks
    reliableUdp.onPacketReceived = [this](uint32_t sessionId, std::span<const uint8_t> data) {
        HandlePacket(sessionId, data);
    };

    reliableUdp.onSessionConnected = [this](uint32_t sessionId) {
        HandleConnect(sessionId);
    };

    reliableUdp.onSessionDisconnected = [this](uint32_t sessionId) {
        HandleDisconnect(sessionId);
    };

    reliableUdp.onRttUpdated = [this](uint32_t sessionId, float rtt) {
        std::lock_guard lock(sessionsMutex);
        auto it = clientSessions.find(sessionId);
        if (it != clientSessions.end()) {
            it->second.pingMs = rtt * 1000.0f;
        }
    };

    isRunning = true;

    // Start threads
    recvThread = std::thread([this]() { ReceiveLoop(); });
    updateThread = std::thread([this]() { UpdateLoop(); });

    AddLog("[Net] UDP server started on port {}", port);
    return true;
}

void NetworkServer::Shutdown() {
    if (!isRunning) return;

    isRunning = false;

    if (recvThread.joinable()) recvThread.join();
    if (updateThread.joinable()) updateThread.join();

    reliableUdp.Shutdown();
    NetworkShutdown();

    std::lock_guard lock(sessionsMutex);
    clientSessions.clear();

    AddLog("[Net] UDP server shutdown complete");
}

// =============================================================================
// Send Operations
// =============================================================================

void NetworkServer::SendTo(uint32_t sessionId, std::span<const uint8_t> data, bool reliable) {
    if (!isRunning) return;

    auto flags = reliable ? PacketFlags::Reliable : PacketFlags::None;
    reliableUdp.Send(sessionId, data, flags);

    std::lock_guard lock(sessionsMutex);
    auto it = clientSessions.find(sessionId);
    if (it != clientSessions.end()) {
        it->second.bytesSent += data.size();
    }
}

void NetworkServer::Broadcast(std::span<const uint8_t> data, bool reliable, uint32_t exceptSessionId) {
    if (!isRunning) return;

    std::lock_guard lock(sessionsMutex);
    for (auto& [id, session] : clientSessions) {
        if (id == exceptSessionId) continue;

        auto flags = reliable ? PacketFlags::Reliable : PacketFlags::None;
        reliableUdp.Send(id, data, flags);
        session.bytesSent += data.size();
    }
}

void NetworkServer::BroadcastToArea(float centerX, float centerZ, float radius,
                                    std::span<const uint8_t> data, bool reliable) {
    if (!isRunning) return;

    // TODO: Integrate with spatial hash (AP-40)
    // For now, broadcast to all (fallback)
    Broadcast(data, reliable);
}

// =============================================================================
// Session Management
// =============================================================================

bool NetworkServer::HasSession(uint32_t sessionId) const {
    std::lock_guard lock(sessionsMutex);
    return clientSessions.contains(sessionId);
}

size_t NetworkServer::GetSessionCount() const {
    std::lock_guard lock(sessionsMutex);
    return clientSessions.size();
}

UdpClientSession* NetworkServer::GetSession(uint32_t sessionId) {
    std::lock_guard lock(sessionsMutex);
    auto it = clientSessions.find(sessionId);
    return it != clientSessions.end() ? &it->second : nullptr;
}

void NetworkServer::DisconnectSession(uint32_t sessionId) {
    reliableUdp.DestroySession(sessionId);

    std::lock_guard lock(sessionsMutex);
    clientSessions.erase(sessionId);
}

// =============================================================================
// Loops
// =============================================================================

void NetworkServer::ReceiveLoop() {
    AddLog("[Net] Receive thread started");

    while (isRunning) {
        size_t processed = reliableUdp.Receive();

        if (processed == 0) {
            // No data, yield to prevent busy-waiting
            std::this_thread::sleep_for(std::chrono::microseconds(100));
        }
    }

    AddLog("[Net] Receive thread stopped");
}

void NetworkServer::UpdateLoop() {
    AddLog("[Net] Update thread started");

    auto lastTime = std::chrono::steady_clock::now();

    while (isRunning) {
        auto now = std::chrono::steady_clock::now();
        float deltaTime = std::chrono::duration<float>(now - lastTime).count();
        lastTime = now;

        reliableUdp.Update(deltaTime);

        // Target tick rate
        float targetFrameTime = 1.0f / tickRate;
        if (deltaTime < targetFrameTime) {
            std::this_thread::sleep_for(
                std::chrono::duration<float>(targetFrameTime - deltaTime));
        }
    }

    AddLog("[Net] Update thread stopped");
}

// =============================================================================
// Event Handlers
// =============================================================================

void NetworkServer::HandlePacket(uint32_t sessionId, std::span<const uint8_t> data) {
    std::lock_guard lock(sessionsMutex);
    auto it = clientSessions.find(sessionId);
    if (it != clientSessions.end()) {
        it->second.bytesRecv += data.size();
    }

    if (onPacketReceived) {
        onPacketReceived(sessionId, data);
    }
}

void NetworkServer::HandleConnect(uint32_t sessionId) {
    std::lock_guard lock(sessionsMutex);

    UdpClientSession session;
    session.sessionId = sessionId;
    session.entityId = 0; // Will be assigned by game logic

    clientSessions[sessionId] = session;

    AddLog("[Net] Client connected: session {}", sessionId);

    if (onClientConnected) {
        onClientConnected(sessionId);
    }
}

void NetworkServer::HandleDisconnect(uint32_t sessionId) {
    std::lock_guard lock(sessionsMutex);
    clientSessions.erase(sessionId);

    AddLog("[Net] Client disconnected: session {}", sessionId);

    if (onClientDisconnected) {
        onClientDisconnected(sessionId);
    }
}

// =============================================================================
// Statistics
// =============================================================================

float NetworkServer::GetAveragePing() const {
    std::lock_guard lock(sessionsMutex);
    if (clientSessions.empty()) return 0.0f;

    float total = 0.0f;
    for (const auto& [id, session] : clientSessions) {
        total += session.pingMs;
    }
    return total / static_cast<float>(clientSessions.size());
}

void NetworkServer::PrintStatistics() const {
    std::lock_guard lock(sessionsMutex);

    uint64_t totalSent = 0, totalRecv = 0;
    for (const auto& [id, session] : clientSessions) {
        totalSent += session.bytesSent;
        totalRecv += session.bytesRecv;
    }

    uint64_t udpSent = 0, udpRecv = 0, udpLost = 0;
    reliableUdp.GetStatistics(udpSent, udpRecv, udpLost);

    AddLog("[Net] === Server Statistics ===");
    AddLog("[Net] Sessions: {}", clientSessions.size());
    AddLog("[Net] Avg Ping: {:.1f}ms", GetAveragePing());
    AddLog("[Net] App Bytes Sent: {}, Recv: {}", totalSent, totalRecv);
    AddLog("[Net] UDP Packets Sent: {}, Recv: {}, Lost: {}", udpSent, udpRecv, udpLost);
}

} // namespace net
