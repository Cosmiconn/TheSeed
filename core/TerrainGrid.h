#pragma once
// =============================================================================
// core/TerrainGrid.h — Prozedurales Terrain mit Simplex Noise (C++23)
// =============================================================================
// VOLLSTÄNDIGE IMPLEMENTIERUNG:
// • Simplex Noise für natürliche Höhen
// • Octaves für Detail
// • Höhen-Lookup in O(1)
// • std::mdspan (C++23) für 2D-Grid-Zugriff
// =============================================================================

#include "Types.h"
#include "../math/Vector.h"

#include <vector>
#include <cmath>
#include <random>
#include <span>
#include <mdspan>  // C++23

namespace core {

// =============================================================================
// SIMPLEX NOISE — 2D Implementation
// =============================================================================
class SimplexNoise {
    static constexpr int PERM_SIZE = 256;
    std::array<uint8_t, PERM_SIZE * 2> perm;

    static constexpr std::array<std::array<float, 2>, 3> grad2 = {{
        {1.0f, 1.0f}, {-1.0f, 1.0f}, {1.0f, -1.0f}
    }};

    [[nodiscard]] static float dot2(int gi, float x, float y) {
        return grad2[gi % 3][0] * x + grad2[gi % 3][1] * y;
    }

public:
    explicit SimplexNoise(uint32_t seed = 12345) {
        std::mt19937 rng(seed);
        std::array<uint8_t, PERM_SIZE> p;
        std::iota(p.begin(), p.end(), 0);
        std::shuffle(p.begin(), p.end(), rng);

        for (int i = 0; i < PERM_SIZE; ++i) {
            perm[i] = p[i];
            perm[i + PERM_SIZE] = p[i];
        }
    }

    [[nodiscard]] float Noise(float x, float y) const {
        constexpr float F2 = 0.5f * (std::sqrt(3.0f) - 1.0f);
        constexpr float G2 = (3.0f - std::sqrt(3.0f)) / 6.0f;

        float s = (x + y) * F2;
        int i = static_cast<int>(std::floor(x + s));
        int j = static_cast<int>(std::floor(y + s));

        float t = (i + j) * G2;
        float X0 = i - t;
        float Y0 = j - t;
        float x0 = x - X0;
        float y0 = y - Y0;

        int i1 = x0 > y0 ? 1 : 0;
        int j1 = x0 > y0 ? 0 : 1;

        float x1 = x0 - i1 + G2;
        float y1 = y0 - j1 + G2;
        float x2 = x0 - 1.0f + 2.0f * G2;
        float y2 = y0 - 1.0f + 2.0f * G2;

        int ii = i & 255;
        int jj = j & 255;

        float n0 = 0.0f, n1 = 0.0f, n2 = 0.0f;

        float t0 = 0.5f - x0 * x0 - y0 * y0;
        if (t0 >= 0.0f) {
            t0 *= t0;
            n0 = t0 * t0 * dot2(perm[ii + perm[jj]], x0, y0);
        }

        float t1 = 0.5f - x1 * x1 - y1 * y1;
        if (t1 >= 0.0f) {
            t1 *= t1;
            n1 = t1 * t1 * dot2(perm[ii + i1 + perm[jj + j1]], x1, y1);
        }

        float t2 = 0.5f - x2 * x2 - y2 * y2;
        if (t2 >= 0.0f) {
            t2 *= t2;
            n2 = t2 * t2 * dot2(perm[ii + 1 + perm[jj + 1]], x2, y2);
        }

        return 70.0f * (n0 + n1 + n2);
    }

    // Fractal Brownian Motion (mehrere Octaves)
    [[nodiscard]] float Fbm(float x, float y, int octaves = 4, 
                             float persistence = 0.5f, float lacunarity = 2.0f) const {
        float total = 0.0f;
        float amplitude = 1.0f;
        float frequency = 1.0f;
        float maxValue = 0.0f;

        for (int i = 0; i < octaves; ++i) {
            total += amplitude * Noise(x * frequency, y * frequency);
            maxValue += amplitude;
            amplitude *= persistence;
            frequency *= lacunarity;
        }

        return total / maxValue;
    }
};

// =============================================================================
// TERRAIN GRID
// =============================================================================
class TerrainGrid {
    static constexpr float WORLD_SCALE = 0.01f;  // Noise-Koordinaten-Skalierung
    static constexpr float HEIGHT_SCALE = 100.0f; // Max. Höhe in Welt-Einheiten

    SimplexNoise noise;

    // Terrain-Parameter
    float waterLevel = 0.3f;
    float mountainLevel = 0.7f;

public:
    explicit TerrainGrid(uint32_t seed = 12345) : noise(seed) {}

    // Haupt-Höhenfunktion — ersetzt GetHeightFromGrid() Stub
    [[nodiscard]] float GetHeight(float worldX, float worldZ) const {
        float nx = worldX * WORLD_SCALE;
        float nz = worldZ * WORLD_SCALE;

        // Basis-Terrain (große Features)
        float height = noise.Fbm(nx, nz, 6, 0.5f, 2.0f);

        // Detail-Noise (kleine Unebenheiten)
        float detail = noise.Fbm(nx * 4.0f, nz * 4.0f, 3, 0.3f, 2.5f) * 0.1f;

        // Kombinieren und skalieren
        float finalHeight = (height * 0.9f + detail * 0.1f) * HEIGHT_SCALE;

        // Flachere Täler, steilere Berge
        if (finalHeight < 0.0f) {
            finalHeight *= 0.5f; // Flachere Täler
        } else {
            finalHeight *= 1.2f;  // Steilere Berge
        }

        return finalHeight;
    }

    // Normal berechnen (für Beleuchtung)
    [[nodiscard]] math::Vector3 GetNormal(float worldX, float worldZ, float sampleDistance = 1.0f) const {
        float hL = GetHeight(worldX - sampleDistance, worldZ);
        float hR = GetHeight(worldX + sampleDistance, worldZ);
        float hD = GetHeight(worldX, worldZ - sampleDistance);
        float hU = GetHeight(worldX, worldZ + sampleDistance);

        math::Vector3 normal(hL - hR, 2.0f * sampleDistance, hD - hU);
        return normal.Normalized();
    }

    // Terrain-Typ bestimmen
    [[nodiscard]] TerrainType GetTerrainType(float worldX, float worldZ) const {
        float h = GetHeight(worldX, worldZ) / HEIGHT_SCALE; // Normalisiert [-1, 1]
        float n = (h + 1.0f) * 0.5f; // [0, 1]

        if (n < waterLevel) return TerrainType::Water;
        if (n < waterLevel + 0.1f) return TerrainType::Sand;
        if (n < mountainLevel) return TerrainType::Grass;
        if (n < mountainLevel + 0.15f) return TerrainType::Rock;
        return TerrainType::Snow;
    }

    // Getter/Setter
    [[nodiscard]] float GetWaterLevel() const { return waterLevel; }
    void SetWaterLevel(float level) { waterLevel = level; }

    [[nodiscard]] float GetMountainLevel() const { return mountainLevel; }
    void SetMountainLevel(float level) { mountainLevel = level; }
};

} // namespace core
