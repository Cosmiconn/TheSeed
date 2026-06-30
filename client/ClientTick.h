// =============================================================================
// client/ClientTick.h — Client Game Loop (AP-32 Fix)
// =============================================================================
// KORREKTUR: TCP Legacy entfernt, UDP + Snapshot-Interpolation implementiert.
// 60fps Client-Tick mit Entity-Interpolation aus 20Hz Server-Snapshots.
// =============================================================================
#pragma once
#include "Connection.h"
#include "Interpolation.h"
#include "Renderer.h"
#include "../ecs/ecs_EcsWorld.h"
#include "../ecs/Components.h"
#include "../core/EventSystem.h"

#include <chrono>
#include <memory>
#include <atomic>

namespace client {

// =============================================================================
// ClientGame — Haupt-Client-Loop
// =============================================================================
class ClientGame {
    std::unique_ptr<UdpClientConnection> connection;
    std::unique_ptr<Interpolator> interpolator;
    std::unique_ptr<ecs::EcsWorld> clientWorld;

    // Local player state
    uint32_t localEntityId = 0;
    float targetX = 0.0f, targetZ = 0.0f;

    // Timing
    std::chrono::steady_clock::time_point lastTick;
    float accumulator = 0.0f;
    static constexpr float TICK_RATE = 1.0f / 60.0f; // 60fps

    std::atomic<bool> running{false};

public:
    ClientGame();
    ~ClientGame();

    ClientGame(const ClientGame&) = delete;
    ClientGame& operator=(const ClientGame&) = delete;

    // ===================================================================
    // Lifecycle
    // ===================================================================
    [[nodiscard]] bool Initialize(std::string_view serverIp, uint16_t serverPort);
    void Shutdown();
    [[nodiscard]] bool IsRunning() const { return running.load(); }

    // ===================================================================
    // Main Loop
    // ===================================================================
    void Run();
    void Stop() { running.store(false); }

    // ===================================================================
    // Input Handling
    // ===================================================================
    void HandleMoveInput(float x, float z);
    void HandleCombatInput(uint32_t targetId);
    void HandleChatInput(std::string_view text);
    void HandleInteractInput(uint32_t targetId);

    // ===================================================================
    // Accessors
    // ===================================================================
    [[nodiscard]] UdpClientConnection* GetConnection() const { return connection.get(); }
    [[nodiscard]] ecs::EcsWorld* GetWorld() const { return clientWorld.get(); }
    [[nodiscard]] uint32_t GetLocalEntityId() const { return localEntityId; }

private:
    void FixedUpdate(float deltaTime);
    void RenderFrame();
    void ProcessSnapshots();
    void UpdateLocalPlayer();
};

} // namespace client
