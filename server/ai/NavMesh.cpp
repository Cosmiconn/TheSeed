// =============================================================================
// server/ai/NavMesh.cpp — NavMesh Generation (AP-47) C++23
// =============================================================================

#include "NavMesh.h"

#include <algorithm>
#include <numeric>
#include <queue>
#include <stack>

namespace ai {

// =============================================================================
// HEIGHTFIELD
// =============================================================================

void HeightField::AddSpan(int x, int y, uint16_t min, uint16_t max, uint8_t area) {
    if (x < 0 || y < 0 || x >= width || y >= height) return;

    size_t idx = y * width + x;
    auto* span = spans[idx].get();

    if (!span) {
        spans[idx] = std::make_unique<Span>();
        span = spans[idx].get();
        span->min = min;
        span->max = max;
        span->area = area;
        return;
    }

    // Merge oder Einfügen
    Span* prev = nullptr;
    while (span) {
        if (max < span->min) {
            // Vor diesem Span einfügen
            auto* newSpan = new Span{min, max, area, nullptr};
            if (prev) {
                newSpan->next = std::unique_ptr<Span>(prev->next.release());
                prev->next.reset(newSpan);
            } else {
                newSpan->next = std::move(spans[idx]);
                spans[idx].reset(newSpan);
            }
            return;
        }

        if (min <= span->max + 1 && max >= span->min - 1) {
            // Überlappen → mergen
            span->min = std::min(span->min, min);
            span->max = std::max(span->max, max);
            span->area = std::min(span->area, area); // Walkable hat Vorrang
            return;
        }

        prev = span;
        span = span->next.get();
    }

    // Am Ende anhängen
    if (prev) {
        prev->next = std::make_unique<Span>(min, max, area, nullptr);
    }
}

// =============================================================================
// COMPACT HEIGHTFIELD
// =============================================================================

const CompactHeightField::Span* CompactHeightField::GetSpan(int x, int y, int i) const {
    if (x < 0 || y < 0 || x >= width || y >= height) return nullptr;
    const auto& cell = cells[y * width + x];
    if (i < 0 || static_cast<uint32_t>(i) >= cell.count) return nullptr;
    return &spans[cell.index + i];
}

// =============================================================================
// NAVMESH BUILD PIPELINE
// =============================================================================

std::expected<void, std::string> NavMesh::BuildFromGeometry(
    std::span<const math::Vector3> vertices,
    std::span<const uint32_t> indices,
    const NavMeshConfig& config,
    const math::Vector3& worldOrigin,
    const math::Vector3& worldBounds) {

    auto startTime = std::chrono::steady_clock::now();

    // 1. Rasterization
    auto hfResult = RasterizeGeometry(vertices, indices, config);
    if (!hfResult) {
        return std::unexpected("Rasterization failed: " + hfResult.error());
    }

    // 2. Compact Heightfield
    auto chfResult = BuildCompactField(*hfResult, config);
    if (!chfResult) {
        return std::unexpected("Compact field failed: " + chfResult.error());
    }

    // 3. Erode walkable area (Agent-Radius)
    auto erodeResult = ErodeWalkableArea(*chfResult, config);
    if (!erodeResult) {
        return std::unexpected("Erosion failed: " + erodeResult.error());
    }

    // 4. Build regions
    auto regionResult = BuildRegions(*chfResult, config);
    if (!regionResult) {
        return std::unexpected("Region build failed: " + regionResult.error());
    }

    // 5. Build tile mesh
    auto tileResult = BuildTileMesh(*chfResult, config);
    if (!tileResult) {
        return std::unexpected("Tile mesh failed: " + tileResult.error());
    }

    auto* tile = new NavMeshTile(std::move(*tileResult));
    tiles.push_back(std::unique_ptr<NavMeshTile>(tile));

    auto duration = std::chrono::steady_clock::now() - startTime;
    AddLog("[NavMesh] Built in {}ms, {} tiles, {} polygons",
           std::chrono::duration_cast<std::chrono::milliseconds>(duration).count(),
           tiles.size(), tile->polys.size());

    return {};
}

std::expected<HeightField, std::string> NavMesh::RasterizeGeometry(
    std::span<const math::Vector3> vertices,
    std::span<const uint32_t> indices,
    const NavMeshConfig& config) {

    HeightField hf;

    // Bounding Box berechnen
    math::Vector3 bmin(std::numeric_limits<float>::max());
    math::Vector3 bmax(std::numeric_limits<float>::lowest());

    for (const auto& v : vertices) {
        bmin.x = std::min(bmin.x, v.x);
        bmin.y = std::min(bmin.y, v.y);
        bmin.z = std::min(bmin.z, v.z);
        bmax.x = std::max(bmax.x, v.x);
        bmax.y = std::max(bmax.y, v.y);
        bmax.z = std::max(bmax.z, v.z);
    }

    // Heightfield-Dimensionen
    hf.width = static_cast<int>((bmax.x - bmin.x) / config.cellSize) + 1;
    hf.height = static_cast<int>((bmax.z - bmin.z) / config.cellSize) + 1;
    hf.origin = bmin;
    hf.cellSize = config.cellSize;
    hf.cellHeight = config.cellHeight;
    hf.spans.resize(hf.width * hf.height);

    // Triangles rasterisieren
    for (size_t i = 0; i < indices.size(); i += 3) {
        if (i + 2 >= indices.size()) break;

        const auto& v0 = vertices[indices[i]];
        const auto& v1 = vertices[indices[i + 1]];
        const auto& v2 = vertices[indices[i + 2]];

        // Triangle-Bounds in Grid-Koordinaten
        float triMinX = std::min({v0.x, v1.x, v2.x});
        float triMaxX = std::max({v0.x, v1.x, v2.x});
        float triMinZ = std::min({v0.z, v1.z, v2.z});
        float triMaxZ = std::max({v0.z, v1.z, v2.z});

        int x0 = static_cast<int>((triMinX - bmin.x) / config.cellSize);
        int x1 = static_cast<int>((triMaxX - bmin.x) / config.cellSize);
        int z0 = static_cast<int>((triMinZ - bmin.z) / config.cellSize);
        int z1 = static_cast<int>((triMaxZ - bmin.z) / config.cellSize);

        x0 = std::max(0, x0); x1 = std::min(hf.width - 1, x1);
        z0 = std::max(0, z0); z1 = std::min(hf.height - 1, z1);

        // Für jede Zelle im Bounding Box
        for (int z = z0; z <= z1; ++z) {
            for (int x = x0; x <= x1; ++x) {
                // Zellen-Bounds
                float cellMinX = bmin.x + x * config.cellSize;
                float cellMaxX = cellMinX + config.cellSize;
                float cellMinZ = bmin.z + z * config.cellSize;
                float cellMaxZ = cellMinZ + config.cellSize;

                // Einfache AABB-Triangle-Overlap (konservativ)
                // In Produktion: exakte Triangle-AABB-Intersection
                if (triMinX <= cellMaxX && triMaxX >= cellMinX &&
                    triMinZ <= cellMaxZ && triMaxZ >= cellMinZ) {

                    // Höhe berechnen (Durchschnitt der Triangle-Vertices)
                    float h = (v0.y + v1.y + v2.y) / 3.0f;
                    uint16_t hSpan = static_cast<uint16_t>((h - bmin.y) / config.cellHeight);

                    // Walkability-Check: Steigung
                    math::Vector3 edge1 = v1 - v0;
                    math::Vector3 edge2 = v2 - v0;
                    math::Vector3 normal = edge1.Cross(edge2).Normalized();
                    float slope = std::acos(normal.y) * 180.0f / 3.14159265f;
                    uint8_t area = (slope <= config.agentMaxSlope) ? 1 : 2; // 1=walkable, 2=obstacle

                    hf.AddSpan(x, z, hSpan, hSpan + 1, area);
                }
            }
        }
    }

    return hf;
}

std::expected<CompactHeightField, std::string> NavMesh::BuildCompactField(
    const HeightField& hf,
    const NavMeshConfig& config) {

    CompactHeightField chf;
    chf.width = hf.width;
    chf.height = hf.height;
    chf.origin = hf.origin;
    chf.cellSize = hf.cellSize;
    chf.cellHeight = hf.cellHeight;

    chf.cells.resize(hf.width * hf.height);

    // Zähle Spans
    int totalSpans = 0;
    for (const auto& spanPtr : hf.spans) {
        for (const Span* s = spanPtr.get(); s; s = s->next.get()) {
            totalSpans++;
        }
    }

    chf.spans.reserve(totalSpans);

    // Fülle Cells und Spans
    for (int y = 0; y < hf.height; ++y) {
        for (int x = 0; x < hf.width; ++x) {
            size_t cellIdx = y * hf.width + x;
            chf.cells[cellIdx].index = static_cast<uint32_t>(chf.spans.size());

            for (const Span* s = hf.spans[cellIdx].get(); s; s = s->next.get()) {
                CompactHeightField::Span cs;
                cs.y = s->max;
                cs.area = s->area;
                chf.spans.push_back(cs);
                chf.cells[cellIdx].count++;
            }
        }
    }

    chf.spanCount = static_cast<int>(chf.spans.size());
    return chf;
}

std::expected<void, std::string> NavMesh::ErodeWalkableArea(
    CompactHeightField& chf,
    const NavMeshConfig& config) {

    int radius = static_cast<int>(std::ceil(config.agentRadius / config.cellSize));
    if (radius <= 0) return {};

    // Einfache Erosion: Markiere Spans am Rand als nicht-walkable
    for (int y = 0; y < chf.height; ++y) {
        for (int x = 0; x < chf.width; ++x) {
            size_t cellIdx = y * chf.width + x;
            auto& cell = chf.cells[cellIdx];

            for (uint32_t i = 0; i < cell.count; ++i) {
                auto& span = chf.spans[cell.index + i];
                if (span.area != 1) continue; // Nur walkable

                // Prüfe Nachbarn
                bool isBorder = false;
                for (int dy = -radius; dy <= radius && !isBorder; ++dy) {
                    for (int dx = -radius; dx <= radius && !isBorder; ++dx) {
                        if (dx == 0 && dy == 0) continue;

                        int nx = x + dx, ny = y + dy;
                        if (nx < 0 || ny < 0 || nx >= chf.width || ny >= chf.height) {
                            isBorder = true;
                            break;
                        }

                        size_t nIdx = ny * chf.width + nx;
                        if (chf.cells[nIdx].count == 0) {
                            isBorder = true;
                            break;
                        }
                    }
                }

                if (isBorder) {
                    span.area = 2; // Zu nah an Hindernis
                }
            }
        }
    }

    return {};
}

std::expected<void, std::string> NavMesh::BuildRegions(
    CompactHeightField& chf,
    const NavMeshConfig& config) {

    // Einfache Region-Labeling: Flood-Fill von walkable Spans
    uint16_t nextReg = 1;

    for (int y = 0; y < chf.height; ++y) {
        for (int x = 0; x < chf.width; ++x) {
            size_t cellIdx = y * chf.width + x;
            auto& cell = chf.cells[cellIdx];

            for (uint32_t i = 0; i < cell.count; ++i) {
                auto& span = chf.spans[cell.index + i];
                if (span.area != 1 || span.reg != 0) continue;

                // Flood-Fill
                std::queue<std::tuple<int, int, uint32_t>> queue;
                queue.push({x, y, i});
                span.reg = nextReg;

                int regionSize = 0;
                while (!queue.empty() && regionSize < 10000) {
                    auto [cx, cy, ci] = queue.front();
                    queue.pop();
                    regionSize++;

                    // 4-Nachbarn
                    const int dx[4] = {-1, 1, 0, 0};
                    const int dy[4] = {0, 0, -1, 1};

                    for (int dir = 0; dir < 4; ++dir) {
                        int nx = cx + dx[dir];
                        int ny = cy + dy[dir];

                        if (nx < 0 || ny < 0 || nx >= chf.width || ny >= chf.height) continue;

                        size_t nIdx = ny * chf.width + nx;
                        auto& nCell = chf.cells[nIdx];

                        for (uint32_t ni = 0; ni < nCell.count; ++ni) {
                            auto& nSpan = chf.spans[nCell.index + ni];
                            if (nSpan.area != 1 || nSpan.reg != 0) continue;

                            // Höhen-Unterschied prüfen (Step Height)
                            auto& cSpan = chf.spans[chf.cells[cy * chf.width + cx].index + ci];
                            if (std::abs(static_cast<int>(nSpan.y) - static_cast<int>(cSpan.y)) 
                                <= static_cast<int>(config.agentMaxStep / config.cellHeight)) {

                                nSpan.reg = nextReg;
                                queue.push({nx, ny, ni});
                            }
                        }
                    }
                }

                if (regionSize >= static_cast<int>(config.regionMinSize)) {
                    nextReg++;
                } else {
                    // Zu klein → als nicht-walkable markieren
                    for (auto& span : chf.spans) {
                        if (span.reg == nextReg) {
                            span.area = 2;
                            span.reg = 0;
                        }
                    }
                }
            }
        }
    }

    AddLog("[NavMesh] Generated {} regions", nextReg - 1);
    return {};
}

std::expected<NavMeshTile, std::string> NavMesh::BuildTileMesh(
    const CompactHeightField& chf,
    const NavMeshConfig& config) {

    NavMeshTile tile;
    tile.origin = chf.origin;

    // Sammle Region-Vertices
    std::unordered_map<uint64_t, uint16_t> vertMap;

    for (int y = 0; y < chf.height; ++y) {
        for (int x = 0; x < chf.width; ++x) {
            size_t cellIdx = y * chf.width + x;
            const auto& cell = chf.cells[cellIdx];

            for (uint32_t i = 0; i < cell.count; ++i) {
                const auto& span = chf.spans[cell.index + i];
                if (span.reg == 0) continue;

                // Zellen-Center als Vertex
                float vx = chf.origin.x + x * chf.cellSize + chf.cellSize * 0.5f;
                float vy = chf.origin.y + span.y * chf.cellHeight;
                float vz = chf.origin.z + y * chf.cellSize + chf.cellSize * 0.5f;

                uint64_t key = (static_cast<uint64_t>(x) << 32) | y;
                if (!vertMap.contains(key)) {
                    vertMap[key] = static_cast<uint16_t>(tile.verts.size());
                    tile.verts.push_back(math::Vector3(vx, vy, vz));
                }
            }
        }
    }

    // Erstelle Polygone aus Regions (vereinfacht)
    // In Produktion: Kontur-Extraktion + Triangulation
    for (const auto& [key, vertIdx] : vertMap) {
        NavMeshPoly poly;
        poly.verts[0] = vertIdx;
        poly.vertCount = 1;
        poly.area = 1;
        tile.polys.push_back(poly);
    }

    return tile;
}

// =============================================================================
// QUERIES
// =============================================================================

const NavMeshTile* NavMesh::GetTileAt(float x, float z) const {
    for (const auto& tile : tiles) {
        // Einfacher Bounds-Check
        float dx = x - tile->origin.x;
        float dz = z - tile->origin.z;
        if (std::abs(dx) < tileSize && std::abs(dz) < tileSize) {
            return tile.get();
        }
    }
    return nullptr;
}

uint16_t NavMesh::FindNearestPoly(float x, float y, float z, 
                                   math::Vector3* closestPt) const {
    const NavMeshTile* tile = GetTileAt(x, z);
    if (!tile || tile->polys.empty()) return 0;

    uint16_t nearest = 0;
    float minDist = std::numeric_limits<float>::max();

    for (uint16_t i = 0; i < tile->polys.size(); ++i) {
        const auto& poly = tile->polys[i];
        if (poly.vertCount == 0) continue;

        const auto& vert = tile->verts[poly.verts[0]];
        float dist = std::sqrt(
            (vert.x - x) * (vert.x - x) +
            (vert.y - y) * (vert.y - y) +
            (vert.z - z) * (vert.z - z)
        );

        if (dist < minDist) {
            minDist = dist;
            nearest = i;
            if (closestPt) *closestPt = vert;
        }
    }

    return nearest;
}

bool NavMesh::IsWalkable(float x, float y, float z) const {
    const NavMeshTile* tile = GetTileAt(x, z);
    if (!tile) return false;

    uint16_t polyIdx = FindNearestPoly(x, y, z, nullptr);
    if (polyIdx >= tile->polys.size()) return false;

    return tile->polys[polyIdx].area == 1;
}

math::Vector3 NavMeshTile::GetPolyCenter(uint16_t polyIdx) const {
    if (polyIdx >= polys.size() || polys[polyIdx].vertCount == 0) {
        return math::Vector3{};
    }

    math::Vector3 center;
    const auto& poly = polys[polyIdx];
    for (int i = 0; i < poly.vertCount; ++i) {
        center = center + verts[poly.verts[i]];
    }
    return center * (1.0f / poly.vertCount);
}

} // namespace ai
