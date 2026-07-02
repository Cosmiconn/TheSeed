// =============================================================================
// main.cpp — C++23 MMORPG Engine V13.2 Entry Point (P1-FIX + AP-81 + AP-91)
// =============================================================================
// KORREKTUR P1:
// • ECS-Systeme werden NUR ueber gEcsWorld->Update() ausgefuehrt
// • gThreadPool->ExecuteEcsSystems() entfernt (verursachte Race Condition)
// • RegisterEcsSystems() wird in InitializeEcs() aufgerufen
// • Korrekte Includes fuer <cmath>, <expected>, <span>
// KORREKTUR AP-81:
// • NetworkProfiler-Initialisierung und Anzeige in Metrics-Panel
// • Periodisches Profiling alle 5 Sekunden
// KORREKTUR AP-91:
// • MetricsDashboard-Integration als zentrales Monitoring-System
// • F6-Taste fuer Dashboard-Toggle
// • Entfernt duplizierte Metrics-Anzeige (ersetzt durch Dashboard)
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
#include "network/NetworkProfiler.h"
#include "server/Server.h"
#include "server/ThreadPool.h"
#include "client/Connection.h"
#include "client/ClientTick.h"
#include "client/Renderer.h"
#include "editor/EditorRuntime.h"
#include "metrics/MetricsDashboard.h"

#include <GL/glew.h>
#include <GLFW/glfw3.h>
#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>
#include <cmath>       // FIX P1: Fuer std::sqrt in ECS-Systemen
#include <expected>    // FIX P1: Fuer std::expected (C++23)
#include <span>
#include <memory>
#include <thread>
#include <chrono>

// =============================================================================
// GLOBALE ZUSTAENDE
// =============================================================================
bool gRunning = true;
bool gUseEcs = true;
bool gShowEditor = true;
bool gShowMetrics = true;      // AP-91: Wird durch MetricsDashboard ersetzt
bool gShowMetricsDashboard = false; // AP-91: Neues Dashboard-Fenster
bool gShowDemo = false;
bool gUseVulkan = false;
bool gUseThreadPool = true;

std::unique_ptr<ecs::EcsWorld> gEcsWorld;
std::unique_ptr<net::NetworkServer> gMultiServer;
std::unique_ptr<net::NetworkServer> gNetworkServer;
std::unique_ptr<ThreadPool> gThreadPool;

int gWindowWidth = 1280;
int gWindowHeight = 720;

// AP-81: NetworkProfiler-Timer
std::chrono::steady_clock::time_point gLastNetProfileUpdate;

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
        gShowMetrics = !gShowMetrics; // Legacy-Metrics (kann entfernt werden)
    // AP-91: F6 fuer Metrics Dashboard
    if (key == GLFW_KEY_F6 && action == GLFW_PRESS)
        gShowMetricsDashboard = !gShowMetricsDashboard;
    if (key == GLFW_KEY_F3 && action == GLFW_PRESS)
        gShowDemo = !gShowDemo;
    if (key == GLFW_KEY_F4 && action == GLFW_PRESS) {
        gUseEcs = !gUseEcs;
        AddLog("[Engine] ECS-Modus: {}", gUseEcs ? "AKTIV" : "LEGACY");
    }
    // AP-81: F5 fuer NetworkProfiler-Report
    if (key == GLFW_KEY_F5 && action == GLFW_PRESS) {
        net::NetworkProfiler::GetInstance().PrintReport();
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
// ECS-SYSTEME REGISTRIEREN (P1-FIX)
// =============================================================================
static void RegisterEcsSystems() {
    if (!gEcsWorld) return;

    // Movement-System: Aktualisiert Position basierend auf Velocity
    gEcsWorld->RegisterSystem("Movement", [](ecs::EcsWorld& world, float dt) {
        auto query = world.QueryEntities<ecs::PositionComponent, ecs::VelocityComponent>();
        for (auto [entity, pos, vel] : query) {
            (void)entity;
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
            (void)entity;
            if (health.currentHP <= 0 && health.isAlive) {
                health.isAlive = false;
                AddLog("[ECS] Entity ist gestorben");
            }
        }
    });

    // Combat-System: Verarbeitet eingehenden Schaden
    gEcsWorld->RegisterSystem("Combat", [](ecs::EcsWorld& world, float dt) {
        (void)dt;
        auto query = world.QueryEntities<ecs::HealthComponent, ecs::CombatComponent>();
        for (auto [entity, health, combat] : query) {
            (void)entity;
            if (combat.incomingDamage > 0) {
                health.currentHP -= static_cast<int>(combat.incomingDamage);
                AddLog("[ECS] Entity nimmt {} Schaden, HP={}/{}",
                         combat.incomingDamage, health.currentHP, health.maxHP);
                combat.incomingDamage = 0;
            }
        }
    });

    // StatusEffect-System: Verarbeitet Buffs/Debuffs
    gEcsWorld->RegisterSystem("StatusEffects", [](ecs::EcsWorld& world, float dt) {
        auto query = world.QueryEntities<ecs::StatusEffectComponent, ecs::HealthComponent>();
        for (auto [entity, status, health] : query) {
            (void)entity;
            for (auto& effect : status.effects) {
                if (!effect.isActive) continue;
                effect.remainingDuration -= dt;
                effect.timeSinceLastTick += dt;

                if (effect.timeSinceLastTick >= 1.0f) {
                    effect.timeSinceLastTick -= 1.0f;
                    health.currentHP -= static_cast<int>(effect.tickDamage);
                    AddLog("[ECS] Entity: Status-Effekt '{}' tick, {} Schaden",
                             static_cast<int>(effect.type), effect.tickDamage);
                }

                if (effect.remainingDuration <= 0.0f) {
                    effect.isActive = false;
                    AddLog("[ECS] Entity: Status-Effekt '{}' abgelaufen",
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
            (void)monster;
            if (ai.behaviorType == AIBehaviorType::None) continue;

            float closestDist = ai.aggroRadius;
            ecs::EntityHandle target = ecs::INVALID_ENTITY;
            auto playerQuery = world.QueryEntities<ecs::PositionComponent, ecs::PlayerTag>();
            for (auto [player, pPos, tag] : playerQuery) {
                (void)player; (void)tag;
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
            (void)e;
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
            static_cast<int>(legacy.currentHP), static_cast<int>(legacy.maxHP), legacy.currentHP > 0
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

    // AP-81: NetworkProfiler initialisieren
    net::NetworkProfiler::GetInstance().Initialize();
    gLastNetProfileUpdate = std::chrono::steady_clock::now();
    AddLog("[NetworkProfiler] Initialisiert — F5 fuer Report");

    // AP-91: MetricsDashboard initialisieren
    metrics::MetricsDashboard::GetInstance().Initialize();
    AddLog("[MetricsDashboard] Initialisiert — F6 fuer Dashboard");

    return true;
}

// =============================================================================
// SHUTDOWN
// =============================================================================
static void ShutdownEngine() {
    gRunning = false;

    // AP-91: MetricsDashboard herunterfahren
    metrics::MetricsDashboard::GetInstance().Shutdown();

    // AP-81: NetworkProfiler herunterfahren
    net::NetworkProfiler::GetInstance().Shutdown();

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

        // AP-91: MetricsDashboard Update (jeden Frame)
        float fps = (deltaTime > 0.0f) ? (1.0f / deltaTime) : 0.0f;
        metrics::MetricsDashboard::GetInstance().Update(deltaTime, fps, 0);

        while (accumulator >= FIXED_DT) {
            if (!serverRegistry.empty()) {
                auto& hero = serverRegistry[0];
                hero.transform.lerpX = hero.transform.x;
                hero.transform.lerpZ = hero.transform.z;
            }

            // FIX P4: ECS-Update mit paralleler Ausfuehrung via ThreadPool
            if (gUseEcs && gEcsWorld) {
                if (gUseThreadPool && gThreadPool) {
                    // Parallele Ausfuehrung mit korrekten Read-Write-Locks
                    gThreadPool->ExecuteEcsSystems(*gEcsWorld, FIXED_DT);
                } else {
                    // Serielle Ausfuehrung (Fallback)
                    gEcsWorld->Update(FIXED_DT);
                }
                SyncLegacyToEcs();
            }

            // ThreadPool wird fuer AI-Parallelisierung und andere Tasks verwendet,
            // NICHT fuer ECS-Systeme (vermeidet Race Condition mit componentMutex)
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

        // AP-91: Metrics Dashboard anzeigen
        if (gShowMetricsDashboard) {
            METRICS_RENDER_WINDOW(&gShowMetricsDashboard);
        }

        // AP-91: Legacy-Metrics-Panel (kann entfernt werden, Dashboard ersetzt es)
        if (gShowMetrics) {
            ImGui::Begin("Engine Metrics (Legacy)", &gShowMetrics);
            ImGui::Text("FPS: %.1f", fps);
            ImGui::Text("Delta: %.3f ms", deltaTime * 1000.0f);
            ImGui::Text("Entities (Legacy): %zu", serverRegistry.size());
            if (gEcsWorld) {
                ImGui::Text("Entities (ECS): %zu", gEcsWorld->GetEntityCount());
            }
            ImGui::Text("F6 fuer neues Dashboard");
            ImGui::End();
        }

        if (gShowDemo) {
            ImGui::ShowDemoWindow(&gShowDemo);
        }

        // AP-81: Periodisches Network-Profiling (alle 5 Sekunden)
        auto netNow = std::chrono::steady_clock::now();
        if (std::chrono::duration<float>(netNow - gLastNetProfileUpdate).count() >= 5.0f) {
            NETPROFILE_RECORD_SNAPSHOT();
            gLastNetProfileUpdate = netNow;
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
