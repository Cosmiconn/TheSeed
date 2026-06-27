// =============================================================================
// editor/AssetDatabase.cpp  —  AssetDatabase Implementation
// =============================================================================
#include "AssetDatabase.h"

std::map<uint32_t, AssetMeta>       gAssetMeta;
std::map<std::string, uint32_t>     gAssetNameToId;
uint32_t                              gNextAssetId = 1000;
std::mutex                            gAssetDbMutex;

uint32_t AssetRegister(AssetType type, std::string_view name,
                       std::string_view sourcePath) {
    std::lock_guard lock(gAssetDbMutex);
    if (gAssetNameToId.contains(std::string(name))) return gAssetNameToId[std::string(name)];
    uint32_t id = gNextAssetId++;
    AssetMeta m; m.id = id; m.type = type; m.name = std::string(name);
    m.sourcePath = std::string(sourcePath); m.lastWrite = std::chrono::steady_clock::now();
    gAssetMeta[id] = m;
    gAssetNameToId[std::string(name)] = id;
    return id;
}

AssetMeta* AssetGetMeta(uint32_t id) {
    std::lock_guard lock(gAssetDbMutex);
    auto it = gAssetMeta.find(id);
    return it != gAssetMeta.end() ? &it->second : nullptr;
}

void AssetMarkDirty(uint32_t id) {
    std::lock_guard lock(gAssetDbMutex);
    if (gAssetMeta.contains(id)) {
        gAssetMeta[id].isDirty = true;
        gAssetMeta[id].lastWrite = std::chrono::steady_clock::now();
    }
}

void AssetClearDirty(uint32_t id) {
    std::lock_guard lock(gAssetDbMutex);
    if (gAssetMeta.contains(id)) gAssetMeta[id].isDirty = false;
}

// =============================================================================
// DATA-DRIVEN EXPORT / IMPORT
// =============================================================================
void ExportQuestsToCSV(std::string_view path) {
    std::ofstream f(std::string(path));
    f << "questId,title,description,objType,objTargetId,objRequired,rewardGold,rewardXP\n";
    for (const auto& [id, qt] : questDatabase) {
        if (qt.objectives.empty()) continue;
        const auto& obj = qt.objectives[0];
        f << std::format("{},{},{},{},{},{},{}\n",
            qt.id, qt.title, qt.description,
            std::to_underlying(obj.type), obj.targetId, obj.requiredCount,
            qt.rewardGold, qt.rewardXP);
    }
    AddLog("[AssetDB] Quests exported: {}", path);
}

void ExportNpcsToCSV(std::string_view path) {
    std::ofstream f(std::string(path));
    f << "id,name,x,z,greeting,offeredQuestId\n";
    for (const auto& [id, npc] : npcDatabase) {
        f << std::format("{},{},{},{},{},{}\n",
            npc.id, npc.name, npc.x, npc.z, npc.greeting, npc.offeredQuestId);
    }
    AddLog("[AssetDB] NPCs exported: {}", path);
}

void ExportItemsToCSV(std::string_view path) {
    std::ofstream f(std::string(path));
    f << "id,name,isStackable,maxStack,slot,minLevel\n";
    for (const auto& [id, itm] : itemDatabase) {
        f << std::format("{},{},{},{},{},{}\n",
            itm.id, itm.name, itm.isStackable ? 1 : 0,
            itm.maxStack, itm.slot, itm.minLevel);
    }
    AddLog("[AssetDB] Items exported: {}", path);
}

void ExportSkillsToCSV(std::string_view path) {
    std::ofstream f(std::string(path));
    f << "id,name,range,fov,damage,cooldown,statusEffectType,statusEffectDur,tickDmg\n";
    for (const auto& [id, sk] : skillDatabase) {
        f << std::format("{},{},{},{},{},{},{},{},{}\n",
            sk.id, sk.name, sk.range, sk.fov, sk.damage, sk.cooldown,
            sk.statusEffectType, sk.statusEffectDur, sk.statusEffectTickDmg);
    }
    AddLog("[AssetDB] Skills exported: {}", path);
}

// =============================================================================
// PROJECT CONTEXT
// =============================================================================
std::string gProjectName = "DefaultProject";
std::string gProjectPath = "./project/";

void ProjectSetContext(std::string_view name, std::string_view path) {
    gProjectName = name;
    gProjectPath = path;
    if (gProjectPath.back() != '/' && gProjectPath.back() != '\\')
        gProjectPath += '/';
    AddLog("[Project] Context: {} @ {}", name, gProjectPath);
}

void ProjectSaveAllDirty() {
    auto hasDirty = [](AssetType t) {
        return std::ranges::any_of(gAssetMeta,
            [t](const auto& p){ return p.second.type == t && p.second.isDirty; });
    };
    if (hasDirty(AssetType::Quest))   ExportQuestsToCSV(gProjectPath + "quests.csv");
    if (hasDirty(AssetType::NPC))     ExportNpcsToCSV(gProjectPath + "npcs.csv");
    if (hasDirty(AssetType::Item))    ExportItemsToCSV(gProjectPath + "items.csv");
    if (hasDirty(AssetType::Skill))   ExportSkillsToCSV(gProjectPath + "skills.csv");

    for (auto& [id, meta] : gAssetMeta) meta.isDirty = false;
    AddLog("[Project] All dirty assets saved.");
}
