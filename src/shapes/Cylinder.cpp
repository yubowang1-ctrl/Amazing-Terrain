#include "Cylinder.h"
#include <glm/gtc/constants.hpp>
#include <algorithm>
#include <cmath>

// Small epsilon to detect the tip
constexpr float EPS = 1e-6f;

void Cylinder::updateParams(int param1, int param2) {
    // Clamp to avoid degeneracy (shapes must not disappear at low tessellation)
    m_param1 = std::max(1, param1); // at least one vertical band
    m_param2 = std::max(3, param2); // at least three radial wedges
    m_vertexData.clear();
    setVertexData();
}

static inline glm::vec3 cyl(float r, float y, float theta) {
    return { r * std::cos(theta), y, r * std::sin(theta) };
}


// Build one wedge: side strip + top cap ring sector + bottom cap ring sector

// Curved side: p1 vertical bands per wedge; each band emits two triangles
void Cylinder::makeSideStrip(float th0, float th1) {
    const int p1 = std::max(1, m_param1);
    const float dy = (m_yTop - m_yBot) / static_cast<float>(p1); //total height = 1

    // Precompute radial normals at the two wedge boundaries
    const glm::vec3 n0 = glm::normalize(glm::vec3(std::cos(th0), 0.f, std::sin(th0)));
    const glm::vec3 n1 = glm::normalize(glm::vec3(std::cos(th1), 0.f, std::sin(th1)));

    for (int i = 0; i < p1; ++i) {
        const float yTop = m_yTop - i * dy;
        const float yBot = m_yTop - (i + 1) * dy;

        // Four corners on the curved surface (th0 ... th1)
        glm::vec3 p00 = cyl(m_radius, yTop, th0); // top-left
        glm::vec3 p01 = cyl(m_radius, yTop, th1); // top-right
        glm::vec3 p10 = cyl(m_radius, yBot, th0); // bottom-left
        glm::vec3 p11 = cyl(m_radius, yBot, th1); // bottom-right

        // Radial normals per corner (smooth shading)
        glm::vec3 n00 = n0, n01 = n1, n10 = n0, n11 = n1;

        // Ensure CCW from outside: use a simple face-normal check
        const glm::vec3 e1 = p10 - p00;
        const glm::vec3 e2 = p01 - p00;
        glm::vec3 nface = glm::cross(e1, e2);

        // Average outward direction (approx radial) for this quad
        glm::vec3 navg = glm::normalize(glm::vec3(
            p00.x + p01.x + p10.x + p11.x,
            0.f,
            p00.z + p01.z + p10.z + p11.z
            ));

        // Security Check
        if (glm::dot(nface, navg) < 0.f) {
            // Flip diagonal to restore CCW winding
            std::swap(p01, p10);
            std::swap(n01, n10);
        }

        // Triangle 1: p00 -> p10 -> p01
        insertVec3(m_vertexData, p00); insertVec3(m_vertexData, n00);
        insertVec3(m_vertexData, p10); insertVec3(m_vertexData, n10);
        insertVec3(m_vertexData, p01); insertVec3(m_vertexData, n01);

        // Triangle 2: p10 -> p11 -> p01
        insertVec3(m_vertexData, p10); insertVec3(m_vertexData, n10);
        insertVec3(m_vertexData, p11); insertVec3(m_vertexData, n11);
        insertVec3(m_vertexData, p01); insertVec3(m_vertexData, n01);
    }
}

// Disk cap as concentric rings: p1 rings, each ring sector emits two triangles
void Cylinder::makeCapRing(bool isTop, float th0, float th1) {
    const int p1 = std::max(1, m_param1);
    const float y = isTop ? m_yTop : m_yBot;
    const glm::vec3 nCap = isTop ? glm::vec3(0, 1, 0) : glm::vec3(0, -1, 0);

    for (int i = 0; i < p1; ++i) {
        const float rInner = m_radius * (static_cast<float>(i)   / p1);
        const float rOuter = m_radius * (static_cast<float>(i+1) / p1);

        // handle degenracy
        if (rInner < EPS) {
            glm::vec3 center = glm::vec3(0.f, y, 0.f);
            glm::vec3 c10 = cyl(rOuter, y, th0);
            glm::vec3 c11 = cyl(rOuter, y, th1);

            glm::vec3 e1 = c10 - center;
            glm::vec3 e2 = c11 - center;
            glm::vec3 nface = glm::cross(e1, e2);
            if (glm::dot(nface, nCap) < 0.f) {
                std::swap(c10, c11);
            }

            insertVec3(m_vertexData, center);
            insertVec3(m_vertexData, nCap);
            insertVec3(m_vertexData, c10);
            insertVec3(m_vertexData, nCap);
            insertVec3(m_vertexData, c11);
            insertVec3(m_vertexData, nCap);
        }
        else {
            glm::vec3 c00 = cyl(rInner, y, th0); // inner at th0
            glm::vec3 c01 = cyl(rInner, y, th1); // inner at th1
            glm::vec3 c10 = cyl(rOuter, y, th0); // outer at th0
            glm::vec3 c11 = cyl(rOuter, y, th1); // outer at th1

            // Keep CCW from outside: face normal should point along nCap
            glm::vec3 e1 = c10 - c00;
            glm::vec3 e2 = c01 - c00;
            glm::vec3 nface = glm::cross(e1, e2);
            if (glm::dot(nface, nCap) < 0.f) {
                std::swap(c10, c01);
            }

            // Triangle 1: c00 -> c10 -> c01
            insertVec3(m_vertexData, c00); insertVec3(m_vertexData, nCap);
            insertVec3(m_vertexData, c10); insertVec3(m_vertexData, nCap);
            insertVec3(m_vertexData, c01); insertVec3(m_vertexData, nCap);

            // Triangle 2: c10 -> c11 -> c01
            insertVec3(m_vertexData, c10); insertVec3(m_vertexData, nCap);
            insertVec3(m_vertexData, c11); insertVec3(m_vertexData, nCap);
            insertVec3(m_vertexData, c01); insertVec3(m_vertexData, nCap);
        }
    }
}

void Cylinder::makeWedge(float th0, float th1) {
    makeSideStrip(th0, th1);
    // top cap
    makeCapRing(true,  th0, th1);
    // bottom cap
    makeCapRing(false, th0, th1);
}

void Cylinder::setVertexData() {
    // TODO for Project 5: Lights, Camera
    m_vertexData.clear();

    const int   p2 = std::max(3, m_param2);
    const float dth = glm::two_pi<float>() / static_cast<float>(p2);

    // Sweep wedges around the axis
    for (int k = 0; k < p2; ++k) {
        const float th0 = k * dth;
        const float th1 = (k + 1) * dth;
        makeWedge(th0, th1);
    }
}

// Inserts a glm::vec3 into a vector of floats.
// This will come in handy if you want to take advantage of vectors to build your shape!
void Cylinder::insertVec3(std::vector<float> &data, glm::vec3 v) {
    data.push_back(v.x);
    data.push_back(v.y);
    data.push_back(v.z);
}
