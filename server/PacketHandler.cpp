// =============================================================================
// server/PacketHandler.cpp  —  PacketHandler Implementation
// =============================================================================
#include "PacketHandler.h"

void ProcessPacketFromClient(ClientSession& session, std::span<const uint8_t> payload) {
    ByteBuffer buf{std::vector<uint8_t>(payload.begin(), payload.end())};
    auto bc = [](std::span<const uint8_t> d){ BroadcastToAll(d); };

    try {
        uint8_t op = buf.ReadUInt8();

        // -----------------------------------------------------------------
        // MSG_MOVE_REQ
        // -----------------------------------------------------------------
        if (op == std::to_underlying(PacketType::MSG_MOVE_REQ)) {
            uint32_t entId = buf.ReadUInt32();
            uint32_t seqId = buf.ReadUInt32();
            float    tX    = buf.ReadFloat();
            float    tZ    = buf.ReadFloat();

            auto it = std::ranges::find_if(serverRegistry,
                [&](const Entity& e){ return e.id == entId; });
            if (it == serverRegistry.end()) return;

            if (HasStatusEffect(entId, StatusEffectType::Stun)) {
                tX = it->transform.x; tZ = it->transform.z;
                ByteBuffer n; n.WriteUInt8(std::to_underlying(PacketType::MSG_MOVE_NOTIFY));
                n.WriteUInt32(entId); n.WriteUInt32(seqId);
                n.WriteFloat(tX); n.WriteFloat(tZ);
                BroadcastToAll(std::span(n.data)); return;
            }

            float cX   = it->transform.x, cZ = it->transform.z;
            float dist = std::sqrt(std::pow(tX-cX, 2) + std::pow(tZ-cZ, 2));
            float maxDist = HasStatusEffect(entId, StatusEffectType::Slow) ? 2.0f : 4.0f;
            if (dist > maxDist) {
                tX = cX; tZ = cZ;
                AddLog("[Anti-Cheat] Speedhack blockiert: Entity {}", entId);
            } else {
                float dX = tX-cX, dZ = tZ-cZ, len = std::sqrt(dX*dX+dZ*dZ);
                if (len > 0.1f) { it->transform.lookX=dX/len; it->transform.lookZ=dZ/len; }
            }
            it->transform.targetX     = tX;
            it->transform.targetZ     = tZ;
            it->persistence.isDirty   = true;

            UpdateQuestProgress(entId, QuestObjectiveType::ReachZone,
                static_cast<uint32_t>(currentSectorX * 100 + currentSectorZ));

            ByteBuffer n; n.WriteUInt8(std::to_underlying(PacketType::MSG_MOVE_NOTIFY));
            n.WriteUInt32(entId); n.WriteUInt32(seqId);
            n.WriteFloat(tX); n.WriteFloat(tZ);
            BroadcastToAll(std::span(n.data));

            gEventBus.Publish(EntityMovedEvent{
                .entityId = entId,
                .x = it->transform.x,
                .z = it->transform.z,
                .targetX = tX,
                .targetZ = tZ
            });
        }

        // -----------------------------------------------------------------
        // MSG_CAST_SKILL
        // -----------------------------------------------------------------
        else if (op == std::to_underlying(PacketType::MSG_CAST_SKILL)) {
            uint32_t skillId  = buf.ReadUInt32();
            uint32_t targetId = buf.ReadUInt32();

            if (HasStatusEffect(session.entityId, StatusEffectType::Stun)) {
                AddLog("[Status] Skill blockiert – gestunnt: Entity {}", session.entityId);
                return;
            }
            if (!CheckAndUpdateCooldown(session.entityId, skillId)) return;

            auto casterIt = std::ranges::find_if(serverRegistry,
                [&](const Entity& e){ return e.id == session.entityId; });
            auto targetIt = std::ranges::find_if(serverRegistry,
                [&](const Entity& e){ return e.id == targetId; });
            if (casterIt == serverRegistry.end() || targetIt == serverRegistry.end()) return;
            if (!skillDatabase.contains(skillId)) return;

            auto& skill = skillDatabase[skillId];
            float dx   = targetIt->transform.x - casterIt->transform.x;
            float dz   = targetIt->transform.z - casterIt->transform.z;
            float dist = std::sqrt(dx*dx + dz*dz);
            if (dist > skill.range + 1.0f) {
                AddLog("[Anti-Cheat] Damage/Range Validation fehlgeschlagen."); return;
            }

            targetIt->currentHP -= static_cast<int>(skill.damage);
            if (targetIt->currentHP < 0) targetIt->currentHP = 0;

            gEventBus.Publish(EntityDamagedEvent{
                .targetId = targetId,
                .sourceId = session.entityId,
                .newHP = targetIt->currentHP,
                .damage = static_cast<int>(skill.damage)
            });

            ByteBuffer cb; cb.WriteUInt8(std::to_underlying(PacketType::MSG_COMBAT_NOTIFY));
            cb.WriteUInt32(targetId);
            cb.WriteUInt32(static_cast<uint32_t>(targetIt->currentHP));
            BroadcastToAll(std::span(cb.data));

            if (skill.statusEffectType >= 0 && targetIt->currentHP > 0) {
                ApplyStatusEffect(targetId,
                    static_cast<StatusEffectType>(skill.statusEffectType),
                    skill.statusEffectDur, session.entityId,
                    skill.statusEffectTickDmg, bc);
            }

            if (targetIt->isMonster && targetIt->currentHP == 0) {
                gEventBus.Publish(EntityDiedEvent{
                    .entityId = targetId,
                    .killerId = session.entityId,
                    .monsterTemplateId = targetIt->monsterTemplateId,
                    .originSpawnId = targetIt->originSpawnId,
                    .x = targetIt->transform.x,
                    .z = targetIt->transform.z
                });

                bool questDone = UpdateQuestProgress(session.entityId,
                    QuestObjectiveType::KillMonster, targetIt->monsterTemplateId);
                if (questDone) {
                    auto pIt = std::ranges::find_if(serverRegistry,
                        [&](const Entity& e){ return e.id == session.entityId; });
                    if (pIt != serverRegistry.end() && playerQuestLog.contains(session.entityId)) {
                        for (const auto& prog : playerQuestLog[session.entityId])
                            if (prog.state == QuestState::Completed && questDatabase.contains(prog.questId)) {
                                pIt->persistence.gold += questDatabase[prog.questId].rewardGold;
                                gEventBus.Publish(QuestCompletedEvent{
                                    .playerId = session.entityId,
                                    .questId = prog.questId,
                                    .rewardGold = questDatabase[prog.questId].rewardGold,
                                    .rewardXP = questDatabase[prog.questId].rewardXP
                                });
                            }
                        pIt->persistence.isDirty = true;
                    }
                    ByteBuffer qpkt; qpkt.WriteUInt8(std::to_underlying(PacketType::MSG_QUEST_COMPLETE));
                    for (const auto& prog : playerQuestLog[session.entityId])
                        if (prog.state == QuestState::Completed)
                            qpkt.WriteUInt32(prog.questId);
                    SendToClient(session, std::span(qpkt.data));
                    AddLog("[Quest] Abgeschlossen! Gold fur Entity {}", session.entityId);
                }

                uint32_t deadId        = targetIt->id;
                uint32_t deadTemplate  = targetIt->monsterTemplateId;
                uint32_t deadSpawnId   = targetIt->originSpawnId;
                float    deadX         = targetIt->transform.x;
                float    deadZ         = targetIt->transform.z;
                float    respawnSec    = 15.0f;
                for (const auto& sp : sectorSpawns)
                    if (sp.id == deadSpawnId) { respawnSec = sp.respawnTime; break; }

                ByteBuffer dp; dp.WriteUInt8(std::to_underlying(PacketType::MSG_ENTITY_DESPAWN));
                dp.WriteUInt32(deadId);
                BroadcastToAll(std::span(dp.data));
                serverRegistry.erase(
                    std::ranges::remove_if(serverRegistry,
                        [deadId](const Entity& e){ return e.id == deadId; }).begin(),
                    serverRegistry.end());
                ScheduleRespawn(deadSpawnId, deadTemplate, deadX, deadZ, respawnSec);
            }
        }

        // -----------------------------------------------------------------
        // MSG_QUEST_ACCEPT
        // -----------------------------------------------------------------
        else if (op == std::to_underlying(PacketType::MSG_QUEST_ACCEPT)) {
            uint32_t questId = buf.ReadUInt32();
            AcceptQuest(session.entityId, questId);
            ByteBuffer upkt; upkt.WriteUInt8(std::to_underlying(PacketType::MSG_QUEST_UPDATE));
            upkt.WriteUInt32(questId);
            upkt.WriteUInt8(std::to_underlying(QuestState::Active));
            SendToClient(session, std::span(upkt.data));
        }

        // -----------------------------------------------------------------
        // MSG_CHAT
        // -----------------------------------------------------------------
        else if (op == std::to_underlying(PacketType::MSG_CHAT)) {
            uint32_t    channel = buf.ReadUInt32();
            std::string text    = buf.ReadString();
            auto it = std::ranges::find_if(serverRegistry,
                [&](const Entity& e){ return e.id == session.entityId; });
            AddChatMessage(
                (it != serverRegistry.end()) ? it->name : "Unknown",
                text, channel);
            gEventBus.Publish(ChatMessageEvent{
                .sender = (it != serverRegistry.end()) ? it->name : "Unknown",
                .text = text,
                .channel = channel,
                .isWhisper = false,
                .target = ""
            });
        }

        // -----------------------------------------------------------------
        // MSG_EQUIP_REQ
        // -----------------------------------------------------------------
        else if (op == std::to_underlying(PacketType::MSG_EQUIP_REQ)) {
            uint32_t slotIdx = buf.ReadUInt32();
            if (EquipItem(session.entityId, slotIdx)) {
                gEventBus.Publish(ItemEquippedEvent{
                    .playerId = session.entityId,
                    .slotIndex = slotIdx,
                    .templateId = 0,
                    .equipped = true
                });
                SendInventoryToSession(session.entityId);
            }
        }

        // -----------------------------------------------------------------
        // MSG_TRADE_REQ
        // -----------------------------------------------------------------
        else if (op == std::to_underlying(PacketType::MSG_TRADE_REQ)) {
            uint32_t targetId = buf.ReadUInt32();
            std::lock_guard lock(tradeMutex);
            TradeSession ts; ts.p1Id = session.entityId; ts.p2Id = targetId;
            activeTrades.push_back(ts);
            SyncTradeState(ts);
        }

        // -----------------------------------------------------------------
        // MSG_TRADE_ADD_ITEM
        // -----------------------------------------------------------------
        else if (op == std::to_underlying(PacketType::MSG_TRADE_ADD_ITEM)) {
            uint32_t slot  = buf.ReadUInt32();
            uint32_t count = buf.ReadUInt32();
            std::lock_guard lock(tradeMutex);
            for (auto& ts : activeTrades) {
                if (ts.p1Id == session.entityId) {
                    ts.p1OfferSlot=slot; ts.p1OfferCount=count;
                    ts.p1Confirm=false;  ts.p2Confirm=false;
                    SyncTradeState(ts);
                } else if (ts.p2Id == session.entityId) {
                    ts.p2OfferSlot=slot; ts.p2OfferCount=count;
                    ts.p1Confirm=false;  ts.p2Confirm=false;
                    SyncTradeState(ts);
                }
            }
        }

        // -----------------------------------------------------------------
        // MSG_TRADE_CONFIRM
        // -----------------------------------------------------------------
        else if (op == std::to_underlying(PacketType::MSG_TRADE_CONFIRM)) {
            std::lock_guard lock(tradeMutex);
            for (auto it = activeTrades.begin(); it != activeTrades.end(); ++it) {
                if      (it->p1Id == session.entityId) it->p1Confirm = true;
                else if (it->p2Id == session.entityId) it->p2Confirm = true;

                if (it->p1Confirm && it->p2Confirm) {
                    ExecuteTradeValidation(*it);
                    gEventBus.Publish(TradeCompletedEvent{
                        .player1Id = it->p1Id,
                        .player2Id = it->p2Id
                    });
                    SendInventoryToSession(it->p1Id);
                    SendInventoryToSession(it->p2Id);
                    activeTrades.erase(it);
                    break;
                } else {
                    SyncTradeState(*it);
                }
            }
        }

        // -----------------------------------------------------------------
        // MSG_WHISPER
        // -----------------------------------------------------------------
        else if (op == std::to_underlying(PacketType::MSG_WHISPER)) {
            std::string targetName = buf.ReadString();
            std::string text       = buf.ReadString();
            if (text.empty() || text.size() > 256) return;
            SendWhisper(session, targetName, text);
            auto it = std::ranges::find_if(serverRegistry,
                [&](const Entity& e){ return e.id == session.entityId; });
            gEventBus.Publish(ChatMessageEvent{
                .sender = (it != serverRegistry.end()) ? it->name : "Unknown",
                .text = text,
                .channel = 99,
                .isWhisper = true,
                .target = targetName
            });
        }

        // -----------------------------------------------------------------
        // MSG_TALK_NPC
        // -----------------------------------------------------------------
        else if (op == std::to_underlying(PacketType::MSG_TALK_NPC)) {
            uint32_t npcId = buf.ReadUInt32();
            std::lock_guard npcLock(npcMutex);
            if (!npcDatabase.contains(npcId)) return;
            auto& npc = npcDatabase[npcId];

            auto playerIt = std::ranges::find_if(serverRegistry,
                [&](const Entity& e){ return e.id == session.entityId; });
            if (playerIt == serverRegistry.end()) return;
            float dx = npc.x - playerIt->transform.x;
            float dz = npc.z - playerIt->transform.z;
            if (std::sqrt(dx*dx + dz*dz) > 5.0f) {
                AddLog("[Anti-Cheat] TalkToNPC Reichweiten-Check fehlgeschlagen.");
                return;
            }
            AddLog("[Quest] {} spricht mit {}", playerIt->name, npc.name);

            if (UpdateQuestProgress(session.entityId, QuestObjectiveType::TalkToNPC, npcId)) {
                auto pIt = std::ranges::find_if(serverRegistry,
                    [&](const Entity& e){ return e.id == session.entityId; });
                if (pIt != serverRegistry.end() && playerQuestLog.contains(session.entityId)) {
                    for (const auto& prog : playerQuestLog[session.entityId])
                        if (prog.state == QuestState::Completed && questDatabase.contains(prog.questId)) {
                            pIt->persistence.gold += questDatabase[prog.questId].rewardGold;
                            gEventBus.Publish(QuestCompletedEvent{
                                .playerId = session.entityId,
                                .questId = prog.questId,
                                .rewardGold = questDatabase[prog.questId].rewardGold,
                                .rewardXP = questDatabase[prog.questId].rewardXP
                            });
                        }
                    pIt->persistence.isDirty = true;
                }
                ByteBuffer qpkt; qpkt.WriteUInt8(std::to_underlying(PacketType::MSG_QUEST_COMPLETE));
                for (const auto& prog : playerQuestLog[session.entityId])
                    if (prog.state == QuestState::Completed)
                        qpkt.WriteUInt32(prog.questId);
                SendToClient(session, std::span(qpkt.data));
            }

            if (npc.offeredQuestId != 0) {
                bool alreadyHas = false;
                if (playerQuestLog.contains(session.entityId))
                    for (const auto& p : playerQuestLog[session.entityId])
                        if (p.questId == npc.offeredQuestId) { alreadyHas=true; break; }
                if (!alreadyHas) {
                    ByteBuffer upkt; upkt.WriteUInt8(std::to_underlying(PacketType::MSG_QUEST_UPDATE));
                    upkt.WriteUInt32(npc.offeredQuestId);
                    upkt.WriteUInt8(std::to_underlying(QuestState::Inactive));
                    SendToClient(session, std::span(upkt.data));
                }
            }
        }

    } catch (const std::exception& ex) {
        AddLog("[Server] Paket-Fehler: {}", ex.what());
    }
}
