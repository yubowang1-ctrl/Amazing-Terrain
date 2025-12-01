#include "Sphere.h"
#include <glm/gtc/constants.hpp>

void Sphere::updateParams(int param1, int param2) {
    m_vertexData = std::vector<float>();
    m_param1 = param1;
    m_param2 = param2;
    setVertexData();
}

void Sphere::makeTile(glm::vec3 topLeft,
                      glm::vec3 topRight,
                      glm::vec3 bottomLeft,
                      glm::vec3 bottomRight) {
    // Task 5: Implement the makeTile() function for a Sphere
    // Note: this function is very similar to the makeTile() function for Cube,
    //       but the normals are calculated in a different way!
    glm::vec3 e1 = bottomLeft - topLeft;
    glm::vec3 e2 = topRight   - topLeft;
    glm::vec3 n_face = glm::cross(e1, e2); // unnormalized face normal

    // For a sphere centered at the origin, averaging the four positions
    // points roughly outward.
    glm::vec3 navg = glm::normalize(topLeft + topRight + bottomLeft + bottomRight);

    // If the face normal points inward, flip the winding by swapping two corners.
    if (glm::dot(n_face, navg) < 0.0f) {
        std::swap(topRight, bottomLeft);
    }

    // tri1: topLeft -> bottomLeft -> topRight  (CCW when viewed from outside)
    insertVec3(m_vertexData, topLeft);
    insertVec3(m_vertexData, glm::normalize(topLeft));
    insertVec3(m_vertexData, bottomLeft);
    insertVec3(m_vertexData, glm::normalize(bottomLeft));
    insertVec3(m_vertexData, topRight);
    insertVec3(m_vertexData, glm::normalize(topRight));


    // tri2: topRight -> bottomLeft -> bottomRight
    insertVec3(m_vertexData, topRight);
    insertVec3(m_vertexData, glm::normalize(topRight));
    insertVec3(m_vertexData, bottomLeft);
    insertVec3(m_vertexData, glm::normalize(bottomLeft));
    insertVec3(m_vertexData, bottomRight);
    insertVec3(m_vertexData, glm::normalize(bottomRight));
}

void Sphere::makeWedge(float currentTheta, float nextTheta) {
    // Task 6: create a single wedge of the sphere using the
    //         makeTile() function you implemented in Task 5
    // Note: think about how param 1 comes into play here!
    const int   p1 = std::max(2, m_param1);      // at least 2 bands
    const float r  = 0.5f;                       // sphere radius
    const float dphi = glm::pi<float>() / p1;    // phi step over [0, pi]

    auto sph = [&](float phi, float theta) -> glm::vec3 {
        // UV-sphere parameterization (matches course convention).
        float x =  r * std::sin(phi) * std::cos(theta);
        float y =  r * std::cos(phi);
        float z = -r * std::sin(phi) * std::sin(theta); //note the minus on z
        return glm::vec3(x, y, z);
    };

    for (int i = 0; i < p1; ++i) {
        float phiTop =  i  * dphi;  // upper latitude
        float phiBot = (i + 1) * dphi;  // lower latitude

        // Four corners of the current latitudinal band within this wedge
        glm::vec3 topLeft     = sph(phiTop, currentTheta);
        glm::vec3 topRight    = sph(phiTop, nextTheta);
        glm::vec3 bottomLeft  = sph(phiBot, currentTheta);
        glm::vec3 bottomRight = sph(phiBot, nextTheta);

        // Delegate to makeTile (which also auto-fixes winding)
        makeTile(topLeft, topRight, bottomLeft, bottomRight);
    }
}

void Sphere::makeSphere() {
    // Task 7: create a full sphere using the makeWedge() function you
    //         implemented in Task 6
    // Note: think about how param 2 comes into play here!

    m_vertexData.clear();

    // Number of wedges (longitude divisions). Must be at least 3.
    const int p2 = std::max(3, m_param2);

    // Step in theta to sweep a full circle [0, 2π)
    const float dtheta = glm::two_pi<float>() / p2;

    // Stitch wedges around the Y axis
    for (int k = 0; k < p2; ++k) {
        float currentTheta = k * dtheta;         // left boundary of this wedge
        float nextTheta = (k + 1) * dtheta;   // right boundary of this wedge
        makeWedge(currentTheta, nextTheta);      // builds all latitudinal tiles in this wedge
    }
}

void Sphere::setVertexData() {
    // Uncomment these lines to make a wedge for Task 6, then comment them out for Task 7:
    // m_vertexData.clear();
    // float thetaStep = glm::radians(360.f / m_param2);
    // float currentTheta = 0 * thetaStep;
    // float nextTheta = 1 * thetaStep;
    // makeWedge(currentTheta, nextTheta);

    // Uncomment these lines to make sphere for Task 7:

    makeSphere();
}

// Inserts a glm::vec3 into a vector of floats.
// This will come in handy if you want to take advantage of vectors to build your shape!
void Sphere::insertVec3(std::vector<float> &data, glm::vec3 v) {
    data.push_back(v.x);
    data.push_back(v.y);
    data.push_back(v.z);
}
