// =============================================================================
// editor/CommandSystem.cpp  —  CommandSystem Implementation
// =============================================================================
#include "CommandSystem.h"

std::vector<std::unique_ptr<ICommand>> gCmdUndoStack;
std::vector<std::unique_ptr<ICommand>> gCmdRedoStack;

void CmdExecute(std::unique_ptr<ICommand> cmd) {
    cmd->Execute();
    cmd->executed = true;
    gCmdUndoStack.push_back(std::move(cmd));
    if (gCmdUndoStack.size() > CMD_MAX_DEPTH)
        gCmdUndoStack.erase(gCmdUndoStack.begin());
    gCmdRedoStack.clear();
}

bool CmdCanUndo() { return !gCmdUndoStack.empty(); }
bool CmdCanRedo() { return !gCmdRedoStack.empty(); }

void CmdUndo() {
    if (!CmdCanUndo()) return;
    auto& cmd = gCmdUndoStack.back();
    cmd->Undo();
    cmd->executed = false;
    gCmdRedoStack.push_back(std::move(cmd));
    gCmdUndoStack.pop_back();
}

void CmdRedo() {
    if (!CmdCanRedo()) return;
    auto& cmd = gCmdRedoStack.back();
    cmd->Execute();
    cmd->executed = true;
    gCmdUndoStack.push_back(std::move(cmd));
    gCmdRedoStack.pop_back();
}

std::string_view CmdGetUndoName() {
    return gCmdUndoStack.empty() ? "" : gCmdUndoStack.back()->GetName();
}
std::string_view CmdGetRedoName() {
    return gCmdRedoStack.empty() ? "" : gCmdRedoStack.back()->GetName();
}

void CmdClear() {
    gCmdUndoStack.clear();
    gCmdRedoStack.clear();
}

// --- CmdTerrainBrush ---------------------------------------------------------
void CmdTerrainBrush::Execute() {
    heightMap = after;
    RebuildTerrainMeshOnGPU();
}
void CmdTerrainBrush::Undo() {
    heightMap = before;
    RebuildTerrainMeshOnGPU();
}

// --- CmdSetTransform ---------------------------------------------------------
void CmdSetTransform::Execute() {
    auto it = std::ranges::find_if(serverRegistry,
        [this](const Entity& e){ return e.id == entityId; });
    if (it != serverRegistry.end()) it->transform = newT;
}
void CmdSetTransform::Undo() {
    auto it = std::ranges::find_if(serverRegistry,
        [this](const Entity& e){ return e.id == entityId; });
    if (it != serverRegistry.end()) it->transform = oldT;
}

// --- CmdSpawnEntity ----------------------------------------------------------
void CmdSpawnEntity::Execute() {
    if (spawned) return;
    serverRegistry.push_back(entity);
    spawned = true;
    ByteBuffer pkt; pkt.WriteUInt8(std::to_underlying(PacketType::MSG_ENTITY_SPAWN));
    pkt.WriteUInt32(entity.id); pkt.WriteString(entity.name);
    pkt.WriteUInt8(entity.isMonster ? 1 : 0);
    pkt.WriteFloat(entity.transform.x); pkt.WriteFloat(entity.transform.z);
    pkt.WriteString(entity.render.materialId);
    pkt.WriteFloat(entity.render.scaleY);
    pkt.WriteString(entity.render.meshId);
    BroadcastToAll(std::span(pkt.data));
}

void CmdSpawnEntity::Undo() {
    if (!spawned) return;
    serverRegistry.erase(
        std::ranges::remove_if(serverRegistry,
            [this](const Entity& e){ return e.id == entity.id; }).begin(),
        serverRegistry.end());
    spawned = false;
    ByteBuffer pkt; pkt.WriteUInt8(std::to_underlying(PacketType::MSG_ENTITY_DESPAWN));
    pkt.WriteUInt32(entity.id);
    BroadcastToAll(std::span(pkt.data));
}

// --- CmdDeleteEntity ---------------------------------------------------------
void CmdDeleteEntity::Execute() {
    if (deleted) return;
    serverRegistry.erase(
        std::ranges::remove_if(serverRegistry,
            [this](const Entity& e){ return e.id == backup.id; }).begin(),
        serverRegistry.end());
    deleted = true;
    ByteBuffer pkt; pkt.WriteUInt8(std::to_underlying(PacketType::MSG_ENTITY_DESPAWN));
    pkt.WriteUInt32(backup.id);
    BroadcastToAll(std::span(pkt.data));
}

void CmdDeleteEntity::Undo() {
    if (!deleted) return;
    serverRegistry.push_back(backup);
    deleted = false;
    ByteBuffer pkt; pkt.WriteUInt8(std::to_underlying(PacketType::MSG_ENTITY_SPAWN));
    pkt.WriteUInt32(backup.id); pkt.WriteString(backup.name);
    pkt.WriteUInt8(backup.isMonster ? 1 : 0);
    pkt.WriteFloat(backup.transform.x); pkt.WriteFloat(backup.transform.z);
    pkt.WriteString(backup.render.materialId);
    pkt.WriteFloat(backup.render.scaleY); pkt.WriteString(backup.render.meshId);
    BroadcastToAll(std::span(pkt.data));
}
