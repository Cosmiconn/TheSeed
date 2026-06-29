#pragma once
// =============================================================================
// server/Server.h — Updated for UDP + ECS + Multi-Threading (V13.2)
// =============================================================================
#include "../network/network_NetworkServer.h"
#include "ThreadPool.h"
#include "../core/ByteBuffer.h"
#include <span>
#include <functional>

// Forward declarations
struct Entity;

// Legacy server init (to be replaced)
bool ServerInit(uint16_t port);
void ServerShutdown();
void ProcessServerTick(std::move_only_function<void()> rebuildGPU);
void ExecutePlayerLogout();

// Legacy broadcast (to be replaced by NetworkServer::Broadcast)
void BroadcastToAll(std::span<const uint8_t> data);

// =============================================================================
// NEW: UDP Server Integration (AP-32/33)
// =============================================================================
namespace net { class NetworkServer; }

class GameServer {
    std::unique_ptr<net::NetworkServer> network;
    std::unique_ptr<server::MultiThreadedServer> threadedServer;
    bool running = false;

public:
    GameServer() = default;
    ~GameServer() { Shutdown(); }

    [[nodiscard]] bool Startup(uint16_t port, bool multiThreaded = true);
    void Shutdown();
    void Tick(float deltaTime);

    [[nodiscard]] bool IsRunning() const { return running; }
    [[nodiscard]] size_t GetClientCount() const;

    // Send snapshot to all clients (AP-37)
    void BroadcastSnapshot(std::span<const uint8_t> snapshotData);

    // Submit work to thread pool (AP-42)
    template<typename F, typename... Args>
    auto SubmitWork(F&& f, Args&&... args) {
        if (threadedServer) {
            return threadedServer->threadPool->Submit(std::forward<F>(f), std::forward<Args>(args)...);
        }
        throw std::runtime_error("Thread pool not initialized");
    }

private:
    void OnClientConnected(uint32_t clientId);
    void OnClientDisconnected(uint32_t clientId);
    void OnPacketReceived(uint32_t clientId, std::span<const uint8_t> payload);
};

// Global instance (managed by main.cpp)
extern std::unique_ptr<GameServer> gGameServer;
