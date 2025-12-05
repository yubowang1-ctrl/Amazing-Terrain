#include "particlesystem.h"
#include "utils/shaderloader.h"
#include <cstdlib> // for rand()

ParticleSystem::ParticleSystem()
{
}

ParticleSystem::~ParticleSystem()
{
    glDeleteBuffers(1, &m_vbo_pos);
    glDeleteBuffers(1, &m_vbo_color);
    glDeleteBuffers(1, &m_vbo_size);
    glDeleteVertexArrays(1, &m_vao);
    glDeleteProgram(m_shaderProgram);
}

void ParticleSystem::init()
{
    // 1. Initialize Particles
    m_particles.resize(m_maxParticles);
    for (auto &p : m_particles)
    {
        respawnParticle(p);
        // Give them random initial life so they don't all die at once
        p.m_lifeRemaining = static_cast<float>(rand()) / RAND_MAX * p.m_lifeSpan;
    }

    // 2. Load Shaders
    // Note: You need to ensure these paths are correct relative to your executable or resource loader
    m_shaderProgram = ShaderLoader::createShaderProgram(":/resources/shaders/particle.vert", ":/resources/shaders/particle.frag");

    // 3. Setup VAO/VBO
    glGenVertexArrays(1, &m_vao);
    glBindVertexArray(m_vao);

    // We will use a simple quad for the particle (2 triangles)
    // But to use Instanced Rendering efficiently, we can just generate the geometry in the Geometry Shader
    // OR use a simple VBO for the quad vertices and attribute divisors for per-instance data.
    // Let's use the "Attribute Divisor" method with a simple quad.

    // Quad Vertices (x, y, z) - centered at 0,0
    float quadVertices[] = {
        -0.5f, -0.5f, 0.0f,
        0.5f, -0.5f, 0.0f,
        -0.5f, 0.5f, 0.0f,
        0.5f, 0.5f, 0.0f};

    GLuint quadVBO;
    glGenBuffers(1, &quadVBO);
    glBindBuffer(GL_ARRAY_BUFFER, quadVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(quadVertices), quadVertices, GL_STATIC_DRAW);

    glEnableVertexAttribArray(0); // Position (local)
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void *)0);

    // Instance Data VBOs
    // Position
    glGenBuffers(1, &m_vbo_pos);
    glBindBuffer(GL_ARRAY_BUFFER, m_vbo_pos);
    glBufferData(GL_ARRAY_BUFFER, m_maxParticles * sizeof(glm::vec3), nullptr, GL_STREAM_DRAW);
    glEnableVertexAttribArray(1); // World Position
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 0, (void *)0);
    glVertexAttribDivisor(1, 1); // Tell OpenGL this is per-instance

    // Color
    glGenBuffers(1, &m_vbo_color);
    glBindBuffer(GL_ARRAY_BUFFER, m_vbo_color);
    glBufferData(GL_ARRAY_BUFFER, m_maxParticles * sizeof(glm::vec4), nullptr, GL_STREAM_DRAW);
    glEnableVertexAttribArray(2); // Color
    glVertexAttribPointer(2, 4, GL_FLOAT, GL_FALSE, 0, (void *)0);
    glVertexAttribDivisor(2, 1);

    // Size
    glGenBuffers(1, &m_vbo_size);
    glBindBuffer(GL_ARRAY_BUFFER, m_vbo_size);
    glBufferData(GL_ARRAY_BUFFER, m_maxParticles * sizeof(float), nullptr, GL_STREAM_DRAW);
    glEnableVertexAttribArray(3); // Size
    glVertexAttribPointer(3, 1, GL_FLOAT, GL_FALSE, 0, (void *)0);
    glVertexAttribDivisor(3, 1);

    glBindVertexArray(0);
}

void ParticleSystem::respawnParticle(Particle &p)
{
    // Random position in a box around the origin (or camera)
    // For now, let's assume a fixed world box: x[-20, 20], y[0, 20], z[-20, 20]
    float x = (static_cast<float>(rand()) / RAND_MAX * 40.0f) - 20.0f;
    float y = (static_cast<float>(rand()) / RAND_MAX * 10.0f) + 10.0f; // Start high up
    float z = (static_cast<float>(rand()) / RAND_MAX * 40.0f) - 20.0f;

    p.m_position = glm::vec3(x, y, z);
    p.m_lifeSpan = 20.0f + static_cast<float>(rand()) / RAND_MAX * 10.0f; // Increased to 20-30 seconds to ensure they hit ground
    p.m_lifeRemaining = p.m_lifeSpan;
    p.m_state = 0; // Reset to Falling

    if (m_type == 0)
    { // Snow
        // Wider area for snow
        float x = (static_cast<float>(rand()) / RAND_MAX * 60.0f) - 30.0f;
        float z = (static_cast<float>(rand()) / RAND_MAX * 60.0f) - 30.0f;
        p.m_position = glm::vec3(x, 25.0f, z); // Start higher

        p.m_velocity = glm::vec3(0.0f, -1.0f - (static_cast<float>(rand()) / RAND_MAX * 1.0f), 0.0f); // Slower fall

        // Random horizontal drift (wind)
        float driftX = (static_cast<float>(rand()) / RAND_MAX * 0.5f) - 0.25f;
        float driftZ = (static_cast<float>(rand()) / RAND_MAX * 0.5f) - 0.25f;
        p.m_acceleration = glm::vec3(driftX, 0.0f, driftZ);

        p.m_color = glm::vec4(1.0f, 0.98f, 0.98f, 0.9f);                    // Warm White
        p.m_size = 0.02f + (static_cast<float>(rand()) / RAND_MAX * 0.03f); // Much smaller (approx 1/5)
        p.m_deltaColor = glm::vec4(0.f, 0.f, 0.f, -0.02f);                  // Fade out very slowly
    }
    else
    { // Rain
        // Reduced speed: -8.0 to -12.0 (was -15 to -20)
        p.m_velocity = glm::vec3(0.0f, -8.0f - (static_cast<float>(rand()) / RAND_MAX * 4.0f), 0.0f);
        p.m_acceleration = glm::vec3(0.0f, -5.0f, 0.0f); // Reduced gravity effect
        p.m_color = glm::vec4(0.8f, 0.9f, 1.0f, 0.5f);   // Slightly more transparent
        p.m_size = 0.03f;                                // Much smaller (approx 1/5)
        p.m_deltaColor = glm::vec4(0.f);
    }
}

void ParticleSystem::update(float deltaTime)
{
    m_time += deltaTime;
    for (auto &p : m_particles)
    {
        p.update(deltaTime);

        // Rain Splash Logic
        if (m_type == 1)
        { // Rain
            if (p.m_state == 0)
            { // Falling
                // Check if hit ground (approximate at y = -5.0f for now, or use terrain height if available)
                if (p.m_position.y < 0.0f) // Raised ground check to 0.0f
                {
                    // Switch to Splashing
                    p.m_state = 1;
                    p.m_position.y = 0.0f; // Clamp to ground

                    // Bounce up with random spread
                    float rndX = (static_cast<float>(rand()) / RAND_MAX * 2.0f) - 1.0f;
                    float rndZ = (static_cast<float>(rand()) / RAND_MAX * 2.0f) - 1.0f;
                    p.m_velocity = glm::vec3(rndX, 1.0f + static_cast<float>(rand()) / RAND_MAX * 1.0f, rndZ);

                    p.m_acceleration = glm::vec3(0.0f, -9.8f, 0.0f); // Normal gravity
                    p.m_lifeRemaining = 0.2f;                        // Short life for splash
                    p.m_size = 0.02f;                                // Smaller splash
                }
            }
            else
            { // Splashing
                if (p.isDead())
                {
                    respawnParticle(p);
                }
            }
        }
        else
        {
            // Snow Logic
            if (p.m_state == 0)
            {                              // Falling
                if (p.m_position.y < 0.0f) // Raised ground check to 0.0f
                {
                    // Hit ground -> Accumulate/Melt
                    p.m_state = 1; // On Ground
                    p.m_position.y = 0.0f;
                    p.m_velocity = glm::vec3(0.f);
                    p.m_acceleration = glm::vec3(0.f);
                }
            }

            // If dead (life ran out), respawn
            if (p.isDead())
            {
                respawnParticle(p);
            }
        }
    }
}

void ParticleSystem::draw(const glm::mat4 &view, const glm::mat4 &proj)
{
    glUseProgram(m_shaderProgram);

    // Update GPU buffers
    std::vector<glm::vec3> positions;
    std::vector<glm::vec4> colors;
    std::vector<float> sizes;

    positions.reserve(m_particles.size());
    colors.reserve(m_particles.size());
    sizes.reserve(m_particles.size());

    for (const auto &p : m_particles)
    {
        positions.push_back(p.m_position);
        colors.push_back(p.m_color);
        sizes.push_back(p.m_size);
    }

    glBindBuffer(GL_ARRAY_BUFFER, m_vbo_pos);
    glBufferSubData(GL_ARRAY_BUFFER, 0, positions.size() * sizeof(glm::vec3), positions.data());

    glBindBuffer(GL_ARRAY_BUFFER, m_vbo_color);
    glBufferSubData(GL_ARRAY_BUFFER, 0, colors.size() * sizeof(glm::vec4), colors.data());

    glBindBuffer(GL_ARRAY_BUFFER, m_vbo_size);
    glBufferSubData(GL_ARRAY_BUFFER, 0, sizes.size() * sizeof(float), sizes.data());

    // Set Uniforms
    GLint viewLoc = glGetUniformLocation(m_shaderProgram, "view");
    GLint projLoc = glGetUniformLocation(m_shaderProgram, "proj");
    glUniformMatrix4fv(viewLoc, 1, GL_FALSE, &view[0][0]);
    glUniformMatrix4fv(projLoc, 1, GL_FALSE, &proj[0][0]);

    glUniform1i(glGetUniformLocation(m_shaderProgram, "uType"), m_type);
    glUniform1f(glGetUniformLocation(m_shaderProgram, "uTime"), m_time);

    // Draw
    glBindVertexArray(m_vao);
    glDrawArraysInstanced(GL_TRIANGLE_STRIP, 0, 4, m_particles.size());
    glBindVertexArray(0);
    glUseProgram(0);
}

void ParticleSystem::setType(int type)
{
    m_type = type;
    // Reset all particles to new type
    for (auto &p : m_particles)
    {
        respawnParticle(p);
    }
}
