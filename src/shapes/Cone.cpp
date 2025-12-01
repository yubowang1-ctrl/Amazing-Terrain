#include "Cone.h"
#include <glm/gtc/constants.hpp>

namespace {
// Small epsilon to detect the tip
constexpr float EPS = 1e-6f;

// cylindrical => cartesian at a given y
inline glm::vec3 cyl(float r, float y, float theta) {
    return glm::vec3(r * std::cos(theta), y, r * std::sin(theta));
}

// radius on the cone as a function of y (tip at y=+0.5, base at y=-0.5, base R=0.5)
inline float radiusOfY(float y) {
    // Map y in [+0.5, -0.5] linearly to r in [0, 0.5]
    return 0.5f * (0.5f - y); // (0.5 - y) in [0,1]
}
} // namespace

void Cone::updateParams(int param1, int param2) {
    m_vertexData = std::vector<float>();
    m_param1 = std::max(1, param1);
    m_param2 = std::max(3, param2);
    setVertexData();
}

// provided stencil code
glm::vec3 Cone::calcNorm(glm::vec3& pt) {
    float xNorm = (2 * pt.x);
    float yNorm = -(1.f/4.f) * (2.f * pt.y - 1.f);
    float zNorm = (2 * pt.z);

    return glm::normalize(glm::vec3{ xNorm, yNorm, zNorm });
}

// Task 8: create function(s) to make tiles which you can call later on
// Note: Consider your makeTile() functions from Sphere and Cube
void Cone::makeCapSlice(float currentTheta, float nextTheta){
    // Task 8: create a slice of the cap face using your
    //         make tile function(s)
    // Note: think about how param 1 comes into play here!
    const int p1 = std::max(1, m_param1); // at least 1 radial band
    const float y = -0.5f;                // cap plane
    const glm::vec3 nCap(0.f, -1.f, 0.f); // outward normal of the cap

    for (int i = 0; i < p1; ++i) {
        float ri = (0.5f * i) / p1;   // inner radius of this band
        float ro = (0.5f * (i+1)) / p1;   // outer radius

        // handle degeneracy
        if (ri < EPS) {
            // Innermost circle: Triangles generated from the center point
            glm::vec3 center = glm::vec3(0.f, y, 0.f);  // Midpt
            glm::vec3 c10 = cyl(ro, y, currentTheta);
            glm::vec3 c11 = cyl(ro, y, nextTheta);

            // CCW winding
            glm::vec3 e1 = c10 - center;
            glm::vec3 e2 = c11 - center;
            glm::vec3 nface = glm::cross(e1, e2);
            if (glm::dot(nface, nCap) < 0.0f) {
                std::swap(c10, c11);
            }

            //Single triangle: center -> c10 -> c11
            insertVec3(m_vertexData, center);
            insertVec3(m_vertexData, nCap);
            insertVec3(m_vertexData, c10);
            insertVec3(m_vertexData, nCap);
            insertVec3(m_vertexData, c11);
            insertVec3(m_vertexData, nCap);
        } else {
            // Four corners on the cap ring sector
            glm::vec3 c00 = cyl(ri, y, currentTheta);
            glm::vec3 c01 = cyl(ri, y, nextTheta);
            glm::vec3 c10 = cyl(ro, y, currentTheta);
            glm::vec3 c11 = cyl(ro, y, nextTheta);

            // ensure CCW when viewed from -Y (outward normal is nCap = (0,-1,0))
            glm::vec3 e1 = c10 - c00;
            glm::vec3 e2 = c01 - c00;
            glm::vec3 nface = glm::cross(e1, e2);
            if (glm::dot(nface, nCap) < 0.0f) {
                // flip winding by swapping two opposite corners
                std::swap(c10, c01);
            }

            // Two triangles, CCW when viewed
            // tri1: c00 -> c10 -> c01
            insertVec3(m_vertexData, c00);
            insertVec3(m_vertexData, nCap);
            insertVec3(m_vertexData, c10);
            insertVec3(m_vertexData, nCap);
            insertVec3(m_vertexData, c01);
            insertVec3(m_vertexData, nCap);

            // tri2: c10 -> c11 -> c01
            insertVec3(m_vertexData, c10);
            insertVec3(m_vertexData, nCap);
            insertVec3(m_vertexData, c11);
            insertVec3(m_vertexData, nCap);
            insertVec3(m_vertexData, c01);
            insertVec3(m_vertexData, nCap);
        }
    }
}

void Cone::makeSlopeSlice(float currentTheta, float nextTheta){
    // Task 9: create a single sloped face using your make
    //         tile function(s)
    // Note: think about how param 1 comes into play here!
    const int p1 = std::max(1, m_param1); // at least 1 vertical band

    // y goes from +0.5 (tip) down to -0.5 (base)
    const float dy = 1.0f / p1; // total height is 1.0

    for (int i = 0; i < p1; ++i) {
        float yTop =  0.5f - i     * dy;  // upper edge of band
        float yBot =  0.5f - (i+1) * dy;  // lower edge of band

        float rTop = radiusOfY(yTop);
        float rBot = radiusOfY(yBot);

        // handle degeneracy at pin pt
        if (rTop < EPS) {
            glm::vec3 tip(0.f, yTop, 0.f);
            glm::vec3 p10 = cyl(rBot, yBot, currentTheta);
            glm::vec3 p11 = cyl(rBot, yBot, nextTheta);

            glm::vec3 n10 = Cone::calcNorm(p10);
            glm::vec3 n11 = Cone::calcNorm(p11);
            glm::vec3 nTip = glm::normalize((n10 + n11) / 2.f); // average then normalized

            // CCW
            glm::vec3 e1 = p10 - tip;
            glm::vec3 e2 = p11 - tip;
            glm::vec3 navg = glm::normalize(tip + p10 + p11);
            if (glm::dot(glm::cross(e1, e2), navg) < 0.0f) {
                std::swap(p10, p11);
                std::swap(n10, n11);
            }

            // Single triangle: tip -> p10 -> p11
            insertVec3(m_vertexData, tip);
            insertVec3(m_vertexData, nTip);
            insertVec3(m_vertexData, p10);
            insertVec3(m_vertexData, n10);
            insertVec3(m_vertexData, p11);
            insertVec3(m_vertexData, n11);
        } else {
            // Four corners on the sloped surface for this band & wedge
            glm::vec3 p00 = cyl(rTop, yTop, currentTheta); // "top-left"  (th = currentTheta)
            glm::vec3 p01 = cyl(rTop, yTop, nextTheta);    // "top-right" (th = nextTheta)
            glm::vec3 p10 = cyl(rBot, yBot, currentTheta); // "bottom-left"
            glm::vec3 p11 = cyl(rBot, yBot, nextTheta);    // "bottom-right"

            // estimate outward direction by averaging positions (cone centered at origin)
            glm::vec3 navg = glm::normalize(p00 + p01 + p10 + p11);

            // ensure CCW from outside
            glm::vec3 e1 = p10 - p00;
            glm::vec3 e2 = p01 - p00;
            glm::vec3 nface = glm::cross(e1, e2);
            if (glm::dot(nface, navg) < 0.0f) {
                std::swap(p01, p10);
            }

            // Compute base normals at the lower edge using implicit cone normal
            glm::vec3 n10 = Cone::calcNorm(p10);
            glm::vec3 n11 = Cone::calcNorm(p11);
            glm::vec3 n00 = Cone::calcNorm(p00);
            glm::vec3 n01 = Cone::calcNorm(p01);

            // Now emit two triangles for this slope tile (CCW when viewed from outside).
            // tri1: p00 -> p10 -> p01
            // tri1: p00 -> p10 -> p01
            insertVec3(m_vertexData, p00);
            insertVec3(m_vertexData, n00);
            insertVec3(m_vertexData, p10);
            insertVec3(m_vertexData, n10);
            insertVec3(m_vertexData, p01);
            insertVec3(m_vertexData, n01);

            // tri2: p10 -> p11 -> p01
            insertVec3(m_vertexData, p10);
            insertVec3(m_vertexData, n10);
            insertVec3(m_vertexData, p11);
            insertVec3(m_vertexData, n11);
            insertVec3(m_vertexData, p01);
            insertVec3(m_vertexData, n01);
        }
    }
}

void Cone::makeWedge(float currentTheta, float nextTheta) {
    // Task 10: create a single wedge of the Cone using the
    //          makeCapSlice() and makeSlopeSlice() functions you
    //          implemented in Task 5
    makeCapSlice(currentTheta, nextTheta);  // base (cap) sector
    makeSlopeSlice(currentTheta, nextTheta);  // lateral (slope) sector
}

void Cone::setVertexData() {
    // Task 10: create a full cone using the makeWedge() function you
    //          just implemented
    // Note: think about how param 2 comes into play here!
    m_vertexData.clear();

    const int p2 = std::max(3, m_param2);             // at least 3 wedges
    const float dtheta = glm::two_pi<float>() / p2;         // step over [0, 2π)

    for (int k = 0; k < p2; ++k) {
        float th0 = k * dtheta;   // left boundary of wedge k
        float th1 = (k + 1) * dtheta;   // right boundary of wedge k
        makeWedge(th0, th1);
    }
}

// Inserts a glm::vec3 into a vector of floats.
// This will come in handy if you want to take advantage of vectors to build your shape!
void Cone::insertVec3(std::vector<float> &data, glm::vec3 v) {
    data.push_back(v.x);
    data.push_back(v.y);
    data.push_back(v.z);
}
