#include "terraingenerator.h"

#include <cmath>
#include <cstdlib>
#include "glm/glm.hpp"

// helpers: fbm & terrace
inline float fbm(TerrainGenerator *self,
                 glm::vec2 p,
                 int oct,
                 float baseFreq,
                 float lac,
                 float gain)
{
    float f = baseFreq;
    float a = 1.f;
    float h = 0.f;
    for (int i = 0; i < oct; i++) {
        h += a * self->computePerlin(p.x * f, p.y * f);
        f *= lac;
        a *= gain;
    }
    return h;
}

inline float terrace01(float h01, int steps, float smooth) {
    if (steps <= 1) return h01;
    float x = h01 * steps;
    float i = floorf(x), f = x - i;
    float ramp = glm::smoothstep(0.5f - smooth, 0.5f + smooth, f);
    return (i + ramp) / steps;
}

//  public: params
void TerrainGenerator::setParams(const TerrainParams &p) {
    m_params = p;
}

// ctor / dtor
TerrainGenerator::TerrainGenerator()
{
    m_wireshade = false;

    m_resolution = 256;

    m_lookupSize = 1024;
    m_randVecLookup.reserve(m_lookupSize);

    std::srand(1230);
    for (int i = 0; i < m_lookupSize; i++) {
        m_randVecLookup.push_back(glm::vec2(std::rand() * 2.0 / RAND_MAX - 1.0,
                                            std::rand() * 2.0 / RAND_MAX - 1.0));
    }
}

TerrainGenerator::~TerrainGenerator()
{
    m_randVecLookup.clear();
}

// helper for generateTerrain
static void addPointToVector(glm::vec3 p, std::vector<float> &v) {
    v.push_back(p.x); v.push_back(p.y); v.push_back(p.z);
}

// function for texture wrappping
static void addVertex(const glm::vec3 &pos,
                      const glm::vec3 &nor,
                      const glm::vec2 &uv,
                      std::vector<float> &v)
{
    // position
    v.push_back(pos.x); v.push_back(pos.y); v.push_back(pos.z);
    // normal
    v.push_back(nor.x); v.push_back(nor.y); v.push_back(nor.z);
    // "color" slot: uv.xy + placeholder 0.
    v.push_back(uv.x);  v.push_back(uv.y);  v.push_back(0.f);
}

// ===== mesh generation =============================================

std::vector<float> TerrainGenerator::generateTerrain()
{
    std::vector<float> verts;
    verts.reserve(m_resolution * m_resolution * 6 * 9); // 6 verts * 9 floats

    const float uvScale = 30.0f; // Adjustible: number of times the texture tiled.

    for (int x = 0; x < m_resolution; x++) {
        for (int y = 0; y < m_resolution; y++) {
            int x1 = x;
            int y1 = y;
            int x2 = x + 1;
            int y2 = y + 1;

            glm::vec3 p1 = getPosition(x1, y1);
            glm::vec3 p2 = getPosition(x2, y1);
            glm::vec3 p3 = getPosition(x2, y2);
            glm::vec3 p4 = getPosition(x1, y2);

            glm::vec3 n1 = getNormal(x1, y1);
            glm::vec3 n2 = getNormal(x2, y1);
            glm::vec3 n3 = getNormal(x2, y2);
            glm::vec3 n4 = getNormal(x1, y2);

            // apply uniform UV light over [0,1], then scale up the uvScale and repeat.
            glm::vec2 uv1 = glm::vec2(float(x1) / m_resolution,
                                      float(y1) / m_resolution) * uvScale;
            glm::vec2 uv2 = glm::vec2(float(x2) / m_resolution,
                                      float(y1) / m_resolution) * uvScale;
            glm::vec2 uv3 = glm::vec2(float(x2) / m_resolution,
                                      float(y2) / m_resolution) * uvScale;
            glm::vec2 uv4 = glm::vec2(float(x1) / m_resolution,
                                      float(y2) / m_resolution) * uvScale;

            // tri 1: p1 p2 p3
            addVertex(p1, n1, uv1, verts);
            addVertex(p2, n2, uv2, verts);
            addVertex(p3, n3, uv3, verts);

            // tri 2: p1 p3 p4
            addVertex(p1, n1, uv1, verts);
            addVertex(p3, n3, uv3, verts);
            addVertex(p4, n4, uv4, verts);
        }
    }
    return verts;
}

// ===== random gradient lookup =====================================

glm::vec2 TerrainGenerator::sampleRandomVector(int row, int col)
{
    std::hash<int> intHash;
    int index = intHash(row * 41 + col * 43) % m_lookupSize;
    return m_randVecLookup.at(index);
}

// ===== position / height / normal / color ==========================

glm::vec3 TerrainGenerator::getPosition(int row, int col)
{
    float x = 1.0f * row / m_resolution;
    float y = 1.0f * col / m_resolution;

    float z = getHeight(x, y);     // world-space height
    // float sea = m_params.seaLevel * m_params.heightScale;

    // // flatten water
    // if (z < sea) z = sea;

    return glm::vec3(x, y, z);
}

static inline float smoothstep3(float t) {
    t = glm::clamp(t, 0.f, 1.f);
    return t * t * (3.f - 2.f * t); // 3t^2 - 2t^3
}

static inline float interp(float A, float B, float alpha) {
    return A + smoothstep3(alpha) * (B - A);
}

float TerrainGenerator::getHeight(float x, float y)
{
    // sample noise on [0,1]^2
    glm::vec2 p(x, y);

    // 1) domain warping
    if (m_params.warpStrength > 0.f) {
        glm::vec2 w;
        w.x = fbm(this, p * 2.0f + glm::vec2(13.2f, 7.1f), 3, 1.0f, 2.0f, 0.5f);
        w.y = fbm(this, p * 2.0f + glm::vec2(-9.7f, 5.4f), 3, 1.0f, 2.0f, 0.5f);
        p  += m_params.warpStrength * w;
    }

    // 2) basic fBm mountain
    float h = fbm(this, p,
                  m_params.octaves,
                  m_params.baseFreq,
                  m_params.lacunarity,
                  m_params.gain);

    // 3) cliff (stairs)
    if (m_params.cliffSteps > 1) {
        float h01 = 0.5f * (h + 1.0f);
        h01 = terrace01(h01, m_params.cliffSteps, m_params.cliffSmooth);
        h   = h01 * 2.0f - 1.0f;
    }

    // 4) rivers: ridged noise for "bottom valley"
    if (m_params.enableRivers) {
        // ridged noise: the closer to 0, the higher the ridge value.
        float r = fbm(this, p * m_params.riverFreq, 4, 1.0f, 2.0f, 0.5f);
        float ridged = powf(1.f - fabsf(r), m_params.riverSharp);

        // width half-width of the river channel;
        const float width = 0.02f;

        float t0 = m_params.riverThresh + width;   // upper threshold: begins to turn into a river
        float t1 = m_params.riverThresh;           // lower threshold: River center
        float mask = glm::smoothstep(t0, t1, ridged);

        h -= m_params.riverDepth * mask;          // digging valley
    }

    // 5) craters
    if (m_params.enableCraters && m_params.craterDensity > 0.f) {
        glm::vec2 g = p * m_params.craterDensity;
        glm::ivec2 cell = glm::floor(g);
        float crater = 0.f;

        for (int dj = -1; dj <= 1; ++dj) {
            for (int di = -1; di <= 1; ++di) {
                glm::ivec2 C = cell + glm::ivec2(di, dj);
                glm::vec2 rnd = 0.5f + 0.5f * sampleRandomVector(C.x, C.y);
                glm::vec2 center = (glm::vec2(C) + rnd) / m_params.craterDensity;

                glm::vec2 d = p - center;
                float R = m_params.craterRadius * (0.6f + 0.8f *
                                                              (0.5f + 0.5f * sampleRandomVector(C.x + 73, C.y - 41).x));
                float dist = glm::length(d);
                float fall = glm::smoothstep(R, 0.0f, dist);
                float bowl = fall * (1.0f - dist / (R + 1e-6f));
                crater = std::max(crater, bowl);
            }
        }
        h -= m_params.craterDepth * crater;
    }

    // 6) shift the entire area down slightly, making some areas below seaLevel appear as water.
    h -= m_params.oceanBias;

    // 7) rescale to world height
    return h * m_params.heightScale;
}

// returns a height approximately in the [0,1] range, used for logic such as planting trees/sea level.
float TerrainGenerator::sampleHeight01(float x, float y) const {
    // note: getHeight is multiplied by heightScale
    float z = const_cast<TerrainGenerator*>(this)->getHeight(x, y);

    // raises the area below sea level to seaLevel.
    float sea = m_params.seaLevel * m_params.heightScale;
    if (z < sea) z = sea;

    return z;
}

// return a surface point on the local (0..1)^2, z will be clamped by the sea level.
glm::vec3 TerrainGenerator::sampleSurfacePos(float x, float y) const {
    float h = const_cast<TerrainGenerator*>(this)->getHeight(x, y);

    float sea = m_params.seaLevel * m_params.heightScale;
    if (h < sea) h = sea;

    return glm::vec3(x, y, h);
}

// normal from neighbor ring
glm::vec3 TerrainGenerator::getNormal(int row, int col)
{
    glm::vec3 normal(0.f);

    static const int OFF[8][2] = {
        {-1,-1}, {0,-1}, {1,-1}, {1, 0},
        { 1, 1}, {0, 1}, {-1, 1}, {-1, 0}
    };

    const glm::vec3 V = getPosition(row, col);

    for (int i = 0; i < 8; ++i) {
        int r1 = row + OFF[i][0];
        int c1 = col + OFF[i][1];
        int r2 = row + OFF[(i+1)%8][0];
        int c2 = col + OFF[(i+1)%8][1];

        glm::vec3 p1 = getPosition(r1, c1);
        glm::vec3 p2 = getPosition(r2, c2);

        normal += glm::cross(p1 - V, p2 - V);
    }

    if (glm::length(normal) < 1e-12f) return glm::vec3(0,0,1);
    normal = glm::normalize(normal);
    if (normal.z < 0.f) normal = -normal;
    return normal;
}

static inline glm::vec3 gray(float g) { return glm::vec3(g, g, g); }

glm::vec3 TerrainGenerator::getColor(glm::vec3 normal, glm::vec3 position)
{
    float sea = m_params.seaLevel * m_params.heightScale;

    // water surface: blue, slightly brighter when closer to the mountain.
    if (position.z <= sea + 1e-4f) {
        glm::vec2 uv(position.x, position.y);
        // use the distance from the valley center to create variations in depth.
        float cx = 0.5f;
        float dx = fabsf(uv.x - cx);
        float t  = glm::clamp(dx / 0.25f, 0.f, 1.f);

        glm::vec3 deepWater    = glm::vec3(0.02f, 0.10f, 0.25f);
        glm::vec3 shallowWater = glm::vec3(0.10f, 0.35f, 0.55f);
        return (1.f - t) * shallowWater + t * deepWater;
    }

    // 2) land: New England style biomes (mainly green)
    normal = glm::normalize(normal);
    float h = position.z;

    glm::vec3 grassLow  = glm::vec3(0.23f, 0.48f, 0.24f); // valley floor darker green
    glm::vec3 grassHigh = glm::vec3(0.33f, 0.60f, 0.30f); // hillside bright green
    glm::vec3 rock      = glm::vec3(0.45f, 0.45f, 0.45f);

    // Adjustable: map the height to 0..1.
    float seaWorld = sea;
    float h01 = glm::clamp((h - seaWorld) / (m_params.heightScale * 2.0f), 0.f, 1.f);
    float slope = glm::clamp(1.f - normal.z, 0.f, 1.f);// steeper the slope, closer to 1

    // low area: grass + river (lake)
    glm::vec3 col = glm::mix(grassLow, grassHigh, h01);

    // steep slope / high altitude mixed with rocks
    float rockMask = glm::smoothstep(0.3f, 0.8f, glm::max(h01, slope));
    col = glm::mix(col, rock, rockMask);

    return col;
}

// ===== Perlin =====================================================

float TerrainGenerator::computePerlin(float x, float y)
{
    int x0 = static_cast<int>(floorf(x));
    int y0 = static_cast<int>(floorf(y));
    int x1 = x0 + 1;
    int y1 = y0 + 1;

    glm::vec2 dTL(x - x0, y - y1);
    glm::vec2 dTR(x - x1, y - y1);
    glm::vec2 dBR(x - x1, y - y0);
    glm::vec2 dBL(x - x0, y - y0);

    glm::vec2 gTL = sampleRandomVector(x0, y1);
    glm::vec2 gTR = sampleRandomVector(x1, y1);
    glm::vec2 gBR = sampleRandomVector(x1, y0);
    glm::vec2 gBL = sampleRandomVector(x0, y0);

    float A = glm::dot(gTL, dTL);
    float B = glm::dot(gTR, dTR);
    float C = glm::dot(gBR, dBR);
    float D = glm::dot(gBL, dBL);

    float u = x - static_cast<float>(x0);
    float v = y - static_cast<float>(y0);

    float bottom = interp(D, C, u);
    float top    = interp(A, B, u);
    return interp(bottom, top, v);
}
