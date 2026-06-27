#pragma once
// =============================================================================
// editor/DataDefinitionEditor.h  —  C++23 Modernized
// Deklarationen only. Implementation in DataDefinitionEditor.cpp
// =============================================================================
#if ENGINE_BUILD_EDITOR

#include "imgui.h"
#include "AssetDatabase.h"
#include "CommandSystem.h"
#include "../core/World.h"
#include "../core/ECS.h"
#include "../core/GameSystems.h"
#include "../core/Log.h"
#include <algorithm>
#include <ranges>
#include <format>

// =============================================================================
// INDIVIDUAL EDITORS
// =============================================================================
void DrawQuestEditor();
void DrawNpcEditor();
void DrawItemEditor();
void DrawSkillEditor();

// =============================================================================
// COMBINED DATA DEFINITIONS PANEL
// =============================================================================
void DrawDataDefinitionsPanel();

#endif // ENGINE_BUILD_EDITOR
