#pragma once
// =============================================================================
// network/NetworkServer.h — High-Level UDP Server Interface (AP-34/AP-36)
// Replaces legacy TCP server with UDP+Reliability
// =============================================================================
#include "ReliableUdp.h"
#include "../core/ByteBuffer.h"
#include <thread>
#include <atomic>
#include <functional>
#include <span>

namespace net {

// =============================================================================
// CLIENT SESSION (UDP version)
// =============================================================================
struct UdpClientSession {
    uint32_t sessionId = 0;
    uint32_t entityId = 0;
    bool isGM = false;
    bool isAuthenticated = false;
    std::vector<uint8_t> recvBuffer;
    std::set<uint32_t> knownEntities;

    // For interest management
    float lastPosX = 0.0f;
    float lastPosZ = 0.0f;

    // Statistics
    float pingMs = 0.0f;
    uint64_t bytesRecv = 0;
    uint64_t bytesSent = 0;
};

// =============================================================================
// NETWORK SERVER
// =============================================================================
class NetworkServer {
    ReliableUdp reliableUdp;
    std::map<uint32_t, UdpClientSession> clientSessions;
    std::mutex sessionsMutex;

    std::atomic<bool> isRunning{false};
    std::thread recvThread;
    std::thread updateThread;

    uint16_t listenPort = 54000;
    float tickRate = 60.0f; // Updates per second

public:
    NetworkServer() = default;
    ~NetworkServer() { Shutdown(); }

    NetworkServer(const NetworkServer&) = delete;
    NetworkServer& operator=(const NetworkServer&) = delete;

    // Lifecycle
    [[nodiscard]] bool Startup(uint16_t port);
    void Shutdown();
    [[nodiscard]] bool IsRunning() const noexcept { return isRunning; }

    // Send operations
    void SendTo(uint32_t sessionId, std::span<const uint8_t> data, bool reliable = false);
    void Broadcast(std::span<const uint8_t> data, bool reliable = false, uint32_t exceptSessionId = 0);
    void BroadcastToArea(float centerX, float centerZ, float radius, 
                         std::span<const uint8_t> data, bool reliable = false);

    // Session management
    [[nodiscard]] bool HasSession(uint32_t sessionId) const;
    [[nodiscard]] size_t GetSessionCount() const;
    [[nodiscard]] UdpClientSession* GetSession(uint32_t sessionId);
    void DisconnectSession(uint32_t sessionId);

    // Statistics
    [[nodiscard]] float GetAveragePing() const;
    void PrintStatistics() const;

    // Callbacks (set before Startup)
    std::function<void(uint32_t sessionId, std::span<const uint8_t> data)> onPacketReceived;
    std::function<void(uint32_t sessionId)> onClientConnected;
    std::function<void(uint32_t sessionId)> onClientDisconnected;

private:
    void ReceiveLoop();
    void UpdateLoop();
    void HandlePacket(uint32_t sessionId, std::span<const uint8_t> data);
    void HandleConnect(uint32_t sessionId);
    void HandleDisconnect(uint32_t sessionId);
};

// Global instance (replaces legacy server globals)
inline NetworkServer gNetworkServer;

} // namespace net
