// =============================================================================
// server/Validation.cpp  —  Validation Implementation
// =============================================================================
#include "Validation.h"

// =============================================================================
// CHAT – Broadcast & Whisper
// =============================================================================
void AddChatMessage(std::string_view sender,
                    std::string_view text,
                    uint32_t         channel) {
    {
        std::lock_guard lock(chatMutex);
        chatHistory.push_back({std::string(sender), std::string(text), channel});
        if (chatHistory.size() > MAX_CHAT_HISTORY)
            chatHistory.erase(chatHistory.begin());
    }
    ByteBuffer pkt; pkt.WriteUInt8(std::to_underlying(PacketType::MSG_CHAT));
    pkt.WriteUInt32(channel);
    pkt.WriteString(std::string(sender));
    pkt.WriteString(std::string(text));
    BroadcastToAll(std::span(pkt.data));
}

void SendWhisper(ClientSession&     senderSession,
                 std::string_view targetName,
                 std::string_view text) {
    auto senderIt = std::ranges::find_if(serverRegistry,
        [&](const Entity& e){ return e.id == senderSession.entityId; });
    if (senderIt == serverRegistry.end()) return;
    const std::string& senderName = senderIt->name;

    std::lock_guard lock(sessionsMutex);
    ClientSession* targetSession = nullptr;
    for (auto& cs : clientSessions) {
        auto entIt = std::ranges::find_if(serverRegistry,
            [&](const Entity& e){ return e.id == cs.entityId && e.name == targetName; });
        if (entIt != serverRegistry.end()) { targetSession = &cs; break; }
    }

    if (!targetSession) {
        ByteBuffer err; err.WriteUInt8(std::to_underlying(PacketType::MSG_WHISPER_NOTIFY));
        err.WriteString("[System]");
        err.WriteString(std::format("Spieler '{}' nicht gefunden.", targetName));
        SendToClient(senderSession, std::span(err.data));
        AddLog("[Chat] Whisper-Ziel nicht gefunden: {}", targetName);
        return;
    }
    {
        ByteBuffer pkt; pkt.WriteUInt8(std::to_underlying(PacketType::MSG_WHISPER_NOTIFY));
        pkt.WriteString(senderName); pkt.WriteString(std::string(text));
        SendToClient(*targetSession, std::span(pkt.data));
    }
    {
        ByteBuffer echo; echo.WriteUInt8(std::to_underlying(PacketType::MSG_WHISPER_NOTIFY));
        echo.WriteString(std::format("[An {}]", targetName)); echo.WriteString(std::string(text));
        SendToClient(senderSession, std::span(echo.data));
    }
    AddLog("[Chat] Whisper: {} -> {}", senderName, targetName);
    {
        std::lock_guard chatLock(chatMutex);
        chatHistory.push_back({std::format("{}->{}", senderName, targetName), std::string(text), 99});
        if (chatHistory.size() > MAX_CHAT_HISTORY)
            chatHistory.erase(chatHistory.begin());
    }
}

// =============================================================================
// ITEM VALIDATION & EQUIP
// =============================================================================
bool EquipItem(uint32_t playerId, uint32_t slotIndex) {
    std::lock_guard lock(inventoryMutex);
    auto& inv = playerInventories[playerId];
    if (slotIndex >= inv.size() || inv[slotIndex].templateId == 0) return false;

    uint32_t tid  = inv[slotIndex].templateId;
    if (!itemDatabase.contains(tid)) return false;
    auto& tmpl = itemDatabase[tid];
    if (tmpl.slot == 0) return false;

    uint32_t playerLevel = 1;
    auto pIt = std::ranges::find_if(serverRegistry,
        [&](const Entity& e){ return e.id == playerId; });
    if (pIt != serverRegistry.end()) playerLevel = pIt->persistence.level;

    if (playerLevel < tmpl.minLevel) {
        AddLog("[Anti-Cheat] Item Validation: Level zu gering (Entity {})", playerId);
        return false;
    }

    if (inv[slotIndex].isEquipped) {
        inv[slotIndex].isEquipped = false;
    } else {
        for (auto& item : inv)
            if (item.templateId != 0 && itemDatabase.contains(item.templateId) &&
                itemDatabase[item.templateId].slot == tmpl.slot && item.isEquipped)
                item.isEquipped = false;
        inv[slotIndex].isEquipped = true;
    }
    return true;
}

void SendInventoryToSession(uint32_t playerId) {
    std::lock_guard sessLock(sessionsMutex);
    auto sIt = std::ranges::find_if(clientSessions,
        [playerId](const ClientSession& cs){ return cs.entityId == playerId; });
    if (sIt == clientSessions.end()) return;

    ByteBuffer pkt; pkt.WriteUInt8(std::to_underlying(PacketType::MSG_INVENTORY_UPDATE));
    std::lock_guard invLock(inventoryMutex);
    auto& inv = playerInventories[playerId];
    pkt.WriteUInt32(static_cast<uint32_t>(inv.size()));
    for (const auto& item : inv) {
        pkt.WriteUInt32(item.templateId);
        pkt.WriteUInt32(item.count);
        pkt.WriteUInt8(item.isEquipped ? 1 : 0);
    }
    SendToClient(*sIt, std::span(pkt.data));
}

// =============================================================================
// HANDEL – Sync & Validation
// =============================================================================
void SyncTradeState(const TradeSession& ts) {
    ByteBuffer pkt; pkt.WriteUInt8(std::to_underlying(PacketType::MSG_TRADE_STATE));
    pkt.WriteUInt32(ts.p1Id); pkt.WriteUInt32(ts.p2Id);
    pkt.WriteUInt32(ts.p1OfferSlot);  pkt.WriteUInt32(ts.p1OfferCount);
    pkt.WriteUInt8 (ts.p1Confirm ? 1 : 0);
    pkt.WriteUInt32(ts.p2OfferSlot);  pkt.WriteUInt32(ts.p2OfferCount);
    pkt.WriteUInt8 (ts.p2Confirm ? 1 : 0);

    std::lock_guard lock(sessionsMutex);
    for (auto& s : clientSessions)
        if (s.entityId == ts.p1Id || s.entityId == ts.p2Id)
            SendToClient(s, std::span(pkt.data));
}

void ExecuteTradeValidation(TradeSession& ts) {
    std::lock_guard lock(inventoryMutex);
    auto& inv1 = playerInventories[ts.p1Id];
    auto& inv2 = playerInventories[ts.p2Id];

    auto valid = [](const std::vector<Item>& inv, uint32_t slot, uint32_t count) -> bool {
        if (slot == 999) return true;
        return slot < INVENTORY_SIZE &&
               inv[slot].templateId != 0 &&
               inv[slot].count >= count &&
               !inv[slot].isEquipped;
    };
    if (!valid(inv1, ts.p1OfferSlot, ts.p1OfferCount) ||
        !valid(inv2, ts.p2OfferSlot, ts.p2OfferCount)) {
        AddLog("[Anti-Cheat] Trade Validation fehlgeschlagen.");
        return;
    }

    auto extract = [](std::vector<Item>& inv, uint32_t slot, uint32_t count) -> Item {
        if (slot == 999) return { 0, 0, false };
        Item tmp = { inv[slot].templateId, count, false };
        inv[slot].count -= count;
        if (inv[slot].count == 0) inv[slot].templateId = 0;
        return tmp;
    };
    auto insert = [](std::vector<Item>& inv, Item itm) {
        if (itm.templateId == 0) return;
        for (auto& slot : inv)
            if (slot.templateId == 0) { slot = itm; return; }
    };

    Item from1 = extract(inv1, ts.p1OfferSlot, ts.p1OfferCount);
    Item from2 = extract(inv2, ts.p2OfferSlot, ts.p2OfferCount);
    insert(inv1, from2);
    insert(inv2, from1);
    AddLog("[Server] Tausch erfolgreich abgeschlossen.");
}
