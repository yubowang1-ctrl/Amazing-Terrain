#pragma once

#include <vector>
#include "glm/glm.hpp"

class TerrainGenerator
{
public:
    bool m_wireshade;

    TerrainGenerator();
    ~TerrainGenerator();

    int getResolution() { return m_resolution; }
    std::vector<float> generateTerrain();

    struct TerrainParams {
        // base fBm
        int   octaves      = 4;
        float baseFreq     = 1.0f;
        float lacunarity   = 2.0f;
        float gain         = 0.5f;
        float heightScale  = 1.0f;

        // domain warping
        float warpStrength = 0.0f;   // 0..1

        // cliffs (terraces)
        int   cliffSteps   = 1;      // 1 = off
        float cliffSmooth  = 0.15f;  // 0..0.5

        // rivers (ridged noise valleys)
        bool  enableRivers = false;
        float riverFreq    = 0.8f;
        float riverSharp   = 1.5f;
        float riverThresh  = 0.85f;  // 0..1
        float riverDepth   = 0.20f;  // subtract from height

        // ===== New England style valley / water =====
        // seaLevel is still used as water surface height
        // in the h space of getHeight
        float seaLevel      = -0.15f; // height of rivers/lakes (unscaled h-space)
        float oceanBias     = 0.0f;   // reserved item = 0 (no longer using)

        // large-scale canyon: a large valley along the Y-axis.
        float valleyWidth   = 0.18f;  // larger -> wider
        float valleyDepth   = 0.8f;   // larger -> deeper
        float valleyMeander = 0.12f;  // canyon's horizontal "serpentine" shape: larger -> more curvy

        // size/depth of the lake at the bottom of the valley
        float lakeRadius    = 0.05f;
        float lakeDepth     = 0.6f;   // how much lower is the lake than the surrounding valley floor?

        // craters
        bool  enableCraters = false;
        float craterDensity = 6.0f;  // cells per 1x1
        float craterRadius  = 0.06f; // normalized radius
        float craterDepth   = 0.25f;

        int   seed = 1230;
    };

    void setParams(const TerrainParams& p);

    float sampleHeight01(float x, float y) const;

    glm::vec3 sampleSurfacePos(float x, float y) const;

    // Perlin noise
    float computePerlin(float x, float y);

private:
    std::vector<glm::vec2> m_randVecLookup;
    int m_resolution;
    int m_lookupSize;

    TerrainParams m_params;

    glm::vec2 sampleRandomVector(int row, int col);
    glm::vec3 getPosition(int row, int col);
    float     getHeight(float x, float y);
    glm::vec3 getNormal(int row, int col);
    glm::vec3 getColor(glm::vec3 normal, glm::vec3 position);
};
