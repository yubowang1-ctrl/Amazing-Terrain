#pragma once
#include <vector>
#include <glm/glm.hpp>
#include <cstdint>
#include <functional>

struct VoxelChunk {
    // size
    int sx=64, sy=64, sz=64;
    glm::ivec3 origin{0,0,0};
    // params
    unsigned seed = 1230;
    int   octaves = 6;
    float baseFreq = 0.08f;
    float lacunarity = 2.0f;
    float gain = 0.5f;
    float ridgeExp = 2.0f;
    int   baseHeight = 16;
    int   heightAmp  = 24;

    std::vector<uint8_t> vox; // sx * sy * sz

    std::vector<float> build();

private:
    inline int idx(int x,int y,int z) const { return x + sx*(z + sz*y); }
    bool solid(int x,int y,int z) const {
        if (x<0||x>=sx||y<0||y>=sy||z<0||z>=sz) return false;
        return vox[idx(x,y,z)] != 0;
    }
    glm::vec2 randGrad(int gx,int gy) const;
    float perlin(float x,float y) const;
    float heightRidged(float x,float z) const;

    void emitFace(std::vector<float>& out,
                  glm::vec3 a, glm::vec3 b, glm::vec3 c, glm::vec3 d,
                  glm::vec3 n, glm::vec3 col);
};
