#pragma once
// =============================================================================
// network/ReliableUdp.h — Reliable UDP Layer (AP-33)
// Sequence numbers, ACKs, resend queue, fragmentation
// =============================================================================
#include "UdpSocket.h"
#include <map>
#include <deque>
#include <chrono>
#include <mutex>
#include <atomic>
#include <optional>
#include <span>
#include <vector>
#include <format>

namespace net {

// =============================================================================
// PACKET HEADER (8 bytes)
// =============================================================================
#pragma pack(push, 1)
struct UdpPacketHeader {
    uint32_t sessionId;      // Session identifier
    uint16_t sequence;       // Packet sequence number
    uint16_t ack;            // ACK for received packets
    uint32_t ackBits;        // 32-bit ACK bitfield
    uint16_t flags;          // Packet flags
    uint16_t fragmentId;     // Fragment ID (0 = not fragmented)
    uint16_t fragmentCount;  // Total fragments
    uint16_t payloadLength;  // Payload length
    // CRC32 follows header (4 bytes)
};
#pragma pack(pop)

static_assert(sizeof(UdpPacketHeader) == 20, "UdpPacketHeader size mismatch");

// Packet flags
enum class PacketFlags : uint16_t {
    None        = 0,
    Reliable    = 1 << 0,   // Requires ACK
    Fragment    = 1 << 1,   // Part of fragmented packet
    Connect     = 1 << 2,   // Connection request
    Disconnect  = 1 << 3,   // Disconnection
    Heartbeat   = 1 << 4,   // Keep-alive
    Compressed  = 1 << 5,   // Payload is compressed
    Encrypted   = 1 << 6,   // Payload is encrypted
};

inline PacketFlags operator|(PacketFlags a, PacketFlags b) {
    return static_cast<PacketFlags>(static_cast<uint16_t>(a) | static_cast<uint16_t>(b));
}
inline bool HasFlag(PacketFlags flags, PacketFlags check) {
    return (static_cast<uint16_t>(flags) & static_cast<uint16_t>(check)) != 0;
}

// Maximum payload per UDP packet (avoiding fragmentation by network layer)
inline constexpr size_t MAX_UDP_PAYLOAD = 1200;
inline constexpr size_t MAX_PACKET_SIZE = MAX_UDP_PAYLOAD + sizeof(UdpPacketHeader) + 4; // +CRC32

// =============================================================================
// RELIABLE PACKET RECORD
// =============================================================================

struct ReliableRecord {
    std::vector<uint8_t> data;
    SocketAddress address;
    std::chrono::steady_clock::time_point sentTime;
    uint16_t sequence = 0;
    uint8_t retryCount = 0;
    bool acked = false;
};

// =============================================================================
// SESSION STATE
// =============================================================================

struct UdpSession {
    uint32_t id = 0;
    SocketAddress address;

    // Sequence numbers
    uint16_t localSequence = 0;   // Next sequence to send
    uint16_t remoteSequence = 0;  // Last received sequence
    uint32_t remoteAckBits = 0;   // ACK bitfield for remote packets

    // Timing
    std::chrono::steady_clock::time_point lastRecvTime;
    std::chrono::steady_clock::time_point lastSendTime;

    // Round-trip time estimation
    float smoothedRTT = 0.1f;     // 100ms default
    float rttVariance = 0.05f;

    // Reliability
    std::map<uint16_t, ReliableRecord> sentReliable;  // Pending ACKs
    std::deque<uint16_t> receivedReliable;            // Already received (dedup)

    // Connection state
    bool isConnected = false;
    bool isAuthenticated = false;

    // Statistics
    uint64_t packetsSent = 0;
    uint64_t packetsRecv = 0;
    uint64_t bytesSent = 0;
    uint64_t bytesRecv = 0;
    uint64_t packetsLost = 0;

    [[nodiscard]] bool IsTimedOut() const {
        auto now = std::chrono::steady_clock::now();
        auto elapsed = std::chrono::duration<float>(now - lastRecvTime).count();
        return elapsed > 30.0f; // 30 second timeout
    }

    [[nodiscard]] bool NeedsHeartbeat() const {
        auto now = std::chrono::steady_clock::now();
        auto elapsed = std::chrono::duration<float>(now - lastSendTime).count();
        return elapsed > 5.0f; // Heartbeat every 5 seconds
    }
};

// =============================================================================
// RELIABLE UDP MANAGER
// =============================================================================

class ReliableUdp {
    UdpSocket socket;
    std::map<uint32_t, UdpSession> sessions;
    std::map<std::string, uint32_t> addressToSession;
    std::mutex sessionsMutex;

    uint32_t nextSessionId = 1;

    // Configuration
    float resendTimeout = 0.1f;      // Initial resend timeout (seconds)
    uint8_t maxRetries = 10;         // Max resend attempts
    float heartbeatInterval = 5.0f;  // Heartbeat interval
    float timeoutThreshold = 30.0f;  // Session timeout

public:
    ReliableUdp() = default;
    ~ReliableUdp() = default;

    ReliableUdp(const ReliableUdp&) = delete;
    ReliableUdp& operator=(const ReliableUdp&) = delete;

    // Initialize and bind socket
    [[nodiscard]] bool Initialize(uint16_t port);
    void Shutdown();

    // Session management
    [[nodiscard]] uint32_t CreateSession(const SocketAddress& address);
    void DestroySession(uint32_t sessionId);
    [[nodiscard]] UdpSession* GetSession(uint32_t sessionId);
    [[nodiscard]] const UdpSession* GetSession(uint32_t sessionId) const;
    [[nodiscard]] bool HasSession(const SocketAddress& address) const;

    // Send data (unreliable or reliable)
    [[nodiscard]] bool Send(uint32_t sessionId, std::span<const uint8_t> data, 
                            PacketFlags flags = PacketFlags::None);

    // Send unreliable (fire-and-forget)
    [[nodiscard]] bool SendUnreliable(uint32_t sessionId, std::span<const uint8_t> data);

    // Send reliable (guaranteed delivery)
    [[nodiscard]] bool SendReliable(uint32_t sessionId, std::span<const uint8_t> data);

    // Broadcast to all sessions
    void Broadcast(std::span<const uint8_t> data, PacketFlags flags = PacketFlags::None,
                   uint32_t exceptSessionId = 0);

    // Receive and process packets (call frequently)
    // Returns: number of packets processed
    [[nodiscard]] size_t Receive();

    // Process pending operations (resends, heartbeats, timeouts)
    void Update(float deltaTime);

    // Fragment large packets
    [[nodiscard]] bool SendFragmented(uint32_t sessionId, std::span<const uint8_t> data,
                                      PacketFlags flags = PacketFlags::None);

    // Statistics
    [[nodiscard]] size_t GetSessionCount() const;
    void GetStatistics(uint64_t& outSent, uint64_t& outRecv, uint64_t& outLost) const;

    // Callbacks (set these to handle events)
    std::function<void(uint32_t sessionId, std::span<const uint8_t> data)> onPacketReceived;
    std::function<void(uint32_t sessionId)> onSessionConnected;
    std::function<void(uint32_t sessionId)> onSessionDisconnected;
    std::function<void(uint32_t sessionId, float rtt)> onRttUpdated;

private:
    // Internal packet processing
    void ProcessPacket(const UdpPacketHeader& header, std::span<const uint8_t> payload,
                       const SocketAddress& sender);
    void ProcessAck(uint32_t sessionId, uint16_t ack, uint32_t ackBits);
    void ProcessConnect(const SocketAddress& sender, std::span<const uint8_t> payload);
    void ProcessDisconnect(uint32_t sessionId);

    // Packet construction
    [[nodiscard]] std::vector<uint8_t> BuildPacket(uint32_t sessionId, uint16_t sequence,
                                                    std::span<const uint8_t> payload,
                                                    PacketFlags flags);

    // Resend handling
    void ResendPending(uint32_t sessionId);
    void SendHeartbeat(uint32_t sessionId);

    // Sequence number helpers
    [[nodiscard]] static bool IsNewer(uint16_t a, uint16_t b);
    [[nodiscard]] static int SequenceDiff(uint16_t a, uint16_t b);

    // CRC32
    [[nodiscard]] static uint32_t CalculateCRC32(std::span<const uint8_t> data);
};

} // namespace net
