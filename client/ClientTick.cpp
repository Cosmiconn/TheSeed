// =============================================================================
// client/ClientTick.cpp — Client Game Loop (P5-FIX)
// =============================================================================
// KORREKTUR P5:
// • Client-Prediction fuer lokale Bewegung
// • Server Reconciliation mit Input-History
// • Fehlende Includes ergaenzt (<algorithm>, <cmath>)
// • Korrekte ECS-Komponenten (ecs:: statt game::)
// =============================================================================
#include "ClientTick.h"
#include "../core/Log.h"
#include "../math/Vector.h"

#include <algorithm>
#include <cmath>

namespace client {

// =============================================================================
// Construction / Destruction
// =============================================================================
ClientGame::ClientGame() = default;

ClientGame::~ClientGame() {
    Shutdown();
}

// =============================================================================
// Initialize
// =============================================================================
bool ClientGame::Initialize(std::string_view serverIp, uint16_t serverPort) {
    // Create client ECS world
    clientWorld = std::make_unique<ecs::EcsWorld>();

    // Create connection
    connection = std::make_unique<UdpClientConnection>();

    auto result = connection->Connect(serverIp, serverPort);
    if (!result) {
        AddLog("[Client] Failed to connect: {}", static_cast<int>(result.error()));
        return false;
    }

    // Setup snapshot callback
    connection->SetSnapshotCallback([this](const std::vector<InterpSnapshot>& snapshot) {
        if (!interpolator) return;

        for (const auto& state : snapshot) {
            interpolator->AddSnapshot(state.sequenceId, state);

            // P5: Server Reconciliation fuer lokalen Spieler
            if (state.sequenceId == localEntityId) {
                ReconcileWithServer(state);
            }
        }
    });

    // Setup interpolator
    interpolator = std::make_unique<InterpolationManager>();

    lastTick = std::chrono::steady_clock::now();
    running.store(true);

    AddLog("[Client] Game initialized, connected to {}:{}", serverIp, serverPort);
    return true;
}

// =============================================================================
// Shutdown
// =============================================================================
void ClientGame::Shutdown() {
    running.store(false);
    if (connection) {
        connection->Disconnect();
    }
    AddLog("[Client] Game shutdown");
}

// =============================================================================
// Main Loop — Fixed 60fps
// =============================================================================
void ClientGame::Run() {
    while (running.load()) {
        auto now = std::chrono::steady_clock::now();
        float frameTime = std::chrono::duration<float>(now - lastTick).count();
        lastTick = now;

        // Cap frame time to prevent spiral of death
        frameTime = std::min(frameTime, 0.25f);

        accumulator += frameTime;

        // Fixed update at 60Hz
        while (accumulator >= TICK_RATE) {
            FixedUpdate(TICK_RATE);
            accumulator -= TICK_RATE;
        }

        // Render with interpolation
        RenderFrame();

        // Frame limit (optional, vsync preferred)
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
}

// =============================================================================
// Fixed Update — 60Hz
// =============================================================================
void ClientGame::FixedUpdate(float deltaTime) {
    if (!connection || !connection->IsConnected()) return;

    // Update network (process incoming, retransmissions)
    connection->Update(deltaTime);

    // P5: Client-Prediction anwenden
    ApplyClientPrediction(deltaTime);

    // Update interpolation
    if (interpolator) {
        interpolator->Update(deltaTime);
    }

    // Update local player
    UpdateLocalPlayer();

    // Process snapshots into ECS world
    ProcessSnapshots();
}

// =============================================================================
// Client-Prediction (P5)
// =============================================================================
void ClientGame::ApplyClientPrediction(float deltaTime) {
    if (localEntityId == 0 || inputHistory.empty()) return;

    // Berechne vorhergesagte Position basierend auf letzter Eingabe
    auto& lastInput = inputHistory.back();
    float speed = 5.0f; // Bewegungsgeschwindigkeit
    math::Vector3 moveDir(lastInput.targetX - lastInput.predictedPos.x,
                          0.0f,
                          lastInput.targetZ - lastInput.predictedPos.z);
    float dist = moveDir.Length();

    if (dist > 0.1f) {
        moveDir = moveDir.Normalized();
        lastInput.predictedPos = lastInput.predictedPos + moveDir * speed * deltaTime;
    }

    // Aktualisiere interpolator mit vorhergesagter Position
    if (interpolator) {
        InterpSnapshot predictedSnap;
        predictedSnap.x = lastInput.predictedPos.x;
        predictedSnap.y = lastInput.predictedPos.y;
        predictedSnap.z = lastInput.predictedPos.z;
        predictedSnap.sequenceId = localEntityId;
        predictedSnap.timestamp = std::chrono::steady_clock::now();
        interpolator->AddSnapshot(localEntityId, predictedSnap);
    }
}

// =============================================================================
// Server Reconciliation (P5)
// =============================================================================
void ClientGame::ReconcileWithServer(const InterpSnapshot& serverState) {
    math::Vector3 serverPos(serverState.x, serverState.y, serverState.z);

    // Finde alle Inputs die aelter als der Server-Status sind
    auto cutoff = serverState.timestamp;
    auto it = std::remove_if(inputHistory.begin(), inputHistory.end(),
        [&cutoff](const PredictedInput& input) {
            return input.timestamp <= cutoff;
        });

    if (it != inputHistory.begin()) {
        // Server-Position ist aelter als unsere Vorhersage
        // Berechne Abweichung
        math::Vector3 predictedPos = inputHistory.front().predictedPos;
        float error = (serverPos - predictedPos).Length();

        if (error > 0.5f) {
            // Zu grosser Fehler → Position korrigieren
            for (auto& input : inputHistory) {
                input.predictedPos = input.predictedPos + (serverPos - predictedPos);
            }
            AddLog("[Client] Reconciliation: error={:.2f}m, corrected", error);
        }

        inputHistory.erase(inputHistory.begin(), it);
    }
}

// =============================================================================
// Process Snapshots — Apply interpolated state to ECS
// =============================================================================
void ClientGame::ProcessSnapshots() {
    if (!clientWorld || !interpolator) return;

    auto snapshot = connection->GetLastSnapshot();
    if (snapshot.empty()) return;

    for (const auto& state : snapshot) {
        // Find or create entity
        ecs::EntityHandle handle(state.sequenceId);

        auto* transform = clientWorld->GetComponent<ecs::PositionComponent>(handle);
        if (!transform) {
            clientWorld->AddComponent(handle, ecs::PositionComponent{});
            transform = clientWorld->GetComponent<ecs::PositionComponent>(handle);
        }

        auto* health = clientWorld->GetComponent<ecs::HealthComponent>(handle);
        if (!health) {
            clientWorld->AddComponent(handle, ecs::HealthComponent{});
            health = clientWorld->GetComponent<ecs::HealthComponent>(handle);
        }

        auto* name = clientWorld->GetComponent<ecs::NameComponent>(handle);
        if (!name) {
            clientWorld->AddComponent(handle, ecs::NameComponent{});
            name = clientWorld->GetComponent<ecs::NameComponent>(handle);
        }

        // Apply interpolated position
        auto interpPos = interpolator->GetInterpolatedPosition(state.sequenceId);
        if (interpPos.Length() > 0.0f) {
            transform->x = interpPos.x;
            transform->y = interpPos.y;
            transform->z = interpPos.z;
        } else {
            // Fallback to snapshot position
            transform->x = state.x;
            transform->y = state.y;
            transform->z = state.z;
        }

        // Apply health
        health->currentHP = static_cast<int>(state.currentHP);
        health->maxHP = static_cast<int>(state.maxHP);

        // Apply name
        if (name->name.empty() && !state.name.empty()) {
            name->name = state.name;
        }
    }
}

// =============================================================================
// Update Local Player
// =============================================================================
void ClientGame::UpdateLocalPlayer() {
    if (!connection) return;

    localEntityId = connection->GetLocalEntityId();

    // Send move request if target changed
    static float lastTargetX = 0.0f, lastTargetZ = 0.0f;
    if (targetX != lastTargetX || targetZ != lastTargetZ) {
        connection->SendMoveRequest(targetX, targetZ);

        // P5: Speichere Eingabe fuer Prediction
        PredictedInput input;
        input.sequenceId = nextInputSequence++;
        input.targetX = targetX;
        input.targetZ = targetZ;
        input.predictedPos = interpolator ? interpolator->GetInterpolatedPosition(localEntityId) : math::Vector3{};
        input.timestamp = std::chrono::steady_clock::now();
        inputHistory.push_back(input);

        // Begrenze History auf 60 Eintraege (~1 Sekunde)
        if (inputHistory.size() > 60) {
            inputHistory.pop_front();
        }

        lastTargetX = targetX;
        lastTargetZ = targetZ;
    }
}

// =============================================================================
// Render Frame
// =============================================================================
void ClientGame::RenderFrame() {
    // TODO: Integrate with actual renderer
    // For now, just update the ECS world for the renderer to use

    if (!clientWorld) return;

    // Calculate interpolation ratio for visual smoothness
    float alpha = accumulator / TICK_RATE;
    (void)alpha; // Would be used by renderer for interpolation
}

// =============================================================================
// Input Handling
// =============================================================================
void ClientGame::HandleMoveInput(float x, float z) {
    targetX = x;
    targetZ = z;
}

void ClientGame::HandleCombatInput(uint32_t targetId) {
    if (connection) {
        connection->SendCombatAction(targetId);
    }
}

void ClientGame::HandleChatInput(std::string_view text) {
    if (connection) {
        connection->SendChatMessage("Player", text, 0); // 0 = global channel
    }
}

void ClientGame::HandleInteractInput(uint32_t targetId) {
    if (connection) {
        connection->SendInteract(targetId);
    }
}

} // namespace client
