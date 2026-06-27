// =============================================================================
// editor/EditorPanels.cpp  —  EditorPanels Implementation
// =============================================================================
#include "EditorPanels.h"

std::deque<EventLogEntry> gEventLog;
std::mutex                gEventLogMutex;

void AddEventLog(std::string_view category, std::string_view msg, ImVec4 color) {
    std::lock_guard lock(gEventLogMutex);
    auto now = std::chrono::steady_clock::now();
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count() % 1000;
    auto sec = std::chrono::duration_cast<std::chrono::seconds>(now.time_since_epoch()).count() % 60;
    auto min = (std::chrono::duration_cast<std::chrono::minutes>(now.time_since_epoch()).count()) % 60;
    std::string ts = std::format("{:02d}:{:02d}.{:03d}", min, sec, ms);
    gEventLog.push_back({ts, std::string(category), std::string(msg), color});
    if (gEventLog.size() > EVENT_LOG_MAX)
        gEventLog.pop_front();
}

void InitEventSubscriptions() {
    gEventBus.Subscribe<EntitySpawnedEvent>([](const EntitySpawnedEvent& e) {
        AddEventLog("ENTITY", std::format("Spawned: {} (ID:{}) at ({:.1f},{:.1f})", e.name, e.entityId, e.x, e.z),
            ImVec4(0.3f, 1.0f, 0.3f, 1.0f));
    });
    gEventBus.Subscribe<EntityDespawnedEvent>([](const EntityDespawnedEvent& e) {
        AddEventLog("ENTITY", std::format("Despawned: ID:{}", e.entityId),
            ImVec4(1.0f, 0.3f, 0.3f, 1.0f));
    });
    gEventBus.Subscribe<EntityDamagedEvent>([](const EntityDamagedEvent& e) {
        AddEventLog("COMBAT", std::format("Entity {} took {} dmg from {} (HP:{})", e.targetId, e.damage, e.sourceId, e.newHP),
            ImVec4(1.0f, 0.6f, 0.2f, 1.0f));
    });
    gEventBus.Subscribe<EntityDiedEvent>([](const EntityDiedEvent& e) {
        AddEventLog("COMBAT", std::format("Entity {} died (Killer:{}, Template:{})", e.entityId, e.killerId, e.monsterTemplateId),
            ImVec4(1.0f, 0.1f, 0.1f, 1.0f));
    });
    gEventBus.Subscribe<QuestAcceptedEvent>([](const QuestAcceptedEvent& e) {
        AddEventLog("QUEST", std::format("Player {} accepted Quest {}", e.playerId, e.questId),
            ImVec4(0.4f, 0.7f, 1.0f, 1.0f));
    });
    gEventBus.Subscribe<QuestCompletedEvent>([](const QuestCompletedEvent& e) {
        AddEventLog("QUEST", std::format("Player {} completed Quest {} (+{}g, +{}xp)", e.playerId, e.questId, e.rewardGold, e.rewardXP),
            ImVec4(0.2f, 1.0f, 0.8f, 1.0f));
    });
    gEventBus.Subscribe<QuestUpdatedEvent>([](const QuestUpdatedEvent& e) {
        AddEventLog("QUEST", std::format("Player {} Quest {} progress {}/{}", e.playerId, e.questId, e.currentCount, e.requiredCount),
            ImVec4(0.5f, 0.8f, 1.0f, 1.0f));
    });
    gEventBus.Subscribe<StatusEffectAppliedEvent>([](const StatusEffectAppliedEvent& e) {
        const char* names[] = {"Poison", "Slow", "Stun"};
        std::string name = e.type < StatusEffectType(3) ? names[std::to_underlying(e.type)] : "Unknown";
        AddEventLog("STATUS", std::format("Entity {} gained {} ({}s) from {}", e.targetId, name, e.durationSec, e.sourceId),
            ImVec4(1.0f, 0.8f, 0.2f, 1.0f));
    });
    gEventBus.Subscribe<StatusEffectRemovedEvent>([](const StatusEffectRemovedEvent& e) {
        const char* names[] = {"Poison", "Slow", "Stun"};
        std::string name = e.type < StatusEffectType(3) ? names[std::to_underlying(e.type)] : "Unknown";
        AddEventLog("STATUS", std::format("Entity {} lost {}", e.targetId, name),
            ImVec4(0.8f, 0.8f, 0.8f, 1.0f));
    });
    gEventBus.Subscribe<ItemEquippedEvent>([](const ItemEquippedEvent& e) {
        AddEventLog("ITEM", std::format("Player {} {} slot {}", e.playerId, e.equipped ? "equipped" : "unequipped", e.slotIndex),
            ImVec4(0.7f, 0.5f, 1.0f, 1.0f));
    });
    gEventBus.Subscribe<TradeCompletedEvent>([](const TradeCompletedEvent& e) {
        AddEventLog("TRADE", std::format("Trade completed: {} <-> {}", e.player1Id, e.player2Id),
            ImVec4(1.0f, 0.9f, 0.4f, 1.0f));
    });
    gEventBus.Subscribe<ChatMessageEvent>([](const ChatMessageEvent& e) {
        AddEventLog("CHAT", std::format("{}{}: {}", e.isWhisper ? "[W] " : "", e.sender, e.text),
            e.isWhisper ? ImVec4(1.0f, 0.8f, 0.2f, 1.0f) : ImVec4(0.8f, 0.9f, 1.0f, 1.0f));
    });
    gEventBus.Subscribe<SectorSwitchedEvent>([](const SectorSwitchedEvent& e) {
        AddEventLog("WORLD", std::format("Sector: ({},{}) -> ({},{})", e.oldSectorX, e.oldSectorZ, e.newSectorX, e.newSectorZ),
            ImVec4(0.4f, 1.0f, 0.6f, 1.0f));
    });
    gEventBus.Subscribe<PlayerLoggedInEvent>([](const PlayerLoggedInEvent& e) {
        AddEventLog("SESSION", std::format("Login: {} (ID:{}) at ({:.1f},{:.1f},{:.1f})", e.name, e.entityId, e.x, e.y, e.z),
            ImVec4(0.2f, 1.0f, 0.4f, 1.0f));
    });
    gEventBus.Subscribe<PlayerLoggedOutEvent>([](const PlayerLoggedOutEvent& e) {
        AddEventLog("SESSION", std::format("Logout: {} (ID:{})", e.name, e.entityId),
            ImVec4(0.8f, 0.8f, 0.8f, 1.0f));
    });
    gEventBus.Subscribe<TerrainModifiedEvent>([](const TerrainModifiedEvent& e) {
        AddEventLog("TERRAIN", std::format("Brush {} at ({},{}) R={:.1f} I={:.1f}", e.raise ? "Raise" : "Lower", e.brushX, e.brushZ, e.brushRadius, e.intensity),
            ImVec4(0.6f, 0.4f, 0.2f, 1.0f));
    });
}

// =============================================================================
// PANEL 1 – SYSTEM-LOG
// =============================================================================
void DrawPanelLog() {
    ImGui::SetNextWindowPos (ImVec2(0, 0));
    ImGui::SetNextWindowSize(ImVec2(320, 180));
    ImGui::Begin("System-Log", nullptr,
                 ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove);

    static char filter[32] = "";
    ImGui::InputText("Filter", filter, sizeof(filter));

    {
        std::lock_guard lock(logMutex);
        for (const auto& entry : engineLog) {
            if (filter[0] != '\0' && entry.find(filter) == std::string::npos) continue;
            ImGui::TextUnformatted(entry.c_str());
        }
    }
    if (ImGui::GetScrollY() >= ImGui::GetScrollMaxY())
        ImGui::SetScrollHereY(1.0f);
    ImGui::End();
}

// =============================================================================
// PANEL 2 – QUESTS / NPCS / HANDEL
// =============================================================================
void DrawPanelQuestsNPCsTrade(uint32_t playerId) {
    ImGui::SetNextWindowPos (ImVec2(0, 180));
    ImGui::SetNextWindowSize(ImVec2(320, 220));
    ImGui::Begin("Quests / NPCs / Handel", nullptr,
                 ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove);

    if (playerQuestLog.contains(playerId)) {
        for (const auto& q : playerQuestLog[playerId]) {
            const char* st =
                q.state == QuestState::Completed ? "Erledigt" :
                q.state == QuestState::Active    ? "Aktiv"    : "Inaktiv";
            ImVec4 col = q.state == QuestState::Completed
                ? ImVec4(0.2f, 1.0f, 0.2f, 1.0f)
                : ImVec4(0.3f, 0.7f, 1.0f, 1.0f);
            ImGui::TextColored(col, "Quest %d [%s]", q.questId, st);
            for (const auto& obj : q.objectives)
                ImGui::Text("  %s: %d/%d",
                            obj.description.c_str(),
                            obj.currentCount,
                            obj.requiredCount);
        }
    }

    ImGui::Separator();
    ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.2f, 1.0f), "NPCs in der Welt:");
    {
        std::lock_guard lock(npcMutex);
        for (auto& [nid, npc] : npcDatabase) {
            ImGui::Text("  [%d] %s (%.1f, %.1f)",
                        nid, npc.name.c_str(), npc.x, npc.z);
            ImGui::SameLine();
            std::string label = std::format("Sprechen##npc{}", nid);
            if (ImGui::SmallButton(label.c_str())) {
                ByteBuffer pkt;
                pkt.WriteUInt8 (std::to_underlying(PacketType::MSG_TALK_NPC));
                pkt.WriteUInt32(nid);
                ClientSend(pkt);
            }
        }
    }

    {
        std::lock_guard lock(respawnMutex);
        if (!respawnQueue.empty()) {
            ImGui::Separator();
            ImGui::TextColored(ImVec4(1.0f, 0.4f, 0.4f, 1.0f),
                               "Respawn-Queue (%d):",
                               static_cast<int>(respawnQueue.size()));
            for (const auto& r : respawnQueue)
                ImGui::Text("  Template %d in %.1fs",
                            r.monsterTemplateId, r.respawnTime - r.elapsed);
        }
    }

    ImGui::Separator();
    {
        std::lock_guard lock(tradeMutex);
        bool inTrade = false;
        for (const auto& ts : activeTrades) {
            if (ts.p1Id != playerId && ts.p2Id != playerId) continue;
            inTrade = true;
            bool iConf = (ts.p1Id == playerId) ? ts.p1Confirm : ts.p2Confirm;
            bool oConf = (ts.p1Id == playerId) ? ts.p2Confirm : ts.p1Confirm;
            uint32_t partnerId = (ts.p1Id == playerId) ? ts.p2Id : ts.p1Id;
            ImGui::Text("Handel mit Entity %d", partnerId);
            ImGui::TextColored(iConf ? ImVec4(0,1,0,1) : ImVec4(1,1,1,1),
                               "Du: %s", iConf ? "Bereit" : "Wartet");
            ImGui::TextColored(oConf ? ImVec4(0,1,0,1) : ImVec4(1,1,1,1),
                               "Partner: %s", oConf ? "Bereit" : "Wartet");
            if (!iConf && ImGui::Button("Handel bestatigen")) {
                ByteBuffer pkt; pkt.WriteUInt8(std::to_underlying(PacketType::MSG_TRADE_CONFIRM));
                ClientSend(pkt);
            }
        }
        if (!inTrade) ImGui::TextDisabled("Kein aktiver Handel.");
    }

    ImGui::End();
}

// =============================================================================
// PANEL 3 – TERRAIN-EDITOR
// =============================================================================
void DrawPanelTerrain() {
    static int           brushX  = 20, brushZ = 20;
    static float         brushR  = 2.0f, brushI = 0.5f;

    ImGui::SetNextWindowPos (ImVec2(0, 400));
    ImGui::SetNextWindowSize(ImVec2(320, 260));
    ImGui::Begin("Terrain-Editor", nullptr,
                 ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove);

    ImGui::SliderInt  ("X",      &brushX, 0, GRID_SIZE - 1);
    ImGui::SliderInt  ("Z",      &brushZ, 0, GRID_SIZE - 1);
    ImGui::SliderFloat("Radius", &brushR, 0.5f, 8.0f);
    ImGui::SliderFloat("Starke", &brushI, 0.1f, 2.0f);

    auto applyBrush = [&](float sign) {
        std::vector<float> before = heightMap;
        for (int z = 0; z < GRID_SIZE; ++z)
            for (int x = 0; x < GRID_SIZE; ++x) {
                float dx = static_cast<float>(x - brushX);
                float dz = static_cast<float>(z - brushZ);
                if (std::sqrt(dx*dx + dz*dz) <= brushR)
                    heightMap[z * GRID_SIZE + x] += sign * brushI;
            }
        RebuildTerrainMeshOnGPU();
        gEventBus.Publish(TerrainModifiedEvent{
            .brushX = brushX,
            .brushZ = brushZ,
            .brushRadius = brushR,
            .intensity = brushI,
            .raise = sign > 0
        });
        CmdExecute(std::make_unique<CmdTerrainBrush>(
            std::move(before), heightMap,
            sign > 0 ? "Terrain Raise" : "Terrain Lower"));
    };

    if (ImGui::Button("Anheben"))  applyBrush(+1.0f);
    ImGui::SameLine();
    if (ImGui::Button("Absenken")) applyBrush(-1.0f);

    if (CmdCanUndo() && ImGui::Button(std::format("Undo: {}", CmdGetUndoName()).c_str()))
        CmdUndo();
    ImGui::SameLine();
    if (CmdCanRedo() && ImGui::Button(std::format("Redo: {}", CmdGetRedoName()).c_str()))
        CmdRedo();

    ImGui::Separator();
    std::string sektorName = GetSectorName(currentSectorX, currentSectorZ);
    ImGui::Text("Sektor: %s", sektorName.c_str());

    if (ImGui::Button("HDT Speichern"))
        SaveHDTBinary(sektorName + ".hdt");
    ImGui::SameLine();
    if (ImGui::Button("HDT Laden")) {
        LoadHDTBinary(sektorName + ".hdt");
        RebuildTerrainMeshOnGPU();
    }

    ImGui::End();
}

// =============================================================================
// PANEL 4 – INVENTAR + SICHERHEIT + WHISPER + STATUSEFFEKTE + COOLDOWNS
// =============================================================================
void DrawPanelInventarSecurity(uint32_t playerId) {
    ImGui::SetNextWindowPos (ImVec2(960, 0));
    ImGui::SetNextWindowSize(ImVec2(320, 520));
    ImGui::Begin("Inventar & Sicherheit", nullptr,
                 ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove);

    if (ImGui::CollapsingHeader("Spieler-Inventar",
                                ImGuiTreeNodeFlags_DefaultOpen)) {
        std::vector<Item> invSnapshot;
        {
            std::lock_guard lock(inventoryMutex);
            invSnapshot = playerInventories[playerId];
        }

        int equipSlot = -1;
        for (size_t i = 0; i < invSnapshot.size(); ++i) {
            if (invSnapshot[i].templateId != 0 &&
                itemDatabase.contains(invSnapshot[i].templateId)) {
                auto& tmpl = itemDatabase[invSnapshot[i].templateId];
                ImGui::Text("[%2d] %s x%d %s",
                            static_cast<int>(i),
                            tmpl.name.c_str(),
                            invSnapshot[i].count,
                            invSnapshot[i].isEquipped ? "[E]" : "");
                ImGui::SameLine(240);
                std::string btn = std::format("{}{}",
                    invSnapshot[i].isEquipped ? "Unequip##" : "Equip##", i);
                if (ImGui::SmallButton(btn.c_str()))
                    equipSlot = static_cast<int>(i);
            } else {
                ImGui::TextDisabled("[%2d] Leer", static_cast<int>(i));
            }
        }
        if (equipSlot >= 0)
            EquipItem(playerId, static_cast<uint32_t>(equipSlot));
    }

    if (ImGui::CollapsingHeader("Account-Sicherheit (Argon2)")) {
        static char regUser[16] = "", regPass[16] = "";
        ImGui::InputText("Username", regUser, sizeof(regUser));
        ImGui::InputText("Passwort", regPass, sizeof(regPass),
                         ImGuiInputTextFlags_Password);
        if (ImGui::Button("Registrieren")) {
            if (RegisterAccount(regUser, regPass))
                AddLog("[Argon2] Account registriert.");
        }
        ImGui::SameLine();
        if (ImGui::Button("Prufen")) {
            if (VerifyAccount(regUser, regPass))
                AddLog("[Argon2] Verifiziert.");
            else
                AddLog("[Argon2] Ungultig.");
        }
    }

    if (ImGui::CollapsingHeader("Whisper")) {
        static char wTarget[16] = "", wText[128] = "";
        ImGui::InputText("An##wt",   wTarget, sizeof(wTarget));
        ImGui::InputText("Text##wm", wText,   sizeof(wText));
        if (ImGui::Button("Senden##whisper") &&
            wTarget[0] != '\0' && wText[0] != '\0') {
            ByteBuffer pkt;
            pkt.WriteUInt8 (std::to_underlying(PacketType::MSG_WHISPER));
            pkt.WriteString(std::string(wTarget));
            pkt.WriteString(std::string(wText));
            ClientSend(pkt);
            wText[0] = '\0';
        }
        ImGui::Separator();
        {
            std::lock_guard lock(chatMutex);
            for (const auto& msg : chatHistory)
                if (msg.channel == 99)
                    ImGui::TextColored(
                        ImVec4(1.0f, 0.8f, 0.2f, 1.0f),
                        "[W] %s: %s",
                        msg.senderName.c_str(),
                        msg.text.c_str());
        }
    }

    if (ImGui::CollapsingHeader("Aktive Statuseffekte")) {
        std::lock_guard lock(statusEffectMutex);
        if (entityStatusEffects.empty()) {
            ImGui::TextDisabled("Keine aktiven Effekte.");
        } else {
            for (const auto& [eid, effs] : entityStatusEffects) {
                for (const auto& eff : effs) {
                    const char* typeName =
                        eff.type == StatusEffectType::Poison ? "GIFT"  :
                        eff.type == StatusEffectType::Slow   ? "SLOW"  : "STUN";
                    ImVec4 col =
                        eff.type == StatusEffectType::Poison
                            ? ImVec4(0.2f, 1.0f, 0.2f, 1.0f)
                        : eff.type == StatusEffectType::Slow
                            ? ImVec4(0.5f, 0.5f, 1.0f, 1.0f)
                            : ImVec4(1.0f, 0.3f, 0.3f, 1.0f);
                    float rem = eff.durationSec - eff.elapsedSec;
                    ImGui::TextColored(col,
                        "Entity %d [%s] %.1fs", eid, typeName, rem);
                }
            }
        }
    }

    if (ImGui::CollapsingHeader("Skill-Cooldowns")) {
        std::lock_guard lock(skillCooldownMutex);
        auto now = std::chrono::steady_clock::now();
        for (const auto& [sid, tmpl] : skillDatabase) {
            bool  onCd = false;
            float rem  = 0.0f;
            if (skillLastCastTime.contains(playerId) &&
                skillLastCastTime[playerId].contains(sid)) {
                float elapsed = std::chrono::duration<float>(
                    now - skillLastCastTime[playerId][sid]).count();
                rem  = tmpl.cooldown - elapsed;
                onCd = rem > 0.0f;
            }
            ImGui::TextColored(
                onCd ? ImVec4(1.0f, 0.4f, 0.4f, 1.0f)
                     : ImVec4(0.4f, 1.0f, 0.4f, 1.0f),
                "%s: %s",
                tmpl.name.c_str(),
                onCd ? std::format("CD: {:.2f}s", rem).c_str() : "Bereit");
        }
    }

    ImGui::End();
}

// =============================================================================
// PANEL 5 – EVENT MONITOR
// =============================================================================
void DrawPanelEventMonitor() {
    ImGui::SetNextWindowPos (ImVec2(620, 520));
    ImGui::SetNextWindowSize(ImVec2(340, 200));
    ImGui::Begin("Event Monitor", nullptr,
                 ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove);

    static bool filterEntity = true;
    static bool filterCombat = true;
    static bool filterQuest  = true;
    static bool filterStatus = true;
    static bool filterItem   = true;
    static bool filterTrade  = true;
    static bool filterChat   = true;
    static bool filterWorld  = true;
    static bool filterSession= true;
    static bool filterTerrain= true;

    if (ImGui::Button("Clear")) {
        std::lock_guard lock(gEventLogMutex);
        gEventLog.clear();
    }
    ImGui::SameLine();
    if (ImGui::Button("All")) {
        filterEntity = filterCombat = filterQuest = filterStatus =
        filterItem = filterTrade = filterChat = filterWorld =
        filterSession = filterTerrain = true;
    }
    ImGui::SameLine();
    if (ImGui::Button("None")) {
        filterEntity = filterCombat = filterQuest = filterStatus =
        filterItem = filterTrade = filterChat = filterWorld =
        filterSession = filterTerrain = false;
    }

    ImGui::Separator();
    ImGui::Columns(5, nullptr, false);
    ImGui::Checkbox("Entity", &filterEntity); ImGui::NextColumn();
    ImGui::Checkbox("Combat", &filterCombat); ImGui::NextColumn();
    ImGui::Checkbox("Quest", &filterQuest); ImGui::NextColumn();
    ImGui::Checkbox("Status", &filterStatus); ImGui::NextColumn();
    ImGui::Checkbox("Item", &filterItem); ImGui::NextColumn();
    ImGui::Checkbox("Trade", &filterTrade); ImGui::NextColumn();
    ImGui::Checkbox("Chat", &filterChat); ImGui::NextColumn();
    ImGui::Checkbox("World", &filterWorld); ImGui::NextColumn();
    ImGui::Checkbox("Session", &filterSession); ImGui::NextColumn();
    ImGui::Checkbox("Terrain", &filterTerrain); ImGui::NextColumn();
    ImGui::Columns(1);
    ImGui::Separator();

    ImGui::BeginChild("EventScroll", ImVec2(0, 0), false, ImGuiWindowFlags_HorizontalScrollbar);
    {
        std::lock_guard lock(gEventLogMutex);
        for (const auto& entry : gEventLog) {
            bool show = false;
            if (entry.category == "ENTITY"  && filterEntity) show = true;
            if (entry.category == "COMBAT"  && filterCombat) show = true;
            if (entry.category == "QUEST"   && filterQuest)  show = true;
            if (entry.category == "STATUS"  && filterStatus) show = true;
            if (entry.category == "ITEM"    && filterItem)   show = true;
            if (entry.category == "TRADE"   && filterTrade)  show = true;
            if (entry.category == "CHAT"    && filterChat)   show = true;
            if (entry.category == "WORLD"   && filterWorld)  show = true;
            if (entry.category == "SESSION" && filterSession)show = true;
            if (entry.category == "TERRAIN" && filterTerrain)show = true;
            if (!show) continue;

            ImGui::TextColored(entry.color, "[%s]", entry.timestamp.c_str());
            ImGui::SameLine();
            ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f), "[%s]", entry.category.c_str());
            ImGui::SameLine();
            ImGui::TextUnformatted(entry.message.c_str());
        }
    }
    if (ImGui::GetScrollY() >= ImGui::GetScrollMaxY())
        ImGui::SetScrollHereY(1.0f);
    ImGui::EndChild();

    ImGui::End();
}
