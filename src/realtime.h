#pragma once

// Defined before including GLEW to suppress deprecation messages on macOS
#ifdef __APPLE__
#define GL_SILENCE_DEPRECATION
#endif
#include <GL/glew.h>
#include <glm/glm.hpp>

#include <unordered_map>
#include <QElapsedTimer>
#include <QOpenGLWidget>
#include <QTime>
#include <QTimer>

#include <unordered_map>
#include "utils/gl_mesh.h"
#include "utils/sceneparser.h"
#include "utils/shaderloader.h"  // shader program builder
#include "camera.h"        // Camera class (view/proj, yaw/pitch/move)

// #include "terrain/voxel_chunk.h"
#include "terrain/terraingenerator.h"
#include "vegetation/lsystem_tree.h"
#include "lut_utils.h"

class Realtime : public QOpenGLWidget
{
public:
    Realtime(QWidget *parent = nullptr);
    void finish();                                      // Called on program exit
    void sceneChanged();
    void settingsChanged();
    void saveViewportImage(std::string filePath);

public slots:
    void tick(QTimerEvent* event);                      // Called once per tick of m_timer

protected:
    void initializeGL() override;                       // Called once at the start of the program
    void paintGL() override;                            // Called whenever the OpenGL context changes or by an update() request
    void resizeGL(int width, int height) override;      // Called when window size changes

private:
    void keyPressEvent(QKeyEvent *event) override;
    void keyReleaseEvent(QKeyEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void timerEvent(QTimerEvent *event) override;

    // Tick Related Variables
    int m_timer;                                        // Stores timer which attempts to run ~60 times per second
    QElapsedTimer m_elapsedTimer;                       // Stores timer which keeps track of actual time between frames

    // Input Related Variables
    bool m_mouseDown = false;                           // Stores state of left mouse button
    glm::vec2 m_prev_mouse_pos;                         // Stores mouse position
    std::unordered_map<Qt::Key, bool> m_keyMap;         // Stores whether keys are pressed or not

    // Device Correction Variables
    double m_devicePixelRatio;

    // bool var states whether GL is initialized
    bool m_glInitialized = false;

    // Scene -> DrawList types

    // Minimal CPU-side material copied from scenefile
    struct MaterialCPU {
        glm::vec3 ka{0.f};
        glm::vec3 kd{1.f};
        glm::vec3 ks{0.f};
        float shininess = 32.f;

    };

    // One renderable instance: shared mesh + per-instance model/material
    struct DrawItem {
        GLMesh* mesh = nullptr; // shared geometry (from mesh cache)
        glm::mat4 model = glm::mat4(1.f);
        glm::mat3 normalMat = glm::mat3(1.f);
        MaterialCPU mat;

        // LOD related specss
        PrimitiveType type = PrimitiveType::PRIMITIVE_CUBE; // primitive kind
        int p1_base = 1;    // base tessellation copied from GUI at build time
        int p2_base = 3;
        int lodLevel = -1;  // cached distance band level
        float lastDist = -1.f;
    };

    // mesh cache key: (primitive type, p1, p2)
    struct MeshKey {
        int type = 0; // ScenePrimitive.type as int
        int p1 = 1; // vertical tessellation
        int p2 = 3; // radial tessellation
        bool operator==(const MeshKey& o) const {
            return type == o.type && p1 == o.p1 && p2 == o.p2;
        }
    };

    // provided quick serach "hack" for unordered_map
    struct MeshKeyHash {
        size_t operator()(const MeshKey& k) const {
            // simple mixed hash for small ints
            return ( (size_t)k.type << 24 ) ^ ( (size_t)k.p1 << 12 ) ^ (size_t)k.p2;
        }
    };

    // Runtime state
    GLuint m_prog = 0;   // shader program handle
    Camera m_cam;        // CPU-side camera (view/proj + motion)

    RenderData m_rd;     // parsed scene data (camera/global/lights/shapes)
    std::unordered_map<MeshKey, GLMesh, MeshKeyHash> m_meshCache; // shared geometries
    std::vector<DrawItem> m_drawList; // per-instance draw commands

    // terrain
    // reserved for infinite generation (if needed)
    // data for a single tile: its own mesh + model matrix
    struct TerrainTile {
        GLMesh   mesh;
        glm::mat4 model = glm::mat4(1.f);
    };

    GLMesh m_terrainMesh;
    GLuint m_progTerrain = 0;
    bool   m_hasTerrain  = false;
    bool   m_terrainWire = false;
    glm::mat4 m_terrainModel = glm::mat4(1.f); // single-block reference model matrix (R*S*T)
    TerrainGenerator m_terrainGen;

    float m_seaHeightWorld    = 0.f; // World Height at Sea Level
    float m_heightScaleWorld  = 1.f; // The current terrain's heightScale (world height)

    TerrainGenerator::TerrainParams m_terrainParams; // save the most recent setParams value

    // terrain textures
    GLuint m_texGrassAlbedo = 0;
    GLuint m_texRockAlbedo  = 0;
    GLuint m_texBeachAlbedo = 0;
    GLuint m_texRockHighAlbedo = 0;
    GLuint m_texSnowAlbedo = 0;

    // normal/disp
    GLuint m_texGrassNormal = 0;
    GLuint m_texRockNormal  = 0;
    GLuint m_texBeachNormal = 0;
    GLuint m_texRockHighNormal = 0;
    GLuint m_texSnowNormal = 0;

    GLuint m_texGrassRough  = 0;
    GLuint m_texRockRough  = 0;
    GLuint m_texBeachRough  = 0;
    GLuint m_texRockHighRough  = 0;
    GLuint m_texSnowRough = 0;


    // --- water ---
    GLMesh m_waterMesh;
    GLuint m_progWater      = 0;
    GLuint m_texWaterNormal = 0;
    float  m_time           = 0.f; // time used for rolling UV

    // fog
    bool m_enableFog = true;
    bool m_enableHeightFog = true;
    float m_fogDensity = 0.015f;
    float m_fogHeightFalloff = 0.08f;
    float m_fogStart = 2.0f;
    glm::vec3 m_fogColor = glm::vec3(0.7f, 0.75f, 0.8f);

    // LUT
    GLuint m_texColorLUT = 0;
    int    m_lutSize = 32;
    bool   m_enableColorLUT = false;
    int    m_lutPreset = 0;

    // skybox
    GLMesh* m_skyCube = nullptr;
    GLuint  m_progSky = 0;

    // --- Vegetation / L-system forest ---
    GLuint m_progForest = 0;
    GLMesh* m_treeCylinderMesh = nullptr; // shared cylinder geometry (from mesh cache)
    GLMesh *m_leafMesh = nullptr;
    bool m_drawForest = false;
    std::vector<BranchInstance> m_forestBranches; // all branch instances (including all trees)
    std::vector<glm::mat4> m_forestLeaves;

    // specs for branch / leave instance rendering
    GLuint  m_branchInstanceVBO = 0;
    GLuint  m_leafInstanceVBO   = 0;
    GLsizei m_branchInstanceCount = 0;
    GLsizei m_leafInstanceCount   = 0;

    // --- Post-processing / FBO ---
    GLuint m_fboScene        = 0;
    GLuint m_texSceneColor   = 0;
    GLuint m_texSceneDepth   = 0;
    int    m_sceneWidth      = 0;
    int    m_sceneHeight     = 0;

    // GLuint m_fboPingPong[2] = {0, 0};
    // GLuint m_texPingPong[2] = {0, 0};

    GLuint m_progPost = 0; // bus post-process shader
    GLMesh m_screenQuad; // full-screen triangles/quadrilaterals

    // helpers

    // Get or create a shared GLMesh for a primitive (by type + p1 + p2). Never duplicates buffers.
    GLMesh* getOrCreateMesh(const ScenePrimitive& prim, int p1, int p2);

    // Rebuild m_drawList from m_rd using current tessellation (p1,p2)
    void rebuildDrawListFromRenderData(int p1, int p2);

    // Upload lights from m_rd.lights into shader uniform arrays (uDirs/uPoints/uSpots)
    void uploadLights(GLuint prog, const std::vector<SceneLightData>& lights);

    // (Optional) clear all GL meshes in cache (used in finish() or when forcing rebuild)
    void destroyMeshCache();

    // (overload) fetch by PrimitiveType instead of full ScenePrimitive
    GLMesh* getOrCreateMesh(PrimitiveType type, int p1, int p2);

    void buildForest(); // Generate/Rebuild Forest

    GLuint loadTexture2D(const QString &path, bool srgb = false);

    void rebuildWaterMesh();

    void ensureSceneFBO(int w, int h);  // create/resize scene FBO （color+depth texture）
    void destroySceneFBO();

    void createScreenQuad();            // create [-1,1]^2 full-screen triangular grid
    void renderScene();

    void calculateFrustumCorners(glm::vec3 corners[4]) const;
};

