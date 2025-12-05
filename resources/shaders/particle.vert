#version 330 core

layout(location = 0) in vec3 aLocalPos; // Quad vertex position
layout(location = 1) in vec3 aInstancePos; // Particle world position
layout(location = 2) in vec4 aInstanceColor;
layout(location = 3) in float aInstanceSize;

out vec4 fragColor;
out vec2 texCoord;

uniform mat4 view;
uniform mat4 proj;
uniform int uType; // 0 = Snow, 1 = Rain
uniform float uTime;

void main() {
    fragColor = aInstanceColor;
    texCoord = aLocalPos.xy + 0.5; // Map [-0.5, 0.5] to [0, 1]

    vec3 cameraRight = vec3(view[0][0], view[1][0], view[2][0]);
    vec3 cameraUp = vec3(view[0][1], view[1][1], view[2][1]);

    vec3 offset = vec3(0.0);
    vec3 scale = vec3(1.0);

    if (uType == 0) { // Snow
        // Swaying motion
        float swayAmount = 0.5;
        float swaySpeed = 2.0;
        offset.x = sin(uTime * swaySpeed + aInstancePos.y) * swayAmount;
        offset.z = cos(uTime * swaySpeed + aInstancePos.x) * swayAmount;
    } else if (uType == 1) { // Rain
        // Stretch along Y (or velocity direction, simplified here to Y)
        // Make it thin and long
        scale.x = 0.05; // Even thinner (was 0.1)
        scale.y = 8.0;  // Longer relative to width (was 5.0), but base size is smaller now
        
        // Rain usually falls straight down, but billboarding makes it face camera.
        // For rain streaks, we often want them to align with gravity/velocity.
        // But simple billboarding with stretch is often "good enough" for this style.
    }

    vec3 vertexPos = aInstancePos + offset
                   + cameraRight * aLocalPos.x * aInstanceSize * scale.x
                   + cameraUp * aLocalPos.y * aInstanceSize * scale.y;

    gl_Position = proj * view * vec4(vertexPos, 1.0);
}
