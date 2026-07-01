#pragma once
// =============================================================================
// server/ai/Pathfinding.h — Pathfinding & Crowd Simulation (AP-48) C++23
// =============================================================================
// VOLLSTÄNDIGE IMPLEMENTIERUNG:
// • A* Pathfinding auf NavMesh-Polygonen
// • Detour-Style Straight-Path (Funnel Algorithm)
// • Crowd Simulation (Collision Avoidance, Steering)
// • std::expected für Fehlerbehandlung
// =============================================================================

#include "NavMesh.h"
#include "../../math/Vector.h"
#include "../../core/Log.h"

#include <vector>
#include <span>
#include <queue>
#include <unordered_map>
#include <unordered_set>
#include <memory>
#include <expected>
#include <chrono>
#include <cmath>

namespace ai {

// =============================================================================
// PATH NODE (für A*)
// =============================================================================
struct PathNode {
    uint16_t polyRef = 0;            // Polygon-Referenz
    float gCost = 0.0f;              // Kosten vom Start
    float hCost = 0.0f;              // Heuristik zum Ziel
    uint16_t parent = 0;             // Parent-Polygon

    [[nodiscard]] float fCost() const { return gCost + hCost; }

    bool operator>(const PathNode& other) const {
        return fCost() > other.fCost();
    }
};

// =============================================================================
// PATH RESULT
// =============================================================================
struct PathResult {
    std::vector<math::Vector3> waypoints;
    float totalLength = 0.0f;
    int polyCount = 0;
    bool partial = false;            // True wenn Ziel nicht erreichbar

    [[nodiscard]] bool IsValid() const { return !waypoints.empty(); }
    [[nodiscard]] math::Vector3 GetEnd() const { 
        return waypoints.empty() ? math::Vector3{} : waypoints.back(); 
    }
};

// =============================================================================
// PATHFINDER
// =============================================================================
class Pathfinder {
    const NavMesh* navMesh = nullptr;

public:
    explicit Pathfinder(const NavMesh* mesh) : navMesh(mesh) {}

    // A* Pathfinding zwischen zwei Welt-Positionen
    [[nodiscard]] std::expected<PathResult, std::string> FindPath(
        const math::Vector3& start,
        const math::Vector3& end,
        float agentRadius = 0.4f) const;

    // Detour-Style Straight-Path (Funnel Algorithm)
    [[nodiscard]] std::expected<PathResult, std::string> FindStraightPath(
        const math::Vector3& start,
        const math::Vector3& end) const;

    // Raycast auf NavMesh
    [[nodiscard]] bool Raycast(const math::Vector3& start, 
                                const math::Vector3& end,
                                math::Vector3* hitPoint = nullptr) const;

    // Zufälliger Punkt auf NavMesh
    [[nodiscard]] std::expected<math::Vector3, std::string> FindRandomPoint(
        const math::Vector3& center, float radius) const;

private:
    // A* auf Polygon-Ebene
    [[nodiscard]] std::expected<std::vector<uint16_t>, std::string> AStarSearch(
        uint16_t startPoly, uint16_t endPoly) const;

    // Heuristik (Euklidisch)
    [[nodiscard]] float Heuristic(uint16_t polyA, uint16_t polyB) const;

    // Kante zwischen zwei Polygonen finden
    [[nodiscard]] bool GetPortalPoints(uint16_t fromPoly, uint16_t toPoly,
                                        math::Vector3& left, math::Vector3& right) const;

    // Funnel Algorithm für geraden Pfad
    [[nodiscard]] std::vector<math::Vector3> StringPull(
        const math::Vector3& start,
        const math::Vector3& end,
        std::span<const uint16_t> polyPath) const;
};

// =============================================================================
// CROWD AGENT
// =============================================================================
struct CrowdAgent {
    uint32_t id = 0;
    math::Vector3 position;
    math::Vector3 velocity;
    math::Vector3 target;

    float radius = 0.4f;
    float height = 2.0f;
    float maxSpeed = 5.0f;
    float maxAcceleration = 20.0f;

    std::vector<math::Vector3> path;
    size_t pathIndex = 0;

    bool active = true;
    bool reachedTarget = false;

    // Steering
    math::Vector3 steering;
    std::chrono::steady_clock::time_point lastUpdate;
};

// =============================================================================
// CROWD SIMULATION (Detour-Style)
// =============================================================================
class CrowdSimulation {
    std::vector<std::unique_ptr<CrowdAgent>> agents;
    const Pathfinder* pathfinder = nullptr;

    // Grid-basiertes Spatial-Hashing für Nachbarschafts-Queries
    static constexpr float GRID_CELL_SIZE = 5.0f;
    std::unordered_map<uint64_t, std::vector<uint32_t>> spatialGrid;

public:
    explicit CrowdSimulation(const Pathfinder* pf) : pathfinder(pf) {}

    // Agent hinzufügen
    [[nodiscard]] uint32_t AddAgent(const math::Vector3& pos, 
                                     float radius = 0.4f,
                                     float maxSpeed = 5.0f);

    void RemoveAgent(uint32_t id);

    // Ziel setzen → Pfad wird berechnet
    void SetAgentTarget(uint32_t id, const math::Vector3& target);

    // Simulation-Step (60Hz)
    void Update(float deltaTime);

    [[nodiscard]] CrowdAgent* GetAgent(uint32_t id);
    [[nodiscard]] const CrowdAgent* GetAgent(uint32_t id) const;

    [[nodiscard]] size_t GetAgentCount() const { return agents.size(); }

    void Clear();

private:
    void UpdateSpatialGrid();
    [[nodiscard]] std::vector<uint32_t> GetNeighbors(uint32_t agentId, float radius);

    // Steering Behaviors
    [[nodiscard]] math::Vector3 CalculateSteering(CrowdAgent& agent, float dt);
    [[nodiscard]] math::Vector3 Seek(const CrowdAgent& agent, const math::Vector3& target);
    [[nodiscard]] math::Vector3 Separation(const CrowdAgent& agent, 
                                            std::span<const uint32_t> neighbors);
    [[nodiscard]] math::Vector3 CollisionAvoidance(const CrowdAgent& agent,
                                                     std::span<const uint32_t> neighbors);

    // Grid-Key
    [[nodiscard]] static uint64_t GetGridKey(float x, float z) {
        int gx = static_cast<int>(std::floor(x / GRID_CELL_SIZE));
        int gz = static_cast<int>(std::floor(z / GRID_CELL_SIZE));
        return (static_cast<uint64_t>(gx) << 32) | static_cast<uint32_t>(gz);
    }
};

} // namespace ai
