#include "camera.h"
#include <glm/gtc/constants.hpp>
#include <algorithm>   // <-- needed for std::max
#include <cmath>

namespace{
constexpr float EPS = 1e-6f;
}

// Build view matrix equivalent to glm::lookAt(eye, eye + look, up)
glm::mat4 Camera::view() const {
    // Gram-Schmidt Normalization
    glm::vec3 w = glm::normalize(-look);
    glm::vec3 v = glm::normalize(up - glm::dot(up, w) * w);
    glm::vec3 u = glm::cross(v, w);

    return glm::mat4(
        u.x, v.x, w.x, 0.0f,
        u.y, v.y, w.y, 0.0f,
        u.z, v.z, w.z, 0.0f,
        -glm::dot(u, eye), -glm::dot(v, eye), -glm::dot(w, eye), 1.0f
    );
}


glm::mat4 Camera::makeScaleSxyz(float fovy, float aspect_) const {
    // Note: tan(θw/2) = aspect * tan(θh/2)
    float f = std::max(farP, nearP + EPS);
    float t = std::tan(0.5f * fovy);          // t = tan(θ_h/2)
    glm::mat4 S(1.f);
    S[0][0] = 1.f / (f * aspect_ * t);        // 1/(far * tan(θ_w/2))
    S[1][1] = 1.f / (f * t);                  // 1/(far * tan(θ_h/2))
    S[2][2] = 1.f / f;                        // 1/far
    return S;
}

glm::mat4 Camera::makeUnhinge(float n, float f) const {
    float c = -n / f;
    glm::mat4 M(1.f);
    M[2][2] =  1.f/(1.f + c);
    M[2][3] = -1.f;
    M[3][2] = -c/(1.f + c);
    M[3][3] =  0.f;
    return M;
}

glm::mat4 Camera::makeOpenGLZFix() const {
    glm::mat4 L(1.f);
    L[2][2] = -2.f;
    L[3][2] = -1.f;
    return L;
}

glm::mat4 Camera::proj() const {
    float n = std::max(nearP, EPS);
    float f = std::max(farP,  n + EPS);
    glm::mat4 S   = makeScaleSxyz(fovyRad, aspect);
    glm::mat4 Mpp = makeUnhinge(n, f);
    glm::mat4 L   = makeOpenGLZFix();
    return L * Mpp * S;
}


// Axis-angle rotation (Rodrigues)
glm::mat3 Camera::makeAxisAngleMat3(const glm::vec3& axis, float radians) {
    glm::vec3 u = glm::normalize(axis);
    const float ux = u.x, uy = u.y, uz = u.z;
    const float c  = std::cos(radians);
    const float s  = std::sin(radians);
    const float omc = 1.f - c;

    glm::mat3 R(1.f);
    R[0][0] = c + ux*ux*omc;     R[0][1] = uy*ux*omc + uz*s; R[0][2] = uz*ux*omc - uy*s;
    R[1][0] = ux*uy*omc - uz*s;  R[1][1] = c + uy*uy*omc;    R[1][2] = uz*uy*omc + ux*s;
    R[2][0] = ux*uz*omc + uy*s;  R[2][1] = uy*uz*omc - ux*s; R[2][2] = c + uz*uz*omc;
    return R;
}

glm::vec3 Camera::rotateAxis(const glm::vec3& v, const glm::vec3& axis, float rad) {
    glm::mat3 R = makeAxisAngleMat3(axis, rad);
    return glm::normalize(R * v);
}

void Camera::yaw(float radians) {
    const glm::vec3 worldUp = {0.f, 1.f, 0.f};
    glm::mat3 R = makeAxisAngleMat3(worldUp, radians);
    look = glm::normalize(R * look);
    up   = glm::normalize(R * up);
}

void Camera::pitch(float radians) {
    const glm::vec3 right = glm::normalize(glm::cross(look, up));
    glm::mat3 R = makeAxisAngleMat3(right, radians);
    look = glm::normalize(R * look);
    up   = glm::normalize(glm::cross(right, look)); // re-orthogonalize
}

void Camera::translateWorld(const glm::vec3& d) {
    eye += d;
}
