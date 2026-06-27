// =============================================================================
// editor/DataDefinitionEditor.cpp  —  DataDefinitionEditor Implementation
// =============================================================================
#include "DataDefinitionEditor.h"

// =============================================================================
// QUEST EDITOR
// =============================================================================
void DrawQuestEditor() {
    if (ImGui::Button("+ New Quest")) {
        uint32_t newId = 1;
        for (const auto& [id, _] : questDatabase) newId = std::max(newId, id + 1);
        QuestTemplate qt;
        qt.id = newId; qt.title = "New Quest"; qt.description = "...";
        qt.rewardGold = 0; qt.rewardXP = 0;
        QuestObjective obj;
        obj.type = QuestObjectiveType::KillMonster; obj.targetId = 101;
        obj.requiredCount = 1; obj.description = "Kill target";
        qt.objectives.push_back(obj);
        questDatabase[newId] = qt;
        AssetRegister(AssetType::Quest, qt.title);
        AssetMarkDirty(newId);
    }

    ImGui::Separator();

    for (auto& [id, qt] : questDatabase) {
        ImGui::PushID(static_cast<int>(id));
        bool open = ImGui::CollapsingHeader(
            std::format("{} (ID: {})", qt.title, id).c_str());
        if (open) {
            char title[64]; std::strncpy(title, qt.title.c_str(), 63); title[63]='\0';
            if (ImGui::InputText("Title", title, sizeof(title)))
                { qt.title = title; AssetMarkDirty(id); }

            char desc[256]; std::strncpy(desc, qt.description.c_str(), 255); desc[255]='\0';
            if (ImGui::InputTextMultiline("Description", desc, sizeof(desc), ImVec2(0,60)))
                { qt.description = desc; AssetMarkDirty(id); }

            ImGui::DragInt("Reward Gold", reinterpret_cast<int*>(&qt.rewardGold), 1, 0, 10000);
            ImGui::DragInt("Reward XP",   reinterpret_cast<int*>(&qt.rewardXP),   1, 0, 10000);

            if (!qt.objectives.empty()) {
                auto& obj = qt.objectives[0];
                int objType = std::to_underlying(obj.type);
                const char* types[] = {"KillMonster", "ReachZone", "TalkToNPC"};
                if (ImGui::Combo("Objective Type", &objType, types, 3)) {
                    obj.type = static_cast<QuestObjectiveType>(objType);
                    AssetMarkDirty(id);
                }
                ImGui::DragInt("Target ID", reinterpret_cast<int*>(&obj.targetId), 1, 0, 9999);
                ImGui::DragInt("Required",  reinterpret_cast<int*>(&obj.requiredCount), 1, 1, 999);
            }

            if (ImGui::Button("Delete Quest")) {
                questDatabase.erase(id);
                AssetMarkDirty(id);
                ImGui::PopID();
                break;
            }
        }
        ImGui::PopID();
    }
}

// =============================================================================
// NPC EDITOR
// =============================================================================
void DrawNpcEditor() {
    if (ImGui::Button("+ New NPC")) {
        uint32_t newId = 1;
        for (const auto& [id, _] : npcDatabase) newId = std::max(newId, id + 1);
        NpcTemplate npc;
        npc.id = newId; npc.name = "New NPC"; npc.x = 0.0f; npc.z = 0.0f;
        npc.greeting = "Hello!"; npc.offeredQuestId = 0;
        npcDatabase[newId] = npc;
        AssetRegister(AssetType::NPC, npc.name);
        AssetMarkDirty(newId);
    }

    ImGui::Separator();

    for (auto& [id, npc] : npcDatabase) {
        ImGui::PushID(static_cast<int>(id) + 10000);
        bool open = ImGui::CollapsingHeader(
            std::format("{} (ID: {})", npc.name, id).c_str());
        if (open) {
            char name[64]; std::strncpy(name, npc.name.c_str(), 63); name[63]='\0';
            if (ImGui::InputText("Name", name, sizeof(name)))
                { npc.name = name; AssetMarkDirty(id); }

            ImGui::DragFloat("Pos X", &npc.x, 0.1f, -100.0f, 100.0f);
            ImGui::DragFloat("Pos Z", &npc.z, 0.1f, -100.0f, 100.0f);
            if (ImGui::IsItemDeactivatedAfterEdit()) AssetMarkDirty(id);

            char greet[128]; std::strncpy(greet, npc.greeting.c_str(), 127); greet[127]='\0';
            if (ImGui::InputText("Greeting", greet, sizeof(greet)))
                { npc.greeting = greet; AssetMarkDirty(id); }

            ImGui::DragInt("Offered Quest", reinterpret_cast<int*>(&npc.offeredQuestId), 1, 0, 9999);
            if (ImGui::IsItemDeactivatedAfterEdit()) AssetMarkDirty(id);

            if (ImGui::Button("Delete NPC")) {
                npcDatabase.erase(id);
                AssetMarkDirty(id);
                ImGui::PopID();
                break;
            }
        }
        ImGui::PopID();
    }
}

// =============================================================================
// ITEM EDITOR
// =============================================================================
void DrawItemEditor() {
    if (ImGui::Button("+ New Item")) {
        uint32_t newId = 100;
        for (const auto& [id, _] : itemDatabase) newId = std::max(newId, id + 1);
        ItemTemplate it;
        it.id = newId; it.name = "New Item"; it.isStackable = false;
        it.maxStack = 1; it.slot = 0; it.minLevel = 1;
        itemDatabase[newId] = it;
        AssetRegister(AssetType::Item, it.name);
        AssetMarkDirty(newId);
    }

    ImGui::Separator();

    for (auto& [id, it] : itemDatabase) {
        ImGui::PushID(static_cast<int>(id) + 20000);
        bool open = ImGui::CollapsingHeader(
            std::format("{} (ID: {})", it.name, id).c_str());
        if (open) {
            char name[64]; std::strncpy(name, it.name.c_str(), 63); name[63]='\0';
            if (ImGui::InputText("Name", name, sizeof(name)))
                { it.name = name; AssetMarkDirty(id); }

            ImGui::Checkbox("Stackable", &it.isStackable);
            if (ImGui::IsItemDeactivatedAfterEdit()) AssetMarkDirty(id);

            ImGui::DragInt("Max Stack", reinterpret_cast<int*>(&it.maxStack), 1, 1, 999);
            ImGui::DragInt("Slot",      reinterpret_cast<int*>(&it.slot),      1, 0, 10);
            ImGui::DragInt("Min Level", reinterpret_cast<int*>(&it.minLevel),  1, 1, 99);
            if (ImGui::IsItemDeactivatedAfterEdit()) AssetMarkDirty(id);

            if (ImGui::Button("Delete Item")) {
                itemDatabase.erase(id);
                AssetMarkDirty(id);
                ImGui::PopID();
                break;
            }
        }
        ImGui::PopID();
    }
}

// =============================================================================
// SKILL EDITOR
// =============================================================================
void DrawSkillEditor() {
    if (ImGui::Button("+ New Skill")) {
        uint32_t newId = 10;
        for (const auto& [id, _] : skillDatabase) newId = std::max(newId, id + 1);
        SkillTemplate sk;
        sk.id = newId; sk.name = "New Skill"; sk.range = 5.0f; sk.fov = 360.0f;
        sk.damage = 10; sk.cooldown = 1.0f;
        sk.statusEffectType = -1; sk.statusEffectDur = 0.0f; sk.statusEffectTickDmg = 0;
        skillDatabase[newId] = sk;
        AssetRegister(AssetType::Skill, sk.name);
        AssetMarkDirty(newId);
    }

    ImGui::Separator();

    for (auto& [id, sk] : skillDatabase) {
        ImGui::PushID(static_cast<int>(id) + 30000);
        bool open = ImGui::CollapsingHeader(
            std::format("{} (ID: {})", sk.name, id).c_str());
        if (open) {
            char name[64]; std::strncpy(name, sk.name.c_str(), 63); name[63]='\0';
            if (ImGui::InputText("Name", name, sizeof(name)))
                { sk.name = name; AssetMarkDirty(id); }

            ImGui::DragFloat("Range",    &sk.range,    0.1f, 0.0f, 100.0f);
            ImGui::DragFloat("FOV",      &sk.fov,      1.0f, 0.0f, 360.0f);
            ImGui::DragInt("Damage",     reinterpret_cast<int*>(&sk.damage),   1, 0, 9999);
            ImGui::DragFloat("Cooldown", &sk.cooldown, 0.1f, 0.0f, 60.0f);
            if (ImGui::IsItemDeactivatedAfterEdit()) AssetMarkDirty(id);

            int seType = sk.statusEffectType + 1;
            const char* effNames[] = {"None", "Poison", "Slow", "Stun"};
            if (ImGui::Combo("Status Effect", &seType, effNames, 4)) {
                sk.statusEffectType = seType - 1;
                AssetMarkDirty(id);
            }
            if (sk.statusEffectType >= 0) {
                ImGui::DragFloat("Effect Duration", &sk.statusEffectDur, 0.1f, 0.0f, 60.0f);
                ImGui::DragInt("Tick Damage", reinterpret_cast<int*>(&sk.statusEffectTickDmg), 1, 0, 999);
            }

            if (ImGui::Button("Delete Skill")) {
                skillDatabase.erase(id);
                AssetMarkDirty(id);
                ImGui::PopID();
                break;
            }
        }
        ImGui::PopID();
    }
}

// =============================================================================
// COMBINED DATA DEFINITIONS PANEL
// =============================================================================
void DrawDataDefinitionsPanel() {
    ImGui::SetNextWindowPos (ImVec2(620, 0));
    ImGui::SetNextWindowSize(ImVec2(340, 520));
    ImGui::Begin("Data Definitions", nullptr,
                 ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove);

    if (ImGui::BeginTabBar("DataTabs")) {
        if (ImGui::BeginTabItem("Quests")) { DrawQuestEditor(); ImGui::EndTabItem(); }
        if (ImGui::BeginTabItem("NPCs"))   { DrawNpcEditor();   ImGui::EndTabItem(); }
        if (ImGui::BeginTabItem("Items"))  { DrawItemEditor();  ImGui::EndTabItem(); }
        if (ImGui::BeginTabItem("Skills")) { DrawSkillEditor(); ImGui::EndTabItem(); }
        ImGui::EndTabBar();
    }

    ImGui::Separator();
    if (ImGui::Button("Save All Dirty")) ProjectSaveAllDirty();
    ImGui::SameLine();
    if (ImGui::Button("Reload from Disk")) {
        LoadQuestsFromCSV(gProjectPath + "quests.csv");
        LoadNpcsFromCSV(gProjectPath + "npcs.csv");
        LoadItemDatabase();
        LoadSkillsFromCSV(gProjectPath + "skills.csv");
        AddLog("[Editor] Data definitions reloaded from disk.");
    }

    ImGui::End();
}
