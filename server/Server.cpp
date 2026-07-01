// =============================================================================
// server/Server.cpp — Multi-Threaded Game Server (P1/P2-FIX)
// =============================================================================
// KORREKTUR P1/P2:
// • ECS-Queries verwenden ecs:: Namespace (ecs::PositionComponent etc.)
// • Spatial Hash wird bei Entity-Bewegung aktualisiert
// • Interest Management mit Spatial Hash statt O(n²)
// =============================================================================
#include "Server.h"
#include "PacketHandler.h"
#include "ThreadPool.h"
#include "../core/GameSystems.h"
#include "../core/EventSystem.h"
#include "../core/EventTypes.h"
#include "../ecs/ecs_EcsWorld.h"
#include "../ecs/Components.h"
#include "../core/Log.h"

#include <thread>
#include <chrono>
#include <cmath>

extern std::unique_ptr<ecs::EcsWorld> gEcsWorld;
extern std::unique_ptr<ThreadPool> gThreadPool;

// =============================================================================
// SERVER MAIN LOOP
// =============================================================================
void ProcessServerTick() {
    // TCP Accept (Legacy)
    sockaddr_in clientAddr{};
    int addrLen = sizeof(clientAddr);
    SOCKET newClient = accept(serverListenSocket,
        reinterpret_cast<sockaddr*>(&clientAddr), &addrLen);

    if (newClient != INVALID_SOCKET) {
        u_long nb = 1;
        ioctlsocket(newClient, FIONBIO, &nb);

        std::lock_guard lock(sessionsMutex);
        ClientSession session;
        session.socket = newClient;
        session.entityId = nextEntityId++;
        clientSessions.push_back(session);

        // Entity für neuen Spieler erstellen
        Entity player;
        player.id = session.entityId;
        player.name = "Player_" + std::to_string(session.entityId);
        player.currentHP = player.maxHP = 100;
        player.transform.x = player.transform.targetX = player.transform.lerpX = 0.0f;
        player.transform.z = player.transform.targetZ = player.transform.lerpZ = 0.0f;
        player.transform.y = GetHeightFromGrid(0.0f, 0.0f) + 0.5f;
        serverRegistry.push_back(player);

        AddLog("[Server] Client verbunden: Entity {}", session.entityId);
    }

    // TCP Empfang
    std::vector<ClientSession*> toRemove;
    {
        std::lock_guard lock(sessionsMutex);
        for (auto& session : clientSessions) {
            uint8_t lenBuf[2];
            int recvd = recv(session.socket,
                reinterpret_cast<char*>(lenBuf), 2, 0);
            if (recvd == 0) {
                toRemove.push_back(&session);
                continue;
            }
            if (recvd == 2) {
                uint16_t len = (static_cast<uint16_t>(lenBuf[0]) << 8) | lenBuf[1];
                if (len > 0 && len < 65535) {
                    std::vector<uint8_t> payload(len);
                    int totalRecvd = 0;
                    while (totalRecvd < static_cast<int>(len)) {
                        int r = recv(session.socket,
                            reinterpret_cast<char*>(payload.data()) + totalRecvd,
                            len - totalRecvd, 0);
                        if (r <= 0) break;
                        totalRecvd += r;
                    }
                    if (totalRecvd == static_cast<int>(len)) {
                        ProcessPacketFromClient(session, std::span(payload));
                    }
                }
            }
        }
    }

    // Disconnects verarbeiten
    for (auto* session : toRemove) {
        std::lock_guard lock(sessionsMutex);
        auto it = std::find_if(clientSessions.begin(), clientSessions.end(),
            [session](const ClientSession& s) { return &s == session; });
        if (it != clientSessions.end()) {
            closesocket(it->socket);
            // Entity entfernen
            serverRegistry.erase(
                std::remove_if(serverRegistry.begin(), serverRegistry.end(),
                    [session](const Entity& e) { return e.id == session->entityId; }),
                serverRegistry.end());
            clientSessions.erase(it);
            AddLog("[Server] Client disconnected: Entity {}", session->entityId);
        }
    }

    // FIX P2: Interest Management mit Spatial Hash
    {
        std::lock_guard lock(sessionsMutex);
        for (auto& session : clientSessions) {
            UpdateInterestManagement(session);
        }
    }

    // FIX P2: Spatial Hash aktualisieren
    static auto lastSpatialUpdate = std::chrono::steady_clock::now();
    auto now = std::chrono::steady_clock::now();
    if (std::chrono::duration<float>(now - lastSpatialUpdate).count() >= 0.5f) {
        UpdateSpatialHash();
        lastSpatialUpdate = now;
    }
}

// =============================================================================
// ECS-LEGACY-SYNC (P1-FIX: Korrekte ECS-Komponenten)
// =============================================================================
void SyncLegacyToEcsServer() {
    if (!gEcsWorld) return;

    // FIX P1: Verwendet ecs:: Namespace statt game::
    auto query = gEcsWorld->QueryEntities<ecs::PositionComponent, ecs::VelocityComponent>();
    for (auto [entityHandle, pos, vel] : query) {
        (void)entityHandle;
        auto* legacy = gEcsWorld->GetComponent<ecs::LegacyIdComponent>(entityHandle);
        if (!legacy) continue;

        // Finde Legacy-Entity
        auto it = std::ranges::find_if(serverRegistry,
            [legacy](const Entity& e) { return e.id == legacy->legacyId; });
        if (it == serverRegistry.end()) continue;

        // Sync Position zurück zu Legacy
        it->transform.x = pos.x;
        it->transform.y = pos.y;
        it->transform.z = pos.z;
    }
}
