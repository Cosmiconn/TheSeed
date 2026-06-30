// =============================================================================
// client/ClientTick.cpp — Client Game Loop Implementation (AP-32 Fix)
// =============================================================================
#include "ClientTick.h"
#include "../core/Log.h"
#include "../math/Vector.h"

#include <thread>
#include <chrono>

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
    connection->SetSnapshotCallback([this](const std::vector<SnapshotEntity>& snapshot) {
        if (!interpolator) return;

        // Convert snapshot to interpolation state
        for (const auto& entity : snapshot) {
            InterpState state{};
            state.id = entity.id;
            state.x = entity.x;
            state.y = entity.y;
            state.z = entity.z;
            state.timestamp = std::chrono::duration<float>(
                entity.receivedAt.time_since_epoch()).count();

            interpolator->AddSnapshot(state.id, state);
        }
    });

    // Setup interpolator
    interpolator = std::make_unique<Interpolator>();

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
// Process Snapshots — Apply interpolated state to ECS
// =============================================================================
void ClientGame::ProcessSnapshots() {
    if (!clientWorld || !interpolator) return;

    auto snapshot = connection->GetLastSnapshot();
    if (snapshot.empty()) return;

    for (const auto& entityData : snapshot) {
        // Find or create entity
        ecs::EntityHandle handle(entityData.id);

        auto* transform = clientWorld->GetComponent<game::Transform>(handle);
        if (!transform) {
            clientWorld->AddComponent(handle, game::Transform{});
            transform = clientWorld->GetComponent<game::Transform>(handle);
        }

        auto* health = clientWorld->GetComponent<game::Health>(handle);
        if (!health) {
            clientWorld->AddComponent(handle, game::Health{});
            health = clientWorld->GetComponent<game::Health>(handle);
        }

        auto* name = clientWorld->GetComponent<game::Name>(handle);
        if (!name) {
            clientWorld->AddComponent(handle, game::Name{});
            name = clientWorld->GetComponent<game::Name>(handle);
        }

        // Apply interpolated position
        auto interpPos = interpolator->Interpolate(entityData.id);
        if (interpPos) {
            transform->x = interpPos->x;
            transform->y = interpPos->y;
            transform->z = interpPos->z;
        } else {
            // Fallback to snapshot position
            transform->x = entityData.x;
            transform->y = entityData.y;
            transform->z = entityData.z;
        }

        // Apply health
        health->current = static_cast<int>(entityData.currentHP);
        health->max = static_cast<int>(entityData.maxHP);

        // Apply name
        if (name->name.empty() && !entityData.name.empty()) {
            name->name = entityData.name;
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
        // Use local player name as sender
        connection->SendChatMessage("Player", text, 0); // 0 = global channel
    }
}

void ClientGame::HandleInteractInput(uint32_t targetId) {
    if (connection) {
        connection->SendInteract(targetId);
    }
}

} // namespace client
