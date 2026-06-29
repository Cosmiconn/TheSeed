#pragma once
// =============================================================================
// network/ReliableUdp.h — Reliable UDP Protocol (AP-33)
// Features: Sequence numbers, SACK, RTT estimation, fragmentation
// =============================================================================
#include "network_UdpSocket.h"
#include <cstdint>
#include <deque>
#include <vector>
#include <array>
#include <chrono>
#include <span>
#include <functional>

namespace net {

// =============================================================================
// Protocol Constants
// =============================================================================
inline constexpr size_t MAX_PACKET_SIZE = 1200;      // Safe MTU for internet
inline constexpr size_t MAX_FRAGMENT_SIZE = 1100;    // Leave room for headers
inline constexpr size_t MAX_FRAGMENTS = 64;         // Max 64KB payload
inline constexpr uint32_t SACK_BITMAP_BITS = 32;      // 32 acks per packet
inline constexpr float DEFAULT_RTT = 0.1f;            // 100ms initial RTT
inline constexpr float RTT_ALPHA = 0.125f;            // Jacobson smoothing
inline constexpr float RTT_BETA = 0.25f;              // Variance smoothing
inline constexpr uint16_t PROTOCOL_ID = 0x5344;     // "SD" = Seed

// =============================================================================
// Packet Header (16 bytes)
// =============================================================================
#pragma pack(push, 1)
struct PacketHeader {
    uint16_t protocolId = PROTOCOL_ID;
    uint16_t sequence = 0;          // Sender's sequence number
    uint16_t ack = 0;               // Last received sequence
    uint32_t ackBitmap = 0;         // SACK: 32 previous acks
    uint16_t payloadLen = 0;        // Payload length (excluding header)
    uint16_t flags = 0;             // Fragment flags, reliability, etc.
    uint16_t fragmentId = 0;        // Fragment index (0 = unfragmented)
    uint16_t fragmentCount = 0;   // Total fragments (0 = unfragmented)

    static constexpr uint16_t FLAG_RELIABLE = 0x0001;
    static constexpr uint16_t FLAG_FRAGMENT = 0x0002;
    static constexpr uint16_t FLAG_ACK_ONLY = 0x0004;
    static constexpr uint16_t FLAG_CONNECT = 0x0008;
    static constexpr uint16_t FLAG_DISCONNECT = 0x0010;
};
#pragma pack(pop)

static_assert(sizeof(PacketHeader) == 20, "PacketHeader must be 20 bytes");

// =============================================================================
// Packet Record (for send/recv queues)
// =============================================================================
struct PacketRecord {
    std::vector<uint8_t> data;
    uint16_t sequence = 0;
    std::chrono::steady_clock::time_point sendTime;
    bool acked = false;
    int retryCount = 0;
    Endpoint destination;
};

// =============================================================================
// Reliable Channel
// =============================================================================
class ReliableChannel {
public:
    struct Stats {
        uint64_t packetsSent = 0;
        uint64_t packetsRecv = 0;
        uint64_t packetsAcked = 0;
        uint64_t packetsLost = 0;
        uint64_t bytesSent = 0;
        uint64_t bytesRecv = 0;
        float smoothedRtt = DEFAULT_RTT;
        float rttVariance = 0.0f;
        float packetLossRate = 0.0f;
    };

private:
    UdpSocket* socket = nullptr;
    Endpoint remoteEndpoint;

    // Sequence numbers
    uint16_t localSequence = 1;
    uint16_t remoteSequence = 0;
    uint32_t remoteAckBitmap = 0;

    // Send queue (unacked reliable packets)
    std::deque<PacketRecord> sendQueue;

    // Receive queue (out-of-order packets)
    std::deque<PacketRecord> recvQueue;

    // Fragment reassembly
    struct FragmentBuffer {
        std::vector<std::vector<uint8_t>> fragments;
        uint16_t fragmentCount = 0;
        uint16_t receivedCount = 0;
        std::chrono::steady_clock::time_point firstFragmentTime;
    };
    std::unordered_map<uint16_t, FragmentBuffer> fragmentBuffers;

    // RTT estimation
    Stats stats;
    std::unordered_map<uint16_t, std::chrono::steady_clock::time_point> sendTimes;

    // Config
    float timeoutMultiplier = 2.0f;     // Retransmit after RTT * multiplier
    int maxRetries = 10;
    float fragmentTimeout = 5.0f;       // Seconds to keep fragment buffer

public:
    ReliableChannel() = default;
    explicit ReliableChannel(UdpSocket* sock, const Endpoint& remote);

    // ===================================================================
    // Connection
    // ===================================================================
    void BindSocket(UdpSocket* sock, const Endpoint& remote);
    [[nodiscard]] bool IsConnected() const { return socket != nullptr; }

    // ===================================================================
    // Send
    // ===================================================================
    // Send unreliable packet (no guarantee, no retry)
    [[nodiscard]] bool SendUnreliable(std::span<const uint8_t> payload);

    // Send reliable packet (guaranteed delivery, ordered)
    [[nodiscard]] bool SendReliable(std::span<const uint8_t> payload);

    // Send acknowledgment only (no payload)
    void SendAck();

    // ===================================================================
    // Receive
    // ===================================================================
    // Process incoming raw packet. Returns true if a complete message is ready.
    [[nodiscard]] bool ProcessIncoming(std::span<const uint8_t> rawPacket, const Endpoint& sender);

    // Get next complete received message (reliable or unreliable)
    [[nodiscard]] std::optional<std::vector<uint8_t>> PopMessage();

    // ===================================================================
    // Update (call every tick)
    // ===================================================================
    void Update(float deltaTime);

    // ===================================================================
    // Stats
    // ===================================================================
    [[nodiscard]] const Stats& GetStats() const { return stats; }
    [[nodiscard]] float GetRtt() const { return stats.smoothedRtt; }

private:
    [[nodiscard]] bool SendPacket(const PacketHeader& header, std::span<const uint8_t> payload);
    void UpdateRtt(uint16_t ackedSequence);
    void Retransmit(uint16_t sequence);
    [[nodiscard]] std::optional<std::vector<uint8_t>> ReassembleFragments(uint16_t baseSequence);
    void CleanupFragments();
};

} // namespace net
