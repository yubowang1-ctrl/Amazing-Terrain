# CS 1230 Project 6: Final Project Gear Up

## Project Overview

This project extends the Realtime rendering pipeline to create a procedurally generated natural landscape featuring terrain (refined on Lab07 stencil code) with multi-layer PBR texturing, animated water, procedural vegetation using L-Systems, and atmospheric fog effects.

## Testing

---

## Implemented Features

| Feature | Difficulty | Points |
|---------|------------|--------|
| L-Systems | ⭐⭐ | 40 |
| Instanced Rendering | ⭐ | 20 |
| Normal Mapping | ⭐⭐ | 40 |
| Scrolling Textures | ⭐ | 20 |
| Realtime Fog | ⭐ | 20 |
| **Total** | | **140** |

---

## Design Choices

### 1. L-Systems (40 pts)

**Files:** `src/vegetation/lsystem_tree.cpp`, `src/vegetation/lsystem_tree.h`

#### Grammar & Rewriting

We implement a parametric L-System with the following alphabet:

| Symbol | Action |
|--------|--------|
| `F` | Move forward, draw branch segment |
| `X` | Growth node (emits leaf cluster) |
| `+`/`-` | Yaw rotation (±baseAngle with jitter) |
| `&`/`^` | Pitch rotation |
| `[`/`]` | Push/pop turtle state |

Three randomized production rules for variety:
```
Axiom: X
X → F[+FX][-FX][&FX][^FX]FX      (symmetric 4-way branching)
X → F[+F&X][-F^X][+FX][&FX]X     (asymmetric mixed)
X → F[+FX[&X]][-FX[^X]][&FX[+X]][^FX[-X]]X   (nested sub-branches)
F → FF
```

Each tree randomly selects one X-rule, creating diverse canopy shapes across the forest.

#### Turtle Interpreter

The `interpret()` function traverses the L-string maintaining turtle state `(pos, forward, up, right, radius)`. Key design decisions:

1. **Branch Geometry:** Each `F` generates a cylinder via `segmentMatrix()`, computing a model matrix that aligns the unit cylinder to segment endpoints. A 1.05× overlap factor prevents visible gaps.

2. **Radius Decay:** On `[`, radius multiplies by `radiusDecay` (default 0.75), creating natural tapering from trunk to twigs.

3. **Tropism:** Fine branches (radius < 0.7× base) bend slightly toward world +Y, simulating phototropism:
   ```cpp
   float k = 0.05f * (1.0f - rNorm);
   t.forward = normalize(t.forward + vec3(0,1,0) * k);
   ```

4. **Roll Randomization:** On `[`, a random roll around the forward axis (±angleJitter×0.7) breaks coplanar branching for more natural 3D structure.

5. **Leaf Clusters:** `emitLeafCluster()` first generates a short randomized twig, then scatters 26-58 ellipsoidal leaves around its endpoint. Leaf count scales with `leafDensity` parameter and inversely with branch thickness.

#### Forest Generation (`buildForest`)

Trees are placed in clusters with biome-aware constraints:

1. **Three UI Sliders:**
   - **Parameter 4 (s4):** Vegetation coverage — controls cluster count (52–172) and cluster radius
   - **Parameter 5 (s5):** Tree size — controls step length, base radius, iteration count (2–3)
   - **Parameter 6 (s6):** Leaf density — scales leaf count per cluster (0.5×–2.0×)

2. **Placement Logic:**
   - Cluster centers randomly sampled on terrain
   - Only "grassland" biome receives trees; cliffs and snow peaks excluded

3. **Per-Tree Randomization:**
   - Step length: ±30% variation scaled by size slider
   - Base angle: ±6° jitter
   - Radius decay: randomized in [0.6, 0.95]
   - Grammar rule: randomly selected from 3 variants

4. **Instance Limits:** 800,000 branches / 1,600,000 leaves max to prevent GPU memory overflow.

---

### 2. Instanced Rendering (20 pts)

**Files:** `realtime.cpp`, `shaders/forest.vert`

#### Motivation

A single tree generates 50-200 branch segments. With 50+ trees, naive rendering would require 5000+ draw calls per frame, causing severe CPU bottleneck.

#### Implementation

1. **Instance Buffer Setup:**
   ```cpp
   // mat4 occupies attribute locations 2,3,4,5
   for (int i = 0; i < 4; ++i) {
       glEnableVertexAttribArray(2 + i);
       glVertexAttribPointer(2 + i, 4, GL_FLOAT, GL_FALSE,
                             sizeof(glm::mat4), (void*)(i * sizeof(glm::vec4)));
       glVertexAttribDivisor(2 + i, 1);
   }
   ```

2. **Vertex Shader:**
   ```glsl
   layout(location = 2) in mat4 aModel;
   void main() {
       vec4 world = aModel * vec4(a_pos, 1.0);
       mat3 normalMat = mat3(transpose(inverse(aModel)));
       v_worldNormal = normalize(normalMat * a_nor);
       gl_Position = uProj * uView * world;
   }
   ```

3. **Draw Call:** Single `glDrawArraysInstanced()` renders all branches, then all leaves.

#### Shared Geometry

All branches share one cylinder mesh; all leaves share one sphere mesh. This minimizes VRAM usage while supporting thousands of instances at 60 FPS.

---

### 3. Normal Mapping (40 pts)

**Files:** `shaders/terrain.frag`, `shaders/water.frag`

#### TBN Construction

We construct tangent-space basis from the geometric normal:
```glsl
vec3 T = normalize(cross(up, N_geom));
vec3 B = normalize(cross(N_geom, T));
mat3 TBN = mat3(T, B, N_geom);
vec3 nWorld = normalize(TBN * (texture(normalMap, uv).xyz * 2.0 - 1.0));
```

#### Multi-Layer Terrain

Five material layers with full PBR texture sets (albedo, normal, roughness):

| Layer | Condition |
|-------|-----------|
| Grass | Low-mid elevation, flat |
| Rock (low) | Near water, steep slopes |
| Rock (high) | High elevation, steep |
| Beach | Water edge, flat |
| Snow | High elevation, relatively flat |

Blending weights computed from normalized height `hNorm` and slope `(1 - dot(N, up))`, with `smoothstep` transitions.

#### Tri-Planar Projection

Steep surfaces use tri-planar mapping to avoid UV stretching:
```glsl
vec3 n = abs(normal); n /= (n.x + n.y + n.z);
return texture(tex, pos.zy).rgb * n.x +
       texture(tex, pos.xz).rgb * n.y +
       texture(tex, pos.xy).rgb * n.z;
```

---

### 4. Scrolling Textures (20 pts)

**Files:** `shaders/water.frag`, `realtime.cpp`

#### Time Accumulation
```cpp
void Realtime::timerEvent(QTimerEvent*) {
    float dt = m_elapsedTimer.elapsed() * 0.001f;
    m_time += dt;
}
```

#### UV Animation
```glsl
vec2 uv = v_uv * uTiling + uScrollDir * (uTime * uScrollSpeed);
vec3 nTS = texture(uNormalMap, uv).xyz * 2.0 - 1.0;
```

Parameters: `uTiling=3.0`, `uScrollSpeed=0.05`, `uScrollDir=(1.0, 0.3)`.

Only the normal map scrolls—base color remains static—creating subtle wave motion without obvious texture sliding.

---

### 5. Realtime Fog (20 pts)

**Files:** `shaders/terrain.frag`, `shaders/forest.frag`

#### Exponential Distance Fog
```glsl
float dist = length(uEye - v_worldPos);
float fog = 1.0 - exp(-uFogDensity * dist);
color = mix(color, uFogColor, clamp(fog, 0.0, 1.0));
```

Fog color `(0.55, 0.70, 0.90)` matches the sky horizon, creating seamless atmospheric perspective. Density `0.02` provides gradual falloff from ~20 to ~100 world units.

---

## UI Controls

| Slider | Effect |
|--------|--------|
| Parameter 1 | Terrain frequency (low = smooth, high = rugged) |
| Parameter 2 | Terrain height scale |
| Parameter 3 | Domain warping strength; river parameters when EC3 enabled |
| Parameter 4 | Vegetation Coverage |
| Parameter 5 | Tree size and complexity |
| Parameter 6 | Leaf density per tree |

| Checkbox | Effect |
|----------|--------|
| EC1 | Terrain terracing (cliff steps) |
| EC2 | Crater generation |
| EC3 | Ridged-noise river valleys |
| EC4 | L-System forest rendering |

---

## Collaboration / References

### Code References

- **L-Systems:** [shortstheory/l-systems-opengl](https://github.com/shortstheory/l-systems-opengl) 
  — Referenced for L-System grammar design and turtle interpreter structure.
- **Procedural Terrain:** [AntonHakansson/procedural-terrain](https://github.com/AntonHakansson/procedural-terrain) 
  — Referenced for terrain generation techniques and multi-layer texturing approach.

### Learning Resources

- **L-Systems:** [The Algorithmic Beauty of Plants](http://algorithmicbotany.org/papers/#abop), [Wikipedia](https://en.wikipedia.org/wiki/L-system)
- **Instanced Rendering:** [LearnOpenGL - Instancing](https://learnopengl.com/Advanced-OpenGL/Instancing)
- **Normal Mapping:** [LearnOpenGL - Normal Mapping](https://learnopengl.com/Advanced-Lighting/Normal-Mapping)
- **Tri-Planar Mapping:** [Martin Palko's Guide](https://www.martinpalko.com/triplanar-mapping/)
- **Procedural Terrain:** [Inigo Quilez - fBm](https://iquilezles.org/articles/fbm/), [GPU Gems Ch.1](https://developer.nvidia.com/gpugems/gpugems3/part-i-geometry/chapter-1-generating-complex-procedural-terrains-using-gpu)
- **Fog:** [LearnOpenGL - Fog](https://learnopengl.com/Advanced-OpenGL/Fog)

### Textures

1. Some Terrain PBR textures sourced from [ambientCG](https://ambientcg.com/) (CC0 license).
2. Some Terrain PBR texture sets sourced from 
[AntonHakansson/procedural-terrain](https://github.com/AntonHakansson/procedural-terrain/tree/main).

## File Structure

```
├── resources/
│   ├── shaders/                    # GLSL shader source files
│   │   ├── terrain.vert/frag       # Multi-layer PBR terrain
│   │   ├── water.vert/frag         # Animated water surface
│   │   ├── forest.vert/frag        # Instanced vegetation
│   │   ├── sky.vert/frag           # Procedural sky gradient
│   │   └── default.vert/frag       # Basic Phong shader
│   └── textures/                   # PBR texture sets
│
└── src/
    ├── shapes/                     # Primitive tessellation (from Project 5)
    ├── terrain/
    │   ├── terraingenerator.cpp/h  # Procedural terrain (fBm, rivers, craters)
    │   └── voxel_chunk.cpp/h       # (Unused) voxel terrain experiment
    ├── utils/
    │   ├── aspectratiowidget/      # Qt widget utility
    │   ├── gl_mesh.h               # GLMesh wrapper with instanced draw
    │   ├── sceneparser.cpp/h       # Scene file parsing
    │   └── shaderloader.h          # Shader compilation utilities
    ├── vegetation/
    │   └── lsystem_tree.cpp/h      # L-System grammar & turtle interpreter
    ├── camera.cpp/h                # Camera controls (view/proj, movement)
    ├── mainwindow.cpp/h            # Qt main window & UI connections
    └── realtime.cpp/h              # Main render loop, initialization
```
