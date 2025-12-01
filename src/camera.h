#pragma once
#include <glm/glm.hpp>

class Camera {
public:
    // Public state
    glm::vec3 eye  {0.f, 0.f, 5.f};   // camera position
    glm::vec3 look {0.f, 0.f,-1.f};   // forward (view) direction, normalized
    glm::vec3 up   {0.f, 1.f, 0.f};   // up vector, approximately orthonormal to 'look'

    float fovyRad = glm::radians(45.f); // vertical field of view (radians)
    float aspect  = 4.f / 3.f;          // width / height
    float nearP   = 0.1f;               // near plane (> 0)
    float farP    = 100.f;              // far  plane (> near)

    // Build view (lookAt) matrix
    glm::mat4 view() const;

    // Build OpenGL-style perspective matrix (z_NDC in [-1, 1])
    glm::mat4 proj() const;

    // Camera motion helpers
    void translateWorld(const glm::vec3& d);  // translate in world space
    void yaw(float radians);                  // rotate around world +Y (heading)
    void pitch(float radians);                // rotate around camera right

private:
    // Rotate vector v around a (unit) axis by rad (Rodrigues' formula)
    static glm::vec3 rotateAxis(const glm::vec3& v, const glm::vec3& axis, float rad);

    // Decomposed perspective building blocks
    glm::mat4 makeScaleSxyz(float fovyRad, float aspect) const;  // p.21–25
    glm::mat4 makeUnhinge(float nearP, float farP) const;        // p.31–34
    glm::mat4 makeOpenGLZFix() const;                            // p.39

    // Build a 3x3 rotation matrix from a normalized axis and angle (Rodrigues' formula)
    static glm::mat3 makeAxisAngleMat3(const glm::vec3& axis, float radians);
};
