#pragma once
// =============================================================================
// server/Server.h — Multi-Threaded Game Server (AP-37, AP-42)
// =============================================================================
// KORREKTUR: ECS-Integration hinzugefügt. Snapshot-Building vorbereitet.
// OnPacketReceived vollständig implementiert.
// =============================================================================
#include "../network/network_NetworkServer.h"
#include "../network/network_ReliableUdp.h"
#include "../core/World.h"
#include "../core/ECS.h"
#include "../core/ByteBuffer.h"
#include "../core/Log.h"
#include "../ecs/ecs_EcsWorld.h"
#include "../ecs/Components.h"

#include <thread>
#include <atomic>
#include <chrono>
#include <unordered_map>

// =============================================================================
// MULTI-THREADED SERVER
// =============================================================================
class MultiThreadedServer {
private:
    net::NetworkServer networkServer;
    std::thread serverThread;
    std::atomic<bool> running{false};

    // Client-Session-Mapping
    struct UdpClientSession {
        std::string ip;
        uint16_t port = 0;
        uint32_t entityId = 0;
        uint32_t localSequence = 0;
        uint32_t remoteSequence = 0;
        std::chrono::steady_clock::time_point lastActivity;
    };
    std::unordered_map<uint32_t, UdpClientSession> udpSessions;
    std::mutex sessionsMutex;

    // Snapshot-Builder
    std::vector<uint8_t> snapshotBuffer;
    std::mutex snapshotMutex;

public:
    MultiThreadedServer() = default;
    ~MultiThreadedServer() { Stop(); }

    MultiThreadedServer(const MultiThreadedServer&) = delete;
    MultiThreadedServer& operator=(const MultiThreadedServer&) = delete;

    // ===================================================================
    // Lifecycle
    // ===================================================================
    [[nodiscard]] bool Start(uint16_t port);
    void Stop();
    [[nodiscard]] bool IsRunning() const { return running.load(); }

    // ===================================================================
    // Paketverarbeitung
    // ===================================================================
    void OnPacketReceived(const PacketHeader& header,
                          std::span<const uint8_t> payload,
                          std::string_view senderIp,
                          uint16_t senderPort);

    // ===================================================================
    // Snapshot-Building
    // ===================================================================
    void BuildAndBroadcastSnapshot();

    // ===================================================================
    // Session Management
    // ===================================================================
    [[nodiscard]] uint32_t GetOrCreateSession(std::string_view ip, uint16_t port);
    void RemoveSession(uint32_t sessionId);
    void CleanupInactiveSessions(float maxInactiveTime);

private:
    void ServerLoop();
    void ProcessPacket(uint32_t sessionId, uint8_t packetType, std::span<const uint8_t> payload);
};

// =============================================================================
// GLOBALE SERVER-INSTANZ
// =============================================================================
extern std::unique_ptr<MultiThreadedServer> gGameServer;

// =============================================================================
// SERVER FUNKTIONEN
// =============================================================================
void ProcessServerTick();
