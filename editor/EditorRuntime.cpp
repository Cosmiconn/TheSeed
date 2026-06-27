// =============================================================================
// editor/EditorRuntime.cpp  —  EditorRuntime Implementation
// =============================================================================
#include "EditorRuntime.h"

#if ENGINE_BUILD_EDITOR

void EditorInit(GLFWwindow* window) {
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGui::StyleColorsDark();
    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init("#version 130");
    ProjectSetContext("TheSeed_Default", "./project/");
    InitEventSubscriptions();
    AddLog("[Editor] EditorFirst Runtime initialisiert.");
}

void EditorShutdown() {
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
}

void EditorFrame(GLFWwindow* window) {
    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();

    const uint32_t playerId =
        serverRegistry.empty() ? 0 : serverRegistry[0].id;

    DrawPanelLog();
    DrawPanelQuestsNPCsTrade(playerId);
    DrawPanelTerrain();
    DrawPanelInventarSecurity(playerId);
    DrawWorldOutliner();
    DrawComponentInspector();
    DrawDataDefinitionsPanel();
    DrawPanelEventMonitor();

    if (ImGui::BeginMainMenuBar()) {
        if (ImGui::BeginMenu("Project")) {
            if (ImGui::MenuItem("Save All")) ProjectSaveAllDirty();
            if (ImGui::MenuItem("Reload All")) {
                LoadQuestsFromCSV(gProjectPath + "quests.csv");
                LoadNpcsFromCSV(gProjectPath + "npcs.csv");
                LoadSkillsFromCSV(gProjectPath + "skills.csv");
                AddLog("[Editor] Full project reload executed.");
            }
            ImGui::Separator();
            if (ImGui::MenuItem("Clear Undo History")) CmdClear();
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu("Edit")) {
            if (ImGui::MenuItem("Undo", "Ctrl+Z", false, CmdCanUndo())) CmdUndo();
            if (ImGui::MenuItem("Redo", "Ctrl+Y", false, CmdCanRedo())) CmdRedo();
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu("Spawn")) {
            if (ImGui::MenuItem("Slimy at Camera")) {
                Entity e;
                e.id = nextEntityId++;
                e.isMonster = true; e.name = "Slimy"; e.monsterTemplateId = 101;
                e.currentHP = 100;
                e.transform.x = e.transform.targetX = e.transform.lerpX = cameraPos.x;
                e.transform.z = e.transform.targetZ = e.transform.lerpZ = cameraPos.z;
                e.transform.y = GetHeightFromGrid(cameraPos.x, cameraPos.z) + 0.5f;
                e.render = {"mat_slimy", 0.6f, "cube"};
                CmdExecute(std::make_unique<CmdSpawnEntity>(e));
            }
            if (ImGui::MenuItem("Orc at Camera")) {
                Entity e;
                e.id = nextEntityId++;
                e.isMonster = true; e.name = "Orc"; e.monsterTemplateId = 102;
                e.currentHP = 100;
                e.transform.x = e.transform.targetX = e.transform.lerpX = cameraPos.x;
                e.transform.z = e.transform.targetZ = e.transform.lerpZ = cameraPos.z;
                e.transform.y = GetHeightFromGrid(cameraPos.x, cameraPos.z) + 0.5f;
                e.render = {"mat_orc", 1.2f, "pyramid"};
                CmdExecute(std::make_unique<CmdSpawnEntity>(e));
            }
            ImGui::EndMenu();
        }
        ImGui::EndMainMenuBar();
    }

    ImGui::Render();
    int dw, dh;
    glfwGetFramebufferSize(window, &dw, &dh);
    glViewport(0, 0, dw, dh);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
}

#endif // ENGINE_BUILD_EDITOR
