#include "realtime.h"

#include <QCoreApplication>
#include <QMouseEvent>
#include <QKeyEvent>
#include <iostream>
#include "settings.h"

#include "shapes/Cube.h"
#include "shapes/Sphere.h"
#include "shapes/Cone.h"
#include "shapes/Cylinder.h"
#include <algorithm>
#include <cmath>
#include <glm/gtx/norm.hpp>
#include <random>

namespace
{
    constexpr float EPS = 1e-6;

    inline float clamp01(float x) { return x < 0.f ? 0.f : (x > 1.f ? 1.f : x); }

    inline float lerp(float a, float b, float t) { return a + t * (b - a); }

    // Conti. distance-based LOD factor in (0,1], normalized w.r.t. near/far planes (log scale)
    float lodFactorByDistanceLog(float d, float nearP, float farP, float minFactor)
    {
        // Reference window inside the frustum; adapts with the UI sliders.
        const float d0 = std::max(nearP * 2.0f, EPS);     // within 2*near => full detail
        const float d1 = std::max(farP * 0.5f, d0 + EPS); // beyond 0.5*far => min detail

        // log-normalized t in [0,1]
        float t = (std::log(d) - std::log(d0)) / (std::log(d1) - std::log(d0));
        t = clamp01(t);

        // smoothstep(0,1,t) to avoid kinks
        float s = t * t * (3.f - 2.f * t);

        // map to [minFactor, 1]
        return lerp(1.0f, minFactor, s);
    }

    inline float terrainSizeFromSlider(int v)
    {
        return 24.f + 4.f * float(v - 1); // v=10 => 24 + 36 = 60
    }

}

// helper functions

// Map a ScenePrimitive (+ tess params) to an interleaved PN float array
static std::vector<float> buildInterleavedForPrimitive(const ScenePrimitive &prim,
                                                       int p1, int p2)
{
    std::vector<float> data;
    switch (prim.type)
    {
    case PrimitiveType::PRIMITIVE_CUBE:
    {
        Cube s;
        s.updateParams(std::max(1, p1));
        data = s.generateShape();
        break;
    }
    case PrimitiveType::PRIMITIVE_SPHERE:
    {
        Sphere s;
        s.updateParams(std::max(1, p1), std::max(3, p2));
        data = s.generateShape();
        break;
    }
    case PrimitiveType::PRIMITIVE_CYLINDER:
    {
        Cylinder s;
        s.updateParams(std::max(1, p1), std::max(3, p2));
        data = s.generateShape();
        break;
    }
    case PrimitiveType::PRIMITIVE_CONE:
    {
        Cone s;
        s.updateParams(std::max(1, p1), std::max(3, p2));
        data = s.generateShape();
        break;
    }
    default:
        // Unknown primitive; return empty buffer
        break;
    }
    return data;
}

// Get or create a shared GLMesh for (type,p1,p2)
GLMesh *Realtime::getOrCreateMesh(const ScenePrimitive &prim, int p1, int p2)
{
    // construct cache key
    MeshKey key{(int)prim.type, p1, p2};

    // find cache
    auto it = m_meshCache.find(key);
    if (it != m_meshCache.end())
        return &it->second;

    // if cache unhit, construct new mesh
    std::vector<float> interleaved = buildInterleavedForPrimitive(prim, p1, p2);

    // create GLMesh and upload GPU
    GLMesh mesh;
    mesh.uploadinterleavedPN(interleaved);

    // insert cache (use move semantics to avoid copying)
    auto [ins, ok] = m_meshCache.emplace(key, std::move(mesh));

    // returns the pointer to the grid that was just inserted.
    return &ins->second;
}

// Overload
GLMesh *Realtime::getOrCreateMesh(PrimitiveType type, int p1, int p2)
{
    ScenePrimitive tmp{};
    tmp.type = type; // other fields unused
    return getOrCreateMesh(tmp, p1, p2);
}

void Realtime::destroyMeshCache()
{
    for (auto &kv : m_meshCache)
    {
        kv.second.destroy(); // GLMesh::destroy() release GPU resources
    }
    m_meshCache.clear(); // clear map
}

void Realtime::calculateFrustumCorners(glm::vec3 corners[4]) const {
    float aspect = m_cam.aspect;
    float fovY = m_cam.fovyRad;
    float nearDist = m_cam.nearP;

    float nearHeight = 2.0f * nearDist * std::tan(fovY * 0.5f);
    float nearWidth = nearHeight * aspect;

    glm::vec3 forward = glm::normalize(m_cam.look);
    glm::vec3 right = glm::normalize(glm::cross(forward, m_cam.up));
    glm::vec3 up = glm::normalize(glm::cross(right, forward));

    glm::vec3 nearCenter = m_cam.eye + forward * nearDist;
    glm::vec3 halfRight = right * (nearWidth * 0.5f);
    glm::vec3 halfUp = up * (nearHeight * 0.5f);

    corners[0] = (nearCenter - halfRight + halfUp) - m_cam.eye;
    corners[1] = (nearCenter + halfRight + halfUp) - m_cam.eye;
    corners[2] = (nearCenter - halfRight - halfUp) - m_cam.eye;
    corners[3] = (nearCenter + halfRight - halfUp) - m_cam.eye;
}

void Realtime::buildForest() {
    const size_t maxBranches = 800000;
    const size_t maxLeaves = 1600000;

    m_forestBranches.clear();
    m_forestLeaves.clear();

    if (!m_treeCylinderMesh)
        return;

    auto clamp01 = [](float v)
    { return glm::clamp(v, 0.f, 1.f); };

    std::mt19937 rng(1337);
    std::uniform_real_distribution<float> dist01(0.f, 1.f);

    // Adjustable: basic params
    LSystemParams baseP;
    baseP.iterations = 4;
    baseP.stepLength = 0.055f;
    baseP.baseAngleDeg = 30.0f;
    baseP.angleJitterDeg = 15.0f;
    baseP.baseRadius = 0.018f;
    baseP.radiusDecay = 0.75f;
    baseP.leafDensity = 1.0f;

    int s4 = std::max(1, settings.shapeParameter4); // Vegetation clusters / coverage
    int s5 = std::max(1, settings.shapeParameter5); // Tree size / complexity
    int s6 = std::max(1, settings.shapeParameter6); // Leaf density

    float cov01 = clamp01((s4 - 1) / 99.f);
    float size01 = clamp01((s5 - 1) / 39.f);
    float leaf01 = clamp01((s6 - 1) / 39.f);

    // Coverage -> Number of clusters / Radius / Number of trees per cluster

    // cluster number
    int clusterCount = 12 + int(glm::mix(40.f, 160.f, cov01)); // s4=1 → 10 簇, s4=10 → 64, s4=25 → 154

    // number of trees per cluster: The higher the density, the more trees per cluster.
    int treesPerClusterMin = 4 + int(glm::mix(3.f, 10.f, size01)); // 7 ~ 14
    int treesPerClusterMax = treesPerClusterMin + 4;

    // cluster radius: The larger the cov, the more compact the cluster.
    float clusterRadiusBase = glm::mix(0.10f, 0.03f, cov01);

    // World space sea level / heightScale (aligned with terrain shader)
    float seaHeightWorld = m_terrainParams.seaLevel; // uSeaHeight
    float heightScale = m_terrainParams.heightScale; // uHeightScale
    float seaMargin = 0.02f * heightScale;

    // *Tree location estimation: approximates computeGrassRockWeights in terrain.frag,
    // returns [0,1]: the closer to 1, the more it resembles grass.
    auto grassWeightApprox = [](float hNorm, float slope) -> float
    {
        // rockBeach
        float rockBeach = 1.f - glm::smoothstep(0.02f, 0.12f, hNorm);

        // grassBand
        float grassBand = glm::smoothstep(0.05f, 0.80f, hNorm);

        // rockSlope
        float rockSlope = glm::smoothstep(0.75f, 0.90f, slope);

        float wRock = std::max(rockBeach, rockSlope);
        float wGrass = grassBand * (1.f - 0.7f * rockSlope);

        wGrass *= 1.4f;
        wRock *= 0.7f;

        float s = wGrass + wRock + EPS;
        return wGrass / s;
    };

    // Cluster geneartion
    for (int c = 0; c < clusterCount; ++c)
    {
        glm::vec2 centerUV(0.f);
        bool foundCenter = false;

        // Find a center "above the sea surface"
        for (int tries = 0; tries < 32 && !foundCenter; ++tries)
        {
            glm::vec2 uv(dist01(rng), dist01(rng));
            glm::vec3 surfLocal = m_terrainGen.sampleSurfacePos(uv.x, uv.y);
            glm::vec3 surfWorld = glm::vec3(m_terrainModel * glm::vec4(surfLocal, 1.f));
            if (surfWorld.y <= seaHeightWorld + seaMargin)
                continue;

            centerUV = uv;
            foundCenter = true;
        }
        if (!foundCenter)
            continue;

        float clusterRadius = clusterRadiusBase * (0.7f + 0.6f * dist01(rng));
        int bushesPerCluster = treesPerClusterMin +
                               int(dist01(rng) * float(treesPerClusterMax - treesPerClusterMin + 1));

        // Sample a point inside small disk
        for (int k = 0; k < bushesPerCluster; ++k)
        {
            //
            float ang = 2.f * float(M_PI) * dist01(rng);
            float r = clusterRadius * std::sqrt(dist01(rng));
            glm::vec2 uv = centerUV + r * glm::vec2(std::cos(ang), std::sin(ang));
            uv.x = clamp01(uv.x);
            uv.y = clamp01(uv.y);

            glm::vec3 surfLocal = m_terrainGen.sampleSurfacePos(uv.x, uv.y);
            glm::vec3 pWorld = glm::vec3(m_terrainModel * glm::vec4(surfLocal, 1.f));

            if (pWorld.y <= seaHeightWorld + seaMargin)
                continue;

            // height normalization
            float hNorm = glm::clamp(
                (pWorld.y - seaHeightWorld) / std::max(heightScale, EPS),
                0.0f, 1.0f);

            // Estimate normal -> Estimate slope
            auto sampleHeightWorld = [&](float u, float v)
            {
                glm::vec2 uvc(clamp01(u), clamp01(v));
                glm::vec3 pL = m_terrainGen.sampleSurfacePos(uvc.x, uvc.y);
                glm::vec3 pW = glm::vec3(m_terrainModel * glm::vec4(pL, 1.f));
                return pW.y;
            };

            const float eps = 1.0f / 512.0f;
            float h0 = pWorld.y;
            float hdx = sampleHeightWorld(uv.x + eps, uv.y);
            float hdy = sampleHeightWorld(uv.x, uv.y + eps);

            glm::vec3 dx = glm::vec3(eps, hdx - h0, 0.f);
            glm::vec3 dz = glm::vec3(0.f, hdy - h0, eps);
            glm::vec3 nWorld = glm::normalize(glm::cross(dz, dx));

            // slope = 0 (flat), 1 (vertical)
            float slope = glm::clamp(
                1.0f - glm::dot(nWorld, glm::vec3(0, 1, 0)),
                0.0f, 1.0f);

            // grassland weight (0..1), height & slope considered together
            float wGrass = grassWeightApprox(hNorm, slope);

            // too steep will be banned directly;
            // the rest will be determined by the weight of the grass.
            if (slope > 0.96f)
                continue; // almost vertical cliff has no trees.

            // Adjustable: wGrass threshold: between 0.12 and 0.25.
            if (wGrass < 0.18f)
                continue;

            // L-system parameters of tree
            LSystemParams treeP = baseP;

            // Tree size slider (height/thickness/complexity)
            treeP.stepLength *= (0.85f + 0.5f * dist01(rng)) * glm::mix(0.7f, 1.4f, size01);
            treeP.baseRadius *= glm::mix(0.7f, 1.3f, size01);

            treeP.iterations = (size01 > 0.5f && dist01(rng) < 0.5f) ? 3 : 2;

            treeP.baseAngleDeg += (dist01(rng) - 0.5f) * 12.0f;
            treeP.angleJitterDeg *= (0.7f + 0.6f * dist01(rng));
            treeP.radiusDecay = glm::clamp(
                baseP.radiusDecay + (dist01(rng) - 0.5f) * 0.2f,
                0.6f, 0.95f);

            // leafDensity slider： 0.5 ~ 2.0 times the leaf volume
            treeP.leafDensity = glm::mix(0.5f, 2.0f, leaf01);

            // grammar: randomly select one rule X
            LSystemTree tree(treeP);

            std::vector<std::string> xRules = {
                "F[+FX][-FX][&FX][^FX]FX",
                "F[+F&X][-F^X][+FX][&FX]X",
                "F[+FX[&X]][-FX[^X]][&FX[+X]][^FX[-X]]X"};
            int idx = int(dist01(rng) * xRules.size());
            if (idx >= (int)xRules.size())
                idx = (int)xRules.size() - 1;

            std::unordered_map<char, std::string> rules;
            rules['X'] = xRules[idx];
            rules['F'] = "FF";

            tree.generate("X", rules);

            const auto &branches = tree.branches();
            const auto &leaves = tree.leaves();
            if (branches.empty())
                continue;

            // Random Size / Tilt / Orientation
            // World Space Scaling: The size slider controls the overall size again
            float treeScaleBase = glm::mix(0.12f, 0.28f, size01);
            float treeScale = treeScaleBase * (0.8f + 0.4f * dist01(rng));

            // Adjustable: for controlling the size of the generating L-system tree
            const float TREE_GLOBAL_SCALE = 20.f;
            treeScale *= TREE_GLOBAL_SCALE;

            float yaw = 2.f * float(M_PI) * dist01(rng);
            float tiltX = glm::radians((dist01(rng) - 0.5f) * 8.f); // [-4°,4°]
            float tiltZ = glm::radians((dist01(rng) - 0.5f) * 8.f);

            glm::mat4 T = glm::translate(glm::mat4(1.f), pWorld);
            glm::mat4 R_yaw = glm::rotate(glm::mat4(1.f), yaw, glm::vec3(0, 1, 0));
            glm::mat4 R_tiltX = glm::rotate(glm::mat4(1.f), tiltX, glm::vec3(1, 0, 0));
            glm::mat4 R_tiltZ = glm::rotate(glm::mat4(1.f), tiltZ, glm::vec3(0, 0, 1));
            glm::mat4 S = glm::scale(glm::mat4(1.f), glm::vec3(treeScale));

            glm::mat4 baseModel = T * R_yaw * R_tiltZ * R_tiltX * S;

            float bushScaleBase = 0.20f;
            float bushScale = bushScaleBase * (0.7f + 0.6f * dist01(rng));

            // add all branches to the instance list
            for (const BranchInstance &b : branches)
            {
                BranchInstance inst;
                inst.radius = b.radius * bushScale;
                inst.model = baseModel * b.model;
                m_forestBranches.push_back(inst);
            }

            // all leaves
            for (const LeafInstance &leaf : leaves)
            {
                glm::mat4 M = baseModel * leaf.model;
                m_forestLeaves.push_back(M);
            }

            if (m_forestBranches.size() > maxBranches ||
                m_forestLeaves.size() > maxLeaves)
            {
                break; // break bushesPerCluster
            }
        }

        if (m_forestBranches.size() > maxBranches ||
            m_forestLeaves.size() > maxLeaves)
        {
            break; // break clusterCount
        }
    }

    std::cout << "[buildForest] branches=" << m_forestBranches.size()
              << ", leaves=" << m_forestLeaves.size()
              << ", clusters=" << clusterCount
              << " (s4=" << s4 << ", s5=" << s5 << ", s6=" << s6 << ")\n";

    // Upload branch instance matrix to VBO
    m_branchInstanceCount = static_cast<GLsizei>(m_forestBranches.size());
    std::vector<glm::mat4> branchModels;
    branchModels.reserve(m_branchInstanceCount);
    for (const BranchInstance &b : m_forestBranches)
    {
        branchModels.push_back(b.model);
    }

    glBindBuffer(GL_ARRAY_BUFFER, m_branchInstanceVBO);
    glBufferData(GL_ARRAY_BUFFER,
                 branchModels.size() * sizeof(glm::mat4),
                 branchModels.data(),
                 GL_STATIC_DRAW);

    // Upload leaf instance matrix to VBO
    m_leafInstanceCount = static_cast<GLsizei>(m_forestLeaves.size());
    if (!m_forestLeaves.empty())
    {
        glBindBuffer(GL_ARRAY_BUFFER, m_leafInstanceVBO);
        glBufferData(GL_ARRAY_BUFFER,
                     m_forestLeaves.size() * sizeof(glm::mat4),
                     m_forestLeaves.data(),
                     GL_STATIC_DRAW);
    }
    glBindBuffer(GL_ARRAY_BUFFER, 0);
}

void Realtime::buildRocks()
{
    m_rocks.clear();

    if (!m_rockMesh)
        return;

    std::mt19937 rng(5678); // Different seed
    std::uniform_real_distribution<float> dist01(0.f, 1.f);

    // Rock parameters
    // Map slider (1-100) to rock count (e.g., 10 to 1000)
    int rockCount = settings.shapeParameter7 * 10;
    float seaHeightWorld = m_terrainParams.seaLevel;
    float heightScale = m_terrainParams.heightScale;

    auto clamp01 = [](float v)
    { return glm::clamp(v, 0.f, 1.f); };

    for (int i = 0; i < rockCount; ++i)
    {
        glm::vec2 uv(dist01(rng), dist01(rng));
        glm::vec3 surfLocal = m_terrainGen.sampleSurfacePos(uv.x, uv.y);
        glm::vec3 pWorld = glm::vec3(m_terrainModel * glm::vec4(surfLocal, 1.f));

        // Don't place rocks underwater (or maybe some near the shore)
        if (pWorld.y <= seaHeightWorld - 0.05f)
            continue;

        // Calculate slope
        auto sampleHeightWorld = [&](float u, float v)
        {
            glm::vec2 uvc(clamp01(u), clamp01(v));
            glm::vec3 pL = m_terrainGen.sampleSurfacePos(uvc.x, uvc.y);
            glm::vec3 pW = glm::vec3(m_terrainModel * glm::vec4(pL, 1.f));
            return pW.y;
        };

        const float eps = 1.0f / 512.0f;
        float h0 = pWorld.y;
        float hdx = sampleHeightWorld(uv.x + eps, uv.y);
        float hdy = sampleHeightWorld(uv.x, uv.y + eps);

        glm::vec3 dx = glm::vec3(eps, hdx - h0, 0.f);
        glm::vec3 dz = glm::vec3(0.f, hdy - h0, eps);
        glm::vec3 nWorld = glm::normalize(glm::cross(dz, dx));

        float slope = glm::clamp(1.0f - glm::dot(nWorld, glm::vec3(0, 1, 0)), 0.0f, 1.0f);

        // Place rocks on beaches or slopes, but not too steep
        bool isBeach = (pWorld.y < seaHeightWorld + 0.1f * heightScale);
        bool isSlope = (slope > 0.3f && slope < 0.8f);

        if (!isBeach && !isSlope)
        {
            // Randomly place some on flat ground too, but fewer
            if (dist01(rng) > 0.1f)
                continue;
        }

        // Transform
        float scaleBase = 0.5f + 1.5f * dist01(rng); // Random size
        glm::vec3 scale(scaleBase);

        // Deform slightly to look like a rock
        scale.x *= 0.8f + 0.4f * dist01(rng);
        scale.y *= 0.6f + 0.4f * dist01(rng); // Flatter
        scale.z *= 0.8f + 0.4f * dist01(rng);

        float yaw = 2.f * float(M_PI) * dist01(rng);
        float pitch = 2.f * float(M_PI) * dist01(rng);
        float roll = 2.f * float(M_PI) * dist01(rng);

        glm::mat4 T = glm::translate(glm::mat4(1.f), pWorld);
        glm::mat4 R = glm::rotate(glm::mat4(1.f), yaw, glm::vec3(0, 1, 0));
        R = glm::rotate(R, pitch, glm::vec3(1, 0, 0));
        R = glm::rotate(R, roll, glm::vec3(0, 0, 1));
        glm::mat4 S = glm::scale(glm::mat4(1.f), scale);

        // Sink the rock slightly into the ground
        T = glm::translate(T, glm::vec3(0, -0.2f * scale.y, 0));

        m_rocks.push_back(T * R * S);
    }

    std::cout << "[buildRocks] rocks=" << m_rocks.size() << "\n";

    // Upload to VBO
    m_rockInstanceCount = static_cast<GLsizei>(m_rocks.size());
    if (!m_rocks.empty())
    {
        glBindBuffer(GL_ARRAY_BUFFER, m_rockInstanceVBO);
        glBufferData(GL_ARRAY_BUFFER,
                     m_rocks.size() * sizeof(glm::mat4),
                     m_rocks.data(),
                     GL_STATIC_DRAW);
        glBindBuffer(GL_ARRAY_BUFFER, 0);
    }
}

GLuint Realtime::loadTexture2D(const QString &path, bool srgb)
{
    QImage img(path);
    if (img.isNull())
    {
        qWarning("Failed to load texture: %s", qPrintable(path));
        return 0;
    }

    img = img.convertToFormat(QImage::Format_RGBA8888).flipped(Qt::Vertical); // OpenGL: origin left-bottom corner

    GLuint tex = 0;
    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_2D, tex);

    GLenum internalFmt = srgb ? GL_SRGB8_ALPHA8 : GL_RGBA8;

    glTexImage2D(GL_TEXTURE_2D, 0, internalFmt,
                 img.width(), img.height(), 0,
                 GL_RGBA, GL_UNSIGNED_BYTE, img.bits());

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);

    glGenerateMipmap(GL_TEXTURE_2D);

    glBindTexture(GL_TEXTURE_2D, 0);
    return tex;
}

void Realtime::destroySceneFBO()
{
    if (m_texSceneColor)
    {
        glDeleteTextures(1, &m_texSceneColor);
        m_texSceneColor = 0;
    }
    if (m_texSceneDepth)
    {
        glDeleteTextures(1, &m_texSceneDepth);
        m_texSceneDepth = 0;
    }
    if (m_fboScene)
    {
        glDeleteFramebuffers(1, &m_fboScene);
        m_fboScene = 0;
    }
    m_sceneWidth = 0;
    m_sceneHeight = 0;
}

void Realtime::ensureSceneFBO(int w, int h)
{
    if (w <= 0 || h <= 0)
        return;

    if (w == m_sceneWidth && h == m_sceneHeight &&
        m_fboScene && m_texSceneColor && m_texSceneDepth)
    {
        return;
    }

    destroySceneFBO();

    m_sceneWidth = w;
    m_sceneHeight = h;

    // FBO
    glGenFramebuffers(1, &m_fboScene);
    glBindFramebuffer(GL_FRAMEBUFFER, m_fboScene);

    // Color attachment (HDR-friendly, use RGBA16F)
    glGenTextures(1, &m_texSceneColor);
    glBindTexture(GL_TEXTURE_2D, m_texSceneColor);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F,
                 w, h, 0, GL_RGBA, GL_FLOAT, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                           GL_TEXTURE_2D, m_texSceneColor, 0);

    // Depth attachment
    glGenTextures(1, &m_texSceneDepth);
    glBindTexture(GL_TEXTURE_2D, m_texSceneDepth);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT24,
                 w, h, 0, GL_DEPTH_COMPONENT, GL_FLOAT, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT,
                           GL_TEXTURE_2D, m_texSceneDepth, 0);

    GLenum bufs[1] = {GL_COLOR_ATTACHMENT0};
    glDrawBuffers(1, bufs);

    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
    {
        qWarning("Scene FBO incomplete!");
    }

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void Realtime::createScreenQuad()
{
    std::vector<float> verts;
    verts.reserve(6 * 9);

    auto addV = [&](float x, float y, float u, float v)
    {
        // position (clip space)
        verts.push_back(x);
        verts.push_back(y);
        verts.push_back(0.f);

        // normal (unused feature)
        verts.push_back(0.f);
        verts.push_back(0.f);
        verts.push_back(1.f);

        // "color" slot: put uv.xy, z=0
        verts.push_back(u);
        verts.push_back(v);
        verts.push_back(0.f);
    };

    // two triangles cover [-1,1]^2
    addV(-1.f, -1.f, 0.f, 0.f);
    addV(1.f, -1.f, 1.f, 0.f);
    addV(1.f, 1.f, 1.f, 1.f);

    addV(-1.f, -1.f, 0.f, 0.f);
    addV(1.f, 1.f, 1.f, 1.f);
    addV(-1.f, 1.f, 0.f, 1.f);

    m_screenQuad.uploadinterleavedPNC(verts);
}

glm::mat4 Realtime::createMirroredViewMatrix(float waterHeight)
{
    // Mirror camera position about waterHeight
    glm::vec3 mirroredCamPos = m_cam.eye;
    mirroredCamPos.y = 2.0f * waterHeight - m_cam.eye.y;

    // Invert the up direction
    glm::vec3 mirroredUp = glm::vec3(0.0f, -1.0f, 0.0f);

    // Calculate view matrix with mirrored camera position and inverted up
    glm::vec3 w = glm::normalize(-m_cam.look);
    glm::vec3 v = glm::normalize(mirroredUp - glm::dot(mirroredUp, w) * w);
    glm::vec3 u = glm::cross(v, w);

    glm::mat4 Rotate(
        u.x, v.x, w.x, 0.0f,
        u.y, v.y, w.y, 0.0f,
        u.z, v.z, w.z, 0.0f,
        0.0f, 0.0f, 0.0f, 1.0f);
    glm::mat4 Translate(
        1.0f, 0.0f, 0.0f, 0.0f,
        0.0f, 1.0f, 0.0f, 0.0f,
        0.0f, 0.0f, 1.0f, 0.0f,
        -mirroredCamPos.x, -mirroredCamPos.y, -mirroredCamPos.z, 1.0f);

    return Rotate * Translate;
}

void Realtime::renderScene()
{

    // global sun/ambient definition
    glm::vec3 sunDir = glm::normalize(glm::vec3(0.3f, -1.0f, 0.2f));
    glm::vec3 sunColor = glm::vec3(2.5f);
    glm::vec3 ambColor = glm::vec3(0.35f); // unified ambient light of "skylight + ground reflection"

    // sky color + fog density
    glm::vec3 fogColor(0.55f, 0.70f, 0.90f); // FIXME: can be set similar to color of the sky and horizon.
    float fogDensity = 0.02f;                // 0.01 to 0.03

    // skybox
    if (m_progSky && m_skyCube)
    {
        glDepthMask(GL_FALSE); // not specify depth, just draw the background

        // turn off backface culling for "back face" rendering
        glDisable(GL_CULL_FACE);

        glUseProgram(m_progSky);

        auto setSkyMat4 = [&](const char *name, const glm::mat4 &M)
        {
            glUniformMatrix4fv(glGetUniformLocation(m_progSky, name), 1, GL_FALSE, &M[0][0]);
        };

        glm::mat4 viewNoTrans = glm::mat4(glm::mat3(m_cam.view()));
        setSkyMat4("uView", viewNoTrans);
        setSkyMat4("uProj", m_cam.proj());

        glUniform3fv(glGetUniformLocation(m_progSky, "uSunDir"), 1, &sunDir[0]);
        glUniform3fv(glGetUniformLocation(m_progSky, "uSunColor"), 1, &sunColor[0]);

        glm::vec3 skyTop(0.04f, 0.23f, 0.48f);
        glm::vec3 skyHori(0.42f, 0.60f, 0.85f);
        glm::vec3 skyBottom(0.75f, 0.65f, 0.55f);

        glUniform3fv(glGetUniformLocation(m_progSky, "uSkyTopColor"), 1, &skyTop[0]);
        glUniform3fv(glGetUniformLocation(m_progSky, "uSkyHorizonColor"), 1, &skyHori[0]);
        glUniform3fv(glGetUniformLocation(m_progSky, "uSkyBottomColor"), 1, &skyBottom[0]);

        m_skyCube->draw();

        glEnable(GL_CULL_FACE);
        glDepthMask(GL_TRUE);
    }

    // terrain
    if (m_hasTerrain && m_progTerrain)
    {
        glPolygonMode(GL_FRONT_AND_BACK, m_terrainWire ? GL_LINE : GL_FILL);

        glUseProgram(m_progTerrain);

        auto set4 = [&](const char *n, const glm::mat4 &M)
        {
            glUniformMatrix4fv(glGetUniformLocation(m_progTerrain, n),
                               1, GL_FALSE, &M[0][0]);
        };
        set4("uProj", m_cam.proj());
        set4("uView", m_cam.view());
        set4("uModel", m_terrainModel);
        glUniform1i(glGetUniformLocation(m_progTerrain, "wireshade"),
                    m_terrainWire ? 1 : 0);

        // Lighting & Height Parameters
        glUniform3fv(glGetUniformLocation(m_progTerrain, "uEye"), 1, &m_cam.eye[0]);

        glUniform3fv(glGetUniformLocation(m_progTerrain, "uSunDir"), 1, &sunDir[0]);
        glUniform3fv(glGetUniformLocation(m_progTerrain, "uSunColor"), 1, &sunColor[0]);
        glUniform3fv(glGetUniformLocation(m_progTerrain, "uAmbientColor"), 1, &ambColor[0]);

        glUniform1i(glGetUniformLocation(m_progTerrain, "uEnableFog"), m_enableFog);
        glUniform1f(glGetUniformLocation(m_progTerrain, "uFogDensity"), m_fogDensity);
        glUniform3fv(glGetUniformLocation(m_progTerrain, "uFogColor"), 1, &m_fogColor[0]);

        glUniform1f(glGetUniformLocation(m_progTerrain, "uSeaHeight"), m_seaHeightWorld);
        glUniform1f(glGetUniformLocation(m_progTerrain, "uHeightScale"), m_heightScaleWorld);

        // normal intentisty
        glUniform1f(glGetUniformLocation(m_progTerrain, "uNormalStrength"), 1.15f);

        // bind texture to sampler
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, m_texGrassAlbedo);
        glUniform1i(glGetUniformLocation(m_progTerrain, "uGrassAlbedo"), 0);

        glActiveTexture(GL_TEXTURE1);
        glBindTexture(GL_TEXTURE_2D, m_texRockAlbedo);
        glUniform1i(glGetUniformLocation(m_progTerrain, "uRockAlbedo"), 1);

        glActiveTexture(GL_TEXTURE2);
        glBindTexture(GL_TEXTURE_2D, m_texBeachAlbedo);
        glUniform1i(glGetUniformLocation(m_progTerrain, "uBeachAlbedo"), 2);

        glActiveTexture(GL_TEXTURE3);
        glBindTexture(GL_TEXTURE_2D, m_texGrassNormal);
        glUniform1i(glGetUniformLocation(m_progTerrain, "uGrassNormal"), 3);

        glActiveTexture(GL_TEXTURE4);
        glBindTexture(GL_TEXTURE_2D, m_texRockNormal);
        glUniform1i(glGetUniformLocation(m_progTerrain, "uRockNormal"), 4);

        glActiveTexture(GL_TEXTURE5);
        glBindTexture(GL_TEXTURE_2D, m_texBeachNormal);
        glUniform1i(glGetUniformLocation(m_progTerrain, "uBeachNormal"), 5);

        glActiveTexture(GL_TEXTURE6);
        glBindTexture(GL_TEXTURE_2D, m_texGrassRough);
        glUniform1i(glGetUniformLocation(m_progTerrain, "uGrassRough"), 6);

        glActiveTexture(GL_TEXTURE7);
        glBindTexture(GL_TEXTURE_2D, m_texRockRough);
        glUniform1i(glGetUniformLocation(m_progTerrain, "uRockRough"), 7);

        glActiveTexture(GL_TEXTURE8);
        glBindTexture(GL_TEXTURE_2D, m_texBeachRough);
        glUniform1i(glGetUniformLocation(m_progTerrain, "uBeachRough"), 8);

        glActiveTexture(GL_TEXTURE9);
        glBindTexture(GL_TEXTURE_2D, m_texRockHighAlbedo);
        glUniform1i(glGetUniformLocation(m_progTerrain, "uRockHighAlbedo"), 9);

        glActiveTexture(GL_TEXTURE10);
        glBindTexture(GL_TEXTURE_2D, m_texRockHighNormal);
        glUniform1i(glGetUniformLocation(m_progTerrain, "uRockHighNormal"), 10);

        glActiveTexture(GL_TEXTURE11);
        glBindTexture(GL_TEXTURE_2D, m_texRockHighRough);
        glUniform1i(glGetUniformLocation(m_progTerrain, "uRockHighRough"), 11);

        glActiveTexture(GL_TEXTURE12);
        glBindTexture(GL_TEXTURE_2D, m_texSnowAlbedo);
        glUniform1i(glGetUniformLocation(m_progTerrain, "uSnowAlbedo"), 12);

        glActiveTexture(GL_TEXTURE13);
        glBindTexture(GL_TEXTURE_2D, m_texSnowNormal);
        glUniform1i(glGetUniformLocation(m_progTerrain, "uSnowNormal"), 13);

        glActiveTexture(GL_TEXTURE14);
        glBindTexture(GL_TEXTURE_2D, m_texSnowRough);
        glUniform1i(glGetUniformLocation(m_progTerrain, "uSnowRough"), 14);

        m_terrainMesh.draw();

        glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
    }

    // water
    if (m_progWater) {
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        glDepthMask(GL_FALSE);

        glUseProgram(m_progWater);

        glUniformMatrix4fv(glGetUniformLocation(m_progWater, "model_matrix"), 1, GL_FALSE, &m_terrainModel[0][0]);
        glUniformMatrix4fv(glGetUniformLocation(m_progWater, "view_matrix"), 1, GL_FALSE, &m_cam.view()[0][0]);
        glUniformMatrix4fv(glGetUniformLocation(m_progWater, "proj_matrix"), 1, GL_FALSE, &m_cam.proj()[0][0]);
        glUniform3fv(glGetUniformLocation(m_progWater, "ws_cam_pos"), 1, &m_cam.eye[0]);

        glUniform1i(glGetUniformLocation(m_progWater, "uEnableFog"), m_enableFog);
        glUniform1f(glGetUniformLocation(m_progWater, "uFogDensity"), m_fogDensity);
        glUniform3fv(glGetUniformLocation(m_progWater, "uFogColor"), 1, &m_fogColor[0]);

        m_waterMesh.draw();

        glDepthMask(GL_TRUE);
        glDisable(GL_BLEND);
    }

    // forest: use instance rendering shader
    if (m_drawForest && m_treeCylinderMesh && m_branchInstanceCount > 0)
    {
        glUseProgram(m_progForest);

        auto setMat4 = [&](const char *name, const glm::mat4 &M)
        {
            glUniformMatrix4fv(glGetUniformLocation(m_progForest, name),
                               1, GL_FALSE, &M[0][0]);
        };

        setMat4("uView", m_cam.view());
        setMat4("uProj", m_cam.proj());
        glUniform3fv(glGetUniformLocation(m_progForest, "uEye"), 1, &m_cam.eye[0]);

        // sunlight / ambientlLight / fog
        glUniform3fv(glGetUniformLocation(m_progForest, "uSunDir"), 1, &sunDir[0]);
        glUniform3fv(glGetUniformLocation(m_progForest, "uSunColor"), 1, &sunColor[0]);
        glUniform3fv(glGetUniformLocation(m_progForest, "uAmbientColor"), 1, &ambColor[0]);
        glUniform3fv(glGetUniformLocation(m_progForest, "uFogColor"), 1, &fogColor[0]);
        glUniform1f(glGetUniformLocation(m_progForest, "uFogDensity"), fogDensity);

        // first, draw the tree branches (brown texture)
        glm::vec3 barkKa(0.1f, 0.08f, 0.05f);
        glm::vec3 barkKd(0.3f, 0.22f, 0.15f);
        glm::vec3 barkKs(0.02f);

        glUniform3fv(glGetUniformLocation(m_progForest, "u_mat.ka"), 1, &barkKa[0]);
        glUniform3fv(glGetUniformLocation(m_progForest, "u_mat.kd"), 1, &barkKd[0]);
        glUniform3fv(glGetUniformLocation(m_progForest, "u_mat.ks"), 1, &barkKs[0]);
        glUniform1f(glGetUniformLocation(m_progForest, "u_mat.shininess"), 12.f);

        m_treeCylinderMesh->drawInstanced(m_branchInstanceCount);

        // then, draw the leaves (green texture)
        if (m_leafMesh && m_leafInstanceCount > 0)
        {
            glm::vec3 leafKa(0.05f, 0.10f, 0.05f);
            glm::vec3 leafKd(0.20f, 0.70f, 0.25f);
            ;
            glm::vec3 leafKs(0.03f);

            glUniform3fv(glGetUniformLocation(m_progForest, "u_mat.ka"), 1, &leafKa[0]);
            glUniform3fv(glGetUniformLocation(m_progForest, "u_mat.kd"), 1, &leafKd[0]);
            glUniform3fv(glGetUniformLocation(m_progForest, "u_mat.ks"), 1, &leafKs[0]);
            glUniform1f(glGetUniformLocation(m_progForest, "u_mat.shininess"), 10.f);

            m_leafMesh->drawInstanced(m_leafInstanceCount);
        }

        // then, draw the rocks (gray texture)
        if (m_rockMesh && m_rockInstanceCount > 0)
        {
            glm::vec3 rockKa(0.1f, 0.1f, 0.1f);
            glm::vec3 rockKd(0.4f, 0.4f, 0.4f);
            glm::vec3 rockKs(0.1f);

            glUniform3fv(glGetUniformLocation(m_progForest, "u_mat.ka"), 1, &rockKa[0]);
            glUniform3fv(glGetUniformLocation(m_progForest, "u_mat.kd"), 1, &rockKd[0]);
            glUniform3fv(glGetUniformLocation(m_progForest, "u_mat.ks"), 1, &rockKs[0]);
            glUniform1f(glGetUniformLocation(m_progForest, "u_mat.shininess"), 10.f);

            m_rockMesh->drawInstanced(m_rockInstanceCount);
        }
    }

    // Draw Particles
    if (m_particleSystem)
    {
        m_particleSystem->draw(m_cam.view(), m_cam.proj());
    }
}

void Realtime::renderSceneObject(const glm::mat4 &viewMatrix)
{
    // global sun/ambient definition
    glm::vec3 sunDir = glm::normalize(glm::vec3(0.3f, -1.0f, 0.2f));
    glm::vec3 sunColor = glm::vec3(2.5f);
    glm::vec3 ambColor = glm::vec3(0.35f); // unified ambient light of "skylight + ground reflection"

    // sky color + fog density
    glm::vec3 fogColor(0.55f, 0.70f, 0.90f); // FIXME: can be set similar to color of the sky and horizon.
    float fogDensity = 0.02f;                // 0.01 to 0.03

    // skybox
    if (m_progSky && m_skyCube)
    {
        glDepthMask(GL_FALSE); // not specify depth, just draw the background

        // turn off backface culling for "back face" rendering
        glDisable(GL_CULL_FACE);

        glUseProgram(m_progSky);

        auto setSkyMat4 = [&](const char *name, const glm::mat4 &M)
        {
            glUniformMatrix4fv(glGetUniformLocation(m_progSky, name), 1, GL_FALSE, &M[0][0]);
        };

        glm::mat4 viewNoTrans = glm::mat4(glm::mat3(viewMatrix));
        setSkyMat4("uView", viewNoTrans);
        setSkyMat4("uProj", m_cam.proj());

        glUniform3fv(glGetUniformLocation(m_progSky, "uSunDir"), 1, &sunDir[0]);
        glUniform3fv(glGetUniformLocation(m_progSky, "uSunColor"), 1, &sunColor[0]);

        glm::vec3 skyTop(0.04f, 0.23f, 0.48f);
        glm::vec3 skyHori(0.42f, 0.60f, 0.85f);
        glm::vec3 skyBottom(0.75f, 0.65f, 0.55f);

        glUniform3fv(glGetUniformLocation(m_progSky, "uSkyTopColor"), 1, &skyTop[0]);
        glUniform3fv(glGetUniformLocation(m_progSky, "uSkyHorizonColor"), 1, &skyHori[0]);
        glUniform3fv(glGetUniformLocation(m_progSky, "uSkyBottomColor"), 1, &skyBottom[0]);

        m_skyCube->draw();

        glEnable(GL_CULL_FACE);
        glDepthMask(GL_TRUE);
    }

    // terrain
    if (m_hasTerrain && m_progTerrain)
    {
        glPolygonMode(GL_FRONT_AND_BACK, m_terrainWire ? GL_LINE : GL_FILL);

        glUseProgram(m_progTerrain);

        auto set4 = [&](const char *n, const glm::mat4 &M)
        {
            glUniformMatrix4fv(glGetUniformLocation(m_progTerrain, n),
                               1, GL_FALSE, &M[0][0]);
        };
        set4("uProj", m_cam.proj());
        set4("uView", viewMatrix);
        set4("uModel", m_terrainModel);
        glUniform1i(glGetUniformLocation(m_progTerrain, "wireshade"),
                    m_terrainWire ? 1 : 0);

        // Lighting & Height Parameters
        glUniform3fv(glGetUniformLocation(m_progTerrain, "uEye"), 1, &m_cam.eye[0]);

        glUniform3fv(glGetUniformLocation(m_progTerrain, "uSunDir"), 1, &sunDir[0]);
        glUniform3fv(glGetUniformLocation(m_progTerrain, "uSunColor"), 1, &sunColor[0]);
        glUniform3fv(glGetUniformLocation(m_progTerrain, "uAmbientColor"), 1, &ambColor[0]);

        glUniform3fv(glGetUniformLocation(m_progTerrain, "uFogColor"), 1, &fogColor[0]);
        glUniform1f(glGetUniformLocation(m_progTerrain, "uFogDensity"), fogDensity);

        glUniform1f(glGetUniformLocation(m_progTerrain, "uSeaHeight"), m_seaHeightWorld);
        glUniform1f(glGetUniformLocation(m_progTerrain, "uHeightScale"), m_heightScaleWorld);

        // normal intentisty
        glUniform1f(glGetUniformLocation(m_progTerrain, "uNormalStrength"), 1.15f);

        // bind texture to sampler
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, m_texGrassAlbedo);
        glUniform1i(glGetUniformLocation(m_progTerrain, "uGrassAlbedo"), 0);

        glActiveTexture(GL_TEXTURE1);
        glBindTexture(GL_TEXTURE_2D, m_texRockAlbedo);
        glUniform1i(glGetUniformLocation(m_progTerrain, "uRockAlbedo"), 1);

        glActiveTexture(GL_TEXTURE2);
        glBindTexture(GL_TEXTURE_2D, m_texBeachAlbedo);
        glUniform1i(glGetUniformLocation(m_progTerrain, "uBeachAlbedo"), 2);

        glActiveTexture(GL_TEXTURE3);
        glBindTexture(GL_TEXTURE_2D, m_texGrassNormal);
        glUniform1i(glGetUniformLocation(m_progTerrain, "uGrassNormal"), 3);

        glActiveTexture(GL_TEXTURE4);
        glBindTexture(GL_TEXTURE_2D, m_texRockNormal);
        glUniform1i(glGetUniformLocation(m_progTerrain, "uRockNormal"), 4);

        glActiveTexture(GL_TEXTURE5);
        glBindTexture(GL_TEXTURE_2D, m_texBeachNormal);
        glUniform1i(glGetUniformLocation(m_progTerrain, "uBeachNormal"), 5);

        glActiveTexture(GL_TEXTURE6);
        glBindTexture(GL_TEXTURE_2D, m_texGrassRough);
        glUniform1i(glGetUniformLocation(m_progTerrain, "uGrassRough"), 6);

        glActiveTexture(GL_TEXTURE7);
        glBindTexture(GL_TEXTURE_2D, m_texRockRough);
        glUniform1i(glGetUniformLocation(m_progTerrain, "uRockRough"), 7);

        glActiveTexture(GL_TEXTURE8);
        glBindTexture(GL_TEXTURE_2D, m_texBeachRough);
        glUniform1i(glGetUniformLocation(m_progTerrain, "uBeachRough"), 8);

        glActiveTexture(GL_TEXTURE9);
        glBindTexture(GL_TEXTURE_2D, m_texRockHighAlbedo);
        glUniform1i(glGetUniformLocation(m_progTerrain, "uRockHighAlbedo"), 9);

        glActiveTexture(GL_TEXTURE10);
        glBindTexture(GL_TEXTURE_2D, m_texRockHighNormal);
        glUniform1i(glGetUniformLocation(m_progTerrain, "uRockHighNormal"), 10);

        glActiveTexture(GL_TEXTURE11);
        glBindTexture(GL_TEXTURE_2D, m_texRockHighRough);
        glUniform1i(glGetUniformLocation(m_progTerrain, "uRockHighRough"), 11);

        glActiveTexture(GL_TEXTURE12);
        glBindTexture(GL_TEXTURE_2D, m_texSnowAlbedo);
        glUniform1i(glGetUniformLocation(m_progTerrain, "uSnowAlbedo"), 12);

        glActiveTexture(GL_TEXTURE13);
        glBindTexture(GL_TEXTURE_2D, m_texSnowNormal);
        glUniform1i(glGetUniformLocation(m_progTerrain, "uSnowNormal"), 13);

        glActiveTexture(GL_TEXTURE14);
        glBindTexture(GL_TEXTURE_2D, m_texSnowRough);
        glUniform1i(glGetUniformLocation(m_progTerrain, "uSnowRough"), 14);

        m_terrainMesh.draw();

        glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
    }
    // forest: use instance rendering shader
    if (m_drawForest && m_treeCylinderMesh && m_branchInstanceCount > 0)
    {
        glUseProgram(m_progForest);

        auto setMat4 = [&](const char *name, const glm::mat4 &M)
        {
            glUniformMatrix4fv(glGetUniformLocation(m_progForest, name),
                               1, GL_FALSE, &M[0][0]);
        };

        setMat4("uView", viewMatrix);
        setMat4("uProj", m_cam.proj());
        glUniform3fv(glGetUniformLocation(m_progForest, "uEye"), 1, &m_cam.eye[0]);

        // sunlight / ambientlLight / fog
        glUniform3fv(glGetUniformLocation(m_progForest, "uSunDir"), 1, &sunDir[0]);
        glUniform3fv(glGetUniformLocation(m_progForest, "uSunColor"), 1, &sunColor[0]);
        glUniform3fv(glGetUniformLocation(m_progForest, "uAmbientColor"), 1, &ambColor[0]);
        glUniform3fv(glGetUniformLocation(m_progForest, "uFogColor"), 1, &fogColor[0]);
        glUniform1f(glGetUniformLocation(m_progForest, "uFogDensity"), fogDensity);

        // first, draw the tree branches (brown texture)
        glm::vec3 barkKa(0.1f, 0.08f, 0.05f);
        glm::vec3 barkKd(0.3f, 0.22f, 0.15f);
        glm::vec3 barkKs(0.02f);

        glUniform3fv(glGetUniformLocation(m_progForest, "u_mat.ka"), 1, &barkKa[0]);
        glUniform3fv(glGetUniformLocation(m_progForest, "u_mat.kd"), 1, &barkKd[0]);
        glUniform3fv(glGetUniformLocation(m_progForest, "u_mat.ks"), 1, &barkKs[0]);
        glUniform1f(glGetUniformLocation(m_progForest, "u_mat.shininess"), 12.f);

        m_treeCylinderMesh->drawInstanced(m_branchInstanceCount);

        // then, draw the leaves (green texture)
        if (m_leafMesh && m_leafInstanceCount > 0)
        {
            glm::vec3 leafKa(0.05f, 0.10f, 0.05f);
            glm::vec3 leafKd(0.20f, 0.70f, 0.25f);
            ;
            glm::vec3 leafKs(0.03f);

            glUniform3fv(glGetUniformLocation(m_progForest, "u_mat.ka"), 1, &leafKa[0]);
            glUniform3fv(glGetUniformLocation(m_progForest, "u_mat.kd"), 1, &leafKd[0]);
            glUniform3fv(glGetUniformLocation(m_progForest, "u_mat.ks"), 1, &leafKs[0]);
            glUniform1f(glGetUniformLocation(m_progForest, "u_mat.shininess"), 10.f);

            m_leafMesh->drawInstanced(m_leafInstanceCount);
        }

        // then, draw the rocks (gray texture)
        if (m_rockMesh && m_rockInstanceCount > 0)
        {
            glm::vec3 rockKa(0.1f, 0.1f, 0.1f);
            glm::vec3 rockKd(0.4f, 0.4f, 0.4f);
            glm::vec3 rockKs(0.1f);

            glUniform3fv(glGetUniformLocation(m_progForest, "u_mat.ka"), 1, &rockKa[0]);
            glUniform3fv(glGetUniformLocation(m_progForest, "u_mat.kd"), 1, &rockKd[0]);
            glUniform3fv(glGetUniformLocation(m_progForest, "u_mat.ks"), 1, &rockKs[0]);
            glUniform1f(glGetUniformLocation(m_progForest, "u_mat.shininess"), 10.f);

            // Bind texture
            glActiveTexture(GL_TEXTURE0);
            glBindTexture(GL_TEXTURE_2D, m_texRockObjAlbedo);
            glUniform1i(glGetUniformLocation(m_progForest, "uTexture"), 0);
            glUniform1i(glGetUniformLocation(m_progForest, "uUseTexture"), 1);

            m_rockMesh->drawInstanced(m_rockInstanceCount);

            // Reset
            glUniform1i(glGetUniformLocation(m_progForest, "uUseTexture"), 0);
        }
    }
}

// Reflection: Render scene above water to m_reflectionFBO
void Realtime::renderReflection()
{
    glBindFramebuffer(GL_FRAMEBUFFER, m_reflectionFBO);
    glViewport(0, 0, m_fbo_width, m_fbo_height);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    // Enable clipping plane to clip below water
    glEnable(GL_CLIP_PLANE0);
    // Set clipping plane: (0, 1, 0, -WATER_HEIGHT) - clips below water
    double clipPlane[4] = {0.0, 1.0, 0.0, -WATER_HEIGHT};
    glClipPlane(GL_CLIP_PLANE0, clipPlane);

    // Use mirrored view matrix
    glm::mat4 mirroredView = createMirroredViewMatrix(WATER_HEIGHT);
    glm::vec3 originalCamPos = m_cam.eye;
    m_cam.eye.y = 2.0f * WATER_HEIGHT - m_cam.eye.y;

    renderSceneObject(mirroredView);
    m_cam.eye = originalCamPos;

    glDisable(GL_CLIP_PLANE0);
}

// Refraction: Render scene below water to m_refractionFBO
void Realtime::renderRefraction()
{
    glBindFramebuffer(GL_FRAMEBUFFER, m_refractionFBO);
    glViewport(0, 0, m_fbo_width, m_fbo_height);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    // Enable clipping plane to clip above water
    glEnable(GL_CLIP_PLANE0);
    // Set clipping plane: (0, -1, 0, WATER_HEIGHT) - clips above water
    double clipPlane[4] = {0.0, -1.0, 0.0, WATER_HEIGHT};
    glClipPlane(GL_CLIP_PLANE0, clipPlane);

    // Use normal view matrix
    renderSceneObject(m_cam.view());

    glDisable(GL_CLIP_PLANE0);
}

// Water Part
void Realtime::renderWater()
{
    if (!m_progWater)
        return;

    // Enable blending for water transparency
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    // Disable depth writing but keep depth testing for proper occlusion
    glDepthMask(GL_FALSE);

    glUseProgram(m_progWater);
    glUniform1f(glGetUniformLocation(m_progWater, "u_near"), m_cam.nearP);
    glUniform1f(glGetUniformLocation(m_progWater, "u_far"), m_cam.farP);

    // Bind textures to texture units
    // Reflection texture
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, m_reflectionFBO_texture);
    glUniform1i(glGetUniformLocation(m_progWater, "u_reflectionTexture"), 0);

    // Refraction texture
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, m_refractionFBO_texture);
    glUniform1i(glGetUniformLocation(m_progWater, "u_refractionTexture"), 1);

    // Depth texture
    glActiveTexture(GL_TEXTURE2);
    glBindTexture(GL_TEXTURE_2D, m_refractionDepthTexture);
    glUniform1i(glGetUniformLocation(m_progWater, "u_depthTexture"), 2);

    // Normal map
    glActiveTexture(GL_TEXTURE3);
    glBindTexture(GL_TEXTURE_2D, m_texWaterNormal);
    glUniform1i(glGetUniformLocation(m_progWater, "u_normalMap"), 3);

    // DUDV map
    glActiveTexture(GL_TEXTURE4);
    glBindTexture(GL_TEXTURE_2D, m_waterDUDVTexture);
    glUniform1i(glGetUniformLocation(m_progWater, "u_dudvMap"), 4);

    // Set MVP matrix for water quad
    glUniformMatrix4fv(glGetUniformLocation(m_progWater, "model_matrix"), 1, GL_FALSE, &m_terrainModel[0][0]);

    // View & Proj
    glUniformMatrix4fv(glGetUniformLocation(m_progWater, "view_matrix"), 1, GL_FALSE, &m_cam.view()[0][0]);
    glUniformMatrix4fv(glGetUniformLocation(m_progWater, "proj_matrix"), 1, GL_FALSE, &m_cam.proj()[0][0]);

    // Camera position
    glUniform3fv(glGetUniformLocation(m_progWater, "ws_cam_pos"), 1, &m_cam.eye[0]);

    // Time factor (for animation)
    glUniform1f(glGetUniformLocation(m_progWater, "u_timeFactor"), m_time);

    glUniform1i(glGetUniformLocation(m_progWater, "uEnableFog"), m_enableFog);
    glUniform1f(glGetUniformLocation(m_progWater, "uFogDensity"), m_fogDensity);
    glUniform3fv(glGetUniformLocation(m_progWater, "uFogColor"), 1, &m_fogColor[0]);

    glm::vec3 sunDir = glm::normalize(glm::vec3(0.3f, -1.0f, 0.2f));
    glm::vec3 sunColor = glm::vec3(2.5f);

    // Global data
    glUniform1f(glGetUniformLocation(m_progWater, "globalData.ka"), 0.5f);
    glUniform1f(glGetUniformLocation(m_progWater, "globalData.kd"), 0.5f);
    glUniform1f(glGetUniformLocation(m_progWater, "globalData.ks"), 1.0f);

    // Lights
    glUniform1i(glGetUniformLocation(m_progWater, "number_light"), 1);

    glUniform1i(glGetUniformLocation(m_progWater, "light[0].type"), 0);
    glUniform3fv(glGetUniformLocation(m_progWater, "light[0].dir"), 1, &sunDir[0]);
    glUniform3fv(glGetUniformLocation(m_progWater, "light[0].color"), 1, &sunColor[0]);

    glm::vec3 zero(0.0f);
    glUniform3fv(glGetUniformLocation(m_progWater, "light[0].pos"), 1, &zero[0]);
    glUniform3fv(glGetUniformLocation(m_progWater, "light[0].function"), 1, &zero[0]);

    // draw water quad
    m_waterMesh.draw();

    // Restore depth writing and disable blending
    glUseProgram(0);
    glDepthMask(GL_TRUE);
    glDisable(GL_BLEND);

    // Unbind textures
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, 0);
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, 0);
    glActiveTexture(GL_TEXTURE2);
    glBindTexture(GL_TEXTURE_2D, 0);
    glActiveTexture(GL_TEXTURE3);
    glBindTexture(GL_TEXTURE_2D, 0);
    glActiveTexture(GL_TEXTURE4);
    glBindTexture(GL_TEXTURE_2D, 0);
}

// ================== Rendering the Scene!

Realtime::Realtime(QWidget *parent)
    : QOpenGLWidget(parent)
{
    m_prev_mouse_pos = glm::vec2(size().width() / 2, size().height() / 2);
    setMouseTracking(true);
    setFocusPolicy(Qt::StrongFocus);

    m_keyMap[Qt::Key_W] = false;
    m_keyMap[Qt::Key_A] = false;
    m_keyMap[Qt::Key_S] = false;
    m_keyMap[Qt::Key_D] = false;
    m_keyMap[Qt::Key_Control] = false;
    m_keyMap[Qt::Key_Space] = false;

    // If you must use this function, do not edit anything above this
}

void Realtime::rebuildWaterMesh()
{
    // sea level in local space
    float seaLocal = m_terrainParams.seaLevel * m_terrainParams.heightScale;

    // Raise the water level slightly (in a localized area)
    // multiply by the scale in model
    // note: world size will be approximately 0.2.
    float waterLocal = seaLocal + 0.02f * m_terrainParams.heightScale;

    std::vector<float> verts;
    verts.reserve(6 * 9); // two triangles, with a total of 6 vertices, each with 9 floats (PNC)

    auto addV = [&](float x, float y, float z,
                    float nx, float ny, float nz,
                    float u, float v)
    {
        verts.push_back(x);
        verts.push_back(y);
        verts.push_back(z);
        verts.push_back(nx);
        verts.push_back(ny);
        verts.push_back(nz);
        verts.push_back(u);
        verts.push_back(v);
        verts.push_back(0.f); // third attribute used as UV
    };

    // z-up plane is then rotated into the y-up world using m_terrainModel.
    glm::vec3 N(0.f, 0.f, 1.f);

    // quad covers [0,1] x [0,1], with height seaLocal
    addV(0.f, 0.f, waterLocal, N.x, N.y, N.z, 0.f, 0.f);
    addV(1.f, 0.f, waterLocal, N.x, N.y, N.z, 1.f, 0.f);
    addV(1.f, 1.f, waterLocal, N.x, N.y, N.z, 1.f, 1.f);

    addV(0.f, 0.f, waterLocal, N.x, N.y, N.z, 0.f, 0.f);
    addV(1.f, 1.f, waterLocal, N.x, N.y, N.z, 1.f, 1.f);
    addV(0.f, 1.f, waterLocal, N.x, N.y, N.z, 0.f, 1.f);

    m_waterMesh.uploadinterleavedPNC(verts);
}

void Realtime::finish()
{
    killTimer(m_timer);
    this->makeCurrent();

    // Cleanup Particles
    if (m_particleSystem)
    {
        delete m_particleSystem;
        m_particleSystem = nullptr;
    }

    // Students: anything requiring OpenGL calls when the program exits should be done here
    destroyMeshCache();

    if (m_prog)
    {
        glDeleteProgram(m_prog);
        m_prog = 0;
    }
    if (m_progTerrain)
    {
        glDeleteProgram(m_progTerrain);
        m_progTerrain = 0;
    }
    if (m_progWater)
    {
        glDeleteProgram(m_progWater);
        m_progWater = 0;
    }
    if (m_progSky)
    {
        glDeleteProgram(m_progSky);
        m_progSky = 0;
    }
    if (m_progForest)
    {
        glDeleteProgram(m_progForest);
        m_progForest = 0;
    }

    if (m_progPost)
    {
        glDeleteProgram(m_progPost);
        m_progPost = 0;
    }
    destroySceneFBO();
    m_screenQuad.destroy();

    if (m_texColorLUT) {
        glDeleteTextures(1, &m_texColorLUT);
        m_texColorLUT = 0;
    }

    this->doneCurrent();
}

void Realtime::initializeGL()
{
    m_devicePixelRatio = this->devicePixelRatio();

    m_timer = startTimer(1000 / 60);
    m_elapsedTimer.start();

    // Initializing GL.
    // GLEW (GL Extension Wrangler) provides access to OpenGL functions.
    glewExperimental = GL_TRUE;
    GLenum err = glewInit();
    if (err != GLEW_OK)
    {
        std::cerr << "Error while initializing GL: " << glewGetErrorString(err) << std::endl;
    }
    std::cout << "Initialized GL: Version " << glewGetString(GLEW_VERSION) << std::endl;

    // Allows OpenGL to draw objects appropriately on top of one another
    glEnable(GL_DEPTH_TEST);
    // Tells OpenGL to only draw the front face
    glEnable(GL_CULL_FACE);
    // Tells OpenGL how big the screen is
    glViewport(0, 0, size().width() * m_devicePixelRatio, size().height() * m_devicePixelRatio);

    // Students: anything requiring OpenGL calls when the program starts should be done here
    if (GLEW_VERSION_3_0 || GLEW_EXT_framebuffer_sRGB)
    {
        glEnable(GL_FRAMEBUFFER_SRGB);
    }

    m_devicePixelRatio = this->devicePixelRatio();

    glViewport(0, 0, size().width() * m_devicePixelRatio, size().height() * m_devicePixelRatio);

    // Build shader program
    try
    {
        m_prog = ShaderLoader::createShaderProgram(
            ":/resources/shaders/default.vert",
            ":/resources/shaders/default.frag");
    }
    catch (const std::exception &e)
    {
        qWarning("Shader compile/link error: %s", e.what());
        m_prog = 0;
    }

    // terrain shader
    try
    {
        m_progTerrain = ShaderLoader::createShaderProgram(
            ":/resources/shaders/terrain.vert",
            ":/resources/shaders/terrain.frag");
    }
    catch (const std::exception &e)
    {
        qWarning("Terrain shader compile/link error: %s", e.what());
        m_progTerrain = 0;
    }

    // forest shader
    try
    {
        m_progForest = ShaderLoader::createShaderProgram(
            ":/resources/shaders/forest.vert",
            ":/resources/shaders/forest.frag");
    }
    catch (const std::exception &e)
    {
        qWarning("Terrain shader compile/link error: %s", e.what());
        m_progTerrain = 0;
    }

    // water shader
    try
    {
        m_progWater = ShaderLoader::createShaderProgram(
            ":/resources/shaders/water.vert",
            ":/resources/shaders/water.frag");
    }
    catch (const std::exception &e)
    {
        qWarning("Water shader compile/link error: %s", e.what());
        m_progWater = 0;
    }

    // sky shader
    try
    {
        m_progSky = ShaderLoader::createShaderProgram(
            ":/resources/shaders/sky.vert",
            ":/resources/shaders/sky.frag");
    }
    catch (const std::exception &e)
    {
        qWarning("Sky shader compile/link error: %s", e.what());
        m_progSky = 0;
    }

    // use cube mesh as skybox
    m_skyCube = getOrCreateMesh(PrimitiveType::PRIMITIVE_CUBE, 1, 1);

    if (m_progTerrain)
    {
        std::vector<float> interlPNC = m_terrainGen.generateTerrain();
        m_terrainMesh.uploadinterleavedPNC(interlPNC);
        m_hasTerrain = true;

        // loading terrain textures
        m_texGrassAlbedo = loadTexture2D(":/resources/textures/terrain/grass/albedo.jpg", false);
        m_texRockAlbedo = loadTexture2D(":/resources/textures/terrain/rock_beach/albedo.jpg", false);
        m_texBeachAlbedo = loadTexture2D(":/resources/textures/terrain/beach/albedo.jpg", false);
        m_texRockHighAlbedo = loadTexture2D(":/resources/textures/terrain/rock/albedo.jpg", false);
        m_texSnowAlbedo = loadTexture2D(":/resources/textures/terrain/snow/albedo.jpg", false);
        m_texRockObjAlbedo = loadTexture2D("/resources/textures/terrain/rock_beach/displacement.jpg", false);

        m_texGrassNormal = loadTexture2D(":/resources/textures/terrain/grass/normal.jpg", false);
        m_texRockNormal = loadTexture2D(":/resources/textures/terrain/rock_beach/normal.jpg", false);
        m_texBeachNormal = loadTexture2D(":/resources/textures/terrain/beach/normal.jpg", false);
        m_texRockHighNormal = loadTexture2D(":/resources/textures/terrain/rock/normal.jpg", false);
        m_texWaterNormal = loadTexture2D(":/resources/textures/water_normal_tile.jpg", false);
        m_texSnowNormal = loadTexture2D(":/resources/textures/terrain/snow/normal.jpg", false);

        m_texGrassRough = loadTexture2D(":/resources/textures/terrain/grass/roughness.jpg", false);
        m_texRockRough = loadTexture2D(":/resources/textures/terrain/rock_beach/roughness.jpg", false);
        m_texBeachRough = loadTexture2D(":/resources/textures/terrain/beach/roughness.jpg", false);
        m_texRockHighRough  = loadTexture2D(":/resources/textures/terrain/rock/roughness.jpg", false);
        m_texSnowRough  = loadTexture2D(":/resources/textures/terrain/snow/roughness.jpg", false);
    } else {
        m_hasTerrain = false;
    }

    std::vector<float> lutData = LUTUtils::generateIdentityLUT(m_lutSize);
    m_texColorLUT = LUTUtils::createLUT3DTexture(m_lutSize, lutData);

    // z-up (lab07) -> y-up (project) : translate center, scale, rotate -90° around +X
    glm::mat4 T = glm::translate(glm::mat4(1.f), glm::vec3(-0.5f, -0.5f, 0.f));
    glm::mat4 S = glm::scale(glm::mat4(1.f), glm::vec3(120.f, 120.f, 10.f));
    glm::mat4 R = glm::rotate(glm::mat4(1.f),
                              -glm::half_pi<float>(), glm::vec3(1, 0, 0));
    m_terrainModel = R * S * T;

    // cylinder shared mesh for preparing branches
    m_treeCylinderMesh = getOrCreateMesh(PrimitiveType::PRIMITIVE_CYLINDER, 3, 8);

    // coarse sphere mesh for leaves
    m_leafMesh = getOrCreateMesh(PrimitiveType::PRIMITIVE_SPHERE, 3, 6);

    // rock mesh
    m_rockMesh = getOrCreateMesh(PrimitiveType::PRIMITIVE_SPHERE, 4, 8);

    m_drawForest = false; // off by default, controlled by EC4 checkbox.

    // instancing attribute for branches
    glBindVertexArray(m_treeCylinderMesh->vao);
    glGenBuffers(1, &m_branchInstanceVBO);
    glBindBuffer(GL_ARRAY_BUFFER, m_branchInstanceVBO);
    // fill data in later use buildingForest().

    // mat4 occupies 4 vec4 attributes: location 2, 3, 4, 5
    std::size_t vec4Size = sizeof(glm::vec4);
    GLsizei stride = sizeof(glm::mat4);
    for (int i = 0; i < 4; ++i)
    {
        GLuint loc = 2 + i;
        glEnableVertexAttribArray(loc);
        glVertexAttribPointer(loc, 4, GL_FLOAT, GL_FALSE, stride, (void *)(i * vec4Size));
        glVertexAttribDivisor(loc, 1); // one copy per instance
    }
    glBindVertexArray(0);

    // instancing attribute for leaves
    glBindVertexArray(m_leafMesh->vao);
    glGenBuffers(1, &m_leafInstanceVBO);
    glBindBuffer(GL_ARRAY_BUFFER, m_leafInstanceVBO);

    for (int i = 0; i < 4; ++i)
    {
        GLuint loc = 2 + i;
        glEnableVertexAttribArray(loc);
        glVertexAttribPointer(loc, 4, GL_FLOAT, GL_FALSE,
                              stride, (void *)(i * vec4Size));
        glVertexAttribDivisor(loc, 1);
    }
    glBindVertexArray(0);

    // instancing attribute for rocks
    glBindVertexArray(m_rockMesh->vao);
    glGenBuffers(1, &m_rockInstanceVBO);
    glBindBuffer(GL_ARRAY_BUFFER, m_rockInstanceVBO);

    for (int i = 0; i < 4; ++i)
    {
        GLuint loc = 2 + i;
        glEnableVertexAttribArray(loc);
        glVertexAttribPointer(loc, 4, GL_FLOAT, GL_FALSE,
                              stride, (void *)(i * vec4Size));
        glVertexAttribDivisor(loc, 1);
    }
    glBindVertexArray(0);

    // Camera initial values (will be overridden by scene & settings)
    m_cam.aspect = (height() > 0) ? float(width()) / float(height()) : 1.f;
    m_cam.nearP = settings.nearPlane;
    m_cam.farP = settings.farPlane;

    m_particleSystem = new ParticleSystem();
    m_particleSystem->init();

    // --- Camera Path Initialization ---
    // Define a simple circular path around the center
    // Keyframe 0: Start
    m_cameraPath.addKeyframe(glm::vec3(0, 10, 20), glm::quat(glm::vec3(glm::radians(-20.f), 0, 0)), 0.0f);
    // Keyframe 1: Left side
    m_cameraPath.addKeyframe(glm::vec3(-20, 15, 0), glm::quat(glm::vec3(glm::radians(-15.f), glm::radians(-90.f), 0)), 5.0f);
    // Keyframe 2: Back
    m_cameraPath.addKeyframe(glm::vec3(0, 20, -20), glm::quat(glm::vec3(glm::radians(-25.f), glm::radians(-180.f), 0)), 10.0f);
    // Keyframe 3: Right side
    m_cameraPath.addKeyframe(glm::vec3(20, 15, 0), glm::quat(glm::vec3(glm::radians(-15.f), glm::radians(-270.f), 0)), 15.0f);
    // Keyframe 4: Return to start
    m_cameraPath.addKeyframe(glm::vec3(0, 10, 20), glm::quat(glm::vec3(glm::radians(-20.f), glm::radians(-360.f), 0)), 20.0f);

    m_glInitialized = true;

    // post-processing setup
    try
    {
        m_progPost = ShaderLoader::createShaderProgram(
            ":/resources/shaders/post.vert",
            ":/resources/shaders/post.frag");
    }
    catch (const std::exception &e)
    {
        qWarning("Post shader compile/link error: %s", e.what());
        m_progPost = 0;
    }

    // fullscreen quad
    createScreenQuad();

    // initialize the scene FBO (current viewport size)
    GLint vp[4];
    glGetIntegerv(GL_VIEWPORT, vp);
    ensureSceneFBO(vp[2], vp[3]);

    // Initialize FBO dimensions
    m_fbo_width = m_sceneWidth * m_devicePixelRatio;
    m_fbo_height = m_sceneHeight * m_devicePixelRatio;

    // Create and initialize reflection FBO
    glGenTextures(1, &m_reflectionFBO_texture);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, m_reflectionFBO_texture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, m_fbo_width, m_fbo_height, 0,
                 GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glBindTexture(GL_TEXTURE_2D, 0);

    glGenRenderbuffers(1, &m_reflectionFBO_renderbuffer);
    glBindRenderbuffer(GL_RENDERBUFFER, m_reflectionFBO_renderbuffer);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8, m_fbo_width, m_fbo_height);
    glBindRenderbuffer(GL_RENDERBUFFER, 0);

    glGenFramebuffers(1, &m_reflectionFBO);
    glBindFramebuffer(GL_FRAMEBUFFER, m_reflectionFBO);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, m_reflectionFBO_texture, 0);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_RENDERBUFFER, m_reflectionFBO_renderbuffer);

    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
    {
        std::cerr << "Error: Reflection FBO is not complete!" << std::endl;
    }
    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    // Create and initialize refraction FBO
    glGenTextures(1, &m_refractionFBO_texture);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, m_refractionFBO_texture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, m_fbo_width, m_fbo_height, 0,
                 GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glBindTexture(GL_TEXTURE_2D, 0);

    // Create depth texture for refraction FBO
    glGenTextures(1, &m_refractionDepthTexture);
    glBindTexture(GL_TEXTURE_2D, m_refractionDepthTexture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT, m_fbo_width, m_fbo_height, 0,
                 GL_DEPTH_COMPONENT, GL_FLOAT, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glBindTexture(GL_TEXTURE_2D, 0);

    glGenFramebuffers(1, &m_refractionFBO);
    glBindFramebuffer(GL_FRAMEBUFFER, m_refractionFBO);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, m_refractionFBO_texture, 0);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, m_refractionDepthTexture, 0);

    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
    {
        std::cerr << "Error: Refraction FBO is not complete!" << std::endl;
    }
    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    m_texWaterNormal = loadTexture2D(":/resources/textures/normalMap.png", false);
    m_waterDUDVTexture = loadTexture2D(":/resources/textures/waterDUDV.png", false);
}

void Realtime::paintGL() {
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    if (!m_progTerrain || !m_progWater || !m_progSky) {
        // qWarning("No scene shader loaded");
        return;
    }

    GLint prevFBO = 0;
    glGetIntegerv(GL_FRAMEBUFFER_BINDING, &prevFBO);

    GLint vp[4];
    glGetIntegerv(GL_VIEWPORT, vp);
    int w = vp[2];
    int h = vp[3];
    if (w <= 0 || h <= 0)
    {
        w = width() * m_devicePixelRatio;
        h = height() * m_devicePixelRatio;
    }

    // If the post shader fails to compile: draw directly onto the screen.
    if (!m_progPost)
    {
        glBindFramebuffer(GL_FRAMEBUFFER, prevFBO);
        glViewport(0, 0, w, h);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        glEnable(GL_DEPTH_TEST);
        renderScene();
        return;
    }

    // Reflection & Refraction pass
    renderReflection();
    renderRefraction();

    // Scene pass: Draw to m_fboScene
    ensureSceneFBO(w, h);
    glBindFramebuffer(GL_FRAMEBUFFER, m_fboScene);
    glViewport(0, 0, w, h);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glEnable(GL_DEPTH_TEST);

    renderScene();
    renderWater();

    glBindFramebuffer(GL_FRAMEBUFFER, prevFBO);
    glViewport(0, 0, w, h);
    glDisable(GL_DEPTH_TEST);

    if (!m_progPost) {
        // fallback if shader failed
        return;
    }

    glUseProgram(m_progPost);

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, m_texSceneColor);
    glUniform1i(glGetUniformLocation(m_progPost, "uSceneColor"), 0);

    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, m_texSceneDepth);
    glUniform1i(glGetUniformLocation(m_progPost, "uSceneDepth"), 1);

    bool applyLUT = m_enableColorLUT && (m_texColorLUT > 0);

    glActiveTexture(GL_TEXTURE2);
    glBindTexture(GL_TEXTURE_3D, m_texColorLUT);
    glUniform1i(glGetUniformLocation(m_progPost, "uColorLUT"), 2);

    glUniform1i(glGetUniformLocation(m_progPost, "uEnableColorGrading"), applyLUT);

    glUniform1f(glGetUniformLocation(m_progPost, "uNear"), m_cam.nearP);
    glUniform1f(glGetUniformLocation(m_progPost, "uFar"), m_cam.farP);

    // Select the preset and exposure based on the two checkboxes in the UI
    int preset = settings.colorGradePreset; // 0 = none, 1 = cold, 3 = rainy
    float exposure = 0.0f;
    float strength = 0.0f; // color intensity

    if (preset == 1)
    {
        // Cold Blue Snow Mountain
        exposure = 0.18f;
        strength = 0.85f;
    }
    else if (preset == 3)
    {
        // Rainy
        exposure = -0.18f;
        strength = 0.90f;
    }
    else
    {
        // Default
        exposure = 0.0f;
        strength = 0.0f;
    }

    glUniform1f(glGetUniformLocation(m_progPost, "uExposure"), exposure);

    // Adjustable: neutral lift/gamma/gain specs (modified in shader post.frag)
    glUniform3f(glGetUniformLocation(m_progPost, "uLift"), 0.0f, 0.0f, 0.0f);
    glUniform3f(glGetUniformLocation(m_progPost, "uGamma"), 1.0f, 1.0f, 1.0f);
    glUniform3f(glGetUniformLocation(m_progPost, "uGain"), 1.0f, 1.0f, 1.0f);

    // Transmit preset + intensity
    glUniform1i(glGetUniformLocation(m_progPost, "uGradePreset"), preset);
    glUniform1f(glGetUniformLocation(m_progPost, "uGradeStrength"), strength);

    // Adjustable: default tint (modified in shader post.frag)
    glUniform3f(glGetUniformLocation(m_progPost, "uTint"), 1.0f, 1.0f, 1.0f);

    // Draw a full-screen quad, and output the processed result to prevFBO (screen or screenshot FBO).
    m_screenQuad.draw();

    glActiveTexture(GL_TEXTURE2); glBindTexture(GL_TEXTURE_3D, 0);
    glActiveTexture(GL_TEXTURE1); glBindTexture(GL_TEXTURE_2D, 0);
    glActiveTexture(GL_TEXTURE0); glBindTexture(GL_TEXTURE_2D, 0);
    glEnable(GL_DEPTH_TEST);
}

void Realtime::resizeGL(int w, int h)
{
    // Students: anything requiring OpenGL calls when the program starts should be done here

    // Tells OpenGL how big the screen is
    glViewport(0, 0, w * m_devicePixelRatio, h * m_devicePixelRatio);
    if (h > 0)
        m_cam.aspect = float(w) / float(h);
}

void Realtime::sceneChanged()
{
    makeCurrent();
    // Parse the current scene file into m_rd
    RenderData rd;
    if (!SceneParser::parse(settings.sceneFilePath, rd))
    {
        doneCurrent();
        update();
        return;
    }

    m_rd = std::move(rd);

    const auto &C = m_rd.cameraData; // has pos/look/up/heightAngle

    glm::vec3 pos = glm::vec3(C.pos);
    glm::vec3 look = glm::vec3(C.look);
    glm::vec3 up = glm::vec3(C.up);

    glm::vec3 f = glm::length(look) > EPS ? glm::normalize(look) : glm::vec3(0, 0, -1);
    glm::vec3 r = glm::length(glm::cross(f, up)) > EPS ? glm::normalize(glm::cross(f, up))
                                                       : glm::vec3(1, 0, 0);
    glm::vec3 u = glm::normalize(glm::cross(r, f));

    m_cam.eye = pos;
    m_cam.look = f;
    m_cam.up = u;
    m_cam.fovyRad = C.heightAngle;

    m_cam.aspect = (height() > 0) ? float(width()) / float(height()) : m_cam.aspect;

    // keep near/far editable from Settings (live updates)
    m_cam.nearP = std::max(EPS, settings.nearPlane);
    m_cam.farP = std::max(m_cam.nearP + EPS, settings.farPlane);

    doneCurrent();
    update(); // asks for a PaintGL() call to occur
}

void Realtime::settingsChanged()
{

    if (!m_glInitialized)
    {
        m_cam.nearP = std::max(EPS, settings.nearPlane);
        m_cam.farP = std::max(m_cam.nearP + EPS, settings.farPlane);
        return;
    }

    makeCurrent();

    // Update camera near/far immediately
    m_cam.nearP = std::max(EPS, settings.nearPlane);
    m_cam.farP = std::max(m_cam.nearP + EPS, settings.farPlane);

    // map UI -> Terrain Parameters
    TerrainGenerator::TerrainParams P;

    // P1: mountain roughness / frequency
    P.baseFreq = 0.25f * powf(2.f, (settings.shapeParameter1 - 5) / 3.f);

    // P2: mountian heights
    P.heightScale = 0.12f * settings.shapeParameter2;

    // P3: terrain distortion and river curvature (EC3 trigger)
    int s3 = glm::clamp(settings.shapeParameter3, 1, 5);
    float t3 = (s3 - 1) / 4.f; // 0..1

    // domain warping makes the terrain more "organic"
    P.warpStrength = glm::mix(0.10f, 0.45f, t3);

    // EC1 cliff, EC2 crater
    P.cliffSteps = settings.extraCredit1 ? 5 : 1;
    P.enableCraters = settings.extraCredit2;
    P.enableCraters = settings.extraCredit2;
    // Adjustable:
    if (P.enableCraters)
    {
        P.craterDensity = 4.0f; // slightly thinner
        P.craterRadius = 0.05f;
        P.craterDepth = 0.32f; // dig deeper -> bottom of the pit will be below sea level more
    }

    P.enableRivers = settings.extraCredit3;
    if (P.enableRivers)
    {
        // frequency: higher -> the more meandering the river.
        P.riverFreq = glm::mix(0.5f, 1.4f, t3);
        // ridged deg: greater -> sharper trough
        P.riverSharp = glm::mix(1.0f, 2.5f, t3);
        // threshold: the larger t3 is -> the wider the river.
        P.riverThresh = glm::mix(0.92f, 0.75f, t3);
        // depth
        P.riverDepth = glm::mix(0.04f, 0.18f, t3);
    }
    else
    {
        P.riverDepth = 0.0f;
    }

    // water level & overall offset
    P.seaLevel = -0.1f;
    P.oceanBias = 0.0f; // Aborted

    m_terrainParams = P;
    m_terrainGen.setParams(m_terrainParams);

    // calc. sea height / height under world scale for texture coloring
    m_seaHeightWorld = m_terrainParams.seaLevel * m_terrainParams.heightScale * 10.f;
    m_heightScaleWorld = m_terrainParams.heightScale * 10.f;

    std::vector<float> interlPNC = m_terrainGen.generateTerrain();
    m_terrainMesh.uploadinterleavedPNC(interlPNC);

    rebuildWaterMesh();

    m_drawForest = settings.extraCredit4;
    if (m_drawForest)
    {
        buildForest();
        buildRocks();
    }
    else
    {
        m_forestBranches.clear();
        m_rocks.clear();
        m_rockInstanceCount = 0;
    }

    doneCurrent();
    update(); // asks for a PaintGL() call to occur
}

// ================== Camera Movement!

void Realtime::keyPressEvent(QKeyEvent *event)
{
    if (event->key() == Qt::Key_P)
    {
        m_isPathAnimating = !m_isPathAnimating;
        if (m_isPathAnimating)
        {
            m_pathTimer.start();
        }
    }
    m_keyMap[Qt::Key(event->key())] = true;

    // Fog toggle
    if (event->key() == Qt::Key_F) {
        m_enableFog = !m_enableFog;
        update();
    }

    // Color LUT toggle
    if (event->key() == Qt::Key_L) {
        m_enableColorLUT = !m_enableColorLUT;
        update();
    }

    // LUT Preset 1: Warm/Golden
    if (event->key() == Qt::Key_1) {
        std::vector<float> lutData = LUTUtils::generateStyledLUT(m_lutSize, 1);
        glDeleteTextures(1, &m_texColorLUT);
        m_texColorLUT = LUTUtils::createLUT3DTexture(m_lutSize, lutData);
        update();
    }

    // LUT Preset 2: Cool/Blue
    if (event->key() == Qt::Key_2) {
        std::vector<float> lutData = LUTUtils::generateStyledLUT(m_lutSize, 2);
        glDeleteTextures(1, &m_texColorLUT);
        m_texColorLUT = LUTUtils::createLUT3DTexture(m_lutSize, lutData);
        update();
    }
}

void Realtime::keyReleaseEvent(QKeyEvent *event)
{
    m_keyMap[Qt::Key(event->key())] = false;
}

void Realtime::mousePressEvent(QMouseEvent *event)
{
    if (event->buttons().testFlag(Qt::LeftButton))
    {
        m_mouseDown = true;
        m_prev_mouse_pos = glm::vec2(event->position().x(), event->position().y());
    }
}

void Realtime::mouseReleaseEvent(QMouseEvent *event)
{
    if (!event->buttons().testFlag(Qt::LeftButton))
    {
        m_mouseDown = false;
    }
}

void Realtime::mouseMoveEvent(QMouseEvent *event)
{
    if (m_mouseDown)
    {
        int posX = event->position().x();
        int posY = event->position().y();
        int deltaX = posX - m_prev_mouse_pos.x;
        int deltaY = posY - m_prev_mouse_pos.y;
        m_prev_mouse_pos = glm::vec2(posX, posY);

        // Use deltaX and deltaY here to rotate
        // Rotation sensitivity: radians per pixel (tweak to taste)
        const float kSensitivity = 0.0035f;

        // Yaw: rotate around world +Y by -deltaX (right drag -> look to the right)
        if (deltaX != 0.0f)
        {
            m_cam.yaw(-deltaX * kSensitivity);
        }

        // Pitch: rotate around camera "right" by -deltaY (up drag -> look up)
        if (deltaY != 0.0f)
        {
            m_cam.pitch(-deltaY * kSensitivity);
        }

        update(); // asks for a PaintGL() call to occur
    }
}

void Realtime::timerEvent(QTimerEvent *event)
{
    int elapsedms = m_elapsedTimer.elapsed();
    float dt = elapsedms * 0.001f;
    m_elapsedTimer.restart();

    // Use deltaTime and m_keyMap here to move around
    // Clamp dt to avoid huge jumps if the app was paused
    dt = std::min(dt, 0.1f);

    m_time += dt; // water animation time var.

    if (m_isPathAnimating)
    {
        float t = m_pathTimer.elapsed() / 1000.0f;
        // Loop every 20 seconds
        t = fmod(t, 20.0f);

        CameraPath::Pose pose = m_cameraPath.evaluate(t);
        m_cam.eye = pose.position;
        m_cam.look = pose.rotation * glm::vec3(0, 0, -1);
        m_cam.up = pose.rotation * glm::vec3(0, 1, 0);

        update();
        return; // Skip manual control
    }

    // Target speed: 5 world-space units per second (spec requirement)
    const float speed = 5.0f;

    // Camera basis: forward (look), right (perpendicular to look & up), and world up
    glm::vec3 fwd = glm::normalize(m_cam.look);
    glm::vec3 right = glm::normalize(glm::cross(fwd, m_cam.up));
    const glm::vec3 worldUp(0.f, 1.f, 0.f);

    // Accumulate intent based on keys held
    glm::vec3 move(0.f);
    if (m_keyMap[Qt::Key_W])
        move += fwd; // forward
    if (m_keyMap[Qt::Key_S])
        move -= fwd; // backward
    if (m_keyMap[Qt::Key_D])
        move += right; // right
    if (m_keyMap[Qt::Key_A])
        move -= right; // left
    if (m_keyMap[Qt::Key_Space])
        move += worldUp; // up in world space
    if (m_keyMap[Qt::Key_Control])
        move -= worldUp; // down in world space

    // Normalize so diagonals are not faster, then apply speed and delta time
    if (glm::length2(move) > 0.f)
    {
        move = glm::normalize(move) * (speed * dt);
        m_cam.translateWorld(move);
    }

    // Update Particles
    if (m_particleSystem)
    {
        // Check settings for type change
        int targetType = -1;
        if (settings.colorGradePreset == 1)
            targetType = 0; // Snow
        else if (settings.colorGradePreset == 3)
            targetType = 1; // Rain

        // If settings imply a particle type, switch to it.
        // If settings is "None" (0) or "Cold" (1) or "Rainy" (3).
        // Let's say default (0) has no particles? Or maybe Snow?
        // The user asked for "Global Snow or Rain".
        // Let's map: 1 -> Snow, 3 -> Rain. Others -> Maybe Snow?

        if (targetType != -1 && targetType != m_currentParticleType)
        {
            m_currentParticleType = targetType;
            m_particleSystem->setType(m_currentParticleType);
        }

        m_particleSystem->update(dt);
    }

    update(); // asks for a PaintGL() call to occur
}

// DO NOT EDIT
void Realtime::saveViewportImage(std::string filePath)
{
    // Make sure we have the right context and everything has been drawn
    makeCurrent();

    int fixedWidth = 1024;
    int fixedHeight = 768;

    // Create Frame Buffer
    GLuint fbo;
    glGenFramebuffers(1, &fbo);
    glBindFramebuffer(GL_FRAMEBUFFER, fbo);

    // Create a color attachment texture
    GLuint texture;
    glGenTextures(1, &texture);
    glBindTexture(GL_TEXTURE_2D, texture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, fixedWidth, fixedHeight, 0, GL_RGB, GL_UNSIGNED_BYTE, NULL);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, texture, 0);

    // Optional: Create a depth buffer if your rendering uses depth testing
    GLuint rbo;
    glGenRenderbuffers(1, &rbo);
    glBindRenderbuffer(GL_RENDERBUFFER, rbo);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT, fixedWidth, fixedHeight);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, rbo);

    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
    {
        std::cerr << "Error: Framebuffer is not complete!" << std::endl;
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        return;
    }

    // Render to the FBO
    glBindFramebuffer(GL_FRAMEBUFFER, fbo);
    glViewport(0, 0, fixedWidth, fixedHeight);

    // Clear and render your scene here
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    paintGL();

    // Read pixels from framebuffer
    std::vector<unsigned char> pixels(fixedWidth * fixedHeight * 3);
    glReadPixels(0, 0, fixedWidth, fixedHeight, GL_RGB, GL_UNSIGNED_BYTE, pixels.data());

    // Unbind the framebuffer to return to default rendering to the screen
    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    // Convert to QImage
    QImage image(pixels.data(), fixedWidth, fixedHeight, QImage::Format_RGB888);
    // QImage flippedImage = image.mirrored(); // Flip the image vertically
    QImage flippedImage = image.flipped(Qt::Vertical); // Flip the image vertically

    // Save to file using Qt
    QString qFilePath = QString::fromStdString(filePath);
    if (!flippedImage.save(qFilePath))
    {
        std::cerr << "Failed to save image to " << filePath << std::endl;
    }

    // Clean up
    glDeleteTextures(1, &texture);
    glDeleteRenderbuffers(1, &rbo);
    glDeleteFramebuffers(1, &fbo);
}
