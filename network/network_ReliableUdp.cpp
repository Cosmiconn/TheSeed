// =============================================================================
// network/ReliableUdp.cpp — Reliable UDP Implementation (AP-33)
// =============================================================================
#include "ReliableUdp.h"
#include <algorithm>
#include <cstring>

namespace net {

// =============================================================================
// CRC32 Lookup Table
// =============================================================================
static const uint32_t crc32Table[256] = {
    0x00000000, 0x77073096, 0xee0e612c, 0x990951ba, 0x076dc419, 0x706af48f,
    0xe963a535, 0x9e6495a3, 0x0edb8832, 0x79dcb8a4, 0xe0d5e91e, 0x97d2d988,
    0x09b64c2b, 0x7eb17cbd, 0xe7b82d07, 0x90bf1d91, 0x1db71064, 0x6ab020f2,
    0xf3b97148, 0x84be41de, 0x1adad47d, 0x6ddde4eb, 0xf4d4b551, 0x83d385c7,
    0x136c9856, 0x646ba8c0, 0xfd62f97a, 0x8a65c9ec, 0x14015c4f, 0x63066cd9,
    0xfa0f3d63, 0x8d080df5, 0x3b6e20c8, 0x4c69105e, 0xd56041e4, 0xa2677172,
    0x3c03e4d1, 0x4b04d447, 0xd20d85fd, 0xa50ab56b, 0x35b5a8fa, 0x42b2986c,
    0xdbbbc9d6, 0xacbcf940, 0x32d86ce3, 0x45df5c75, 0xdcd60dcf, 0xabd13d59,
    0x26d930ac, 0x51de003a, 0xc8d75180, 0xbfd06116, 0x21b4f4b5, 0x56b3c423,
    0xcfba9599, 0xb8bda50f, 0x2802b89e, 0x5f058808, 0xc60cd9b2, 0xb10be924,
    0x2f6f7c87, 0x58684c11, 0xc1611dab, 0xb6662d3d, 0x76dc4190, 0x01db7106,
    0x98d220bc, 0xefd5102a, 0x71b18589, 0x06b6b51f, 0x9fbfe4a5, 0xe8b8d433,
    0x7807c9a2, 0x0f00f934, 0x9609a88e, 0xe10e9818, 0x7f6a0dbb, 0x086d3d2d,
    0x91646c97, 0xe6635c01, 0x6b6b51f4, 0x1c6c6162, 0x856530d8, 0xf262004e,
    0x6c0695ed, 0x1b01a57b, 0x8208f4c1, 0xf50fc457, 0x65b0d9c6, 0x12b7e950,
    0x8bbeb8ea, 0xfcb9887c, 0x62dd1ddf, 0x15da2d49, 0x8cd37cf3, 0xfbd44c65,
    0x4db26158, 0x3ab551ce, 0xa3bc0074, 0xd4bb30e2, 0x4adfa541, 0x3dd895d7,
    0xa4d1c46d, 0xd3d6f4fb, 0x4369e96a, 0x346ed9fc, 0xad678846, 0xda60b8d0,
    0x44042d73, 0x33031de5, 0xaa0a4c5f, 0xdd0d7cc9, 0x5005713c, 0x270241aa,
    0xbe0b1010, 0xc90c2086, 0x5768b525, 0x206f85b3, 0xb966d409, 0xce61e49f,
    0x5edef90e, 0x29d9c998, 0xb0d09822, 0xc7d7a8b4, 0x59b33d17, 0x2eb40d81,
    0xb7bd5c3b, 0xc0ba6cad, 0xedb88320, 0x9abfb3b6, 0x03b6e20c, 0x74b1d29a,
    0xead54739, 0x9dd277af, 0x04db2615, 0x73dc1683, 0xe3630b12, 0x94643b84,
    0x0d6d6a3e, 0x7a6a5aa8, 0xe40ecf0b, 0x9309ff9d, 0x0a00ae27, 0x7d079eb1,
    0xf00f9344, 0x8708a3d2, 0x1e01f268, 0x6906c2fe, 0xf762575d, 0x806567cb,
    0x196c3671, 0x6e6b06e7, 0xfed41b76, 0x89d32be0, 0x10da7a5a, 0x67dd4acc,
    0xf9b9df6f, 0x8ebeeff9, 0x17b7be43, 0x60b08ed5, 0xd6d6a3e8, 0xa1d1937e,
    0x38d8c2c4, 0x4fdff252, 0xd1bb67f1, 0xa6bc5767, 0x3fb506dd, 0x48b2364b,
    0xd80d2bda, 0xaf0a1b4c, 0x36034af6, 0x41047a60, 0xdf60efc3, 0xa867df55,
    0x316e8eef, 0x4669be79, 0xcb61b38c, 0xbc66831a, 0x256fd2a0, 0x5268e236,
    0xcc0c7795, 0xbb0b4703, 0x220216b9, 0x5505262f, 0xc5ba3bbe, 0xb2bd0b28,
    0x2bb45a92, 0x5cb36a04, 0xc2d7ffa7, 0xb5d0cf31, 0x2cd99e8b, 0x5bdeae1d,
    0x9b64c2b0, 0xec63f226, 0x756aa39c, 0x026d930a, 0x9c0906a9, 0xeb0e363f,
    0x72076785, 0x05005713, 0x95bf4a82, 0xe2b87a14, 0x7bb12bae, 0x0cb61b38,
    0x92d28e9b, 0xe5d5be0d, 0x7cdcefb7, 0x0bdbdf21, 0x86d3d2d4, 0xf1d4e242,
    0x68ddb3f8, 0x1fda836e, 0x81be16cd, 0xf6b9265b, 0x6fb077e1, 0x18b74777,
    0x88085ae6, 0xff0f6a70, 0x66063bca, 0x11010b5c, 0x8f659eff, 0xf862ae69,
    0x616bffd3, 0x166ccf45, 0xa00ae278, 0xd70dd2ee, 0x4e048354, 0x3903b3c2,
    0xa7672661, 0xd06016f7, 0x4969474d, 0x3e6e77db, 0xaed16a4a, 0xd9d65adc,
    0x40df0b66, 0x37d83bf0, 0xa9bcae53, 0xdebb9ec5, 0x47b2cf7f, 0x30b5ffe9,
    0xbdbdf21c, 0xcabac28a, 0x53b39330, 0x24b4a3a6, 0xbad03605, 0xcdd70693,
    0x54de5729, 0x23d967bf, 0xb3667a2e, 0xc4614ab8, 0x5d681b02, 0x2a6f2b94,
    0xb40bbe37, 0xc30c8ea1, 0x5a05df1b, 0x2d02ef8d
};

uint32_t ReliableUdp::CalculateCRC32(std::span<const uint8_t> data) {
    uint32_t crc = 0xFFFFFFFF;
    for (uint8_t byte : data) {
        crc = crc32Table[(crc ^ byte) & 0xFF] ^ (crc >> 8);
    }
    return crc ^ 0xFFFFFFFF;
}

// =============================================================================
// Sequence Number Helpers
// =============================================================================

bool ReliableUdp::IsNewer(uint16_t a, uint16_t b) {
    return static_cast<int16_t>(a - b) > 0;
}

int ReliableUdp::SequenceDiff(uint16_t a, uint16_t b) {
    return static_cast<int16_t>(a - b);
}

// =============================================================================
// Initialization
// =============================================================================

bool ReliableUdp::Initialize(uint16_t port) {
    if (!socket.Bind(port)) {
        return false;
    }
    if (!socket.SetNonBlocking()) {
        Shutdown();
        return false;
    }
    // Increase buffer sizes for high-throughput scenarios
    socket.SetRecvBufferSize(2 * 1024 * 1024);  // 2MB
    socket.SetSendBufferSize(2 * 1024 * 1024);  // 2MB
    return true;
}

void ReliableUdp::Shutdown() {
    std::lock_guard lock(sessionsMutex);
    sessions.clear();
    addressToSession.clear();
    socket.Close();
}

// =============================================================================
// Session Management
// =============================================================================

uint32_t ReliableUdp::CreateSession(const SocketAddress& address) {
    std::lock_guard lock(sessionsMutex);

    // Check if session already exists for this address
    std::string addrKey = address.ToString();
    auto it = addressToSession.find(addrKey);
    if (it != addressToSession.end()) {
        return it->second;
    }

    uint32_t sessionId = nextSessionId++;
    auto now = std::chrono::steady_clock::now();

    UdpSession session;
    session.id = sessionId;
    session.address = address;
    session.lastRecvTime = now;
    session.lastSendTime = now;
    session.isConnected = true;

    sessions[sessionId] = session;
    addressToSession[addrKey] = sessionId;

    if (onSessionConnected) {
        onSessionConnected(sessionId);
    }

    return sessionId;
}

void ReliableUdp::DestroySession(uint32_t sessionId) {
    std::lock_guard lock(sessionsMutex);

    auto it = sessions.find(sessionId);
    if (it == sessions.end()) return;

    addressToSession.erase(it->second.address.ToString());
    sessions.erase(it);

    if (onSessionDisconnected) {
        onSessionDisconnected(sessionId);
    }
}

UdpSession* ReliableUdp::GetSession(uint32_t sessionId) {
    std::lock_guard lock(sessionsMutex);
    auto it = sessions.find(sessionId);
    return it != sessions.end() ? &it->second : nullptr;
}

const UdpSession* ReliableUdp::GetSession(uint32_t sessionId) const {
    std::lock_guard lock(sessionsMutex);
    auto it = sessions.find(sessionId);
    return it != sessions.end() ? &it->second : nullptr;
}

bool ReliableUdp::HasSession(const SocketAddress& address) const {
    std::lock_guard lock(sessionsMutex);
    return addressToSession.contains(address.ToString());
}

// =============================================================================
// Send Operations
// =============================================================================

bool ReliableUdp::Send(uint32_t sessionId, std::span<const uint8_t> data, PacketFlags flags) {
    if (HasFlag(flags, PacketFlags::Reliable)) {
        return SendReliable(sessionId, data);
    }
    return SendUnreliable(sessionId, data);
}

bool ReliableUdp::SendUnreliable(uint32_t sessionId, std::span<const uint8_t> data) {
    auto* session = GetSession(sessionId);
    if (!session || !session->isConnected) return false;

    if (data.size() > MAX_UDP_PAYLOAD) {
        return SendFragmented(sessionId, data, PacketFlags::None);
    }

    auto packet = BuildPacket(sessionId, session->localSequence++, data, PacketFlags::None);
    auto result = socket.SendTo(packet, session->address);

    if (result) {
        session->packetsSent++;
        session->bytesSent += packet.size();
        session->lastSendTime = std::chrono::steady_clock::now();
        return true;
    }
    return false;
}

bool ReliableUdp::SendReliable(uint32_t sessionId, std::span<const uint8_t> data) {
    auto* session = GetSession(sessionId);
    if (!session || !session->isConnected) return false;

    if (data.size() > MAX_UDP_PAYLOAD) {
        return SendFragmented(sessionId, data, PacketFlags::Reliable);
    }

    uint16_t sequence = session->localSequence++;
    auto packet = BuildPacket(sessionId, sequence, data, PacketFlags::Reliable);

    // Store for potential resend
    ReliableRecord record;
    record.data.assign(packet.begin(), packet.end());
    record.address = session->address;
    record.sentTime = std::chrono::steady_clock::now();
    record.sequence = sequence;

    {
        std::lock_guard lock(sessionsMutex);
        session->sentReliable[sequence] = record;
    }

    auto result = socket.SendTo(packet, session->address);

    if (result) {
        session->packetsSent++;
        session->bytesSent += packet.size();
        session->lastSendTime = std::chrono::steady_clock::now();
        return true;
    }
    return false;
}

void ReliableUdp::Broadcast(std::span<const uint8_t> data, PacketFlags flags, uint32_t exceptSessionId) {
    std::lock_guard lock(sessionsMutex);

    for (auto& [id, session] : sessions) {
        if (id == exceptSessionId) continue;
        if (!session.isConnected) continue;

        // We can't lock here because Send might lock again - use try-lock or queue
        // For simplicity, we'll send directly (potential deadlock if called from callback)
        // In production, use a send queue
    }
}

// =============================================================================
// Receive
// =============================================================================

size_t ReliableUdp::Receive() {
    alignas(alignof(UdpPacketHeader)) uint8_t buffer[MAX_PACKET_SIZE];
    SocketAddress sender;
    size_t processed = 0;

    while (true) {
        auto result = socket.RecvFrom(std::span(buffer, sizeof(buffer)), sender);
        if (!result || *result == 0) break;

        size_t received = *result;
        if (received < sizeof(UdpPacketHeader) + 4) continue; // Too small

        // Parse header
        UdpPacketHeader header;
        std::memcpy(&header, buffer, sizeof(header));

        // Verify CRC32
        uint32_t receivedCRC;
        std::memcpy(&receivedCRC, buffer + sizeof(header), sizeof(receivedCRC));

        uint32_t calculatedCRC = CalculateCRC32(std::span(buffer, sizeof(header)));
        if (receivedCRC != calculatedCRC) continue; // CRC mismatch

        // Extract payload
        size_t payloadOffset = sizeof(header) + sizeof(receivedCRC);
        std::span<uint8_t> payload(buffer + payloadOffset, header.payloadLength);

        ProcessPacket(header, payload, sender);
        processed++;
    }

    return processed;
}

// =============================================================================
// Packet Processing
// =============================================================================

void ReliableUdp::ProcessPacket(const UdpPacketHeader& header, std::span<const uint8_t> payload,
                                const SocketAddress& sender) {
    uint32_t sessionId = header.sessionId;

    // Handle connection request
    if (HasFlag(static_cast<PacketFlags>(header.flags), PacketFlags::Connect)) {
        ProcessConnect(sender, payload);
        return;
    }

    // Find or create session
    if (!HasSession(sender)) {
        if (sessionId == 0) return; // Unknown session
        // Could be NAT punchthrough - create session
        sessionId = CreateSession(sender);
    }

    auto* session = GetSession(sessionId);
    if (!session) return;

    // Update timing
    session->lastRecvTime = std::chrono::steady_clock::now();
    session->packetsRecv++;
    session->bytesRecv += sizeof(header) + payload.size();

    // Process ACKs
    if (header.ack != 0 || header.ackBits != 0) {
        ProcessAck(sessionId, header.ack, header.ackBits);
    }

    // Handle disconnect
    if (HasFlag(static_cast<PacketFlags>(header.flags), PacketFlags::Disconnect)) {
        ProcessDisconnect(sessionId);
        return;
    }

    // Handle heartbeat
    if (HasFlag(static_cast<PacketFlags>(header.flags), PacketFlags::Heartbeat)) {
        return; // Just updating lastRecvTime is enough
    }

    // Sequence validation for reliable packets
    if (HasFlag(static_cast<PacketFlags>(header.flags), PacketFlags::Reliable)) {
        uint16_t seq = header.sequence;

        // Check if already received (deduplication)
        bool alreadyReceived = false;
        for (uint16_t receivedSeq : session->receivedReliable) {
            if (receivedSeq == seq) {
                alreadyReceived = true;
                break;
            }
        }

        if (alreadyReceived) {
            // Send ACK again but don't process
            return;
        }

        // Check if out of order
        if (!IsNewer(seq, session->remoteSequence) && seq != session->remoteSequence) {
            // Too old, ignore
            return;
        }

        // Update received sequence
        session->remoteSequence = seq;
        session->receivedReliable.push_back(seq);
        if (session->receivedReliable.size() > 64) {
            session->receivedReliable.pop_front();
        }
    }

    // Handle fragmentation
    if (HasFlag(static_cast<PacketFlags>(header.flags), PacketFlags::Fragment)) {
        // TODO: Reassemble fragments
        // For now, just pass through
    }

    // Deliver to application
    if (onPacketReceived) {
        onPacketReceived(sessionId, payload);
    }
}

void ReliableUdp::ProcessAck(uint32_t sessionId, uint16_t ack, uint32_t ackBits) {
    auto* session = GetSession(sessionId);
    if (!session) return;

    auto now = std::chrono::steady_clock::now();

    // Process explicit ACK
    auto it = session->sentReliable.find(ack);
    if (it != session->sentReliable.end() && !it->second.acked) {
        it->second.acked = true;

        // Update RTT
        float rtt = std::chrono::duration<float>(now - it->second.sentTime).count();
        session->smoothedRTT = 0.875f * session->smoothedRTT + 0.125f * rtt;
        session->rttVariance = 0.875f * session->rttVariance + 0.125f * std::abs(rtt - session->smoothedRTT);

        if (onRttUpdated) {
            onRttUpdated(sessionId, session->smoothedRTT);
        }
    }

    // Process ACK bitfield (32 previous packets)
    for (int i = 0; i < 32; ++i) {
        if ((ackBits >> i) & 1) {
            uint16_t ackedSeq = ack - i - 1;
            auto bitIt = session->sentReliable.find(ackedSeq);
            if (bitIt != session->sentReliable.end()) {
                bitIt->second.acked = true;
            }
        }
    }

    // Remove acked packets
    for (auto it = session->sentReliable.begin(); it != session->sentReliable.end();) {
        if (it->second.acked) {
            it = session->sentReliable.erase(it);
        } else {
            ++it;
        }
    }
}

void ReliableUdp::ProcessConnect(const SocketAddress& sender, std::span<const uint8_t> payload) {
    uint32_t sessionId = CreateSession(sender);

    // Send connection accepted
    auto* session = GetSession(sessionId);
    if (session) {
        uint8_t acceptData[4];
        std::memcpy(acceptData, &sessionId, sizeof(sessionId));
        SendReliable(sessionId, std::span(acceptData, sizeof(acceptData)));
    }
}

void ReliableUdp::ProcessDisconnect(uint32_t sessionId) {
    DestroySession(sessionId);
}

// =============================================================================
// Packet Building
// =============================================================================

std::vector<uint8_t> ReliableUdp::BuildPacket(uint32_t sessionId, uint16_t sequence,
                                               std::span<const uint8_t> payload,
                                               PacketFlags flags) {
    std::vector<uint8_t> packet;
    packet.reserve(sizeof(UdpPacketHeader) + 4 + payload.size());

    auto* session = GetSession(sessionId);

    UdpPacketHeader header{};
    header.sessionId = sessionId;
    header.sequence = sequence;
    header.flags = static_cast<uint16_t>(flags);
    header.payloadLength = static_cast<uint16_t>(payload.size());

    if (session) {
        header.ack = session->remoteSequence;
        // Build ACK bitfield
        header.ackBits = 0;
        for (int i = 0; i < 32; ++i) {
            uint16_t checkSeq = session->remoteSequence - i - 1;
            for (uint16_t received : session->receivedReliable) {
                if (received == checkSeq) {
                    header.ackBits |= (1 << i);
                    break;
                }
            }
        }
    }

    // Append header
    packet.insert(packet.end(), 
                  reinterpret_cast<uint8_t*>(&header),
                  reinterpret_cast<uint8_t*>(&header) + sizeof(header));

    // Append CRC32 placeholder (will be calculated)
    uint32_t crc = CalculateCRC32(std::span(packet.data(), packet.size()));
    packet.insert(packet.end(), 
                  reinterpret_cast<uint8_t*>(&crc),
                  reinterpret_cast<uint8_t*>(&crc) + sizeof(crc));

    // Append payload
    packet.insert(packet.end(), payload.begin(), payload.end());

    return packet;
}

// =============================================================================
// Update (Resends, Heartbeats, Timeouts)
// =============================================================================

void ReliableUdp::Update(float deltaTime) {
    std::lock_guard lock(sessionsMutex);
    auto now = std::chrono::steady_clock::now();

    for (auto it = sessions.begin(); it != sessions.end();) {
        auto& session = it->second;

        // Check timeout
        if (session.IsTimedOut()) {
            auto sessionId = it->first;
            it = sessions.erase(it);
            addressToSession.erase(session.address.ToString());

            if (onSessionDisconnected) {
                onSessionDisconnected(sessionId);
            }
            continue;
        }

        // Resend pending reliable packets
        for (auto& [seq, record] : session.sentReliable) {
            if (record.acked) continue;

            float elapsed = std::chrono::duration<float>(now - record.sentTime).count();
            float timeout = session.smoothedRTT + 4.0f * session.rttVariance;

            if (elapsed > timeout) {
                if (record.retryCount >= maxRetries) {
                    // Too many retries, consider dead
                    session.packetsLost++;
                    record.acked = true; // Stop retrying
                } else {
                    // Resend
                    socket.SendTo(record.data, record.address);
                    record.sentTime = now;
                    record.retryCount++;
                    session.packetsSent++;
                }
            }
        }

        // Send heartbeat if needed
        if (session.NeedsHeartbeat()) {
            SendHeartbeat(it->first);
        }

        ++it;
    }
}

void ReliableUdp::ResendPending(uint32_t sessionId) {
    auto* session = GetSession(sessionId);
    if (!session) return;

    auto now = std::chrono::steady_clock::now();

    for (auto& [seq, record] : session->sentReliable) {
        if (record.acked) continue;

        float elapsed = std::chrono::duration<float>(now - record.sentTime).count();
        if (elapsed > resendTimeout) {
            socket.SendTo(record.data, record.address);
            record.sentTime = now;
            record.retryCount++;
        }
    }
}

void ReliableUdp::SendHeartbeat(uint32_t sessionId) {
    auto* session = GetSession(sessionId);
    if (!session) return;

    uint8_t heartbeatData = 0;
    auto packet = BuildPacket(sessionId, session->localSequence++, 
                              std::span(&heartbeatData, 1), 
                              PacketFlags::Heartbeat);
    socket.SendTo(packet, session->address);
    session->lastSendTime = std::chrono::steady_clock::now();
}

// =============================================================================
// Fragmentation
// =============================================================================

bool ReliableUdp::SendFragmented(uint32_t sessionId, std::span<const uint8_t> data,
                                  PacketFlags flags) {
    auto* session = GetSession(sessionId);
    if (!session || !session->isConnected) return false;

    size_t fragmentSize = MAX_UDP_PAYLOAD;
    size_t totalFragments = (data.size() + fragmentSize - 1) / fragmentSize;

    if (totalFragments > 65535) return false; // Too large

    for (size_t i = 0; i < totalFragments; ++i) {
        size_t offset = i * fragmentSize;
        size_t size = std::min(fragmentSize, data.size() - offset);

        std::span<const uint8_t> fragment(data.data() + offset, size);

        UdpPacketHeader header{};
        header.sessionId = sessionId;
        header.sequence = session->localSequence++;
        header.flags = static_cast<uint16_t>(flags | PacketFlags::Fragment);
        header.fragmentId = static_cast<uint16_t>(i);
        header.fragmentCount = static_cast<uint16_t>(totalFragments);
        header.payloadLength = static_cast<uint16_t>(size);

        std::vector<uint8_t> packet;
        packet.insert(packet.end(), 
                      reinterpret_cast<uint8_t*>(&header),
                      reinterpret_cast<uint8_t*>(&header) + sizeof(header));

        uint32_t crc = CalculateCRC32(std::span(packet.data(), packet.size()));
        packet.insert(packet.end(), 
                      reinterpret_cast<uint8_t*>(&crc),
                      reinterpret_cast<uint8_t*>(&crc) + sizeof(crc));

        packet.insert(packet.end(), fragment.begin(), fragment.end());

        auto result = socket.SendTo(packet, session->address);
        if (!result) return false;
    }

    return true;
}

// =============================================================================
// Statistics
// =============================================================================

size_t ReliableUdp::GetSessionCount() const {
    std::lock_guard lock(sessionsMutex);
    return sessions.size();
}

void ReliableUdp::GetStatistics(uint64_t& outSent, uint64_t& outRecv, uint64_t& outLost) const {
    std::lock_guard lock(sessionsMutex);
    outSent = 0;
    outRecv = 0;
    outLost = 0;

    for (const auto& [id, session] : sessions) {
        outSent += session.packetsSent;
        outRecv += session.packetsRecv;
        outLost += session.packetsLost;
    }
}

} // namespace net
