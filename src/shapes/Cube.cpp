#include "Cube.h"

void Cube::updateParams(int param1) {
    m_vertexData = std::vector<float>();
    m_param1 = param1;
    setVertexData();
}

void Cube::makeTile(glm::vec3 topLeft,
                    glm::vec3 topRight,
                    glm::vec3 bottomLeft,
                    glm::vec3 bottomRight) {
    // Task 2: create a tile (i.e. 2 triangles) based on 4 given points.

    // compute per-face normal from the two edge vector
    glm::vec3 e1 = bottomLeft - topLeft;
    glm::vec3 e2 = topRight - topLeft;
    glm::vec3 n = glm::normalize(glm::cross(e1, e2)); // outward normal

    // Triangle 1: topLeft -> bottomLeft -> topRight  (CCW w.r.t n)
    insertVec3(m_vertexData, topLeft); insertVec3(m_vertexData, n);
    insertVec3(m_vertexData, bottomLeft); insertVec3(m_vertexData, n);
    insertVec3(m_vertexData, topRight); insertVec3(m_vertexData, n);

    // Triangle 2: topRight -> bottomLeft -> bottomRight
    insertVec3(m_vertexData, topRight); insertVec3(m_vertexData, n);
    insertVec3(m_vertexData, bottomLeft); insertVec3(m_vertexData, n);
    insertVec3(m_vertexData, bottomRight); insertVec3(m_vertexData, n);

}

void Cube::makeFace(glm::vec3 topLeft,
                    glm::vec3 topRight,
                    glm::vec3 bottomLeft,
                    glm::vec3 bottomRight) {
    // Task 3: create a single side of the cube out of the 4
    //         given points and makeTile()
    // Note: think about how param 1 affects the number of triangles on
    //       the face of the cube

    m_param1 = std::max(1, m_param1);
    const int p = m_param1;

    auto lerp = [](const glm::vec3& a, const glm::vec3& b, float t){
        return a + t * (b - a);
    };

    auto bilerp = [&](float s, float t){
        glm::vec3 top = lerp(topLeft, topRight, s);
        glm::vec3 bot = lerp(bottomLeft, bottomRight, s);
        return lerp(top, bot, t);
    };

    for (int j = 0; j < p; ++j) {
        float t0 = static_cast<float>(j) / p; // up
        float t1 = static_cast<float>(j+1) / p; // down
        for (int i = 0; i < p; ++i) {
            float s0 = static_cast<float>(i) / p; // left
            float s1 = static_cast<float>(i+1) / p; // right

            glm::vec3 v00 = bilerp(s0, t0); // topLeft
            glm::vec3 v10 = bilerp(s1, t0); // topRight
            glm::vec3 v01 = bilerp(s0, t1); // bottomLeft
            glm::vec3 v11 = bilerp(s1, t1); // bottomRight

            makeTile(v00, v10, v01, v11);
        }
    }

}

void Cube::setVertexData() {
    // Uncomment these lines for Task 2, then comment them out for Task 3:

    // makeTile(glm::vec3(-0.5f,  0.5f, 0.5f),
    //          glm::vec3( 0.5f,  0.5f, 0.5f),
    //          glm::vec3(-0.5f, -0.5f, 0.5f),
    //          glm::vec3( 0.5f, -0.5f, 0.5f));

    // Uncomment these lines for Task 3:

    // // Face +Z (front): viewer in +Z looking toward origin
    // makeFace(glm::vec3(-0.5f,  0.5f, 0.5f),
    //          glm::vec3( 0.5f,  0.5f, 0.5f),
    //          glm::vec3(-0.5f, -0.5f, 0.5f),
    //          glm::vec3( 0.5f, -0.5f, 0.5f));

    // Task 4: Use the makeFace() function to make all 6 sides of the cube

    // Face +Z (front): viewer in +Z looking toward origin
    makeFace(glm::vec3(-0.5f,  0.5f,  0.5f), // topLeft
        glm::vec3( 0.5f,  0.5f,  0.5f), // topRight
        glm::vec3(-0.5f, -0.5f,  0.5f), // bottomLeft
        glm::vec3( 0.5f, -0.5f,  0.5f)); // bottomRight

    // Face -Z (back): viewer in -Z looking toward origin
    makeFace(glm::vec3( 0.5f,  0.5f, -0.5f), // topLeft
        glm::vec3(-0.5f,  0.5f, -0.5f), // topRight
        glm::vec3( 0.5f, -0.5f, -0.5f), // bottomLeft
        glm::vec3(-0.5f, -0.5f, -0.5f)); // bottomRight

    // Face +X (right): viewer in +X looking toward origin
    makeFace(glm::vec3( 0.5f,  0.5f,  0.5f), // topLeft
        glm::vec3( 0.5f,  0.5f, -0.5f), // topRight
        glm::vec3( 0.5f, -0.5f,  0.5f), // bottomLeft
        glm::vec3( 0.5f, -0.5f, -0.5f)); // bottomRight

    // Face -X (left): viewer in -X looking toward origin
    makeFace(glm::vec3(-0.5f,  0.5f, -0.5f), // topLeft
        glm::vec3(-0.5f,  0.5f,  0.5f), // topRight
        glm::vec3(-0.5f, -0.5f, -0.5f), // bottomLeft
        glm::vec3(-0.5f, -0.5f,  0.5f)); // bottomRight

    // Face +Y (top): viewer in +Y looking down toward origin
    makeFace(glm::vec3(-0.5f,  0.5f, -0.5f), // topLeft
        glm::vec3( 0.5f,  0.5f, -0.5f), // topRight
        glm::vec3(-0.5f,  0.5f,  0.5f), // bottomLeft
        glm::vec3( 0.5f,  0.5f,  0.5f)); // bottomRight

    // Face -Y (bottom): viewer in -Y looking up toward origin
    makeFace(glm::vec3(-0.5f, -0.5f,  0.5f), // topLeft
        glm::vec3( 0.5f, -0.5f,  0.5f), // topRight
        glm::vec3(-0.5f, -0.5f, -0.5f), // bottomLeft
        glm::vec3( 0.5f, -0.5f, -0.5f)); // bottomRight

}

// Inserts a glm::vec3 into a vector of floats.
// This will come in handy if you want to take advantage of vectors to build your shape!
void Cube::insertVec3(std::vector<float> &data, glm::vec3 v) {
    data.push_back(v.x);
    data.push_back(v.y);
    data.push_back(v.z);
}
