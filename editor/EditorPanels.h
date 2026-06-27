#pragma once
// =============================================================================
// editor/EditorPanels.h  —  C++23 Modernized + Event Monitor
// Deklarationen only. Implementation in EditorPanels.cpp
// =============================================================================
#if ENGINE_BUILD_EDITOR

#include "imgui.h"
#include "CommandSystem.h"
#include "AssetDatabase.h"
#include "EntityEditor.h"
#include "DataDefinitionEditor.h"
#include "../core/World.h"
#include "../core/Log.h"
#include "../core/ByteBuffer.h"
#include "../core/Database.h"
#include "../core/EventSystem.h"
#include "../core/EventTypes.h"
#include "../client/Connection.h"
#include "../client/Renderer.h"
#include "../server/Network.h"
#include "../server/Validation.h"
#include <chrono>
#include <string>
#include <mutex>
#include <deque>
#include <cmath>
#include <ranges>
#include <format>
#include <string_view>

// =============================================================================
// EVENT MONITOR
// =============================================================================
struct EventLogEntry {
    std::string timestamp;
    std::string category;
    std::string message;
    ImVec4      color;
};

extern std::deque<EventLogEntry> gEventLog;
extern std::mutex                gEventLogMutex;
inline constexpr size_t          EVENT_LOG_MAX = 200;

void AddEventLog(std::string_view category, std::string_view msg, ImVec4 color);
void InitEventSubscriptions();

// =============================================================================
// PANEL DRAW FUNCTIONS
// =============================================================================
void DrawPanelLog();
void DrawPanelQuestsNPCsTrade(uint32_t playerId);
void DrawPanelTerrain();
void DrawPanelInventarSecurity(uint32_t playerId);
void DrawPanelEventMonitor();

#endif // ENGINE_BUILD_EDITOR
