#version 330 core

in vec3 v_worldPos;
in vec3 v_worldNormal;
in vec2 v_uv;

out vec4 fragColor;

uniform vec3 uEye;
uniform vec3 uSunDir;
uniform vec3 uSunColor;
uniform vec3 uAmbientColor;

uniform sampler2D uNormalMap;

// params: color / transparency / animation
uniform vec3  uWaterColorShallow;
uniform vec3  uWaterColorDeep;
uniform float uWaterAlpha;

uniform float uTime;
uniform float uTiling; // Texture tiling frequency
uniform float uScrollSpeed; // Scrolling speed
uniform vec2  uScrollDir;   // Scrolling direction
uniform float uNormalStrength;

// Sample from normal map and transform to world space
vec3 sampleWaterNormal(vec3 N_geom)
{
    // Scrolling UVs: Simple translation
    vec2 uv0 = v_uv * uTiling + uScrollDir * (uTime * uScrollSpeed);
    vec3 nTS = texture(uNormalMap, uv0).xyz * 2.0 - 1.0; // [0,1]->[-1,1]

    vec3 N = normalize(N_geom);
    vec3 up = vec3(0.0, 1.0, 0.0);
    if (abs(dot(up, N)) > 0.8)
        up = vec3(1.0, 0.0, 0.0);

    vec3 T = normalize(cross(up, N));
    vec3 B = normalize(cross(N, T));
    mat3 TBN = mat3(T, B, N);

    vec3 nWorld = normalize(TBN * nTS);

    // Use uNormalStrength to control the disturbance intensity
    return normalize(mix(N, nWorld, uNormalStrength));
}

void main()
{
    vec3 N = sampleWaterNormal(v_worldNormal);
    vec3 V = normalize(uEye - v_worldPos);
    vec3 L = normalize(-uSunDir);

    float NdotL = max(dot(N, L), 0.0);

    // Fresnel: The edge of the sweep angle is a bit brighter.
    float fresnel = pow(1.0 - max(dot(N, V), 0.0), 3.0);

    // Based on the direction of light + Fresnel, mix light/dark water colors.
    vec3 baseColor = mix(uWaterColorDeep, uWaterColorShallow, 0.3 + 0.5 * NdotL);
    baseColor += fresnel * 0.15;

    // softer highlight reduces shimmer
    vec3 H        = normalize(L + V);
    float specExp      = 24.0;
    float specStrength = 0.18;
    float spec    = pow(max(dot(N, H), 0.0), specExp);

    vec3 diffuse  = baseColor * NdotL * uSunColor;
    vec3 specular = spec * uSunColor * specStrength;
    vec3 ambient  = baseColor * uAmbientColor;

    vec3 color = ambient + diffuse + specular;

    fragColor = vec4(color, uWaterAlpha);
}
