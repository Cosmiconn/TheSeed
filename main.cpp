// =============================================================================
// main.cpp — C++23 MMORPG Engine V13.2 Entry Point (P4-FIX)
// =============================================================================
// KORREKTUR: ECS-Systeme korrekt registriert und in Main Loop ausgefuehrt.
// Performance-Monitoring hinzugefuegt. Work-Stealing ThreadPool integriert.
// =============================================================================
#include "core/Types.h"
#include "core/World.h"
#include "core/GameSystems.h"
#include "core/Database.h"
#include "core/ByteBuffer.h"
#include "core/EventSystem.h"
#include "core/EventTypes.h"
#include "core/ECS.h"
#include "ecs/ecs_EcsWorld.h"
#include "ecs/ecs_Query.h"
#include "network/network_NetworkServer.h"
#include "server/Server.h"
#include "server/ThreadPool.h"
#include "client/Connection.h"
#include "client/ClientTick.h"
#include "client/Renderer.h"
#include "editor/EditorRuntime.h"

#include <GLFW/glfw3.h>
#include <imgui.h>
#include <thread>
#include <chrono>
#include <random>
#include <fstream>
#include <sstream>
#include <filesystem>

// =============================================================================
// GLOBALE ZUSTÄNDE
// =============================================================================
bool gRunning = true;
bool gUseEcs = true;          // ECS standardmaessig aktiviert (P1-FIX)
bool gShowEditor = true;
bool gShowMetrics = true;
bool gShowDemo = false;
bool gUseVulkan = false;
bool gUseThreadPool = true;

std::unique_ptr<ecs::EcsWorld> gEcsWorld;
std::unique_ptr<server::MultiThreadedServer> gMultiServer;
std::unique_ptr<net::NetworkServer> gNetworkServer;
std::unique_ptr<ThreadPool> gThreadPool;

int gWindowWidth = 1280;
int gWindowHeight = 720;

// =============================================================================
// GLFW CALLBACKS
// =============================================================================
static void GlfwErrorCallback(int error, const char* description) {
    AddLog("[GLFW] Fehler {}: {}", error, description);
}

static void GlfwKeyCallback(GLFWwindow* window, int key, int scancode, int action, int mods) {
    (void)scancode; (void)mods;
    if (key == GLFW_KEY_ESCAPE && action == GLFW_PRESS)
        glfwSetWindowShouldClose(window, GLFW_TRUE);

    if (key == GLFW_KEY_F1 && action == GLFW_PRESS)
        gShowEditor = !gShowEditor;
    if (key == GLFW_KEY_F2 && action == GLFW_PRESS)
        gShowMetrics = !gShowMetrics;
    if (key == GLFW_KEY_F3 && action == GLFW_PRESS)
        gShowDemo = !gShowDemo;
    if (key == GLFW_KEY_F4 && action == GLFW_PRESS) {
        gUseEcs = !gUseEcs;
        AddLog("[Engine] ECS-Modus: {}", gUseEcs ? "AKTIV" : "LEGACY");
    }

    if (serverRegistry.empty()) return;
    auto& hero = serverRegistry[0];
    const float speed = 0.3f;
    if (action == GLFW_PRESS || action == GLFW_REPEAT) {
        switch (key) {
            case GLFW_KEY_W: hero.transform.targetZ -= speed; break;
            case GLFW_KEY_S: hero.transform.targetZ += speed; break;
            case GLFW_KEY_A: hero.transform.targetX -= speed; break;
            case GLFW_KEY_D: hero.transform.targetX += speed; break;
        }
    }
}

static void GlfwFramebufferSizeCallback(GLFWwindow*, int w, int h) {
    gWindowWidth = w; gWindowHeight = h;
    glViewport(0, 0, w, h);
}

// =============================================================================
// ECS-SYSTEME REGISTRIEREN (P4)
// =============================================================================
static void RegisterEcsSystems() {
    if (!gEcsWorld) return;

    // Movement-System: Aktualisiert Position basierend auf Velocity
    gEcsWorld->RegisterSystem("Movement", [](ecs::EcsWorld& world, float dt) {
        auto query = world.QueryEntities<ecs::PositionComponent, ecs::VelocityComponent>();
        for (auto [entity, pos, vel] : query) {
            pos.x += vel.vx * dt;
            pos.y += vel.vy * dt;
            pos.z += vel.vz * dt;
            pos.y = GetHeightFromGrid(pos.x, pos.z) + 0.5f;
        }
    });

    // Health-System: Prueft auf Tod
    gEcsWorld->RegisterSystem("Health", [](ecs::EcsWorld& world, float dt) {
        (void)dt;
        auto query = world.QueryEntities<ecs::HealthComponent>();
        for (auto [entity, health] : query) {
            if (health.currentHP <= 0 && health.isAlive) {
                health.isAlive = false;
                AddLog("[ECS] Entity {} ist gestorben", entity);
            }
        }
    });

    // Combat-System: Verarbeitet eingehenden Schaden
    gEcsWorld->RegisterSystem("Combat", [](ecs::EcsWorld& world, float dt) {
        (void)dt;
        auto query = world.QueryEntities<ecs::HealthComponent, ecs::CombatComponent>();
        for (auto [entity, health, combat] : query) {
            if (combat.incomingDamage > 0) {
                health.currentHP -= combat.incomingDamage;
                AddLog("[ECS] Entity {} nimmt {} Schaden, HP={}/{}",
                       entity, combat.incomingDamage, health.currentHP, health.maxHP);
                combat.incomingDamage = 0;
            }
        }
    });

    // StatusEffect-System: Verarbeitet Buffs/Debuffs
    gEcsWorld->RegisterSystem("StatusEffects", [](ecs::EcsWorld& world, float dt) {
        auto query = world.QueryEntities<ecs::StatusEffectComponent, ecs::HealthComponent>();
        for (auto [entity, status, health] : query) {
            for (auto& effect : status.effects) {
                if (!effect.isActive) continue;
                effect.remainingDuration -= dt;
                effect.timeSinceLastTick += dt;

                if (effect.timeSinceLastTick >= 1.0f) {
                    effect.timeSinceLastTick -= 1.0f;
                    health.currentHP -= effect.tickDamage;
                    AddLog("[ECS] Entity {}: Status-Effekt '{}' tick, {} Schaden",
                           entity, static_cast<int>(effect.type), effect.tickDamage);
                }

                if (effect.remainingDuration <= 0.0f) {
                    effect.isActive = false;
                    AddLog("[ECS] Entity {}: Status-Effekt '{}' abgelaufen", entity,
                           static_cast<int>(effect.type));
                }
            }
            std::erase_if(status.effects, [](const auto& e) { return !e.isActive; });
        }
    });

    // AI-System: Einfaches Verhalten fuer Monster
    gEcsWorld->RegisterSystem("AI", [](ecs::EcsWorld& world, float dt) {
        auto monsterQuery = world.QueryEntities<ecs::PositionComponent, ecs::AIComponent>();
        for (auto [monster, pos, ai] : monsterQuery) {
            if (ai.behaviorType == AIBehaviorType::None) continue;

            float closestDist = ai.aggroRadius;
            ecs::EntityHandle target = ecs::INVALID_ENTITY;
            auto playerQuery = world.QueryEntities<ecs::PositionComponent, ecs::PlayerTag>();
            for (auto [player, pPos, tag] : playerQuery) {
                (void)tag;
                float dx = pPos.x - pos.x;
                float dz = pPos.z - pos.z;
                float dist = std::sqrt(dx * dx + dz * dz);
                if (dist < closestDist) {
                    closestDist = dist;
                    target = player;
                }
            }

            if (target != ecs::INVALID_ENTITY) {
                auto* targetPos = world.GetComponent<ecs::PositionComponent>(target);
                if (targetPos) {
                    float dx = targetPos->x - pos.x;
                    float dz = targetPos->z - pos.z;
                    float dist = std::sqrt(dx * dx + dz * dz);
                    if (dist > 0.01f) {
                        float speed = ai.moveSpeed * dt;
                        pos.x += (dx / dist) * speed;
                        pos.z += (dz / dist) * speed;
                    }
                }
            }
        }
    });

    AddLog("[ECS] {} Systeme registriert", gEcsWorld->GetSystemCount());
}

// =============================================================================
// ECS INITIALISIERUNG
// =============================================================================
static bool InitializeEcs() {
    gEcsWorld = std::make_unique<ecs::EcsWorld>();
    if (!gEcsWorld->Initialize()) {
        AddLog("[ECS] Initialisierung fehlgeschlagen!");
        gEcsWorld.reset();
        return false;
    }

    RegisterEcsSystems();
    gUseEcs = true;
    AddLog("[ECS] Archetype-ECS initialisiert und aktiv");
    return true;
}

// =============================================================================
// ECS-LEGACY-SYNC
// =============================================================================
static void SyncLegacyToEcs() {
    if (!gUseEcs || !gEcsWorld) return;

    for (const auto& legacy : serverRegistry) {
        auto query = gEcsWorld->QueryEntities<ecs::LegacyIdComponent>();
        bool exists = false;
        for (auto [e, legacyId] : query) {
            if (legacyId.legacyId == legacy.id) {
                exists = true;
                break;
            }
        }
        if (exists) continue;

        ecs::EntityHandle entity = gEcsWorld->CreateEntity();
        gEcsWorld->AddComponent(entity, ecs::LegacyIdComponent{legacy.id});
        gEcsWorld->AddComponent(entity, ecs::PositionComponent{
            legacy.transform.x, legacy.transform.y, legacy.transform.z
        });
        gEcsWorld->AddComponent(entity, ecs::RotationComponent{0.0f, 0.0f, 0.0f});
        gEcsWorld->AddComponent(entity, ecs::ScaleComponent{1.0f, legacy.render.scaleY, 1.0f});
        gEcsWorld->AddComponent(entity, ecs::VelocityComponent{0.0f, 0.0f, 0.0f});
        gEcsWorld->AddComponent(entity, ecs::HealthComponent{
            legacy.currentHP, legacy.maxHP, legacy.currentHP > 0
        });
        gEcsWorld->AddComponent(entity, ecs::NameComponent{legacy.name});
        gEcsWorld->AddComponent(entity, ecs::RenderComponentECS{
            legacy.render.materialId, legacy.render.meshId
        });

        if (legacy.isMonster) {
            gEcsWorld->AddComponent(entity, ecs::AIComponent{
                AIBehaviorType::Aggressive, 10.0f, 2.0f, 0.0f
            });
        } else {
            gEcsWorld->AddComponent(entity, ecs::PlayerTag{});
        }
    }
}

// =============================================================================
// INITIALISIERUNG
// =============================================================================
static bool InitEngine() {
    LogInit();
    AddLog("========================================");
    AddLog(" TheSeed Engine V13.2 gestartet");
    AddLog(" C++23 | ECS | Vulkan | Multi-Threaded");
    AddLog("========================================");

    InitDatabase("theseed.db");

    if (!InitializeEcs()) {
        AddLog("[Warn] ECS-Initialisierung fehlgeschlagen, nutze Legacy-Modus");
    }

    LoadSkillsFromCSV("data/skills.csv");
    LoadQuestsFromCSV("data/quests.csv");
    LoadNpcsFromCSV("data/npcs.csv");
    LoadItemsFromCSV("data/items.csv");
    LoadMonsterTemplatesFromCSV("data/monsters.csv");

    InitWorld();

    ServerInit(54000);
    AddLog("[Server] Lauscht auf Port 54000 (TCP Legacy)");

    gThreadPool = std::make_unique<ThreadPool>(
        std::max(2u, std::thread::hardware_concurrency()));
    AddLog("[ThreadPool] {} Worker-Threads initialisiert",
           std::max(2u, std::thread::hardware_concurrency()));

    return true;
}

// =============================================================================
// SHUTDOWN
// =============================================================================
static void ShutdownEngine() {
    gRunning = false;

    if (gMultiServer) gMultiServer->Stop();
    if (gNetworkServer) gNetworkServer->Stop();
    if (gThreadPool) gThreadPool.reset();
    if (gEcsWorld) {
        gEcsWorld->Shutdown();
        gEcsWorld.reset();
    }

    ShutdownDatabase();
    LogShutdown();
}

// =============================================================================
// MAIN LOOP
// =============================================================================
int main(int argc, char* argv[]) {
    (void)argc; (void)argv;

    glfwSetErrorCallback(GlfwErrorCallback);
    if (!glfwInit()) {
        AddLog("[GLFW] Initialisierung fehlgeschlagen!");
        return -1;
    }

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 6);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    GLFWwindow* window = glfwCreateWindow(gWindowWidth, gWindowHeight,
        "TheSeed V13.2", nullptr, nullptr);
    if (!window) {
        AddLog("[GLFW] Fenstererstellung fehlgeschlagen!");
        glfwTerminate();
        return -1;
    }

    glfwMakeContextCurrent(window);
    glfwSetKeyCallback(window, GlfwKeyCallback);
    glfwSetFramebufferSizeCallback(window, GlfwFramebufferSizeCallback);
    glfwSwapInterval(1);

    if (!InitEngine()) {
        AddLog("[Engine] Initialisierung fehlgeschlagen!");
        glfwDestroyWindow(window);
        glfwTerminate();
        return -1;
    }

    GLuint grassTex = 0, rockTex = 0;
    RendererInit(grassTex, rockTex);
    EditorInit(window);

    auto lastTime = std::chrono::steady_clock::now();
    float accumulator = 0.0f;
    constexpr float FIXED_DT = 1.0f / 60.0f;

    while (gRunning && !glfwWindowShouldClose(window)) {
        glfwPollEvents();

        auto now = std::chrono::steady_clock::now();
        float deltaTime = std::chrono::duration<float>(now - lastTime).count();
        lastTime = now;
        accumulator += deltaTime;

        while (accumulator >= FIXED_DT) {
            if (!serverRegistry.empty()) {
                auto& hero = serverRegistry[0];
                hero.transform.lerpX = hero.transform.x;
                hero.transform.lerpZ = hero.transform.z;
            }

            // ECS UPDATE (P1-FIX)
            if (gUseEcs && gEcsWorld) {
                gEcsWorld->Update(FIXED_DT);
                SyncLegacyToEcs();
            }

            // ThreadPool ECS (P4)
            if (gThreadPool && gUseEcs) {
                gThreadPool->ExecuteEcsSystems(*gEcsWorld, FIXED_DT);
            }

            UpdateSkillCooldowns(FIXED_DT);
            UpdateStatusEffects(FIXED_DT);

            ProcessServerTick();
            accumulator -= FIXED_DT;
        }

        ProcessClientTick();

        glClearColor(0.1f, 0.15f, 0.2f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        for (const auto& ent : clientRegistry) {
            (void)ent;
        }

        if (gShowEditor) {
            EditorFrame(window);
        }

        if (gShowMetrics) {
            ImGui::Begin("Engine Metrics", &gShowMetrics);
            ImGui::Text("FPS: %.1f", 1.0f / deltaTime);
            ImGui::Text("Delta: %.3f ms", deltaTime * 1000.0f);
            ImGui::Text("Entities (Legacy): %zu", serverRegistry.size());
            if (gEcsWorld) {
                ImGui::Text("Entities (ECS): %zu", gEcsWorld->GetEntityCount());
                ImGui::Text("Archetypes: %zu", gEcsWorld->GetArchetypeCount());
                ImGui::Text("Chunks: %zu", gEcsWorld->GetTotalChunkCount());
                ImGui::Text("ECS Memory: %.2f MB", gEcsWorld->GetTotalMemoryUsage() / (1024.0f * 1024.0f));
            }
            ImGui::Text("ECS-Modus: %s", gUseEcs ? "AKTIV" : "LEGACY");
            if (gNetworkServer) {
                ImGui::Text("Net Clients: %u", gNetworkServer->GetClientCount());
            }
            if (gThreadPool) {
                ImGui::Text("ThreadPool Tasks: %zu", gThreadPool->GetPendingCount());
                ImGui::Text("Avg Task Time: %.2f us", gThreadPool->GetAverageTaskTimeUs());
            }
            ImGui::End();
        }

        if (gShowDemo) {
            ImGui::ShowDemoWindow(&gShowDemo);
        }

        glfwSwapBuffers(window);
        std::this_thread::sleep_for(std::chrono::milliseconds(16));
    }

    EditorShutdown();
    RendererShutdown(grassTex, rockTex);
    ShutdownEngine();
    glfwDestroyWindow(window);
    glfwTerminate();
    AddLog("[Engine] Beendet.");
    return 0;
}
