#pragma once
// =============================================================================
// network/network_NetworkServer.h — High-Level Network Server (P2-FIX)
// =============================================================================
// KORREKTUR P2:
// • SendFragmented() für MTU-konforme Pakete
// • ProcessFragment() für Reassembly
// • ProcessReceiveQueue() für Empfangs-Queue-Verarbeitung
// =============================================================================
#include "network_UdpSocket.h"
#include "network_ReliableUdp.h"
#include "../core/Types.h"

#include <span>
#include <vector>
#include <string>
#include <unordered_map>
#include <mutex>
#include <chrono>
#include <functional>
#include <deque>
#include <atomic>

namespace net {

// =============================================================================
// NETZWERK-PAKET (eingehend)
// =============================================================================
struct NetworkPacket {
    uint32_t clientId = 0;
    std::vector<uint8_t> data;
    std::chrono::steady_clock::time_point receiveTime;
};

// =============================================================================
// PENDING PACKET (fuer Retransmission)
// =============================================================================
struct PendingPacket {
    PacketHeader header;
    std::vector<uint8_t> payload;
    std::string destinationIp;
    uint16_t destinationPort = 0;
    std::chrono::steady_clock::time_point sendTime;
    uint32_t retryCount = 0;
    bool acked = false;
};

// =============================================================================
// CLIENT-VERBINDUNG
// =============================================================================
struct ClientConnection {
    uint32_t clientId = 0;
    NetworkAddress address{};
    std::chrono::steady_clock::time_point connectedTime;
    std::chrono::steady_clock::time_point lastActivity;
    std::unique_ptr<class ReliableUdpChannel> reliableChannel;
    bool authenticated = false;
    std::string playerName;
};

// =============================================================================
// NETWORK SERVER
// =============================================================================
class NetworkServer {
public:
    static constexpr uint32_t INVALID_CLIENT_ID = 0xFFFFFFFF;
    static constexpr uint32_t MAX_RETRIES = 5;

    using PacketCallback = std::function<void(const PacketHeader&, std::span<const uint8_t>,
                                              std::string_view, uint16_t)>;

    NetworkServer();
    ~NetworkServer();

    NetworkServer(const NetworkServer&) = delete;
    NetworkServer& operator=(const NetworkServer&) = delete;

    // ===================================================================
    // Lifecycle
    // ===================================================================
    [[nodiscard]] bool Start(uint16_t port);
    void Stop();
    [[nodiscard]] bool IsRunning() const { return running; }

    // ===================================================================
    // Callback
    // ===================================================================
    void SetPacketCallback(PacketCallback callback) { packetCallback = std::move(callback); }

    // ===================================================================
    // Update
    // ===================================================================
    void ProcessIncoming();
    void ProcessRetransmissions();
    void ProcessReceiveQueue(); // P2-FIX: Empfangs-Queue verarbeiten

    // ===================================================================
    // Senden
    // ===================================================================
    void SendPacket(const PacketHeader& header, std::span<const uint8_t> payload,
                    std::string_view ip, uint16_t port);
    void SendReliable(const PacketHeader& header, std::span<const uint8_t> payload,
                      std::string_view ip, uint16_t port);

    // P2-FIX: Fragmentierte Sendung für große Pakete
    void SendFragmented(const PacketHeader& header, std::span<const uint8_t> payload,
                        std::string_view ip, uint16_t port);

    // ===================================================================
    // ACK-Verarbeitung
    // ===================================================================
    void ProcessAck(uint16_t ack, uint32_t ackBitmap);
    [[nodiscard]] bool IsSequenceAcked(uint16_t sequence) const;

    // ===================================================================
    // Statistiken
    // ===================================================================
    [[nodiscard]] uint32_t GetClientCount() const { return static_cast<uint32_t>(clients.size()); }
    [[nodiscard]] float GetAverageRtt() const;

private:
    UdpSocket udpSocket;
    std::unordered_map<uint32_t, ClientConnection> clients;
    mutable std::mutex clientMutex;

    std::vector<PendingPacket> pendingPackets;
    RttEstimator rttEstimator;

    PacketCallback packetCallback;
    uint32_t nextClientId = 1;
    bool running = false;

    void QueueReliablePacket(uint16_t sequence, std::span<const uint8_t> payload,
                             std::string_view ip, uint16_t port);

    // P2-FIX: Fragment-Verarbeitung
    void ProcessFragment(const PacketHeader& header, std::span<const uint8_t> payload,
                         std::string_view ip, uint16_t port);
};

} // namespace net
