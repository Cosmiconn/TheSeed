#pragma once
// =============================================================================
// network/NetworkServer.h — High-Level Network Server (AP-32 + AP-33)
// Replaces server/Network.cpp TCP blocking with UDP + reliability
// =============================================================================
#include "network_UdpSocket.h"
#include "network_ReliableUdp.h"
#include <cstdint>
#include <string>
#include <vector>
#include <unordered_map>
#include <memory>
#include <mutex>
#include <span>

namespace net {

struct ClientConnection {
    uint32_t clientId = 0;
    Endpoint endpoint;
    std::unique_ptr<ReliableChannel> channel;
    std::chrono::steady_clock::time_point lastRecvTime;
    bool isAuthenticated = false;
    std::string username;

    // Stats
    uint64_t bytesSent = 0;
    uint64_t bytesRecv = 0;
    uint32_t packetsSent = 0;
    uint32_t packetsRecv = 0;
};

class NetworkServer {
public:
    using PacketHandler = std::function<void(uint32_t clientId, std::span<const uint8_t> payload)>;
    using ConnectHandler = std::function<void(uint32_t clientId)>;
    using DisconnectHandler = std::function<void(uint32_t clientId)>;

private:
    UdpSocket socket;
    uint16_t port = 0;
    bool running = false;

    std::unordered_map<uint32_t, ClientConnection> clients;
    std::unordered_map<std::string, uint32_t> endpointToClientId;
    uint32_t nextClientId = 1;

    std::mutex clientsMutex;

    // Handlers
    PacketHandler packetHandler;
    ConnectHandler connectHandler;
    DisconnectHandler disconnectHandler;

    // Config
    float clientTimeout = 30.0f;  // Seconds
    size_t maxClients = 1000;

public:
    NetworkServer() = default;
    ~NetworkServer() { Shutdown(); }

    // ===================================================================
    // Lifecycle
    // ===================================================================
    [[nodiscard]] bool Startup(uint16_t listenPort, size_t maxClients = 1000);
    void Shutdown();
    [[nodiscard]] bool IsRunning() const { return running; }

    // ===================================================================
    // Handlers
    // ===================================================================
    void SetPacketHandler(PacketHandler handler) { packetHandler = std::move(handler); }
    void SetConnectHandler(ConnectHandler handler) { connectHandler = std::move(handler); }
    void SetDisconnectHandler(DisconnectHandler handler) { disconnectHandler = std::move(handler); }

    // ===================================================================
    // I/O
    // ===================================================================
    // Poll for incoming packets (non-blocking, call every tick)
    void Poll();

    // Send to specific client
    [[nodiscard]] bool SendToClient(uint32_t clientId, std::span<const uint8_t> payload, bool reliable = true);

    // Broadcast to all clients
    void Broadcast(std::span<const uint8_t> payload, bool reliable = true);

    // Broadcast to all except one
    void BroadcastExcept(uint32_t excludeClientId, std::span<const uint8_t> payload, bool reliable = true);

    // ===================================================================
    // Client Management
    // ===================================================================
    void DisconnectClient(uint32_t clientId);
    [[nodiscard]] bool IsClientConnected(uint32_t clientId) const;
    [[nodiscard]] size_t GetClientCount() const;

    // ===================================================================
    // Stats
    // ===================================================================
    struct ServerStats {
        uint64_t totalBytesSent = 0;
        uint64_t totalBytesRecv = 0;
        uint64_t totalPacketsSent = 0;
        uint64_t totalPacketsRecv = 0;
        float averageRtt = 0.0f;
        float packetLossRate = 0.0f;
    };
    [[nodiscard]] ServerStats GetStats() const;

private:
    [[nodiscard]] uint32_t GetOrCreateClientId(const Endpoint& endpoint);
    void RemoveClient(uint32_t clientId);
    void CleanupTimedOutClients();
};

} // namespace net
