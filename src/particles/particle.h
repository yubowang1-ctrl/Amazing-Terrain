#pragma once
#include <glm/glm.hpp>

struct Particle
{
    // Physics
    glm::vec3 m_position{0.f};
    glm::vec3 m_velocity{0.f};
    glm::vec3 m_acceleration{0.f};

    // Appearance
    glm::vec4 m_color{1.f};      // RGBA
    glm::vec4 m_deltaColor{0.f}; // Change in RGBA per second

    // Size
    float m_size{1.f};
    float m_deltaSize{0.f};

    // Lifecycle
    float m_lifeSpan{0.f};      // How long it lives total
    float m_lifeRemaining{0.f}; // How much time is left

    // State: 0 = Falling, 1 = Splashing
    int m_state{0};

    // Helper to check if dead
    bool isDead() const
    {
        return m_lifeRemaining <= 0.f;
    }

    // Update individual particle state
    void update(float deltaTime)
    {
        m_velocity += m_acceleration * deltaTime;
        m_position += m_velocity * deltaTime;

        m_color += m_deltaColor * deltaTime;
        m_size += m_deltaSize * deltaTime;

        m_lifeRemaining -= deltaTime;
    }
};
