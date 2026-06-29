// =============================================================================
// server/Server.cpp — Updated for UDP + ECS + Multi-Threading (V13.2)
// =============================================================================
#include "Server.h"
#include "Network.h"
#include "PacketHandler.h"
#include "../core/GameSystems.h"
#include "../core/World.h"
#include "../core/Log.h"
#include "../core/ECS.h"
#include "../ecs/ecs_EcsWorld.h"

#include <chrono>
#include <format>

// Legacy globals
int dbFlushTimer = 0;
std::unique_ptr<GameServer> gGameServer;

// =============================================================================
// LEGACY: TCP Server (kept for compatibility during transition)
// =============================================================================
void ExecutePlayerLogout() {
    if (serverRegistry.empty() || !GameDB) return;
    for (const auto& ent : serverRegistry) {
        if (ent.isMonster) continue;
        PlayerProfile p;
        SafeStrCopy(p.username,   ent.name, sizeof(p.username));
        SafeStrCopy(p.lastSector, GetSectorName(currentSectorX, currentSectorZ),
                    sizeof(p.lastSector));
        p.level = ent.persistence.level;
        p.gold  = ent.persistence.gold;
        p.lastX = ent.transform.x;
        p.lastY = ent.transform.y;
        p.lastZ = ent.transform.z;
        GameDB->Push(p);
        GameDB->SaveQuestLog(ent.name, playerQuestLog[ent.id]);
        {
            std::lock_guard lock(inventoryMutex);
            GameDB->SaveInventory(ent.name, playerInventories[ent.id]);
        }
        gEventBus.Publish(PlayerLoggedOutEvent{
            .entityId = ent.id,
            .name = ent.name
        });
    }
    AddLog("[Persistenz] Alle Spielerdaten synchronisiert.");
}

// =============================================================================
// LEGACY: ProcessServerTick (TCP version - will be replaced by GameServer::Tick)
// =============================================================================
void ProcessServerTick(std::move_only_function<void()> rebuildGPU) {
    if (serverListenSocket == INVALID_SOCKET) return;

    // 1) Neue Client-Verbindungen annehmen (TCP - legacy)
    {
        std::lock_guard lock(sessionsMutex);
        while (static_cast<int>(clientSessions.size()) < MAX_CLIENTS) {
            SOCKET inc = accept(serverListenSocket, nullptr, nullptr);
            if (inc == INVALID_SOCKET) break;

            u_long nb = 1; ioctlsocket(inc, FIONBIO, &nb);

            ClientSession s;
            s.socket   = inc;
            s.entityId = nextEntityId++;

            Entity e;
            e.id        = s.entityId;
            e.isMonster = false;
            e.name      = std::format("Hero_{}", e.id);

            PlayerProfile prof;
            if (GameDB && GameDB->GetProfile(e.name, prof)) {
                e.persistence.level = prof.level;
                e.persistence.gold  = prof.gold;
                e.transform.x = prof.lastX;
                e.transform.y = prof.lastY;
                e.transform.z = prof.lastZ;
            } else {
                e.persistence.level = 1;
                e.persistence.gold  = 100;
                e.transform.x = 0.0f;
                e.transform.y = 0.5f;
                e.transform.z = 0.0f;
            }
            e.transform.targetX = e.transform.x;
            e.transform.targetZ = e.transform.z;
            e.render = { "mat_hero", 1.0f, "cube" };
            serverRegistry.push_back(e);

            {
                std::lock_guard invLock(inventoryMutex);
                auto& inv = playerInventories[e.id];
                if (inv.empty()) {
                    inv.resize(INVENTORY_SIZE);
                    if (GameDB) GameDB->LoadInventory(e.name, inv);
                    bool empty = inv[0].templateId == 0;
                    if (empty) {
                        invLock.~lock_guard();
                        AddItemToPlayer(e.id, 10, 5);
                        AddItemToPlayer(e.id, 50, 1);
                    }
                }
            }

            clientSessions.push_back(std::move(s));
            AddLog("[Server] Client verbunden: {}", e.name);

            gEventBus.Publish(PlayerLoggedInEvent{
                .entityId = e.id,
                .name = e.name,
                .x = e.transform.x,
                .y = e.transform.y,
                .z = e.transform.z
            });

            ByteBuffer pkt; pkt.WriteUInt8(std::to_underlying(PacketType::MSG_ENTITY_SPAWN));
            pkt.WriteUInt32(e.id); pkt.WriteString(e.name); pkt.WriteUInt8(0);
            pkt.WriteFloat(e.transform.x); pkt.WriteFloat(e.transform.z);
            pkt.WriteString(e.render.materialId);
            pkt.WriteFloat(e.render.scaleY);
            pkt.WriteString(e.render.meshId);
            BroadcastToAll(std::span(pkt.data));
        }
    }

    // 2) Eingehende Pakete lesen und routen (TCP - legacy)
    {
        std::lock_guard lock(sessionsMutex);
        for (auto& session : clientSessions) {
            if (session.socket == INVALID_SOCKET) continue;

            char chunk[2048];
            int  rc = recv(session.socket, chunk, sizeof(chunk), 0);
            if (rc > 0) {
                session.tcpBuffer.insert(session.tcpBuffer.end(), chunk, chunk + rc);
            } else if (rc == 0 || (rc == SOCKET_ERROR && WSAGetLastError() != WSAEWOULDBLOCK)) {
                closesocket(session.socket);
                session.socket = INVALID_SOCKET;
                continue;
            }

            while (session.tcpBuffer.size() >= 2) {
                uint16_t pLen =
                    (static_cast<uint16_t>(session.tcpBuffer[0]) << 8) |
                     static_cast<uint16_t>(session.tcpBuffer[1]);
                if (session.tcpBuffer.size() < static_cast<size_t>(2 + pLen)) break;
                std::vector<uint8_t> payload(
                    session.tcpBuffer.begin() + 2,
                    session.tcpBuffer.begin() + 2 + pLen);
                session.tcpBuffer.erase(
                    session.tcpBuffer.begin(),
                    session.tcpBuffer.begin() + 2 + pLen);
                ProcessPacketFromClient(session, std::span(payload));
            }
        }
        clientSessions.erase(
            std::ranges::remove_if(clientSessions,
                [](const ClientSession& s){ return s.socket == INVALID_SOCKET; }).begin(),
            clientSessions.end());
    }

    // 3) Welt-Simulation (ECS-ready)
    for (auto& ent : serverRegistry) {
        float dX = ent.transform.targetX - ent.transform.x;
        float dZ = ent.transform.targetZ - ent.transform.z;
        float d  = std::sqrt(dX*dX + dZ*dZ);
        if (d > 0.05f) {
            ent.transform.x += (dX / d) * 0.05f;
            ent.transform.z += (dZ / d) * 0.05f;
        }
        ent.transform.y = GetHeightFromGrid(ent.transform.x, ent.transform.z) + 0.5f;
    }

    // 4) Subsystem-Ticks
    auto bc = [](std::span<const uint8_t> d){ BroadcastToAll(d); };
    ProcessRespawnQueue(TICK_DELTA);
    ProcessStatusEffects(TICK_DELTA, bc);

    // 5) DB-Flush
    dbFlushTimer++;
    if (dbFlushTimer >= DB_FLUSH_INTERVAL) {
        dbFlushTimer = 0;
        if (GameDB) GameDB->Flush();
    }

    // 6) Terrain-GPU rebuild (callback)
    rebuildGPU();
}

// =============================================================================
// NEW: GameServer UDP + Multi-Threaded Implementation (AP-32/33/42)
// =============================================================================
bool GameServer::Startup(uint16_t port, bool multiThreaded) {
    network = std::make_unique<net::NetworkServer>();

    network->SetConnectHandler([this](uint32_t clientId) {
        OnClientConnected(clientId);
    });

    network->SetDisconnectHandler([this](uint32_t clientId) {
        OnClientDisconnected(clientId);
    });

    network->SetPacketHandler([this](uint32_t clientId, std::span<const uint8_t> payload) {
        OnPacketReceived(clientId, payload);
    });

    if (!network->Startup(port)) {
        AddLog("[GameServer] Failed to start network server on port {}", port);
        return false;
    }

    if (multiThreaded) {
        threadedServer = std::make_unique<server::MultiThreadedServer>();
        threadedServer->Startup(4); // 4 worker threads
    }

    running = true;
    AddLog("[GameServer] UDP Server started on port {} (multi-threaded: {})", port, multiThreaded);
    return true;
}

void GameServer::Shutdown() {
    running = false;
    if (threadedServer) {
        threadedServer->Shutdown();
        threadedServer.reset();
    }
    if (network) {
        network->Shutdown();
        network.reset();
    }
    AddLog("[GameServer] Shutdown complete");
}

void GameServer::Tick(float deltaTime) {
    if (!running || !network) return;

    // Poll network (this can be on main thread or network thread)
    network->Poll();

    // If multi-threaded, simulation runs on worker threads
    if (threadedServer) {
        threadedServer->NetworkTick(deltaTime);
        threadedServer->SimulationTick(deltaTime);
    } else {
        // Single-threaded fallback
        // TODO: ECS System execution here (AP-22)
        // TODO: Snapshot building and broadcast (AP-37)
    }

    // Build and broadcast snapshot (AP-37)
    if (gUseEcs && gEcsWorld && network->GetClientCount() > 0) {
        // This would be done on sim thread in multi-threaded mode
        // For now, do it here
        // net::SnapshotBuilder builder(20.0f);
        // auto snapshot = builder.BuildDelta(*gEcsWorld, 0);
        // auto serialized = builder.Serialize(snapshot);
        // network->Broadcast(serialized, false); // Unreliable for snapshots
    }
}

size_t GameServer::GetClientCount() const {
    if (!network) return 0;
    return network->GetClientCount();
}

void GameServer::BroadcastSnapshot(std::span<const uint8_t> snapshotData) {
    if (!network) return;
    network->Broadcast(snapshotData, false); // Unreliable for snapshots
}

void GameServer::OnClientConnected(uint32_t clientId) {
    AddLog("[GameServer] Client {} connected", clientId);

    // Create player entity in ECS (if ECS mode enabled)
    if (gUseEcs && gEcsWorld) {
        auto handle = gEcsWorld->CreateEntity();
        gEcsWorld->AddComponent(handle, game::Transform{.x = 0.0f, .y = 0.5f, .z = 0.0f});
        gEcsWorld->AddComponent(handle, game::Health{.current = 100, .max = 100});
        gEcsWorld->AddComponent(handle, game::Renderable{.materialId = "mat_hero", .meshId = "cube"});
        gEcsWorld->AddComponent(handle, game::PlayerTag{.clientId = clientId});
    }

    // Legacy: create entity in serverRegistry
    Entity e;
    e.id = nextEntityId++;
    e.isMonster = false;
    e.name = std::format("Hero_{}", clientId);
    e.persistence.level = 1;
    e.persistence.gold = 100;
    e.transform.x = 0.0f;
    e.transform.y = 0.5f;
    e.transform.z = 0.0f;
    e.render = {"mat_hero", 1.0f, "cube"};
    serverRegistry.push_back(e);
}

void GameServer::OnClientDisconnected(uint32_t clientId) {
    AddLog("[GameServer] Client {} disconnected", clientId);

    // Remove from ECS
    if (gUseEcs && gEcsWorld) {
        auto query = gEcsWorld->Query<ecs::All<game::PlayerTag>>();
        for (auto [handle] : query) {
            auto* tag = gEcsWorld->GetComponent<game::PlayerTag>(handle);
            if (tag && tag->clientId == clientId) {
                gEcsWorld->DestroyEntity(handle);
                break;
            }
        }
    }

    // Remove from serverRegistry
    std::erase_if(serverRegistry, [clientId](const Entity& e) {
        return e.id == clientId; // Simplified - should map clientId to entityId properly
    });
}

void GameServer::OnPacketReceived(uint32_t clientId, std::span<const uint8_t> payload) {
    // Route to packet handler
    // In multi-threaded mode, queue for sim thread
    if (threadedServer) {
        threadedServer->QueuePacketForSimulation(std::vector<uint8_t>(payload.begin(), payload.end()));
    } else {
        // Direct processing (single-threaded)
        // TODO: Map clientId to ClientSession and call ProcessPacketFromClient
        (void)clientId;
        (void)payload;
    }
}
