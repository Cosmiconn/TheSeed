// =============================================================================
// server/ai/Pathfinding.cpp — A* Pathfinding + NavMesh Implementation (AP-47)
// =============================================================================
#include "Pathfinding.h"
#include "../../core/Log.h"
#include <algorithm>
#include <memory_resource>

namespace ai {

// =============================================================================
// NavigationGrid Implementation
// =============================================================================
NavigationGrid::NavigationGrid(int w, int d, float cs, const math::Vec3& org)
    : width(w), depth(d), cellSize(cs), origin(org) {
    cells.resize(static_cast<size_t>(w) * static_cast<size_t>(d));
    for (int z = 0; z < d; ++z) {
        for (int x = 0; x < w; ++x) {
            size_t idx = static_cast<size_t>(z) * w + x;
            cells[idx].x = x;
            cells[idx].z = z;
            cells[idx].walkable = true;
            cells[idx].cost = 1.0f;
            cells[idx].height = 0.0f;
        }
    }
}

bool NavigationGrid::InitializeFromTerrain(const game::TerrainData& terrain) {
    if (terrain.heightMap.empty()) return false;

    // Map terrain to grid
    for (auto& cell : cells) {
        float worldX = origin.x + cell.x * cellSize;
        float worldZ = origin.z + cell.z * cellSize;

        // Sample terrain height
        cell.height = terrain.GetHeightAt(worldX, worldZ);

        // Mark unwalkable if too steep or underwater
        float slope = terrain.GetSlopeAt(worldX, worldZ);
        if (slope > 45.0f || cell.height < -1.0f) {
            cell.walkable = false;
        }
    }

    AddLog("[NavGrid] Initialized from terrain: {}x{} cells", width, depth);
    return true;
}

void NavigationGrid::SetWalkable(int x, int z, bool walkable) {
    std::lock_guard lock(gridMutex);
    if (!IsInBounds(x, z)) return;
    size_t idx = static_cast<size_t>(z) * width + x;
    cells[idx].walkable = walkable;
}

void NavigationGrid::SetCost(int x, int z, float cost) {
    std::lock_guard lock(gridMutex);
    if (!IsInBounds(x, z)) return;
    size_t idx = static_cast<size_t>(z) * width + x;
    cells[idx].cost = cost;
}

void NavigationGrid::SetHeight(int x, int z, float height) {
    std::lock_guard lock(gridMutex);
    if (!IsInBounds(x, z)) return;
    size_t idx = static_cast<size_t>(z) * width + x;
    cells[idx].height = height;
}

void NavigationGrid::AddDynamicObstacle(int x, int z) {
    std::lock_guard lock(gridMutex);
    dynamicObstacles.insert(GridCell{x, z});
}

void NavigationGrid::RemoveDynamicObstacle(int x, int z) {
    std::lock_guard lock(gridMutex);
    dynamicObstacles.erase(GridCell{x, z});
}

void NavigationGrid::ClearDynamicObstacles() {
    std::lock_guard lock(gridMutex);
    dynamicObstacles.clear();
}

void NavigationGrid::UpdateFromEntities(ecs::EcsWorld& world) {
    ClearDynamicObstacles();

    auto query = world.Query<game::Transform, game::Hitbox>();
    for (auto [handle] : query) {
        auto* transform = world.GetComponent<game::Transform>(handle);
        auto* hitbox = world.GetComponent<game::Hitbox>(handle);
        if (!transform || !hitbox) continue;

        // Mark cells within hitbox radius as obstacles
        GridCell center = WorldToCell(transform->x, transform->z);
        int radius = static_cast<int>(std::ceil(hitbox->radius / cellSize));

        for (int dx = -radius; dx <= radius; ++dx) {
            for (int dz = -radius; dz <= radius; ++dz) {
                if (dx*dx + dz*dz <= radius*radius) {
                    AddDynamicObstacle(center.x + dx, center.z + dz);
                }
            }
        }
    }
}

GridCell* NavigationGrid::GetCell(int x, int z) {
    if (!IsInBounds(x, z)) return nullptr;
    return &cells[static_cast<size_t>(z) * width + x];
}

const GridCell* NavigationGrid::GetCell(int x, int z) const {
    if (!IsInBounds(x, z)) return nullptr;
    return &cells[static_cast<size_t>(z) * width + x];
}

GridCell NavigationGrid::WorldToCell(float x, float z) const {
    int gx = static_cast<int>(std::floor((x - origin.x) / cellSize));
    int gz = static_cast<int>(std::floor((z - origin.z) / cellSize));
    return GridCell{gx, gz, true};
}

math::Vec3 NavigationGrid::CellToWorld(int x, int z) const {
    return math::Vec3{
        origin.x + x * cellSize + cellSize * 0.5f,
        GetCell(x, z) ? GetCell(x, z)->height : 0.0f,
        origin.z + z * cellSize + cellSize * 0.5f
    };
}

bool NavigationGrid::IsWalkable(int x, int z) const {
    const auto* cell = GetCell(x, z);
    if (!cell) return false;
    if (!cell->walkable) return false;
    if (dynamicObstacles.contains(GridCell{x, z})) return false;
    return true;
}

bool NavigationGrid::IsInBounds(int x, int z) const {
    return x >= 0 && x < width && z >= 0 && z < depth;
}

std::vector<GridCell> NavigationGrid::GetNeighbors(const GridCell& cell) const {
    std::vector<GridCell> neighbors;
    neighbors.reserve(8);

    static constexpr int dx[] = {-1, 0, 1, -1, 1, -1, 0, 1};
    static constexpr int dz[] = {-1, -1, -1, 0, 0, 1, 1, 1};

    for (int i = 0; i < 8; ++i) {
        int nx = cell.x + dx[i];
        int nz = cell.z + dz[i];
        if (IsWalkable(nx, nz)) {
            neighbors.push_back(*GetCell(nx, nz));
        }
    }

    return neighbors;
}

size_t NavigationGrid::GetDynamicObstacleCount() const {
    std::lock_guard lock(gridMutex);
    return dynamicObstacles.size();
}

// =============================================================================
// AStarPathfinder Implementation
// =============================================================================
AStarPathfinder::AStarPathfinder(NavigationGrid& g) : grid(g) {
    heuristic = DefaultHeuristic;
}

float AStarPathfinder::DefaultHeuristic(const GridCell& a, const GridCell& b) {
    // Octile distance (allows diagonal movement)
    int dx = std::abs(a.x - b.x);
    int dz = std::abs(a.z - b.z);
    return std::min(dx, dz) * DIAGONAL_COST + std::abs(dx - dz);
}

void AStarPathfinder::SetHeuristic(
    std::function<float(const GridCell&, const GridCell&)> h) {
    heuristic = std::move(h);
}

float AStarPathfinder::GetMovementCost(const GridCell& from, const GridCell& to) const {
    float baseCost = grid.GetCell(to.x, to.z)->cost;

    // Diagonal movement costs more
    if (from.x != to.x && from.z != to.z) {
        baseCost *= DIAGONAL_COST;
    }

    // Height difference adds cost
    float heightDiff = std::abs(grid.GetCell(to.x, to.z)->height - grid.GetCell(from.x, from.z)->height);
    baseCost += heightDiff * 2.0f;

    return baseCost;
}

std::expected<Path, PathError> AStarPathfinder::FindPath(
    const math::Vec3& start,
    const math::Vec3& goal) {

    GridCell startCell = grid.WorldToCell(start.x, start.z);
    GridCell goalCell = grid.WorldToCell(goal.x, goal.z);

    return FindPath(startCell, goalCell);
}

std::expected<Path, PathError> AStarPathfinder::FindPath(
    const GridCell& startCell,
    const GridCell& goalCell) {

    // Validate cells
    if (!grid.IsInBounds(startCell.x, startCell.z)) {
        return std::unexpected(PathError::StartBlocked);
    }
    if (!grid.IsInBounds(goalCell.x, goalCell.z)) {
        return std::unexpected(PathError::GoalBlocked);
    }
    if (!grid.IsWalkable(startCell.x, startCell.z)) {
        return std::unexpected(PathError::StartBlocked);
    }
    if (!grid.IsWalkable(goalCell.x, goalCell.z)) {
        return std::unexpected(PathError::GoalBlocked);
    }

    // A* Algorithm
    std::priority_queue<PathNode, std::vector<PathNode>, std::greater<>> openSet;
    std::unordered_set<GridCell, CellHash> closedSet;
    std::unordered_map<GridCell, float, CellHash> gScore;
    std::unordered_map<GridCell, GridCell, CellHash> cameFrom;

    PathNode startNode;
    startNode.cell = startCell;
    startNode.gCost = 0.0f;
    startNode.hCost = heuristic(startCell, goalCell);
    openSet.push(startNode);
    gScore[startCell] = 0.0f;

    size_t nodesSearched = 0;

    while (!openSet.empty() && nodesSearched < MAX_SEARCH_NODES) {
        PathNode current = openSet.top();
        openSet.pop();

        if (closedSet.contains(current.cell)) continue;
        closedSet.insert(current.cell);
        nodesSearched++;

        // Goal reached
        if (current.cell == goalCell) {
            // Reconstruct path
            Path path;
            GridCell trace = goalCell;

            while (true) {
                path.waypoints.insert(path.waypoints.begin(),
                    grid.CellToWorld(trace.x, trace.z));

                auto it = cameFrom.find(trace);
                if (it == cameFrom.end()) break;
                trace = it->second;
            }

            // Add start position
            path.waypoints.insert(path.waypoints.begin(),
                grid.CellToWorld(startCell.x, startCell.z));

            // Calculate total length
            for (size_t i = 1; i < path.waypoints.size(); ++i) {
                path.totalLength += (path.waypoints[i] - path.waypoints[i - 1]).Length();
            }

            path.isComplete = true;

            AddLog("[A*] Path found: {} nodes, {} waypoints, {:.2f}m, searched {} nodes",
                   path.waypoints.size(), path.waypoints.size(), path.totalLength, nodesSearched);

            return path;
        }

        // Explore neighbors
        for (const auto& neighbor : grid.GetNeighbors(current.cell)) {
            if (closedSet.contains(neighbor)) continue;

            float tentativeG = current.gCost + GetMovementCost(current.cell, neighbor);

            auto it = gScore.find(neighbor);
            if (it == gScore.end() || tentativeG < it->second) {
                cameFrom[neighbor] = current.cell;
                gScore[neighbor] = tentativeG;

                PathNode neighborNode;
                neighborNode.cell = neighbor;
                neighborNode.gCost = tentativeG;
                neighborNode.hCost = heuristic(neighbor, goalCell);
                openSet.push(neighborNode);
            }
        }
    }

    AddLog("[A*] No path found after searching {} nodes", nodesSearched);
    return std::unexpected(PathError::NoPathFound);
}

bool AStarPathfinder::IsLineOfSight(const GridCell& from, const GridCell& to) const {
    // Bresenham's line algorithm for line of sight
    int dx = std::abs(to.x - from.x);
    int dz = std::abs(to.z - from.z);
    int sx = from.x < to.x ? 1 : -1;
    int sz = from.z < to.z ? 1 : -1;
    int err = dx - dz;

    int x = from.x;
    int z = from.z;

    while (true) {
        if (!grid.IsWalkable(x, z)) return false;
        if (x == to.x && z == to.z) break;

        int e2 = 2 * err;
        if (e2 > -dz) {
            err -= dz;
            x += sx;
        }
        if (e2 < dx) {
            err += dx;
            z += sz;
        }
    }

    return true;
}

Path AStarPathfinder::SmoothPath(const Path& path) const {
    if (path.waypoints.size() <= 2) return path;

    Path smoothed;
    smoothed.waypoints.push_back(path.waypoints[0]);

    size_t current = 0;
    while (current < path.waypoints.size() - 1) {
        size_t next = current + 1;

        // Try to skip as many waypoints as possible
        for (size_t i = path.waypoints.size() - 1; i > current; --i) {
            GridCell from = grid.WorldToCell(
                path.waypoints[current].x,
                path.waypoints[current].z);
            GridCell to = grid.WorldToCell(
                path.waypoints[i].x,
                path.waypoints[i].z);

            if (IsLineOfSight(from, to)) {
                next = i;
                break;
            }
        }

        smoothed.waypoints.push_back(path.waypoints[next]);
        current = next;
    }

    smoothed.isComplete = true;

    // Recalculate length
    for (size_t i = 1; i < smoothed.waypoints.size(); ++i) {
        smoothed.totalLength += (smoothed.waypoints[i] - smoothed.waypoints[i - 1]).Length();
    }

    AddLog("[A*] Path smoothed: {} → {} waypoints", path.waypoints.size(), smoothed.waypoints.size());
    return smoothed;
}

// =============================================================================
// PathFollower Implementation
// =============================================================================
void PathFollower::SetPath(const Path& path) {
    currentPath = path;
    currentWaypointIndex = 0;
    isFollowing = !path.IsEmpty();
}

void PathFollower::ClearPath() {
    currentPath = Path{};
    currentWaypointIndex = 0;
    isFollowing = false;
}

std::optional<math::Vec3> PathFollower::Update(const math::Vec3& currentPos, float deltaTime) {
    (void)deltaTime;

    if (!isFollowing || currentPath.IsEmpty()) {
        return std::nullopt;
    }

    if (currentWaypointIndex >= currentPath.waypoints.size()) {
        isFollowing = false;
        currentPath.isComplete = true;
        return std::nullopt;
    }

    math::Vec3 target = currentPath.waypoints[currentWaypointIndex];
    float dist = (target - currentPos).Length();

    if (dist <= waypointReachedDistance) {
        currentWaypointIndex++;
        if (currentWaypointIndex >= currentPath.waypoints.size()) {
            isFollowing = false;
            currentPath.isComplete = true;
            return std::nullopt;
        }
        target = currentPath.waypoints[currentWaypointIndex];
    }

    return target;
}

// =============================================================================
// NavMeshBuilder Implementation
// =============================================================================
std::unique_ptr<NavigationGrid> NavMeshBuilder::BuildFromTerrain(
    const game::TerrainData& terrain,
    float cellSize) {

    // Calculate grid size from terrain bounds
    int gridSize = static_cast<int>(std::ceil(terrain.worldSize / cellSize));

    auto grid = std::make_unique<NavigationGrid>(
        gridSize, gridSize, cellSize,
        math::Vec3{-terrain.worldSize * 0.5f, 0.0f, -terrain.worldSize * 0.5f});

    if (!grid->InitializeFromTerrain(terrain)) {
        return nullptr;
    }

    return grid;
}

std::unique_ptr<NavigationGrid> NavMeshBuilder::BuildFromECS(
    ecs::EcsWorld& world,
    float cellSize,
    const math::Vec3& center,
    int gridSize) {

    auto grid = std::make_unique<NavigationGrid>(
        gridSize, gridSize, cellSize,
        math::Vec3{center.x - gridSize * cellSize * 0.5f, 0.0f, center.z - gridSize * cellSize * 0.5f});

    // Mark unwalkable cells based on entities
    auto query = world.Query<game::Transform, game::Hitbox>();
    for (auto [handle] : query) {
        auto* transform = world.GetComponent<game::Transform>(handle);
        auto* hitbox = world.GetComponent<game::Hitbox>(handle);
        if (!transform || !hitbox) continue;

        GridCell cell = grid->WorldToCell(transform->x, transform->z);
        int radius = static_cast<int>(std::ceil(hitbox->radius / cellSize));

        for (int dx = -radius; dx <= radius; ++dx) {
            for (int dz = -radius; dz <= radius; ++dz) {
                if (dx*dx + dz*dz <= radius*radius) {
                    grid->SetWalkable(cell.x + dx, cell.z + dz, false);
                }
            }
        }
    }

    return grid;
}

} // namespace ai
