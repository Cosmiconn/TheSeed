#pragma once
// =============================================================================
// server/Validation.h  —  C++23 Modernized
// Deklarationen only. Implementation in Validation.cpp
// =============================================================================
#include "Network.h"
#include "../core/World.h"
#include "../core/Log.h"
#include "../core/ByteBuffer.h"
#include "../core/Database.h"

#include <algorithm>
#include <mutex>
#include <ranges>
#include <format>
#include <string_view>

// =============================================================================
// CHAT – Broadcast & Whisper
// =============================================================================
void AddChatMessage(std::string_view sender,
                    std::string_view text,
                    uint32_t         channel = 0);

void SendWhisper(ClientSession&     senderSession,
                 std::string_view targetName,
                 std::string_view text);

// =============================================================================
// ITEM VALIDATION & EQUIP
// =============================================================================
[[nodiscard]] bool EquipItem(uint32_t playerId, uint32_t slotIndex);
void SendInventoryToSession(uint32_t playerId);

// =============================================================================
// HANDEL – Sync & Validation
// =============================================================================
void SyncTradeState(const TradeSession& ts);
void ExecuteTradeValidation(TradeSession& ts);
