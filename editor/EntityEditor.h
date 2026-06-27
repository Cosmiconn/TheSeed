#pragma once
// =============================================================================
// editor/EntityEditor.h  —  C++23 Modernized
// Deklarationen only. Implementation in EntityEditor.cpp
// =============================================================================
#if ENGINE_BUILD_EDITOR

#include "imgui.h"
#include "CommandSystem.h"
#include "../core/World.h"
#include "../core/ECS.h"
#include "../core/Log.h"
#include "../server/Network.h"
#include <algorithm>
#include <string>
#include <ranges>
#include <format>
#include <string_view>

// =============================================================================
// OUTLINER STATE
// =============================================================================
extern int  gOutlinerSelectedEntityId;
extern char gEntityFilter[32];

// =============================================================================
// DRAW FUNCTIONS
// =============================================================================
void DrawWorldOutliner();
void DrawComponentInspector();

#endif // ENGINE_BUILD_EDITOR
