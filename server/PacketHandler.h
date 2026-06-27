#pragma once
// =============================================================================
// server/PacketHandler.h  —  C++23 Modernized
// Nur Deklaration. Implementation in PacketHandler.cpp
// =============================================================================
#include "Network.h"
#include "Validation.h"
#include "../core/GameSystems.h"
#include "../core/World.h"
#include "../core/Log.h"
#include "../core/ByteBuffer.h"
#include "../core/EventSystem.h"
#include "../core/EventTypes.h"

#include <algorithm>
#include <cmath>
#include <ranges>
#include <format>
#include <string_view>

// =============================================================================
// PAKET-VERARBEITUNG
// =============================================================================
void ProcessPacketFromClient(ClientSession& session, std::span<const uint8_t> payload);
