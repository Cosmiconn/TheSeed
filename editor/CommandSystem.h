#pragma once
// =============================================================================
// editor/CommandSystem.h  —  C++23 Modernized
// ICommand Interface + Stack-Operationen. Konkrete Commands in .cpp
// =============================================================================
#include <vector>
#include <string>
#include <memory>
#include <functional>
#include <optional>
#include <format>
#include <string_view>
#include <span>

// =============================================================================
// COMMAND INTERFACE
// =============================================================================
struct ICommand {
    virtual ~ICommand() = default;
    virtual void Execute() = 0;
    virtual void Undo()    = 0;
    [[nodiscard]] virtual std::string_view GetName() const = 0;
    bool executed = false;
};

// =============================================================================
// COMMAND HISTORY (Global, nur Main-Thread)
// =============================================================================
extern std::vector<std::unique_ptr<ICommand>> gCmdUndoStack;
extern std::vector<std::unique_ptr<ICommand>> gCmdRedoStack;
inline constexpr size_t CMD_MAX_DEPTH = 128;

void CmdExecute(std::unique_ptr<ICommand> cmd);
[[nodiscard]] bool CmdCanUndo();
[[nodiscard]] bool CmdCanRedo();
void CmdUndo();
void CmdRedo();
[[nodiscard]] std::string_view CmdGetUndoName();
[[nodiscard]] std::string_view CmdGetRedoName();
void CmdClear();

// =============================================================================
// CONCRETE COMMAND DECLARATIONS
// =============================================================================
#include "../core/World.h"
#include "../core/ECS.h"
#include "../core/Log.h"
#include "../server/Network.h"
#include <algorithm>
#include <ranges>

// --- Terrain Height Command --------------------------------------------------
struct CmdTerrainBrush : ICommand {
    std::vector<float> before;
    std::vector<float> after;
    std::string desc;

    CmdTerrainBrush(std::vector<float> b, std::vector<float> a, std::string_view d)
        : before(std::move(b)), after(std::move(a)), desc(d) {}

    [[nodiscard]] std::string_view GetName() const override { return desc; }
    void Execute() override;
    void Undo() override;
};

// --- Entity Transform Command ----------------------------------------------
struct CmdSetTransform : ICommand {
    uint32_t entityId;
    TransformComponent oldT;
    TransformComponent newT;

    CmdSetTransform(uint32_t id, const TransformComponent& o, const TransformComponent& n)
        : entityId(id), oldT(o), newT(n) {}

    [[nodiscard]] std::string_view GetName() const override { return "Transform edit"; }
    void Execute() override;
    void Undo() override;
};

// --- Entity Spawn Command ----------------------------------------------------
struct CmdSpawnEntity : ICommand {
    Entity entity;
    bool spawned = false;

    explicit CmdSpawnEntity(Entity e) : entity(std::move(e)) {}
    [[nodiscard]] std::string_view GetName() const override { return "Spawn Entity"; }
    void Execute() override;
    void Undo() override;
};

// --- Entity Delete Command ---------------------------------------------------
struct CmdDeleteEntity : ICommand {
    Entity backup;
    bool deleted = false;

    explicit CmdDeleteEntity(Entity e) : backup(std::move(e)) {}
    [[nodiscard]] std::string_view GetName() const override { return "Delete Entity"; }
    void Execute() override;
    void Undo() override;
};
