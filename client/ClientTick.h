#pragma once
// =============================================================================
// client/ClientTick.h — Client Game Loop (P5-FIX)
// =============================================================================
// KORREKTUR P5: Alle fehlenden Includes ergaenzt.
// <memory>, <string>, <atomic>, <chrono> vollstaendig.
// Client-Prediction und Server Reconciliation integriert.
// =============================================================================
#include "Connection.h"
#include "Interpolation.h"
#include "Renderer.h"
#include "../ecs/ecs_EcsWorld.h"
#include "../ecs/Components.h"
#include "../core/EventSystem.h"

#include <memory>
#include <string>
#include <atomic>
#include <chrono>

namespace client {

// =============================================================================
// ClientGame — Haupt-Client-Loop
// =============================================================================
class ClientGame {
    std::unique_ptr<UdpClientConnection> connection;
    std::unique_ptr<InterpolationManager> interpolator;
    std::unique_ptr<ecs::EcsWorld> clientWorld;

    // Local player state
    uint32_t localEntityId = 0;
    float targetX = 0.0f, targetZ = 0.0f;

    // Client-Prediction: Eingabe-History fuer Reconciliation
    struct PredictedInput {
        uint32_t sequenceId;
        float targetX, targetZ;
        math::Vector3 predictedPos;
        std::chrono::steady_clock::time_point timestamp;
    };
    std::deque<PredictedInput> inputHistory;
    uint32_t nextInputSequence = 1;

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

    // P5: Client-Prediction
    void ApplyClientPrediction(float deltaTime);
    void ReconcileWithServer(const InterpSnapshot& serverState);
};

} // namespace client
