// =============================================================================
// server/Server.cpp — Multi-Threaded Game Server Implementation
// =============================================================================
#include "Server.h"
#include "PacketHandler.h"
#include "Validation.h"
#include "ThreadPool.h"
#include "../core/GameSystems.h"
#include "../core/EventSystem.h"
#include "../core/EventTypes.h"

#include <cstring>

// =============================================================================
// GLOBALE SERVER-INSTANZ
// =============================================================================
std::unique_ptr<MultiThreadedServer> gGameServer;

// =============================================================================
// START
// =============================================================================
bool MultiThreadedServer::Start(uint16_t port) {
    if (!networkServer.Start(port)) {
        AddLog("[Server] Netzwerk-Start fehlgeschlagen");
        return false;
    }

    networkServer.SetPacketCallback(
        [this](const PacketHeader& header, std::span<const uint8_t> payload,
               std::string_view ip, uint16_t port) {
            OnPacketReceived(header, payload, ip, port);
        });

    running.store(true);
    serverThread = std::thread(&MultiThreadedServer::ServerLoop, this);

    AddLog("[Server] MultiThreadedServer gestartet auf Port {}", port);
    return true;
}

// =============================================================================
// STOP
// =============================================================================
void MultiThreadedServer::Stop() {
    running.store(false);
    if (serverThread.joinable()) {
        serverThread.join();
    }
    networkServer.Stop();
    AddLog("[Server] MultiThreadedServer gestoppt");
}

// =============================================================================
// SERVER LOOP
// =============================================================================
void MultiThreadedServer::ServerLoop() {
    auto lastTick = std::chrono::steady_clock::now();

    while (running.load()) {
        auto now = std::chrono::steady_clock::now();
        float dt = std::chrono::duration<float>(now - lastTick).count();
        lastTick = now;

        // Eingehende Pakete verarbeiten
        networkServer.ProcessIncoming();

        // Retransmissions
        networkServer.ProcessRetransmissions();

        // Inaktive Sessions bereinigen
        CleanupInactiveSessions(30.0f);

        // Snapshot bauen und broadcasten (20Hz)
        static float snapshotAccumulator = 0.0f;
        snapshotAccumulator += dt;
        if (snapshotAccumulator >= 0.05f) { // 20Hz
            snapshotAccumulator -= 0.05f;
            BuildAndBroadcastSnapshot();
        }

        // Frame-Rate Limitierung
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
}

// =============================================================================
// PACKET RECEIVED
// =============================================================================
void MultiThreadedServer::OnPacketReceived(const PacketHeader& header,
                                            std::span<const uint8_t> payload,
                                            std::string_view senderIp,
                                            uint16_t senderPort) {
    uint32_t sessionId = GetOrCreateSession(senderIp, senderPort);

    {
        std::lock_guard lock(sessionsMutex);
        auto& session = udpSessions[sessionId];
        session.lastActivity = std::chrono::steady_clock::now();
        session.remoteSequence = header.sequence;
    }

    // ACK senden
    PacketHeader ackHeader;
    ackHeader.protocolId = 0x4D4D;
    ackHeader.ack = header.sequence;
    ackHeader.flags = static_cast<uint16_t>(PacketFlags::AckOnly);
    networkServer.SendPacket(ackHeader, {}, senderIp, senderPort);

    // Payload verarbeiten
    if (!payload.empty() && !(header.flags & static_cast<uint16_t>(PacketFlags::AckOnly))) {
        if (payload.size() >= 1) {
            uint8_t packetType = payload[0];
            ProcessPacket(sessionId, packetType, payload.subspan(1));
        }
    }
}

// =============================================================================
// PROCESS PACKET
// =============================================================================
void MultiThreadedServer::ProcessPacket(uint32_t sessionId, uint8_t packetType,
                                         std::span<const uint8_t> payload) {
    uint32_t entityId = 0;
    {
        std::lock_guard lock(sessionsMutex);
        auto it = udpSessions.find(sessionId);
        if (it != udpSessions.end()) {
            entityId = it->second.entityId;
        }
    }

    switch (static_cast<PacketType>(packetType)) {
        case PacketType::MSG_MOVE_REQUEST: {
            if (payload.size() < 8) break;
            float targetX, targetZ;
            std::memcpy(&targetX, payload.data(), 4);
            std::memcpy(&targetZ, payload.data() + 4, 4);

            // Entity bewegen
            if (gUseEcs && gEcsWorld) {
                auto query = gEcsWorld->Query<PositionComponent, VelocityComponent>();
                for (auto [e, pos, vel] : query) {
                    (void)pos;
                    auto* legacy = gEcsWorld->GetComponent<LegacyIdComponent>(e);
                    if (legacy && legacy->legacyId == entityId) {
                        vel.vx = (targetX - pos.x) * 5.0f;
                        vel.vz = (targetZ - pos.z) * 5.0f;
                        break;
                    }
                }
            } else {
                auto it = std::ranges::find_if(serverRegistry,
                    [entityId](const Entity& e) { return e.id == entityId; });
                if (it != serverRegistry.end()) {
                    it->transform.targetX = targetX;
                    it->transform.targetZ = targetZ;
                }
            }
            break;
        }

        case PacketType::MSG_COMBAT_ACTION: {
            if (payload.size() < 4) break;
            uint32_t targetId;
            std::memcpy(&targetId, payload.data(), 4);

            // Kampf ausführen
            if (entityId != 0 && targetId != 0) {
                ApplyDamage(targetId, entityId, 10); // 10 Basis-Schaden
            }
            break;
        }

        case PacketType::MSG_CHAT: {
            if (payload.size() < 1) break;
            ByteBuffer buf(payload);
            uint32_t channel = buf.ReadUInt32();
            std::string sender = buf.ReadString();
            std::string text = buf.ReadString();

            AddChatMessage(sender, text, channel);
            break;
        }

        case PacketType::MSG_INTERACT: {
            if (payload.size() < 4) break;
            uint32_t targetId;
            std::memcpy(&targetId, payload.data(), 4);

            // Interaktion verarbeiten
            AddLog("[Server] Entity {} interagiert mit {}", entityId, targetId);
            break;
        }

        default:
            AddLog("[Server] Unbekannter Pakettyp: {}", static_cast<int>(packetType));
            break;
    }
}

// =============================================================================
// SNAPSHOT BUILDING
// =============================================================================
void MultiThreadedServer::BuildAndBroadcastSnapshot() {
    ByteBuffer snapshot;

    // Snapshot-Header
    snapshot.WriteUInt8(std::to_underlying(PacketType::MSG_SNAPSHOT));
    snapshot.WriteFloat(static_cast<float>(
        std::chrono::duration<float>(
            std::chrono::steady_clock::now().time_since_epoch()).count()));

    if (gUseEcs && gEcsWorld) {
        // ECS-Entities in Snapshot packen
        auto query = gEcsWorld->Query<PositionComponent, HealthComponent>();
        snapshot.WriteUInt32(static_cast<uint32_t>(query.Count()));

        for (auto [entity, pos, health] : query) {
            auto* legacy = gEcsWorld->GetComponent<LegacyIdComponent>(entity);
            auto* name = gEcsWorld->GetComponent<NameComponent>(entity);
            auto* render = gEcsWorld->GetComponent<RenderComponentECS>(entity);

            uint32_t id = legacy ? legacy->legacyId : static_cast<uint32_t>(entity);
            snapshot.WriteUInt32(id);
            snapshot.WriteFloat(pos.x);
            snapshot.WriteFloat(pos.y);
            snapshot.WriteFloat(pos.z);
            snapshot.WriteUInt32(static_cast<uint32_t>(health.currentHP));
            snapshot.WriteUInt32(static_cast<uint32_t>(health.maxHP));
            snapshot.WriteString(name ? name->name : "Unknown");
            snapshot.WriteString(render ? render->materialId : "default");
        }
    } else {
        // Legacy-Entities in Snapshot packen
        snapshot.WriteUInt32(static_cast<uint32_t>(serverRegistry.size()));
        for (const auto& ent : serverRegistry) {
            snapshot.WriteUInt32(ent.id);
            snapshot.WriteFloat(ent.transform.x);
            snapshot.WriteFloat(ent.transform.y);
            snapshot.WriteFloat(ent.transform.z);
            snapshot.WriteUInt32(static_cast<uint32_t>(ent.currentHP));
            snapshot.WriteUInt32(static_cast<uint32_t>(ent.maxHP));
            snapshot.WriteString(ent.name);
            snapshot.WriteString(ent.render.materialId);
        }
    }

    // An alle Clients broadcasten
    std::lock_guard lock(sessionsMutex);
    for (const auto& [sessionId, session] : udpSessions) {
        networkServer.SendReliable(
            PacketHeader{.protocolId = 0x4D4D, .flags = static_cast<uint16_t>(PacketFlags::Reliable)},
            std::span(snapshot.data),
            session.ip,
            session.port
        );
    }
}

// =============================================================================
// SESSION MANAGEMENT
// =============================================================================
uint32_t MultiThreadedServer::GetOrCreateSession(std::string_view ip, uint16_t port) {
    std::lock_guard lock(sessionsMutex);

    // Suche existierende Session
    for (const auto& [id, session] : udpSessions) {
        if (session.ip == ip && session.port == port) {
            return id;
        }
    }

    // Neue Session erstellen
    static uint32_t nextSessionId = 1;
    uint32_t id = nextSessionId++;

    UdpClientSession session;
    session.ip = std::string(ip);
    session.port = port;
    session.lastActivity = std::chrono::steady_clock::now();

    udpSessions[id] = std::move(session);
    AddLog("[Server] Neue UDP-Session {} von {}:{}", id, ip, port);

    return id;
}

void MultiThreadedServer::RemoveSession(uint32_t sessionId) {
    std::lock_guard lock(sessionsMutex);
    udpSessions.erase(sessionId);
    AddLog("[Server] Session {} entfernt", sessionId);
}

void MultiThreadedServer::CleanupInactiveSessions(float maxInactiveTime) {
    auto now = std::chrono::steady_clock::now();
    std::vector<uint32_t> toRemove;

    {
        std::lock_guard lock(sessionsMutex);
        for (const auto& [id, session] : udpSessions) {
            auto inactive = std::chrono::duration<float>(now - session.lastActivity).count();
            if (inactive > maxInactiveTime) {
                toRemove.push_back(id);
            }
        }
    }

    for (uint32_t id : toRemove) {
        RemoveSession(id);
    }
}

// =============================================================================
// GLOBALE SERVER TICK FUNKTION
// =============================================================================
void ProcessServerTick() {
    if (gGameServer && gGameServer->IsRunning()) {
        // Server läuft in eigenem Thread
        // Hier können zusätzliche Main-Thread-Operationen ausgeführt werden
    }
}
