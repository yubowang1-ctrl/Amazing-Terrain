#version 330 core

layout(location = 0) in vec3 a_pos;
layout(location = 2) in vec3 a_uvPacked; // xy = uv

// Frustum corners at near plane (world space)
uniform vec3 uFrustumNearTL;  // Top-left
uniform vec3 uFrustumNearTR;  // Top-right
uniform vec3 uFrustumNearBL;  // Bottom-left
uniform vec3 uFrustumNearBR;  // Bottom-right

out vec2 v_uv;
out vec3 v_nearCorner;  // Interpolated frustum corner direction

void main()
{
    v_uv = a_uvPacked.xy;
    
    // Map screen-space position to frustum corners
    // a_pos.xy is in [-1, 1] range for a fullscreen quad
    float u = (a_pos.x + 1.0) * 0.5;  // [0, 1]
    float v = (a_pos.y + 1.0) * 0.5;  // [0, 1]
    
    // Bilinear interpolation of frustum corners
    vec3 topCorner = mix(uFrustumNearTL, uFrustumNearTR, u);
    vec3 bottomCorner = mix(uFrustumNearBL, uFrustumNearBR, u);
    v_nearCorner = mix(bottomCorner, topCorner, v);
    
    gl_Position = vec4(a_pos.xy, 0.0, 1.0);
}
