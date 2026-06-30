// =============================================================================
// client/Connection.cpp — UDP Client Connection Implementation (AP-32 Fix)
// =============================================================================
#include "Connection.h"
#include "../network/network_NetworkServer.h"
#include "../core/EventSystem.h"
#include "../core/EventTypes.h"

#include <cstring>
#include <chrono>

namespace client {

// =============================================================================
// Connect
// =============================================================================
std::expected<void, ConnectionError> UdpClientConnection::Connect(
    std::string_view ip, uint16_t port) {

    serverIp = std::string(ip);
    serverPort = port;

    if (!socket.Create()) {
        AddLog("[Client] Socket creation failed");
        return std::unexpected(ConnectionError::SocketCreateFailed);
    }

    if (!socket.Bind(0)) { // Bind to any available port
        AddLog("[Client] Socket bind failed");
        return std::unexpected(ConnectionError::BindFailed);
    }

    // Set non-blocking
    socket.SetNonBlocking(true);

    // Initialize reliable UDP
    reliable.Initialize(&socket);

    // Send handshake
    ByteBuffer handshake;
    handshake.WriteUInt8(std::to_underlying(PacketType::MSG_HANDSHAKE));
    handshake.WriteUInt32(1); // Protocol version

    SendPacket(handshake.GetData(), true);

    connected.store(true);
    AddLog("[Client] Connected to {}:{}", ip, port);
    return {};
}

// =============================================================================
// Disconnect
// =============================================================================
void UdpClientConnection::Disconnect() {
    if (!connected.load()) return;

    // Send disconnect packet
    ByteBuffer disconnect;
    disconnect.WriteUInt8(std::to_underlying(PacketType::MSG_DISCONNECT));
    SendPacket(disconnect.GetData(), true);

    connected.store(false);
    socket.Close();
    AddLog("[Client] Disconnected from server");
}

// =============================================================================
// Send Move Request
// =============================================================================
void UdpClientConnection::SendMoveRequest(float targetX, float targetZ) {
    if (!connected.load()) return;

    ByteBuffer buf;
    buf.WriteUInt8(std::to_underlying(PacketType::MSG_MOVE_REQUEST));
    buf.WriteFloat(targetX);
    buf.WriteFloat(targetZ);

    // Move requests are unreliable (frequent, latest is best)
    SendPacket(buf.GetData(), false);
}

// =============================================================================
// Send Combat Action
// =============================================================================
void UdpClientConnection::SendCombatAction(uint32_t targetId) {
    if (!connected.load()) return;

    ByteBuffer buf;
    buf.WriteUInt8(std::to_underlying(PacketType::MSG_COMBAT_ACTION));
    buf.WriteUInt32(targetId);

    // Combat actions are reliable (must arrive)
    SendPacket(buf.GetData(), true);
}

// =============================================================================
// Send Chat Message
// =============================================================================
void UdpClientConnection::SendChatMessage(std::string_view sender,
    std::string_view text, uint32_t channel) {

    if (!connected.load()) return;

    ByteBuffer buf;
    buf.WriteUInt8(std::to_underlying(PacketType::MSG_CHAT));
    buf.WriteUInt32(channel);
    buf.WriteString(sender);
    buf.WriteString(text);

    SendPacket(buf.GetData(), true);
}

// =============================================================================
// Send Interact
// =============================================================================
void UdpClientConnection::SendInteract(uint32_t targetId) {
    if (!connected.load()) return;

    ByteBuffer buf;
    buf.WriteUInt8(std::to_underlying(PacketType::MSG_INTERACT));
    buf.WriteUInt32(targetId);

    SendPacket(buf.GetData(), true);
}

// =============================================================================
// Update — Call every frame
// =============================================================================
void UdpClientConnection::Update(float deltaTime) {
    if (!connected.load()) return;

    // Process incoming packets
    ProcessIncomingPackets();

    // Update reliable UDP (retransmissions, ACKs)
    reliable.Update(deltaTime);
}

// =============================================================================
// Process Incoming Packets
// =============================================================================
void UdpClientConnection::ProcessIncomingPackets() {
    std::array<uint8_t, 2048> buffer{};
    std::string fromIp;
    uint16_t fromPort = 0;

    while (true) {
        int received = socket.Receive(buffer.data(), static_cast<int>(buffer.size()),
            fromIp, fromPort);
        if (received <= 0) break;

        std::span<const uint8_t> packetData(buffer.data(), received);

        // Process through reliable UDP layer
        reliable.ProcessPacket(packetData, fromIp, fromPort);

        // Check if it's a reliable packet or raw packet
        if (packetData.size() < sizeof(PacketHeader)) continue;

        const auto* header = reinterpret_cast<const PacketHeader*>(packetData.data());
        std::span<const uint8_t> payload(packetData.data() + sizeof(PacketHeader),
            packetData.size() - sizeof(PacketHeader));

        // Handle packet types
        switch (static_cast<PacketType>(header->flags & 0x0F)) {
            case PacketType::MSG_SNAPSHOT: {
                ProcessSnapshot(payload);
                break;
            }
            case PacketType::MSG_CHAT: {
                if (onChatReceived && payload.size() >= 4) {
                    ByteBuffer buf(payload);
                    uint32_t channel = buf.ReadUInt32();
                    std::string sender = buf.ReadString();
                    std::string text = buf.ReadString();
                    onChatReceived(std::format("[{}] {}: {}", channel, sender, text));
                }
                break;
            }
            case PacketType::MSG_COMBAT_RESULT: {
                if (onCombatEvent && payload.size() >= 12) {
                    ByteBuffer buf(payload);
                    uint32_t attacker = buf.ReadUInt32();
                    uint32_t victim = buf.ReadUInt32();
                    int damage = static_cast<int>(buf.ReadInt32());
                    onCombatEvent(attacker, victim, damage);
                }
                break;
            }
            case PacketType::MSG_HANDSHAKE_ACK: {
                if (payload.size() >= 4) {
                    uint32_t assignedId = 0;
                    std::memcpy(&assignedId, payload.data(), 4);
                    localEntityId.store(assignedId);
                    AddLog("[Client] Assigned entity ID: {}", assignedId);
                }
                break;
            }
            default:
                break;
        }
    }
}

// =============================================================================
// Process Snapshot — DESERIALISIERUNG
// =============================================================================
void UdpClientConnection::ProcessSnapshot(std::span<const uint8_t> payload) {
    if (payload.size() < 4) return;

    ByteBuffer buf(payload);

    // Read server timestamp
    float serverTime = buf.ReadFloat();
    (void)serverTime; // Used for interpolation timing

    // Read entity count
    uint32_t entityCount = buf.ReadUInt32();

    std::vector<SnapshotEntity> snapshot;
    snapshot.reserve(entityCount);

    for (uint32_t i = 0; i < entityCount; ++i) {
        if (buf.GetReadPos() + 24 > buf.data.size()) break; // Safety check

        SnapshotEntity entity{};
        entity.id = buf.ReadUInt32();
        entity.x = buf.ReadFloat();
        entity.y = buf.ReadFloat();
        entity.z = buf.ReadFloat();
        entity.currentHP = buf.ReadUInt32();
        entity.maxHP = buf.ReadUInt32();
        entity.name = buf.ReadString();
        entity.materialId = buf.ReadString();
        entity.receivedAt = std::chrono::steady_clock::now();

        snapshot.push_back(std::move(entity));
    }

    {
        std::lock_guard lock(snapshotMutex);
        lastSnapshot = std::move(snapshot);
        lastSnapshotTime = std::chrono::steady_clock::now();
    }

    if (onSnapshotReceived) {
        onSnapshotReceived(lastSnapshot);
    }

    AddLog("[Client] Snapshot received: {} entities", entityCount);
}

// =============================================================================
// Get Last Snapshot (Thread-safe)
// =============================================================================
std::vector<SnapshotEntity> UdpClientConnection::GetLastSnapshot() const {
    std::lock_guard lock(snapshotMutex);
    return lastSnapshot;
}

std::chrono::steady_clock::time_point UdpClientConnection::GetLastSnapshotTime() const {
    std::lock_guard lock(snapshotMutex);
    return lastSnapshotTime;
}

// =============================================================================
// Send Packet
// =============================================================================
void UdpClientConnection::SendPacket(std::span<const uint8_t> data, bool useReliable) {
    if (useReliable) {
        reliable.SendReliable(data, serverIp, serverPort);
    } else {
        socket.SendTo(data.data(), static_cast<int>(data.size()), serverIp, serverPort);
    }
}

// =============================================================================
// Callbacks
// =============================================================================
void UdpClientConnection::SetSnapshotCallback(
    std::function<void(const std::vector<SnapshotEntity>&)> cb) {
    onSnapshotReceived = std::move(cb);
}

void UdpClientConnection::SetChatCallback(std::function<void(std::string_view)> cb) {
    onChatReceived = std::move(cb);
}

void UdpClientConnection::SetCombatCallback(
    std::function<void(uint32_t, uint32_t, int)> cb) {
    onCombatEvent = std::move(cb);
}

} // namespace client
