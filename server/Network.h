#pragma once
// =============================================================================
// server/Network.h — Netzwerk-Abstraktion (P2-FIX)
// =============================================================================
// KORREKTUR P2: Interest Management vollständig implementiert.
// Spatial-Hash für AOI-Queries. Delta-Kompression vorbereitet.
// UDP-Session-State aktiviert.
// =============================================================================

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#pragma comment(lib, "ws2_32.lib")

#include "../core/World.h"
#include "../core/Log.h"
#include "../core/ByteBuffer.h"
#include "../core/GameSystems.h"

#include <span>
#include <vector>
#include <set>
#include <mutex>
#include <memory>
#include <chrono>
#include <unordered_map>
#include <unordered_set>
#include <deque>
#include <functional>
#include <cmath>

// =============================================================================
// TRANSPORT ABSTRAKTION
// =============================================================================
enum class TransportProtocol {
    TCP_Legacy,
    UDP_New
};

inline constexpr TransportProtocol ACTIVE_TRANSPORT = TransportProtocol::TCP_Legacy;

#ifdef _WIN32
    using SocketHandle = SOCKET;
    constexpr SocketHandle INVALID_SOCKET_HANDLE = INVALID_SOCKET;
#else
    using SocketHandle = int;
    constexpr SocketHandle INVALID_SOCKET_HANDLE = -1;
#endif

// =============================================================================
// SPATIAL HASH — Interest Management (P2-FIX)
// =============================================================================
// Teilt die Welt in Gitterzellen auf. Jede Zelle enthält die Entity-IDs
// der sich darin befindlichen Entities. O(1) Suche für Nachbarn.
// =============================================================================
class SpatialHash {
public:
    static constexpr float CELL_SIZE = 50.0f; // 50m × 50m Zellen

private:
    // Hash-Funktion für std::pair<int, int>
    struct PairHash {
        std::size_t operator()(const std::pair<int, int>& p) const noexcept {
            return std::hash<int>{}(p.first) ^ (std::hash<int>{}(p.second) << 1);
        }
    };

    std::unordered_map<std::pair<int, int>, std::unordered_set<uint32_t>, PairHash> cells;
    mutable std::mutex mutex;

public:
    // Berechnet die Zellen-Koordinaten für eine Weltposition
    [[nodiscard]] static std::pair<int, int> GetCell(float x, float z) {
        return {static_cast<int>(std::floor(x / CELL_SIZE)),
                static_cast<int>(std::floor(z / CELL_SIZE))};
    }

    // Fügt eine Entity in den Spatial Hash ein
    void Insert(uint32_t entityId, float x, float z) {
        std::lock_guard lock(mutex);
        auto cell = GetCell(x, z);
        cells[cell].insert(entityId);
    }

    // Entfernt eine Entity aus dem Spatial Hash
    void Remove(uint32_t entityId, float x, float z) {
        std::lock_guard lock(mutex);
        auto cell = GetCell(x, z);
        auto it = cells.find(cell);
        if (it != cells.end()) {
            it->second.erase(entityId);
            if (it->second.empty()) {
                cells.erase(it);
            }
        }
    }

    // Aktualisiert die Position einer Entity (entfernt aus alter Zelle, fügt in neue ein)
    void Update(uint32_t entityId, float oldX, float oldZ, float newX, float newZ) {
        auto oldCell = GetCell(oldX, oldZ);
        auto newCell = GetCell(newX, newZ);
        if (oldCell == newCell) return;

        std::lock_guard lock(mutex);
        auto itOld = cells.find(oldCell);
        if (itOld != cells.end()) {
            itOld->second.erase(entityId);
            if (itOld->second.empty()) {
                cells.erase(itOld);
            }
        }
        cells[newCell].insert(entityId);
    }

    // Findet alle Entities in einem Radius um eine Position
    [[nodiscard]] std::vector<uint32_t> QueryRadius(float x, float z, float radius) const {
        std::vector<uint32_t> result;
        std::shared_lock lock(mutex); // FIX P1-2: Read-Lock für parallelen Zugriff

        int minCellX = static_cast<int>(std::floor((x - radius) / CELL_SIZE));
        int maxCellX = static_cast<int>(std::floor((x + radius) / CELL_SIZE));
        int minCellZ = static_cast<int>(std::floor((z - radius) / CELL_SIZE));
        int maxCellZ = static_cast<int>(std::floor((z + radius) / CELL_SIZE));

        float radiusSq = radius * radius;

        for (int cx = minCellX; cx <= maxCellX; ++cx) {
            for (int cz = minCellZ; cz <= maxCellZ; ++cz) {
                auto it = cells.find({cx, cz});
                if (it == cells.end()) continue;
                for (uint32_t entityId : it->second) {
                    // Prüfe tatsächliche Distanz (nicht nur Zelle)
                    auto itEnt = std::ranges::find_if(serverRegistry,
                        [entityId](const Entity& e) { return e.id == entityId; });
                    if (itEnt == serverRegistry.end()) continue;
                    float dx = itEnt->transform.x - x;
                    float dz = itEnt->transform.z - z;
                    if (dx * dx + dz * dz <= radiusSq) {
                        result.push_back(entityId);
                    }
                }
            }
        }
        return result;
    }

    // Löscht alle Einträge
    void Clear() {
        std::lock_guard lock(mutex);
        cells.clear();
    }
};

// =============================================================================
// CLIENT SESSION (P2-FIX: UDP-Session-State aktiviert)
// =============================================================================
struct ClientSession {
    SocketHandle socket = INVALID_SOCKET_HANDLE;
    uint32_t entityId = 0;
    bool isGM = false;
    std::vector<uint8_t> tcpBuffer;
    std::set<uint32_t> knownEntities;

    // UDP-Session-State (P2-FIX: Aktiviert für Reliable UDP)
    uint32_t sessionId = 0;
    uint16_t localSequence = 0;
    uint16_t remoteSequence = 0;
    std::deque<uint8_t> reliableSendQueue;
    std::chrono::steady_clock::time_point lastHeartbeat;
};

// =============================================================================
// GLOBALE NETZWERK-ZUSTÄNDE
// =============================================================================
extern SocketHandle serverListenSocket;
extern std::vector<ClientSession> clientSessions;
extern std::mutex sessionsMutex;
extern SpatialHash gSpatialHash; // P2-FIX: Globaler Spatial Hash

// =============================================================================
// SEND-PRIMITIVE
// =============================================================================
void SendToClient(ClientSession& session, std::span<const uint8_t> data);
void BroadcastToAll(std::span<const uint8_t> data, uint32_t exceptId = UINT32_MAX);
void SendNetworkPacket(SocketHandle sock, std::span<const uint8_t> data);

// =============================================================================
// INTEREST MANAGEMENT (P2-FIX: Spatial-Hash statt O(n²))
// =============================================================================
void UpdateInterestManagement(ClientSession& session);
void UpdateSpatialHash(); // Aktualisiert den Spatial Hash mit allen Entities

// =============================================================================
// DELTA-KOMPRESSION (P2-FIX)
// =============================================================================
// Speichert den letzten Snapshot pro Client für Delta-Kompression
struct DeltaSnapshotState {
    std::unordered_map<uint32_t, std::vector<uint8_t>> lastEntityStates;
    std::chrono::steady_clock::time_point lastSnapshotTime;
    uint32_t snapshotSequence = 0;
};

extern std::unordered_map<uint32_t, DeltaSnapshotState> clientDeltaStates;

// Baut einen Delta-komprimierten Snapshot für einen Client
[[nodiscard]] std::vector<uint8_t> BuildDeltaSnapshot(ClientSession& session);

// =============================================================================
// SEKTOR-PERSISTENZ
// =============================================================================
void SaveHDTBinary(std::string_view fn);
void LoadHDTBinary(std::string_view fn);
void SaveSpawnsBinary(std::string_view fn);
void LoadSpawnsBinary(std::string_view fn);

// =============================================================================
// SPAWN-REALISIERUNG
// =============================================================================
void RealizeServerSpawnsInWorld();

// =============================================================================
// SEKTOR-STREAMING
// =============================================================================
void SwitchSector(int tx, int tz, float ex, float ez,
    std::move_only_function<void()> rebuildGPU = [](){});

// =============================================================================
// SERVER INIT / SHUTDOWN
// =============================================================================
[[nodiscard]] bool ServerInit(uint16_t port = 54000);
void ServerShutdown();
