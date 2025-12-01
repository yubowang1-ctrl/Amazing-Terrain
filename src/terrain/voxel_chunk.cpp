#include "voxel_chunk.h"
#include <cmath>
#include <array>

glm::vec2 VoxelChunk::randGrad(int gx, int gy) const {
    std::hash<int> H;
    int h = H(gx * 41 + gy * 43 + int(seed)) & 1023; // 1024
    float t = (h / 1024.f) * 6.2831853f; // [0,2pi)
    return {std::cos(t), std::sin(t)};
}
static inline float smooth3(float t){ t=glm::clamp(t,0.f,1.f); return t*t*(3.f-2.f*t); }

float VoxelChunk::perlin(float x, float y) const {
    int x0 = (int)std::floor(x), y0 = (int)std::floor(y);
    int x1 = x0 + 1, y1 = y0 + 1;
    glm::vec2 dTL(x - x0, y - y1);
    glm::vec2 dTR(x - x1, y - y1);
    glm::vec2 dBR(x - x1, y - y0);
    glm::vec2 dBL(x - x0, y - y0);
    float A = glm::dot(randGrad(x0,y1), dTL);
    float B = glm::dot(randGrad(x1,y1), dTR);
    float C = glm::dot(randGrad(x1,y0), dBR);
    float D = glm::dot(randGrad(x0,y0), dBL);
    float u = x - x0, v = y - y0;
    float bottom = D + smooth3(u) * (C - D);
    float top    = A + smooth3(u) * (B - A);
    return bottom + smooth3(v) * (top - bottom);
}

float VoxelChunk::heightRidged(float x, float z) const {
    float freq = baseFreq, amp = 1.0f, h = 0.0f;
    for (int i=0;i<octaves;i++){
        float n = perlin(x * freq, z * freq);
        float r = 1.f - std::fabs(n);
        r = std::pow(glm::clamp(r,0.f,1.f), ridgeExp);
        h += amp * r;
        freq *= lacunarity;
        amp  *= gain;
    }
    return float(baseHeight) + float(heightAmp) * h;
}

void VoxelChunk::emitFace(std::vector<float>& out,
                          glm::vec3 a, glm::vec3 b, glm::vec3 c, glm::vec3 d,
                          glm::vec3 n, glm::vec3 col){
    auto put=[&](glm::vec3 p){
        out.insert(out.end(), {p.x,p.y,p.z, n.x,n.y,n.z, col.r,col.g,col.b});
    };
    put(a); put(b); put(c);
    put(a); put(c); put(d);
}

std::vector<float> VoxelChunk::build(){
    vox.assign(size_t(sx)*sy*sz, 0);

    // 1) AIR=0, DIRT=1, GRASS=2
    for (int x=0;x<sx;x++){
        for (int z=0;z<sz;z++){
            float wx = float(origin.x + x);
            float wz = float(origin.z + z);
            int h = int(std::floor(heightRidged(wx, wz)));
            h = std::max(0, std::min(h, sy-1));
            for (int y=0; y<=h; ++y){
                vox[idx(x,y,z)] = (y==h) ? 2 : 1;
            }
        }
    }

    std::vector<float> interl; interl.reserve(size_t(sx)*sy*sz * 6 * 6 * 9 / 4);
    const glm::vec3 GRASS(0.21f, 0.85f, 0.21f);
    const glm::vec3 DIRT (0.55f, 0.36f, 0.16f);

    auto blockColor=[&](int x,int y,int z){
        return (vox[idx(x,y,z)]==2)? GRASS : DIRT;
    };

    for (int x=0;x<sx;x++)for(int y=0;y<sy;y++)for(int z=0;z<sz;z++){
        if (!solid(x,y,z)) continue;
        glm::vec3 col = blockColor(x,y,z);
        float cx = float(origin.x + x) + 0.5f;
        float cy = float(origin.y + y) + 0.5f;
        float cz = float(origin.z + z) + 0.5f;
        // (+Y)
        if (!solid(x, y+1, z)) {
            glm::vec3 n(0,1,0);
            glm::vec3 a(cx-0.5f, cy+0.5f, cz-0.5f);
            glm::vec3 b(cx+0.5f, cy+0.5f, cz-0.5f);
            glm::vec3 c(cx+0.5f, cy+0.5f, cz+0.5f);
            glm::vec3 d(cx-0.5f, cy+0.5f, cz+0.5f);
            emitFace(interl,a,b,c,d,n, col);
        }
        // (-Y)
        if (!solid(x, y-1, z)) {
            glm::vec3 n(0,-1,0);
            glm::vec3 a(cx-0.5f, cy-0.5f, cz+0.5f);
            glm::vec3 b(cx+0.5f, cy-0.5f, cz+0.5f);
            glm::vec3 c(cx+0.5f, cy-0.5f, cz-0.5f);
            glm::vec3 d(cx-0.5f, cy-0.5f, cz-0.5f);
            emitFace(interl,a,b,c,d,n, DIRT);
        }
        // -X
        if (!solid(x-1, y, z)) {
            glm::vec3 n(-1,0,0);
            glm::vec3 a(cx-0.5f, cy+0.5f, cz+0.5f);
            glm::vec3 b(cx-0.5f, cy+0.5f, cz-0.5f);
            glm::vec3 c(cx-0.5f, cy-0.5f, cz-0.5f);
            glm::vec3 d(cx-0.5f, cy-0.5f, cz+0.5f);
            emitFace(interl,a,b,c,d,n, DIRT);
        }
        // +X
        if (!solid(x+1, y, z)) {
            glm::vec3 n(1,0,0);
            glm::vec3 a(cx+0.5f, cy+0.5f, cz-0.5f);
            glm::vec3 b(cx+0.5f, cy+0.5f, cz+0.5f);
            glm::vec3 c(cx+0.5f, cy-0.5f, cz+0.5f);
            glm::vec3 d(cx+0.5f, cy-0.5f, cz-0.5f);
            emitFace(interl,a,b,c,d,n, DIRT);
        }
        // -Z
        if (!solid(x, y, z-1)) {
            glm::vec3 n(0,0,-1);
            glm::vec3 a(cx+0.5f, cy+0.5f, cz-0.5f);
            glm::vec3 b(cx-0.5f, cy+0.5f, cz-0.5f);
            glm::vec3 c(cx-0.5f, cy-0.5f, cz-0.5f);
            glm::vec3 d(cx+0.5f, cy-0.5f, cz-0.5f);
            emitFace(interl,a,b,c,d,n, DIRT);
        }
        // +Z
        if (!solid(x, y, z+1)) {
            glm::vec3 n(0,0,1);
            glm::vec3 a(cx-0.5f, cy+0.5f, cz+0.5f);
            glm::vec3 b(cx+0.5f, cy+0.5f, cz+0.5f);
            glm::vec3 c(cx+0.5f, cy-0.5f, cz+0.5f);
            glm::vec3 d(cx-0.5f, cy-0.5f, cz+0.5f);
            emitFace(interl,a,b,c,d,n, DIRT);
        }
    }
    return interl;
}
