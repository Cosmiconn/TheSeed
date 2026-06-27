#pragma once
// =============================================================================
// server/Network.h — Netzwerk-Abstraktion (TCP Legacy → UDP Migration)
// =============================================================================

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#pragma comment(lib, "ws2_32.lib")

#include "../core/World.h"
#include "../core/Log.h"
#include "../core/ByteBuffer.h"
#include "../core/GameSystems.h"

#include <vector>
#include <set>
#include <mutex>
#include <string>
#include <fstream>
#include <cstring>
#include <cmath>
#include <algorithm>
#include <ranges>
#include <format>
#include <string_view>
#include <span>

// =============================================================================
#warning "LEGACY NETWORK: TCP Blocking wird durch UDP+Reliability ersetzt (AP-32-AP-36)"
// =============================================================================

// =============================================================================
// TRANSPORT ABSTRAKTION (vorbereitet für AP-32)
// =============================================================================
enum class TransportProtocol {
    TCP_Legacy,     // Aktuell: Blocking, Single-Threaded, 64 Clients max
    UDP_New         // Zukunft: Non-blocking, Multi-Threaded, 1000+ CCU
};

inline constexpr TransportProtocol ACTIVE_TRANSPORT = TransportProtocol::TCP_Legacy;

// Platform-agnostischer Socket-Handle
#ifdef _WIN32
    using SocketHandle = SOCKET;
    constexpr SocketHandle INVALID_SOCKET_HANDLE = INVALID_SOCKET;
#else
    using SocketHandle = int;
    constexpr SocketHandle INVALID_SOCKET_HANDLE = -1;
#endif

// =============================================================================
// CLIENT SESSION
// =============================================================================
struct ClientSession {
    SocketHandle socket = INVALID_SOCKET_HANDLE;
    uint32_t entityId = 0;
    bool isGM = false;
    std::vector<uint8_t> tcpBuffer;
    std::set<uint32_t> knownEntities;
    
    // UDP-Zukunft (AP-32): Session-State für Reliable UDP
    // uint32_t sessionId = 0;
    // uint32_t localSequence = 0;
    // uint32_t remoteSequence = 0;
    // std::deque<Packet> reliableSendQueue;
};

// =============================================================================
// GLOBALE NETZWERK-ZUSTÄNDE
// =============================================================================
extern SocketHandle serverListenSocket;
extern std::vector<ClientSession> clientSessions;
extern std::mutex sessionsMutex;

// =============================================================================
// SEND-PRIMITIVE
// =============================================================================
void SendToClient(ClientSession& session, std::span<const uint8_t> data);
void BroadcastToAll(std::span<const uint8_t> data, uint32_t exceptId = UINT32_MAX);
void SendNetworkPacket(SocketHandle sock, std::span<const uint8_t> data);

// =============================================================================
// INTEREST MANAGEMENT (wird ersetzt durch Spatial-Hash in AP-40)
// =============================================================================
void UpdateInterestManagement(ClientSession& session);

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
