// =============================================================================
// server/Network.cpp  —  Network Implementation
// =============================================================================
#include "Network.h"

SOCKET                     serverListenSocket = INVALID_SOCKET;
std::vector<ClientSession> clientSessions;
std::mutex                 sessionsMutex;

// =============================================================================
// SEND-PRIMITIVE
// =============================================================================
void SendToClient(ClientSession& session, std::span<const uint8_t> data) {
    if (session.socket == INVALID_SOCKET) return;
    uint8_t  hdr[2];
    uint16_t len = static_cast<uint16_t>(data.size());
    hdr[0] = static_cast<uint8_t>((len >> 8) & 0xFF);
    hdr[1] = static_cast<uint8_t>( len       & 0xFF);
    send(session.socket, reinterpret_cast<const char*>(hdr), 2, 0);
    send(session.socket, reinterpret_cast<const char*>(data.data()),
         static_cast<int>(data.size()), 0);
}

void BroadcastToAll(std::span<const uint8_t> data, uint32_t exceptId) {
    std::lock_guard lock(sessionsMutex);
    for (auto& s : clientSessions)
        if (s.entityId != exceptId)
            SendToClient(s, data);
}

void SendNetworkPacket(SOCKET sock, std::span<const uint8_t> data) {
    if (sock == INVALID_SOCKET) return;
    uint8_t  hdr[2];
    uint16_t len = static_cast<uint16_t>(data.size());
    hdr[0] = static_cast<uint8_t>((len >> 8) & 0xFF);
    hdr[1] = static_cast<uint8_t>( len       & 0xFF);
    send(sock, reinterpret_cast<const char*>(hdr), 2, 0);
    send(sock, reinterpret_cast<const char*>(data.data()),
         static_cast<int>(data.size()), 0);
}

// =============================================================================
// INTEREST MANAGEMENT
// =============================================================================
void UpdateInterestManagement(ClientSession& session) {
    if (serverRegistry.empty()) return;
    auto heroIt = std::ranges::find_if(serverRegistry,
        [&](const Entity& e){ return e.id == session.entityId; });
    if (heroIt == serverRegistry.end()) return;

    const float px = heroIt->transform.x, pz = heroIt->transform.z;
    for (const auto& ent : serverRegistry) {
        if (ent.id == session.entityId) continue;
        float dx   = ent.transform.x - px;
        float dz   = ent.transform.z - pz;
        float dist = std::sqrt(dx * dx + dz * dz);
        bool  inAOI = dist <= AOI_RADIUS;
        bool  known = session.knownEntities.contains(ent.id);

        if (inAOI && !known) {
            session.knownEntities.insert(ent.id);
            ByteBuffer pkt;
            pkt.WriteUInt8 (std::to_underlying(PacketType::MSG_ENTITY_SPAWN));
            pkt.WriteUInt32(ent.id);
            pkt.WriteString(ent.name);
            pkt.WriteUInt8 (ent.isMonster ? 1 : 0);
            pkt.WriteFloat (ent.transform.x);
            pkt.WriteFloat (ent.transform.z);
            pkt.WriteString(ent.render.materialId);
            pkt.WriteFloat (ent.render.scaleY);
            pkt.WriteString(ent.render.meshId);
            SendToClient(session, std::span(pkt.data));
        } else if (!inAOI && known) {
            session.knownEntities.erase(ent.id);
            ByteBuffer pkt;
            pkt.WriteUInt8 (std::to_underlying(PacketType::MSG_ENTITY_DESPAWN));
            pkt.WriteUInt32(ent.id);
            SendToClient(session, std::span(pkt.data));
        }
    }
}

// =============================================================================
// SEKTOR-PERSISTENZ (HDT / SPW)
// =============================================================================
void SaveHDTBinary(std::string_view fn) {
    std::ofstream f(std::string(fn), std::ios::binary | std::ios::trunc);
    if (!f.is_open()) return;
    char h[16] = {0}; h[0]='H'; h[1]='D'; h[2]='T';
    uint32_t ver=1, w=GRID_SIZE, ht=GRID_SIZE;
    std::memcpy(&h[4],&ver,4); std::memcpy(&h[8],&w,4); std::memcpy(&h[12],&ht,4);
    f.write(h, 16);
    f.write(reinterpret_cast<const char*>(heightMap.data()),
            heightMap.size() * sizeof(float));
    f.close();
    AddLog("[IO] HDT gespeichert: {}", fn);
}

void LoadHDTBinary(std::string_view fn) {
    std::ifstream f(std::string(fn), std::ios::binary);
    if (!f.is_open()) {
        for (int z = 0; z < GRID_SIZE; ++z)
            for (int x = 0; x < GRID_SIZE; ++x) {
                float ax = static_cast<float>(x + currentSectorX * GRID_SIZE);
                float az = static_cast<float>(z + currentSectorZ * GRID_SIZE);
                heightMap[z * GRID_SIZE + x] =
                    std::sin(ax * 0.15f) * 1.5f + std::cos(az * 0.15f) * 1.5f;
            }
        return;
    }
    f.seekg(16);
    f.read(reinterpret_cast<char*>(heightMap.data()),
           GRID_SIZE * GRID_SIZE * sizeof(float));
    f.close();
}

void SaveSpawnsBinary(std::string_view fn) {
    std::ofstream f(std::string(fn), std::ios::binary | std::ios::trunc);
    if (!f.is_open()) return;
    char     m[4] = {'S','P','W','N'};
    uint32_t cnt  = static_cast<uint32_t>(sectorSpawns.size());
    f.write(m, 4);
    f.write(reinterpret_cast<const char*>(&cnt), 4);
    if (cnt > 0)
        f.write(reinterpret_cast<const char*>(sectorSpawns.data()),
                cnt * sizeof(SpawnPoint));
    f.close();
}

void LoadSpawnsBinary(std::string_view fn) {
    sectorSpawns.clear(); nextSpawnPointId = 1;
    std::ifstream f(std::string(fn), std::ios::binary); if (!f.is_open()) return;
    char     m[4]; uint32_t cnt = 0;
    f.read(m, 4); f.read(reinterpret_cast<char*>(&cnt), 4);
    if (cnt > 0) {
        sectorSpawns.resize(cnt);
        f.read(reinterpret_cast<char*>(sectorSpawns.data()),
               cnt * sizeof(SpawnPoint));
        nextSpawnPointId = sectorSpawns.back().id + 1;
    }
    f.close();
}

// =============================================================================
// SPAWN-REALISIERUNG
// =============================================================================
void RealizeServerSpawnsInWorld() {
    serverRegistry.erase(
        std::ranges::remove_if(serverRegistry,
            [](const Entity& e){ return e.isMonster; }).begin(),
        serverRegistry.end());
    for (const auto& sp : sectorSpawns) {
        Entity m;
        m.id = nextEntityId++; m.isMonster = true;
        m.originSpawnId = sp.id; m.currentHP = 100;
        m.monsterTemplateId = sp.monsterTemplateId;
        m.transform.x = m.transform.targetX = m.transform.lerpX = sp.x;
        m.transform.z = m.transform.targetZ = m.transform.lerpZ = sp.z;
        m.transform.y = GetHeightFromGrid(sp.x, sp.z) + 0.5f;
        if (sp.monsterTemplateId == 101) { m.name="Slimy"; m.render={"mat_slimy",0.6f,"cube"}; }
        else                             { m.name="Orc";   m.render={"mat_orc",  1.2f,"pyramid"}; }
        serverRegistry.push_back(m);
    }
}

// =============================================================================
// SEKTOR-STREAMING
// =============================================================================
void SwitchSector(int tx, int tz, float ex, float ez,
                  std::move_only_function<void()> rebuildGPU) {
    currentSectorX = tx; currentSectorZ = tz;
    AddLog("[Streaming] Sektor: {}", GetSectorName(tx, tz));
    if (serverRegistry.empty()) return;

    auto& hero = serverRegistry[0];
    hero.transform.x = hero.transform.targetX = ex;
    hero.transform.z = hero.transform.targetZ = ez;
    hero.persistence.isDirty = true;

    LoadHDTBinary  (GetSectorName(tx, tz) + ".hdt");
    LoadSpawnsBinary(GetSectorName(tx, tz) + ".spw");
    rebuildGPU();
    RealizeServerSpawnsInWorld();
    hero.transform.y = GetHeightFromGrid(ex, ez) + 0.5f;

    ByteBuffer pkt; pkt.WriteUInt8(std::to_underlying(PacketType::MSG_SECTOR_SWITCH));
    pkt.WriteUInt32(static_cast<uint32_t>(tx));
    pkt.WriteUInt32(static_cast<uint32_t>(tz));
    pkt.WriteFloat(ex); pkt.WriteFloat(ez);
    BroadcastToAll(std::span(pkt.data));
}

// =============================================================================
// SERVER INIT / SHUTDOWN
// =============================================================================
bool ServerInit(uint16_t port) {
    WSADATA wsa;
    if (WSAStartup(MAKEWORD(2,2), &wsa) != 0) {
        AddLog("[Net] WSAStartup fehlgeschlagen."); return false;
    }
    serverListenSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (serverListenSocket == INVALID_SOCKET) {
        AddLog("[Net] socket() fehlgeschlagen."); return false;
    }
    u_long nb = 1; ioctlsocket(serverListenSocket, FIONBIO, &nb);
    sockaddr_in sa{};
    sa.sin_family      = AF_INET;
    sa.sin_port        = htons(port);
    sa.sin_addr.s_addr = INADDR_ANY;
    bind(serverListenSocket, reinterpret_cast<sockaddr*>(&sa), sizeof(sa));
    listen(serverListenSocket, SOMAXCONN);
    AddLog("[Net] Server lauscht auf Port {}", port);
    return true;
}

void ServerShutdown() {
    {
        std::lock_guard lock(sessionsMutex);
        for (auto& s : clientSessions)
            if (s.socket != INVALID_SOCKET) closesocket(s.socket);
        clientSessions.clear();
    }
    if (serverListenSocket != INVALID_SOCKET) {
        closesocket(serverListenSocket);
        serverListenSocket = INVALID_SOCKET;
    }
    WSACleanup();
    AddLog("[Net] Server heruntergefahren.");
}
