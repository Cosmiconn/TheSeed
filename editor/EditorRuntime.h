#pragma once
// =============================================================================
// editor/EditorRuntime.h  —  C++23 Modernized
// Editor-Loop Deklarationen. main() ist jetzt in main.cpp
// =============================================================================
#if ENGINE_BUILD_EDITOR

#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"
#include <GLFW/glfw3.h>

#include "EditorPanels.h"
#include "EntityEditor.h"
#include "DataDefinitionEditor.h"
#include "CommandSystem.h"
#include "AssetDatabase.h"

#include "../core/World.h"
#include "../core/GameSystems.h"
#include "../core/Database.h"
#include "../core/Log.h"
#include "../core/EventSystem.h"
#include "../core/EventTypes.h"
#include "../server/Network.h"
#include "../server/Server.h"
#include "../client/Connection.h"
#include "../client/Renderer.h"
#include "../client/ClientTick.h"
#include <format>
#include <string_view>

// =============================================================================
// EDITOR INIT / SHUTDOWN
// =============================================================================
void EditorInit(GLFWwindow* window);
void EditorShutdown();

// =============================================================================
// EDITOR FRAME
// =============================================================================
void EditorFrame(GLFWwindow* window);

#endif // ENGINE_BUILD_EDITOR
