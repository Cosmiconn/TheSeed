// =============================================================================
// server/ai/Pathfinding.cpp — Pathfinding & Crowd Simulation (AP-48) C++23
// =============================================================================

#include "Pathfinding.h"

#include <algorithm>
#include <numeric>
#include <random>

namespace ai {

// =============================================================================
// PATHFINDER — A* Search
// =============================================================================

std::expected<PathResult, std::string> Pathfinder::FindPath(
    const math::Vector3& start,
    const math::Vector3& end,
    float agentRadius) const {

    (void)agentRadius; // Für erweiterte Pfadbreite

    if (!navMesh) return std::unexpected("NavMesh not initialized");

    // Start/End Polygone finden
    math::Vector3 startPt, endPt;
    uint16_t startPoly = navMesh->FindNearestPoly(start.x, start.y, start.z, &startPt);
    uint16_t endPoly = navMesh->FindNearestPoly(end.x, end.y, end.z, &endPt);

    if (startPoly == 0) return std::unexpected("Start position not on NavMesh");
    if (endPoly == 0) return std::unexpected("End position not on NavMesh");

    // A* Search
    auto polyPath = AStarSearch(startPoly, endPoly);
    if (!polyPath) {
        return std::unexpected("No path found: " + polyPath.error());
    }

    // Funnel-Algorithmus für geraden Pfad
    auto waypoints = StringPull(startPt, endPt, *polyPath);

    PathResult result;
    result.waypoints = std::move(waypoints);
    result.polyCount = static_cast<int>(polyPath->size());
    result.partial = (endPoly != (*polyPath).back());

    // Länge berechnen
    for (size_t i = 1; i < result.waypoints.size(); ++i) {
        result.totalLength += (result.waypoints[i] - result.waypoints[i-1]).Length();
    }

    return result;
}

std::expected<std::vector<uint16_t>, std::string> Pathfinder::AStarSearch(
    uint16_t startPoly, uint16_t endPoly) const {

    const NavMeshTile* tile = navMesh->GetTileAt(0, 0); // Simplifiziert
    if (!tile) return std::unexpected("No NavMesh tile");

    // Open/Closed Sets
    std::priority_queue<PathNode, std::vector<PathNode>, std::greater<>> openSet;
    std::unordered_set<uint16_t> closedSet;
    std::unordered_map<uint16_t, PathNode> nodeMap;

    PathNode startNode;
    startNode.polyRef = startPoly;
    startNode.gCost = 0.0f;
    startNode.hCost = Heuristic(startPoly, endPoly);
    startNode.parent = 0;

    openSet.push(startNode);
    nodeMap[startPoly] = startNode;

    while (!openSet.empty()) {
        PathNode current = openSet.top();
        openSet.pop();

        if (closedSet.contains(current.polyRef)) continue;
        closedSet.insert(current.polyRef);

        if (current.polyRef == endPoly) {
            // Pfad rekonstruieren
            std::vector<uint16_t> path;
            uint16_t currentRef = endPoly;

            while (currentRef != 0) {
                path.push_back(currentRef);
                auto it = nodeMap.find(currentRef);
                if (it == nodeMap.end() || it->second.parent == currentRef) break;
                currentRef = it->second.parent;
            }

            std::ranges::reverse(path);
            return path;
        }

        // Nachbarn expandieren
        if (current.polyRef >= tile->polys.size()) continue;
        const auto& poly = tile->polys[current.polyRef];

        for (int i = 0; i < poly.vertCount; ++i) {
            uint16_t neighbor = poly.neis[i];
            if (neighbor == 0 || closedSet.contains(neighbor)) continue;

            float edgeCost = Heuristic(current.polyRef, neighbor);
            float tentativeG = current.gCost + edgeCost;

            auto it = nodeMap.find(neighbor);
            if (it == nodeMap.end() || tentativeG < it->second.gCost) {
                PathNode neighborNode;
                neighborNode.polyRef = neighbor;
                neighborNode.gCost = tentativeG;
                neighborNode.hCost = Heuristic(neighbor, endPoly);
                neighborNode.parent = current.polyRef;

                nodeMap[neighbor] = neighborNode;
                openSet.push(neighborNode);
            }
        }
    }

    return std::unexpected("No path found");
}

float Pathfinder::Heuristic(uint16_t polyA, uint16_t polyB) const {
    const NavMeshTile* tile = navMesh->GetTileAt(0, 0);
    if (!tile || polyA >= tile->polys.size() || polyB >= tile->polys.size()) {
        return std::numeric_limits<float>::max();
    }

    math::Vector3 centerA = tile->GetPolyCenter(polyA);
    math::Vector3 centerB = tile->GetPolyCenter(polyB);

    return (centerB - centerA).Length();
}

// =============================================================================
// FUNNEL ALGORITHM (String Pulling)
// =============================================================================

std::vector<math::Vector3> Pathfinder::StringPull(
    const math::Vector3& start,
    const math::Vector3& end,
    std::span<const uint16_t> polyPath) const {

    if (polyPath.empty()) return {start, end};
    if (polyPath.size() == 1) return {start, end};

    std::vector<math::Vector3> portals;
    portals.push_back(start); // Start-Portal

    const NavMeshTile* tile = navMesh->GetTileAt(0, 0);
    if (!tile) return {start, end};

    // Portale (Linke/Rechte Kante) zwischen Polygonen sammeln
    for (size_t i = 0; i < polyPath.size() - 1; ++i) {
        math::Vector3 left, right;
        if (GetPortalPoints(polyPath[i], polyPath[i + 1], left, right)) {
            portals.push_back(left);
            portals.push_back(right);
        }
    }

    portals.push_back(end); // End-Portal

    // Funnel Algorithm
    std::vector<math::Vector3> path;
    path.push_back(start);

    math::Vector3 apex = start;
    math::Vector3 left = portals[1];
    math::Vector3 right = portals[2];
    size_t apexIndex = 0, leftIndex = 1, rightIndex = 2;

    for (size_t i = 3; i < portals.size(); i += 2) {
        math::Vector3 newLeft = portals[i];
        math::Vector3 newRight = portals[i + 1];

        // Update left funnel
        if (TriArea2D(apex, left, newLeft) >= 0.0f) {
            if (apex == left || TriArea2D(apex, right, newLeft) < 0.0f) {
                left = newLeft;
                leftIndex = i;
            } else {
                // Right überkreuzt → Apex auf right setzen
                path.push_back(right);
                apex = right;
                apexIndex = rightIndex;
                left = apex;
                right = apex;
                leftIndex = rightIndex = apexIndex;
                i = apexIndex;
                continue;
            }
        }

        // Update right funnel
        if (TriArea2D(apex, right, newRight) <= 0.0f) {
            if (apex == right || TriArea2D(apex, left, newRight) > 0.0f) {
                right = newRight;
                rightIndex = i + 1;
            } else {
                // Left überkreuzt → Apex auf left setzen
                path.push_back(left);
                apex = left;
                apexIndex = leftIndex;
                left = apex;
                right = apex;
                leftIndex = rightIndex = apexIndex;
                i = apexIndex;
                continue;
            }
        }
    }

    path.push_back(end);
    return path;
}

// 2D Triangle Area (für Funnel)
static float TriArea2D(const math::Vector3& a, const math::Vector3& b, 
                        const math::Vector3& c) {
    return (b.x - a.x) * (c.z - a.z) - (c.x - a.x) * (b.z - a.z);
}

bool Pathfinder::GetPortalPoints(uint16_t fromPoly, uint16_t toPoly,
                                  math::Vector3& left, math::Vector3& right) const {
    const NavMeshTile* tile = navMesh->GetTileAt(0, 0);
    if (!tile || fromPoly >= tile->polys.size() || toPoly >= tile->polys.size()) {
        return false;
    }

    const auto& poly = tile->polys[fromPoly];

    // Finde gemeinsame Kante
    for (int i = 0; i < poly.vertCount; ++i) {
        if (poly.neis[i] == toPoly) {
            int next = (i + 1) % poly.vertCount;
            left = tile->verts[poly.verts[i]];
            right = tile->verts[poly.verts[next]];
            return true;
        }
    }

    return false;
}

// =============================================================================
// RAYCAST & RANDOM POINT
// =============================================================================

bool Pathfinder::Raycast(const math::Vector3& start, 
                          const math::Vector3& end,
                          math::Vector3* hitPoint) const {
    auto path = FindPath(start, end);
    if (!path || path->waypoints.size() <= 2) {
        return false; // Direkte Sichtlinie oder blockiert
    }

    // Wenn Pfad nur 2 Punkte hat → direkte Sichtlinie
    if (path->waypoints.size() == 2) return true;

    // Pfad hat Umwege → blockiert
    if (hitPoint && path->waypoints.size() > 2) {
        *hitPoint = path->waypoints[1]; // Erster Knickpunkt
    }
    return false;
}

std::expected<math::Vector3, std::string> Pathfinder::FindRandomPoint(
    const math::Vector3& center, float radius) const {

    const NavMeshTile* tile = navMesh->GetTileAt(center.x, center.z);
    if (!tile || tile->polys.empty()) {
        return std::unexpected("No NavMesh at center");
    }

    static thread_local std::mt19937 rng(
        static_cast<uint32_t>(std::chrono::steady_clock::now().time_since_epoch().count())
    );

    std::uniform_real_distribution<float> dist(-radius, radius);

    // Versuche 10 zufällige Punkte
    for (int attempt = 0; attempt < 10; ++attempt) {
        math::Vector3 candidate(
            center.x + dist(rng),
            center.y,
            center.z + dist(rng)
        );

        if (navMesh->IsWalkable(candidate.x, candidate.y, candidate.z)) {
            return candidate;
        }
    }

    return std::unexpected("Could not find valid random point");
}

// =============================================================================
// CROWD SIMULATION
// =============================================================================

uint32_t CrowdSimulation::AddAgent(const math::Vector3& pos, 
                                    float radius, float maxSpeed) {
    auto agent = std::make_unique<CrowdAgent>();
    agent->id = static_cast<uint32_t>(agents.size() + 1);
    agent->position = pos;
    agent->radius = radius;
    agent->maxSpeed = maxSpeed;
    agent->lastUpdate = std::chrono::steady_clock::now();

    uint32_t id = agent->id;
    agents.push_back(std::move(agent));
    return id;
}

void CrowdSimulation::RemoveAgent(uint32_t id) {
    std::erase_if(agents, [id](const auto& a) { return a->id == id; });
}

void CrowdSimulation::SetAgentTarget(uint32_t id, const math::Vector3& target) {
    auto* agent = GetAgent(id);
    if (!agent || !pathfinder) return;

    agent->target = target;
    agent->reachedTarget = false;

    auto path = pathfinder->FindPath(agent->position, target);
    if (path && path->IsValid()) {
        agent->path = std::move(path->waypoints);
        agent->pathIndex = 0;
    }
}

void CrowdSimulation::Update(float deltaTime) {
    UpdateSpatialGrid();

    for (auto& agentPtr : agents) {
        auto& agent = *agentPtr;
        if (!agent.active) continue;

        // Steering berechnen
        math::Vector3 steering = CalculateSteering(agent, deltaTime);

        // Velocity aktualisieren
        agent.velocity = agent.velocity + steering * deltaTime;

        // Max Speed clampen
        float speed = agent.velocity.Length();
        if (speed > agent.maxSpeed) {
            agent.velocity = agent.velocity.Normalized() * agent.maxSpeed;
        }

        // Position aktualisieren
        agent.position = agent.position + agent.velocity * deltaTime;

        // Ziel erreicht?
        if ((agent.target - agent.position).Length() < 0.5f) {
            agent.reachedTarget = true;
            agent.velocity = math::Vector3{};
        }

        agent.lastUpdate = std::chrono::steady_clock::now();
    }
}

math::Vector3 CrowdSimulation::CalculateSteering(CrowdAgent& agent, float dt) {
    (void)dt;
    math::Vector3 steering;

    // Path Following
    if (!agent.path.empty() && agent.pathIndex < agent.path.size()) {
        math::Vector3 target = agent.path[agent.pathIndex];
        math::Vector3 toTarget = target - agent.position;
        float dist = toTarget.Length();

        if (dist < 1.0f) {
            agent.pathIndex++;
        } else {
            steering = steering + Seek(agent, target);
        }
    }

    // Separation (Kollisionsvermeidung)
    auto neighbors = GetNeighbors(agent.id, 3.0f);
    steering = steering + Separation(agent, neighbors);

    // Max Acceleration
    if (steering.Length() > agent.maxAcceleration) {
        steering = steering.Normalized() * agent.maxAcceleration;
    }

    return steering;
}

math::Vector3 CrowdSimulation::Seek(const CrowdAgent& agent, 
                                     const math::Vector3& target) {
    math::Vector3 desired = target - agent.position;
    desired = desired.Normalized() * agent.maxSpeed;
    return desired - agent.velocity;
}

math::Vector3 CrowdSimulation::Separation(const CrowdAgent& agent,
                                            std::span<const uint32_t> neighbors) {
    math::Vector3 separation;

    for (uint32_t neighborId : neighbors) {
        const auto* other = GetAgent(neighborId);
        if (!other || other->id == agent.id) continue;

        math::Vector3 diff = agent.position - other->position;
        float dist = diff.Length();

        if (dist > 0.0f && dist < agent.radius * 3.0f) {
            separation = separation + diff.Normalized() / dist;
        }
    }

    return separation * 50.0f; // Separation-Gewichtung
}

// =============================================================================
// SPATIAL GRID
// =============================================================================

void CrowdSimulation::UpdateSpatialGrid() {
    spatialGrid.clear();

    for (const auto& agent : agents) {
        if (!agent->active) continue;
        uint64_t key = GetGridKey(agent->position.x, agent->position.z);
        spatialGrid[key].push_back(agent->id);
    }
}

std::vector<uint32_t> CrowdSimulation::GetNeighbors(uint32_t agentId, float radius) {
    std::vector<uint32_t> neighbors;

    auto* agent = GetAgent(agentId);
    if (!agent) return neighbors;

    int range = static_cast<int>(std::ceil(radius / GRID_CELL_SIZE));

    for (int dy = -range; dy <= range; ++dy) {
        for (int dx = -range; dx <= range; ++dx) {
            uint64_t key = GetGridKey(
                agent->position.x + dx * GRID_CELL_SIZE,
                agent->position.z + dy * GRID_CELL_SIZE
            );

            auto it = spatialGrid.find(key);
            if (it != spatialGrid.end()) {
                for (uint32_t id : it->second) {
                    if (id != agentId) neighbors.push_back(id);
                }
            }
        }
    }

    return neighbors;
}

CrowdAgent* CrowdSimulation::GetAgent(uint32_t id) {
    for (auto& agent : agents) {
        if (agent->id == id) return agent.get();
    }
    return nullptr;
}

const CrowdAgent* CrowdSimulation::GetAgent(uint32_t id) const {
    for (const auto& agent : agents) {
        if (agent->id == id) return agent.get();
    }
    return nullptr;
}

void CrowdSimulation::Clear() {
    agents.clear();
    spatialGrid.clear();
}

} // namespace ai
