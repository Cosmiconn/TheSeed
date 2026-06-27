// =============================================================================
// main.cpp  —  Engine Entry Point
// THE SEED — EditorFirst Runtime V13.1 (C++23)
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
#include "client/Renderer.h"
#include "client/ClientTick.h"
#include <GLFW/glfw3.h>

int main() {
    if (!ServerInit(54000)) return -1;
    if (!ClientConnect("127.0.0.1", 54000)) return -1;

    if (!glfwInit()) return -1;
    GLFWwindow* window = glfwCreateWindow(
        1280, 720, "THE SEED — EditorFirst Runtime V13.1 (C++23)", nullptr, nullptr);
    if (!window) { glfwTerminate(); return -1; }
    glfwMakeContextCurrent(window);

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

    GLuint grassTex = 0, rockTex = 0;
    RendererInit(grassTex, rockTex);

#if ENGINE_BUILD_EDITOR
    EditorInit(window);
#endif

    while (!glfwWindowShouldClose(window)) {
        glfwPollEvents();
        ProcessServerTick(RebuildTerrainMeshOnGPU);
        ProcessClientTick();
#if ENGINE_BUILD_EDITOR
        EditorFrame(window);
#endif
        glfwSwapBuffers(window);
    }

    ExecutePlayerLogout();
    ProjectSaveAllDirty();
    GameDB.reset();

#if ENGINE_BUILD_EDITOR
    EditorShutdown();
#endif

    RendererShutdown(grassTex, rockTex);
    ClientDisconnect();
    ServerShutdown();
    glfwDestroyWindow(window);
    glfwTerminate();
    return 0;
}
