// =============================================================================
// main.cpp — Engine Entry Point V13.2
// THE SEED — EditorFirst Runtime mit ECS + Vulkan (C++23)
// =============================================================================
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <winsock2.h>
#pragma comment(lib, "ws2_32.lib")
#pragma comment(lib, "sqlite3.lib")

#include "editor/EditorRuntime.h"
#include "core/GameSystems.h"
#include "core/Database.h"
#include "core/World.h"
#include "core/Log.h"
#include "server/Server.h"
#include "server/Network.h"
#include "client/Connection.h"
#include "client/ClientTick.h"
#include "client/Renderer.h"

// ECS-Integration (AP-20–AP-23)
#include "ecs/ecs_ECS.h"
#include "ecs/ecs_EcsWorld.h"
#include "ecs/Components.h"

// Vulkan Renderer (AP-01)
#if defined(ENGINE_VULKAN_BACKEND)
#include "renderer_vulkan/VulkanRenderer.h"
#endif

#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <memory>

// =============================================================================
// GLOBALS
// =============================================================================
std::unique_ptr<ecs::EcsWorld> gEcsWorld;
bool gUseEcs = false;
std::unique_ptr<GameServer> gGameServer;

#if defined(ENGINE_VULKAN_BACKEND)
std::unique_ptr<render::VulkanRenderer> gVulkanRenderer;
#endif

// =============================================================================
// ECS MIGRATION: V13.1 Entity → ECS Entity
// =============================================================================
void MigrateV13EntityToEcs(const Entity& oldEnt, ecs::EntityHandle newHandle) {
    gEcsWorld->AddComponent(newHandle, game::Transform{
        .x = oldEnt.transform.x,
        .y = oldEnt.transform.y,
        .z = oldEnt.transform.z,
        .targetX = oldEnt.transform.targetX,
        .targetZ = oldEnt.transform.targetZ,
        .lerpX = oldEnt.transform.lerpX,
        .lerpZ = oldEnt.transform.lerpZ
    });
    gEcsWorld->AddComponent(newHandle, game::Health{
        .current = oldEnt.currentHP,
        .max = oldEnt.maxHP
    });
    gEcsWorld->AddComponent(newHandle, game::Renderable{
        .materialId = oldEnt.render.materialId,
        .scaleY = oldEnt.render.scaleY,
        .meshId = oldEnt.render.meshId
    });

    if (oldEnt.isMonster) {
        gEcsWorld->AddComponent(newHandle, game::MonsterTag{.templateId = oldEnt.monsterTemplateId});
    }
}

// =============================================================================
// MAIN
// =============================================================================
int main() {
    // --- Netzwerk Init ---
    if (!ServerInit(54000)) return -1;
    if (!ClientConnect("127.0.0.1", 54000)) return -1;

    // --- NEW: UDP GameServer ---
    gGameServer = std::make_unique<GameServer>();
    if (!gGameServer->Startup(54001, true)) {  // UDP on port 54001
        AddLog("[Main] Failed to start UDP game server");
    }

    // --- GLFW / Window ---
    if (!glfwInit()) return -1;

    #if defined(ENGINE_VULKAN_BACKEND)
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    #endif

    GLFWwindow* window = glfwCreateWindow(
        1280, 720, "THE SEED — V13.2 Vulkan/ECS (C++23)", nullptr, nullptr);
    if (!window) { glfwTerminate(); return -1; }

    #if !defined(ENGINE_VULKAN_BACKEND)
    glfwMakeContextCurrent(window);
    #endif

    // --- ECS World Init (AP-20) ---
    gEcsWorld = std::make_unique<ecs::EcsWorld>();
    AddLog("[ECS] Archetype-World initialisiert.");

    // --- Daten laden ---
    LoadSkillsFromCSV(
        "id,name,range,fov,damage,cooldown,statusEffectType,statusEffectDur,tickDmg\n"
        "1,FlameStrike,6.5,60.0,40,2.0,0,5.0,5\n"
        "2,Whirlwind,3.5,360.0,25,1.5,1,3.0,0\n"
        "3,Thunderclap,4.0,180.0,15,3.0,2,2.0,0\n");

    LoadQuestsFromCSV(
        "questId,title,description,objType,objTargetId,objRequired,rewardGold,rewardXP\n"
        "1,Schleim-Jaeger,Toete 3 Slimy,0,101,3,50,100\n"
        "2,Der Waechter,Sprich mit dem Dorfwaechter,2,1,1,20,50\n");

    LoadNpcsFromCSV(
        "id,name,x,z,greeting,offeredQuestId\n"
        "1,Dorfwaechter,5.0,-3.0,Halt! Wer bist du?,2\n"
        "2,Haendlerin,-8.0,4.0,Kauft Ihr Waren?,0\n");

    LoadItemDatabase();
    GameDB.emplace();

    // --- Renderer ---
    #if defined(ENGINE_VULKAN_BACKEND)
    gVulkanRenderer = std::make_unique<render::VulkanRenderer>();
    render::VulkanConfig vkConfig;
    vkConfig.enableValidation = true;
    vkConfig.vsync = true;
    if (!gVulkanRenderer->Init(window, 1280, 720, vkConfig)) {
        AddLog("[Main] Vulkan initialization failed, falling back to OpenGL");
        gVulkanRenderer.reset();
    } else {
        AddLog("[Main] Vulkan renderer active");
    }
    #endif

    GLuint grassTex = 0, rockTex = 0;
    if (!gVulkanRenderer) {
        RendererInit(grassTex, rockTex);  // OpenGL fallback
    }

    // --- Editor ---
    #if ENGINE_BUILD_EDITOR
    EditorInit(window);
    #endif

    // --- HAUPTSCHLEIFE ---
    while (!glfwWindowShouldClose(window)) {
        glfwPollEvents();

        // Server-Tick (Legacy + UDP)
        ProcessServerTick(RebuildTerrainMeshOnGPU);
        if (gGameServer && gGameServer->IsRunning()) {
            gGameServer->Tick(0.016f);  // ~60Hz
        }

        // ECS-System-Preview (AP-22)
        if (gUseEcs && gEcsWorld) {
            auto query = gEcsWorld->Query<ecs::All<game::Transform, game::Health>>();
            // TODO: Run ECS systems here
            (void)query;
        }

        ProcessClientTick();

        #if ENGINE_BUILD_EDITOR
        EditorFrame(window);
        #endif

        // Render
        #if defined(ENGINE_VULKAN_BACKEND)
        if (gVulkanRenderer) {
            gVulkanRenderer->BeginFrame();
            gVulkanRenderer->DrawTriangle(gVulkanRenderer->GetCurrentCommandBuffer());
            gVulkanRenderer->EndFrame();
        } else
        #endif
        {
            glfwSwapBuffers(window);  // OpenGL
        }
    }

    // --- Shutdown ---
    ExecutePlayerLogout();
    ProjectSaveAllDirty();
    GameDB.reset();

    #if ENGINE_BUILD_EDITOR
    EditorShutdown();
    #endif

    if (!gVulkanRenderer) {
        RendererShutdown(grassTex, rockTex);
    }
    gVulkanRenderer.reset();

    ClientDisconnect();
    ServerShutdown();
    if (gGameServer) {
        gGameServer->Shutdown();
        gGameServer.reset();
    }

    gEcsWorld.reset();

    glfwDestroyWindow(window);
    glfwTerminate();
    return 0;
}
