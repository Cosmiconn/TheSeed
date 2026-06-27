#pragma once
// =============================================================================
// server/Server.h  —  C++23 Modernized
// Nur Deklarationen. Implementation in Server.cpp
// =============================================================================
#include "Network.h"
#include "PacketHandler.h"
#include "Validation.h"
#include "../core/GameSystems.h"
#include "../core/Database.h"
#include "../core/World.h"
#include "../core/Log.h"
#include "../core/EventSystem.h"
#include "../core/EventTypes.h"

#include <algorithm>
#include <cmath>
#include <ranges>
#include <format>
#include <string_view>

// =============================================================================
// AUTOSAVE-TIMER
// =============================================================================
extern int dbFlushTimer;

// =============================================================================
// SPIELER-LOGOUT
// =============================================================================
void ExecutePlayerLogout();

// =============================================================================
// HAUPT-SERVER-TICK
// =============================================================================
void ProcessServerTick(std::move_only_function<void()> rebuildGPU = [](){});
