// =============================================================================
// server/Server.cpp  —  Server Implementation
// =============================================================================
#include "Server.h"

int dbFlushTimer = 0;

// =============================================================================
// SPIELER-LOGOUT
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
// HAUPT-SERVER-TICK
// =============================================================================
void ProcessServerTick(std::move_only_function<void()> rebuildGPU) {
    if (serverListenSocket == INVALID_SOCKET) return;

    // 1) Neue Client-Verbindungen annehmen
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

    // 2) Eingehende Pakete lesen und routen
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

    // 3) Welt-Simulation
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

    // 5) Interest Management
    {
        std::lock_guard lock(sessionsMutex);
        for (auto& session : clientSessions)
            UpdateInterestManagement(session);
    }

    // 6) Sektor-Streaming
    if (!serverRegistry.empty()) {
        float pX = serverRegistry[0].transform.x;
        float pZ = serverRegistry[0].transform.z;
        int   tx = currentSectorX, tz = currentSectorZ;
        float ex = pX, ez = pZ;
        bool  stream = false;

        if      (pX >  SECTOR_WORLD_SIZE / 2.0f) { tx++; ex = -18.5f; stream = true; }
        else if (pX < -SECTOR_WORLD_SIZE / 2.0f) { tx--; ex =  18.5f; stream = true; }
        if      (pZ >  SECTOR_WORLD_SIZE / 2.0f) { tz++; ez = -18.5f; stream = true; }
        else if (pZ < -SECTOR_WORLD_SIZE / 2.0f) { tz--; ez =  18.5f; stream = true; }

        if (stream) {
            gEventBus.Publish(SectorSwitchedEvent{
                .oldSectorX = currentSectorX,
                .oldSectorZ = currentSectorZ,
                .newSectorX = tx,
                .newSectorZ = tz,
                .exitX = ex,
                .exitZ = ez
            });
            SwitchSector(tx, tz, ex, ez, std::move(rebuildGPU));
        }
    }

    // 7) Autosave
    if (!serverRegistry.empty() && GameDB) {
        if (++dbFlushTimer >= 300) {
            dbFlushTimer = 0;
            for (auto& ent : serverRegistry) {
                if (!ent.isMonster && ent.persistence.isDirty) {
                    PlayerProfile p;
                    SafeStrCopy(p.username,   ent.name, sizeof(p.username));
                    SafeStrCopy(p.lastSector,
                                GetSectorName(currentSectorX, currentSectorZ),
                                sizeof(p.lastSector));
                    p.level = ent.persistence.level;
                    p.gold  = ent.persistence.gold;
                    p.lastX = ent.transform.x;
                    p.lastY = ent.transform.y;
                    p.lastZ = ent.transform.z;
                    GameDB->Push(p);
                    ent.persistence.isDirty = false;
                }
            }
        }
    }
}
