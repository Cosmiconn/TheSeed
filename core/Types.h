#pragma once
// =============================================================================
// core/Types.h  —  C++23 Modernized
// std::to_underlying, std::string_view, constexpr, using enum
// =============================================================================
#include <cstdint>
#include <string>
#include <cstring>
#include <utility>      // std::to_underlying (C++23)
#include <string_view>
#include <format>

// ---------------------------------------------------------------------------
// Build-Flags
// ---------------------------------------------------------------------------
#define ENGINE_BUILD_EDITOR 1
#define MAX_CLIENTS         64

// ---------------------------------------------------------------------------
// Netzwerk-Opcodes
// ---------------------------------------------------------------------------
enum class PacketType : uint8_t {
    MSG_MOVE_REQ         = 0,
    MSG_MOVE_NOTIFY      = 1,
    MSG_CAST_SKILL       = 2,
    MSG_COMBAT_NOTIFY    = 3,
    MSG_QUEST_ACCEPT     = 10,
    MSG_QUEST_UPDATE     = 11,
    MSG_QUEST_COMPLETE   = 12,
    MSG_ENTITY_SPAWN     = 20,
    MSG_ENTITY_DESPAWN   = 21,
    MSG_CHAT             = 30,
    MSG_INVENTORY_UPDATE = 40,
    MSG_EQUIP_REQ        = 41,
    MSG_TRADE_REQ        = 50,
    MSG_TRADE_ACCEPT     = 51,
    MSG_TRADE_ADD_ITEM   = 52,
    MSG_TRADE_CONFIRM    = 53,
    MSG_TRADE_STATE      = 54,
    MSG_SECTOR_SWITCH    = 60,
    MSG_TALK_NPC         = 61,
    MSG_MOVE_LERP        = 62,
    MSG_WHISPER          = 63,
    MSG_WHISPER_NOTIFY   = 64,
    MSG_STATUS_EFFECT    = 65
};

// ---------------------------------------------------------------------------
// Quest-Typen
// ---------------------------------------------------------------------------
enum class QuestState         : uint8_t { Inactive=0, Active=1, Completed=2, Failed=3 };
enum class QuestObjectiveType : uint8_t { KillMonster=0, ReachZone=1, TalkToNPC=2 };

// ---------------------------------------------------------------------------
// Statuseffekte
// ---------------------------------------------------------------------------
enum class StatusEffectType : uint8_t {
    Poison = 0,
    Slow   = 1,
    Stun   = 2
};

// ---------------------------------------------------------------------------
// Welt-Konstanten
// ---------------------------------------------------------------------------
inline constexpr int   GRID_SIZE         = 40;
inline constexpr float SECTOR_WORLD_SIZE = 40.0f;
inline constexpr float AOI_RADIUS        = 15.0f;
inline constexpr float TICK_DELTA        = 0.016f;

// ---------------------------------------------------------------------------
// Interpolations-Konstanten (Client)
// ---------------------------------------------------------------------------
inline constexpr float LERP_SPEED_REMOTE = 8.0f;
inline constexpr float LERP_SPEED_SELF   = 20.0f;

// ---------------------------------------------------------------------------
// Inventar
// ---------------------------------------------------------------------------
inline constexpr size_t INVENTORY_SIZE = 20;

// ---------------------------------------------------------------------------
// Hilfsfunktionen
// ---------------------------------------------------------------------------
inline void SafeStrCopy(char* dst, std::string_view src, size_t dstSize) {
    if (dstSize == 0) return;
    const size_t copyLen = (src.size() < dstSize - 1) ? src.size() : dstSize - 1;
    std::memcpy(dst, src.data(), copyLen);
    dst[copyLen] = '\0';
}

[[nodiscard]] inline std::string GetSectorName(int x, int z) {
    return std::format("Sektor_{}_{}", x, z);
}
