#version 330 core

in vec3 ws_pos;
in vec3 ws_norm;
in vec2 uv;

out vec4 fragColor;

uniform sampler2D u_reflectionTexture;
uniform sampler2D u_refractionTexture;
uniform sampler2D u_dudvMap;

uniform vec3 ws_cam_pos;
uniform vec3 uFogColor;
uniform float uFogDensity;
uniform bool uEnableFog;
uniform float u_timeFactor;

void main() {
    vec2 screenUV = gl_FragCoord.xy / vec2(textureSize(u_reflectionTexture, 0));

    // Get distortion
    vec2 distortion = (texture(u_dudvMap, uv + u_timeFactor * 0.1).rg * 2.0 - 1.0) * 0.02;
    vec2 distortedUV = clamp(screenUV + distortion, 0.001, 0.999);

    // flip reflection vertically
    vec3 reflection = texture(u_reflectionTexture, vec2(distortedUV.x, 1.0 - distortedUV.y)).rgb;
    vec3 refraction = texture(u_refractionTexture, distortedUV).rgb;

    // calculate fresnel - more reflection at grazing angles
    vec3 viewDir = normalize(ws_cam_pos - ws_pos);
    vec3 normal = normalize(ws_norm);
    float fresnel = pow(1.0 - max(dot(viewDir, normal), 0.0), 3.0);
    fresnel = clamp(fresnel, 0.2, 0.9);

    vec3 waterBase = vec3(0.0, 0.3, 0.5);
    vec3 waterColor = mix(refraction, reflection, fresnel);
    waterColor = mix(waterColor, waterBase, 0.3); // blend with water color

    // Apply fog
    if (uEnableFog) {
        float dist = length(ws_cam_pos - ws_pos);
        float fog = 1.0 - exp(-uFogDensity * dist);
        vec3 fogColor = length(uFogColor) < 0.001 ? vec3(0.6, 0.7, 0.8) : uFogColor;
        waterColor = mix(waterColor, fogColor, clamp(fog, 0.0, 1.0));
    }

    fragColor = vec4(waterColor, 0.85);
}
