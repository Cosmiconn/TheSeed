// =============================================================================
// client/Connection.h — UDP Client Connection (AP-32 Fix)
// =============================================================================
// KORREKTUR: TCP Legacy entfernt, UDP + Reliable UDP implementiert.
// Snapshot-Deserialisierung + Entity-Interpolation vorbereitet.
// =============================================================================
#pragma once
#include "../network/network_ReliableUdp.h"
#include "../network/network_UdpSocket.h"
#include "../core/ByteBuffer.h"
#include "../core/Log.h"
#include "../ecs/ecs_EcsWorld.h"
#include "../ecs/Components.h"

#include <string>
#include <string_view>
#include <span>
#include <vector>
#include <mutex>
#include <atomic>
#include <optional>
#include <expected>
#include <chrono>
#include <functional>

namespace client {

enum class ConnectionError {
    SocketCreateFailed,
    BindFailed,
    ServerUnreachable,
    HandshakeFailed,
    Timeout,
    InvalidSnapshot,
    InternalError
};

// =============================================================================
// Snapshot Entity Data — Deserialisiert vom Server
// =============================================================================
struct SnapshotEntity {
    uint32_t id = 0;
    float x = 0.0f, y = 0.0f, z = 0.0f;
    uint32_t currentHP = 0;
    uint32_t maxHP = 0;
    std::string name;
    std::string materialId;
    std::chrono::steady_clock::time_point receivedAt;
};

// =============================================================================
// UdpClientConnection
// =============================================================================
class UdpClientConnection {
    net::UdpSocket socket;
    net::ReliableUdp reliable;

    std::string serverIp;
    uint16_t serverPort = 0;

    std::atomic<bool> connected{false};
    std::atomic<uint32_t> localEntityId{0};

    // Snapshot Buffer (Thread-safe)
    std::vector<SnapshotEntity> lastSnapshot;
    std::mutex snapshotMutex;
    std::chrono::steady_clock::time_point lastSnapshotTime;

    // Input Buffer
    std::vector<uint8_t> inputBuffer;
    std::mutex inputMutex;

    // Callbacks
    std::function<void(const std::vector<SnapshotEntity>&)> onSnapshotReceived;
    std::function<void(std::string_view)> onChatReceived;
    std::function<void(uint32_t, uint32_t, int)> onCombatEvent; // attacker, victim, damage

public:
    UdpClientConnection() = default;
    ~UdpClientConnection() { Disconnect(); }

    UdpClientConnection(const UdpClientConnection&) = delete;
    UdpClientConnection& operator=(const UdpClientConnection&) = delete;

    // ===================================================================
    // Lifecycle
    // ===================================================================
    [[nodiscard]] std::expected<void, ConnectionError> Connect(
        std::string_view ip, uint16_t port);
    void Disconnect();
    [[nodiscard]] bool IsConnected() const { return connected.load(); }

    // ===================================================================
    // Snapshot Access (Thread-safe)
    // ===================================================================
    [[nodiscard]] std::vector<SnapshotEntity> GetLastSnapshot() const;
    [[nodiscard]] std::chrono::steady_clock::time_point GetLastSnapshotTime() const;

    // ===================================================================
    // Game Actions
    // ===================================================================
    void SendMoveRequest(float targetX, float targetZ);
    void SendCombatAction(uint32_t targetId);
    void SendChatMessage(std::string_view sender, std::string_view text, uint32_t channel);
    void SendInteract(uint32_t targetId);

    // ===================================================================
    // Frame Update (Call every frame)
    // ===================================================================
    void Update(float deltaTime);

    // ===================================================================
    // Callbacks
    // ===================================================================
    void SetSnapshotCallback(std::function<void(const std::vector<SnapshotEntity>&)> cb);
    void SetChatCallback(std::function<void(std::string_view)> cb);
    void SetCombatCallback(std::function<void(uint32_t, uint32_t, int)> cb);

    [[nodiscard]] uint32_t GetLocalEntityId() const { return localEntityId.load(); }

private:
    void ProcessIncomingPackets();
    void ProcessSnapshot(std::span<const uint8_t> payload);
    void SendPacket(std::span<const uint8_t> data, bool reliable = false);
};

} // namespace client
