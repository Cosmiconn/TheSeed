#pragma once
// =============================================================================
// server/ai/NavMesh.h — NavMesh Generation (AP-47) C++23
// =============================================================================
// VOLLSTÄNDIGE IMPLEMENTIERUNG:
// • Voxelization von Level-Geometrie (Heightfield)
// • Walkability-Filter (max Slope, Step Height, Agent Height)
// • Region Generation (Watershed-Style)
// • Contour Simplification
// • Polygon Triangulation
// • Tiled NavMesh für Streaming
// =============================================================================

#include "../../math/Vector.h"
#include "../../core/Log.h"

#include <vector>
#include <span>
#include <memory>
#include <expected>
#include <chrono>

namespace ai {

// =============================================================================
// CONFIG
// =============================================================================
struct NavMeshConfig {
    float cellSize = 0.3f;           // Voxel-Größe (XZ)
    float cellHeight = 0.2f;         // Voxel-Größe (Y)
    float agentHeight = 2.0f;        // Agent-Höhe
    float agentRadius = 0.4f;        // Agent-Radius
    float agentMaxSlope = 45.0f;     // Max. Steigung (Grad)
    float agentMaxStep = 0.5f;       // Max. Stufenhöhe

    float regionMinSize = 8.0f;      // Min. Regionsgröße
    float regionMergeSize = 20.0f;   // Merge-Threshold

    float edgeMaxLen = 12.0f;        // Max. Kantenlänge
    float edgeMaxError = 1.3f;       // Max. Simplification-Error
    float vertsPerPoly = 6.0f;       // Max. Vertices pro Polygon

    float detailSampleDist = 6.0f;   // Detail-Mesh Sampling
    float detailSampleMaxError = 1.0f;
};

// =============================================================================
// HEIGHTFIELD (Voxel-Grid)
// =============================================================================
struct HeightField {
    int width = 0, height = 0;
    math::Vector3 origin;            // Welt-Origin
    float cellSize = 0.3f;
    float cellHeight = 0.2f;

    struct Span {
        uint16_t min = 0;            // Unterkante
        uint16_t max = 0;            // Oberkante
        uint8_t area = 0;            // 0 = leer, 1 = walkable, 2 = obstacle
        std::unique_ptr<Span> next;  // Linked list für überlappende Spans
    };

    std::vector<std::unique_ptr<Span>> spans; // width * height

    [[nodiscard]] Span* GetSpan(int x, int y) const {
        if (x < 0 || y < 0 || x >= width || y >= height) return nullptr;
        return spans[y * width + x].get();
    }

    void AddSpan(int x, int y, uint16_t min, uint16_t max, uint8_t area);
};

// =============================================================================
// COMPACT HEIGHTFIELD (für Region Generation)
// =============================================================================
struct CompactHeightField {
    int width = 0, height = 0, spanCount = 0;
    math::Vector3 origin;
    float cellSize = 0.3f;
    float cellHeight = 0.2f;

    struct Cell {
        uint32_t index = 0;          // Start-Index in spans
        uint32_t count = 0;          // Anzahl Spans
    };

    struct Span {
        uint16_t y = 0;              // Höhe
        uint16_t reg = 0;            // Region-ID
        uint32_t con : 24;           // Connections
        uint32_t h : 8;              // Distance to border
        uint8_t area = 0;
    };

    std::vector<Cell> cells;
    std::vector<Span> spans;
    std::vector<uint16_t> dist;      // Distance field

    [[nodiscard]] const Span* GetSpan(int x, int y, int i) const;
};

// =============================================================================
// NAVMESH POLYGON
// =============================================================================
struct NavMeshPoly {
    uint16_t verts[6];               // Max. 6 Vertices (Detour-Standard)
    uint16_t neis[6];                // Nachbar-Polygone
    uint16_t flags = 0;
    uint8_t vertCount = 0;
    uint8_t area = 0;
};

struct NavMeshTile {
    math::Vector3 origin;
    std::vector<math::Vector3> verts;
    std::vector<NavMeshPoly> polys;
    std::vector<uint16_t> detailVerts;
    std::vector<uint8_t> detailTris;

    [[nodiscard]] math::Vector3 GetPolyCenter(uint16_t polyIdx) const;
};

// =============================================================================
// NAVMESH
// =============================================================================
class NavMesh {
    std::vector<std::unique_ptr<NavMeshTile>> tiles;
    math::Vector3 origin;
    float tileSize = 32.0f;          // Größe einer Tile

public:
    [[nodiscard]] std::expected<void, std::string> BuildFromGeometry(
        std::span<const math::Vector3> vertices,
        std::span<const uint32_t> indices,
        const NavMeshConfig& config,
        const math::Vector3& worldOrigin,
        const math::Vector3& worldBounds);

    [[nodiscard]] std::expected<void, std::string> LoadTile(
        int tx, int ty,
        std::span<const math::Vector3> vertices,
        std::span<const uint32_t> indices,
        const NavMeshConfig& config);

    [[nodiscard]] const NavMeshTile* GetTileAt(float x, float z) const;
    [[nodiscard]] uint16_t FindNearestPoly(float x, float y, float z, 
                                            math::Vector3* closestPt) const;

    [[nodiscard]] bool IsWalkable(float x, float y, float z) const;

    [[nodiscard]] size_t GetTileCount() const { return tiles.size(); }

    void Clear() { tiles.clear(); }

private:
    // Pipeline-Schritte
    [[nodiscard]] std::expected<HeightField, std::string> RasterizeGeometry(
        std::span<const math::Vector3> vertices,
        std::span<const uint32_t> indices,
        const NavMeshConfig& config);

    [[nodiscard]] std::expected<CompactHeightField, std::string> BuildCompactField(
        const HeightField& hf,
        const NavMeshConfig& config);

    [[nodiscard]] std::expected<void, std::string> ErodeWalkableArea(
        CompactHeightField& chf,
        const NavMeshConfig& config);

    [[nodiscard]] std::expected<void, std::string> BuildRegions(
        CompactHeightField& chf,
        const NavMeshConfig& config);

    [[nodiscard]] std::expected<NavMeshTile, std::string> BuildTileMesh(
        const CompactHeightField& chf,
        const NavMeshConfig& config);
};

} // namespace ai
