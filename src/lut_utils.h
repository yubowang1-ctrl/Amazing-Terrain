#pragma once

#include <GL/glew.h>
#include <vector>
#include <string>
#include <glm/glm.hpp>

namespace LUTUtils {

/**
 * @brief Generate an identity 3D LUT (neutral color mapping)
 * @param size Size of the LUT cube (e.g., 32 for 32x32x32)
 * @return Vector of RGB float data arranged as [r, g, b, r, g, b, ...]
 */
inline std::vector<float> generateIdentityLUT(int size) {
    std::vector<float> data;
    data.reserve(size * size * size * 3);
    
    for (int b = 0; b < size; ++b) {
        for (int g = 0; g < size; ++g) {
            for (int r = 0; r < size; ++r) {
                float rf = static_cast<float>(r) / static_cast<float>(size - 1);
                float gf = static_cast<float>(g) / static_cast<float>(size - 1);
                float bf = static_cast<float>(b) / static_cast<float>(size - 1);
                
                data.push_back(rf);
                data.push_back(gf);
                data.push_back(bf);
            }
        }
    }
    
    return data;
}

/**
 * @brief Generate a creative LUT with a specific style
 * @param size Size of the LUT cube
 * @param preset Style preset (0=identity, 1=warm, 2=cool, 3=cinematic, 4=vintage)
 * @return Vector of RGB float data
 */
inline std::vector<float> generateStyledLUT(int size, int preset = 0) {
    std::vector<float> data;
    data.reserve(size * size * size * 3);
    
    for (int b = 0; b < size; ++b) {
        for (int g = 0; g < size; ++g) {
            for (int r = 0; r < size; ++r) {
                float rf = static_cast<float>(r) / static_cast<float>(size - 1);
                float gf = static_cast<float>(g) / static_cast<float>(size - 1);
                float bf = static_cast<float>(b) / static_cast<float>(size - 1);
                
                glm::vec3 color(rf, gf, bf);
                
                // Apply style transformations
                switch (preset) {
                    case 1: // Warm/Golden
                        color.r = glm::pow(color.r, 0.9f);
                        color.g = glm::pow(color.g, 0.95f);
                        color.b = glm::pow(color.b, 1.1f);
                        color.r *= 1.1f;
                        color.g *= 1.05f;
                        break;
                        
                    case 2: // Cool/Blue
                        color.r = glm::pow(color.r, 1.1f);
                        color.g = glm::pow(color.g, 1.05f);
                        color.b = glm::pow(color.b, 0.9f);
                        color.b *= 1.15f;
                        break;
                        
                    case 3: // Cinematic (lifted blacks, crushed highlights)
                        color.r = 0.05f + color.r * 0.90f;
                        color.g = 0.05f + color.g * 0.90f;
                        color.b = 0.05f + color.b * 0.90f;
                        color.r = glm::pow(color.r, 1.2f);
                        color.g = glm::pow(color.g, 1.2f);
                        color.b = glm::pow(color.b, 1.2f);
                        break;
                        
                    case 4: // Vintage (desaturated, warm shadows)
                    {
                        float lum = 0.299f * color.r + 0.587f * color.g + 0.114f * color.b;
                        color = glm::mix(glm::vec3(lum), color, 0.7f); // Desaturate
                        color.r += 0.05f;
                        color.g += 0.03f;
                        break;
                    }
                        
                    default: // Identity
                        break;
                }
                
                // Clamp to [0, 1]
                color = glm::clamp(color, 0.0f, 1.0f);
                
                data.push_back(color.r);
                data.push_back(color.g);
                data.push_back(color.b);
            }
        }
    }
    
    return data;
}

/**
 * @brief Create and upload a 3D LUT texture to OpenGL
 * @param size Size of the LUT cube
 * @param data RGB float data (size^3 * 3 floats)
 * @return OpenGL texture handle
 */
inline GLuint createLUT3DTexture(int size, const std::vector<float>& data) {
    GLuint texture;
    glGenTextures(1, &texture);
    glBindTexture(GL_TEXTURE_3D, texture);
    
    // Set texture parameters
    glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
    
    // Upload data
    glTexImage3D(GL_TEXTURE_3D, 0, GL_RGB16F,
                 size, size, size,
                 0, GL_RGB, GL_FLOAT, data.data());
    
    glBindTexture(GL_TEXTURE_3D, 0);
    
    return texture;
}

/**
 * @brief Load a .cube LUT file (Adobe .cube format)
 * This is a simplified loader for basic .cube files
 * @param filename Path to the .cube file
 * @param outSize Output size of the LUT
 * @param outData Output data vector
 * @return true if successful
 */
inline bool loadCubeLUT(const std::string& filename, int& outSize, std::vector<float>& outData) {
    // This is a placeholder - you would implement full .cube file parsing here
    // For now, return an identity LUT
    outSize = 32;
    outData = generateIdentityLUT(outSize);
    return true;
}

} // namespace LUTUtils
