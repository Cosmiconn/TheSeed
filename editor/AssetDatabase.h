#pragma once
// =============================================================================
// editor/AssetDatabase.h  —  C++23 Modernized
// Deklarationen only. Implementation in AssetDatabase.cpp
// =============================================================================
#include "../core/Types.h"
#include "../core/ECS.h"
#include "../core/World.h"
#include "../core/Log.h"
#include <map>
#include <string>
#include <vector>
#include <memory>
#include <fstream>
#include <sstream>
#include <chrono>
#include <mutex>
#include <ranges>
#include <format>
#include <string_view>

// =============================================================================
// ASSET TYPES
// =============================================================================
enum class AssetType : uint8_t {
    Unknown     = 0,
    Quest       = 1,
    NPC         = 2,
    Item        = 3,
    Skill       = 4,
    SpawnPreset = 5,
    Material    = 6,
    EntityPrefab= 7
};

struct AssetMeta {
    uint32_t    id          = 0;
    AssetType   type        = AssetType::Unknown;
    std::string name;
    std::string sourcePath;
    bool        isDirty     = false;
    bool        isEmbedded  = true;
    std::chrono::steady_clock::time_point lastWrite;
};

// =============================================================================
// ASSET DATABASE
// =============================================================================
extern std::map<uint32_t, AssetMeta>       gAssetMeta;
extern std::map<std::string, uint32_t>     gAssetNameToId;
extern uint32_t                              gNextAssetId;
extern std::mutex                            gAssetDbMutex;

[[nodiscard]] uint32_t AssetRegister(AssetType type, std::string_view name,
                                     std::string_view sourcePath = "");
[[nodiscard]] AssetMeta* AssetGetMeta(uint32_t id);
void AssetMarkDirty(uint32_t id);
void AssetClearDirty(uint32_t id);

// =============================================================================
// DATA-DRIVEN EXPORT / IMPORT
// =============================================================================
void ExportQuestsToCSV(std::string_view path);
void ExportNpcsToCSV(std::string_view path);
void ExportItemsToCSV(std::string_view path);
void ExportSkillsToCSV(std::string_view path);

// =============================================================================
// PROJECT CONTEXT
// =============================================================================
extern std::string gProjectName;
extern std::string gProjectPath;

void ProjectSetContext(std::string_view name, std::string_view path);
void ProjectSaveAllDirty();
