// =============================================================================
// server/ai/Pathfinding.h — A* Pathfinding + NavMesh System (AP-47)
// =============================================================================
// KORREKTUR: Grid-basiertes A* Pathfinding mit NavMesh-Interface.
// Unterstützt statische Hindernisse, dynamische Kollisionsprüfung und
// Pfadglättung (String Pulling). Integriert mit ECS-World.
// =============================================================================
#pragma once
#include "../../math/Vector.h"
#include "../../ecs/ecs_EcsWorld.h"
#include "../../ecs/Components.h"

#include <vector>
#include <span>
#include <unordered_map>
#include <unordered_set>
#include <queue>
#include <memory>
#include <functional>
#include <optional>
#include <expected>
#include <cmath>

namespace ai {

// =============================================================================
// Pathfinding Error
// =============================================================================
enum class PathError {
    NoPathFound,
    StartBlocked,
    GoalBlocked,
    InvalidGrid,
    OutOfMemory
};

// =============================================================================
// Grid Cell
// =============================================================================
struct GridCell {
    int x = 0, z = 0;
    bool walkable = true;
    float cost = 1.0f;  // Movement cost multiplier
    float height = 0.0f; // Terrain height at this cell

    [[nodiscard]] bool operator==(const GridCell& o) const noexcept {
        return x == o.x && z == o.z;
    }
};

struct CellHash {
    [[nodiscard]] size_t operator()(const GridCell& c) const noexcept {
        return std::hash<int64_t>{}((static_cast<int64_t>(c.x) << 32) ^ static_cast<uint32_t>(c.z));
    }
};

// =============================================================================
// Path Node (für A*)
// =============================================================================
struct PathNode {
    GridCell cell;
    float gCost = 0.0f;  // Cost from start
    float hCost = 0.0f;  // Heuristic to goal
    float fCost() const { return gCost + hCost; }

    PathNode* parent = nullptr;

    [[nodiscard]] bool operator>(const PathNode& o) const noexcept {
        return fCost() > o.fCost();
    }
};

// =============================================================================
// Path Result
// =============================================================================
struct Path {
    std::vector<math::Vec3> waypoints;
    float totalLength = 0.0f;
    bool isComplete = false;

    [[nodiscard]] bool IsEmpty() const { return waypoints.empty(); }
    [[nodiscard]] math::Vec3 GetCurrentWaypoint(size_t index) const {
        return index < waypoints.size() ? waypoints[index] : math::Vec3{};
    }
};

// =============================================================================
// Navigation Grid
// =============================================================================
class NavigationGrid {
    int width = 0;
    int depth = 0;
    float cellSize = 1.0f;
    math::Vec3 origin{0.0f, 0.0f, 0.0f};

    std::vector<GridCell> cells;
    std::unordered_set<GridCell, CellHash> dynamicObstacles;
    mutable std::mutex gridMutex;

public:
    NavigationGrid() = default;
    NavigationGrid(int w, int d, float cs, const math::Vec3& org);

    NavigationGrid(const NavigationGrid&) = delete;
    NavigationGrid& operator=(const NavigationGrid&) = delete;

    // ===================================================================
    // Grid Management
    // ===================================================================
    [[nodiscard]] bool InitializeFromTerrain(const game::TerrainData& terrain);
    void SetWalkable(int x, int z, bool walkable);
    void SetCost(int x, int z, float cost);
    void SetHeight(int x, int z, float height);

    // ===================================================================
    // Dynamic Obstacles (Entities, etc.)
    // ===================================================================
    void AddDynamicObstacle(int x, int z);
    void RemoveDynamicObstacle(int x, int z);
    void ClearDynamicObstacles();
    void UpdateFromEntities(ecs::EcsWorld& world);

    // ===================================================================
    // Queries
    // ===================================================================
    [[nodiscard]] GridCell* GetCell(int x, int z);
    [[nodiscard]] const GridCell* GetCell(int x, int z) const;
    [[nodiscard]] GridCell WorldToCell(float x, float z) const;
    [[nodiscard]] math::Vec3 CellToWorld(int x, int z) const;
    [[nodiscard]] bool IsWalkable(int x, int z) const;
    [[nodiscard]] bool IsInBounds(int x, int z) const;
    [[nodiscard]] std::vector<GridCell> GetNeighbors(const GridCell& cell) const;

    // ===================================================================
    // Stats
    // ===================================================================
    [[nodiscard]] int GetWidth() const { return width; }
    [[nodiscard]] int GetDepth() const { return depth; }
    [[nodiscard]] float GetCellSize() const { return cellSize; }
    [[nodiscard]] size_t GetDynamicObstacleCount() const;
};

// =============================================================================
// A* Pathfinder
// =============================================================================
class AStarPathfinder {
    NavigationGrid& grid;

    // Heuristic function (configurable)
    std::function<float(const GridCell&, const GridCell&)> heuristic;

    static constexpr float DIAGONAL_COST = 1.41421356f; // sqrt(2)
    static constexpr size_t MAX_SEARCH_NODES = 10000;

public:
    explicit AStarPathfinder(NavigationGrid& g);

    // ===================================================================
    // Pathfinding
    // ===================================================================
    [[nodiscard]] std::expected<Path, PathError> FindPath(
        const math::Vec3& start,
        const math::Vec3& goal);

    [[nodiscard]] std::expected<Path, PathError> FindPath(
        const GridCell& startCell,
        const GridCell& goalCell);

    // ===================================================================
    // Path Smoothing (String Pulling / Funnel Algorithm)
    // ===================================================================
    [[nodiscard]] Path SmoothPath(const Path& path) const;

    // ===================================================================
    // Heuristic
    // ===================================================================
    void SetHeuristic(std::function<float(const GridCell&, const GridCell&)> h);

private:
    [[nodiscard]] static float DefaultHeuristic(const GridCell& a, const GridCell& b);
    [[nodiscard]] float GetMovementCost(const GridCell& from, const GridCell& to) const;
    [[nodiscard]] bool IsLineOfSight(const GridCell& from, const GridCell& to) const;
};

// =============================================================================
// Path Follower (KI-Integration)
// =============================================================================
class PathFollower {
    Path currentPath;
    size_t currentWaypointIndex = 0;
    float waypointReachedDistance = 0.5f;
    bool isFollowing = false;

public:
    void SetPath(const Path& path);
    void ClearPath();

    [[nodiscard]] bool HasPath() const { return isFollowing && !currentPath.IsEmpty(); }
    [[nodiscard]] bool IsComplete() const { return currentPath.isComplete; }
    [[nodiscard]] size_t GetCurrentWaypointIndex() const { return currentWaypointIndex; }

    // Returns next target position, advances waypoint if reached
    [[nodiscard]] std::optional<math::Vec3> Update(const math::Vec3& currentPos, float deltaTime);

    void SetWaypointReachedDistance(float dist) { waypointReachedDistance = dist; }
};

// =============================================================================
// NavMesh Builder (Future: von Terrain generieren)
// =============================================================================
class NavMeshBuilder {
public:
    [[nodiscard]] static std::unique_ptr<NavigationGrid> BuildFromTerrain(
        const game::TerrainData& terrain,
        float cellSize = 1.0f);

    [[nodiscard]] static std::unique_ptr<NavigationGrid> BuildFromECS(
        ecs::EcsWorld& world,
        float cellSize = 1.0f,
        const math::Vec3& center = math::Vec3{0.0f, 0.0f, 0.0f},
        int gridSize = 256);
};

} // namespace ai
