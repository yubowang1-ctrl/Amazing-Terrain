#pragma once

#include "particle.h"
#include <vector>
#include <GL/glew.h>
#include <glm/glm.hpp>

class ParticleSystem
{
public:
    ParticleSystem();
    ~ParticleSystem();

    // Initialize OpenGL resources
    void init();

    // Update all particles
    void update(float deltaTime);

    // Render all particles
    void draw(const glm::mat4 &view, const glm::mat4 &proj);

    // Reset/Re-emit particles (e.g. for snow/rain)
    void setType(int type); // 0 = Snow, 1 = Rain

private:
    std::vector<Particle> m_particles;
    int m_maxParticles = 10000; // Increased for better density
    int m_type = 0;             // 0: Snow, 1: Rain
    float m_time = 0.0f;

    // OpenGL handles
    GLuint m_vao;
    GLuint m_vbo_pos;   // Instance positions
    GLuint m_vbo_color; // Instance colors
    GLuint m_vbo_size;  // Instance sizes
    GLuint m_shaderProgram;

    // Helper to respawn a particle when it dies
    void respawnParticle(Particle &p);
};
