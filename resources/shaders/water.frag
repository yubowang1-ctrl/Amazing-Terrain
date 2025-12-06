#version 330 core

in vec3 ws_pos;
in vec3 ws_norm;
in vec2 uv;

out vec4 fragColor;

// Texture samplers
uniform sampler2D u_reflectionTexture;  // Reflection FBO texture
uniform sampler2D u_refractionTexture;  // Refraction FBO texture
uniform sampler2D u_depthTexture;        // Depth texture
uniform sampler2D u_normalMap;          // Normal map texture
uniform sampler2D u_dudvMap;            // DUDV map texture

// Matrices
uniform mat4 model_matrix;
uniform mat4 view_matrix;
uniform mat4 proj_matrix;

// Camera
uniform vec3 ws_cam_pos;

uniform float u_near;
uniform float u_far;

// Time factor for animation
uniform float u_timeFactor;

// SceneGlobalData
struct GlobalData{
   float ka;
   float kd;
   float ks;
};

// SceneLightData
struct Light{
   int type;
   vec3 color;
   vec3 function;
   vec3 pos;
   vec3 dir;
   float penumbra;
   float angle;
};

uniform GlobalData globalData;
uniform Light light[8];
uniform int number_light;

// Water parameters
const float waveStrength = 0.02;
const float waterDepthFactor = 0.1;
const float fresnelPower = 2.0;

// GLSL: Distortion - DuDv Map distortion
vec2 getDistortedUV() {
    vec2 dudvOffset1 = texture(u_dudvMap, vec2(uv.x + u_timeFactor * 0.1, uv.y + u_timeFactor * 0.1)).rg * 2.0 - 1.0;
    vec2 dudvOffset2 = texture(u_dudvMap, vec2(-uv.x + u_timeFactor * 0.1, uv.y + u_timeFactor * 0.15)).rg * 2.0 - 1.0;
    vec2 dudvOffset = (dudvOffset1 + dudvOffset2) * waveStrength;
    return uv + dudvOffset;
}

// GLSL: Fresnel - Calculate Fresnel effect
float calculateFresnel(vec3 viewDir, vec3 normal) {
    float fresnel = dot(viewDir, normal);
    fresnel = pow(1.0 - fresnel, fresnelPower);
    return fresnel;
}

// Depth - Calculate soft edge/water depth effect
float calculateWaterDepth(vec2 distortedUV) {
    float depth = texture(u_depthTexture, distortedUV).r;
    float distance = gl_FragCoord.z;

    // Linearize depth values
    float near = u_near;
    float far  = u_far;
    float depthLinear = 2.0 * near * far / (far + near - (2.0 * depth - 1.0) * (far - near));
    float distanceLinear = 2.0 * near * far / (far + near - (2.0 * distance - 1.0) * (far - near));

    float diff = depthLinear - distanceLinear;
    float depthFactor = clamp(diff * waterDepthFactor, 0.0, 1.0);

    return depthFactor;
}

// Phong lighting calculation
vec3 phongLight(Light light, vec3 pos, vec3 normal, vec3 directionToCamera) {
    vec3 V = normalize(directionToCamera);
    vec3 N = normalize(normal);
    vec3 L;
    float attenuation = 1.0;
    float dist = 1.0;

    if (light.type == 0) {
        // Directional light
        L = normalize(-light.dir);
        attenuation = 1.0;
    } else if (light.type == 1) {
        // Point light
        L = normalize(light.pos - pos);
        dist = length(light.pos - pos);
        float c1 = light.function.x, c2 = light.function.y, c3 = light.function.z;
        attenuation = min(1.0, (1.0 / (c1 + c2 * dist + c3 * dist * dist)));
    } else if (light.type == 2) {
        // Spot light
        L = normalize(light.pos - pos);
        dist = length(light.pos - pos);
        float c1 = light.function.x, c2 = light.function.y, c3 = light.function.z;
        attenuation = min(1.0, (1.0 / (c1 + c2 * dist + c3 * dist * dist)));
        vec3 spotdir = normalize(light.dir);
        float Oa = max(0.0, light.angle);
        float Ia = clamp(Oa - light.penumbra, 0.0, Oa);
        vec3 lightToP = normalize(pos - light.pos);
        float cosTheta = dot(spotdir, lightToP);
        float x = acos(cosTheta);
        float falloff = -2.0 * pow((x - Ia) / (Oa - Ia), 3.0) + 3.0 * pow((x - Ia) / (Oa - Ia), 2.0);
        float spotIntensity = 1.0;
        if (x >= Oa) {
            spotIntensity = 0.0;
        } else if(x > Ia) {
            spotIntensity = 1.0 - falloff;
        }
        attenuation *= spotIntensity;
    } else {
        return vec3(0.0);
    }

    float NdotL = max(dot(N, L), 0.0);
    if (NdotL <= 0.0) return vec3(0.0);

    vec3 baseDiffuse = globalData.kd * vec3(0.0, 0.3, 0.5); // Water base color
    vec3 diffuse = baseDiffuse * NdotL * light.color;

    vec3 R = -L + 2.0 * dot(L, N) * N;
    float RdotV = max(dot(R, V), 0.0);
    vec3 specular = vec3(0.0);
    if (RdotV > 0.0) {
        specular = globalData.ks * vec3(1.0) * pow(RdotV, 64.0) * light.color;
    }

    return (diffuse + specular) * attenuation;
}

// GLSL: Combination - Blend all effects
void main() {
    // Get distorted UV coordinates
    vec2 distortedUV = getDistortedUV();

    // Sample reflection and refraction textures with distortion
    vec3 reflectionColor = texture(u_reflectionTexture, distortedUV).rgb;
    vec3 refractionColor = texture(u_refractionTexture, distortedUV).rgb;

    // Get normal from normal map
    vec3 normalMap = texture(u_normalMap, distortedUV).rgb * 2.0 - 1.0;
    vec3 normal = normalize(ws_norm + normalMap * 0.1);

    // Calculate view direction
    vec3 viewDir = normalize(ws_cam_pos - ws_pos);


    // Calculate Fresnel
    float fresnel = calculateFresnel(viewDir, ws_norm);

    // Calculate water depth
    float depthFactor = calculateWaterDepth(distortedUV);

    // Calculate lighting
    vec3 directionToCamera = ws_cam_pos - ws_pos;
    vec3 lighting = globalData.ka * vec3(0.2, 0.5, 0.8); // Ambient water color
    for (int i = 0; i < number_light; i++) {
        lighting += phongLight(light[i], ws_pos, normal, directionToCamera);
    }

    // Combine reflection and refraction based on Fresnel
    vec3 waterColor = mix(refractionColor, reflectionColor, fresnel);

    // Apply depth factor for soft edges
    waterColor = mix(waterColor, refractionColor, depthFactor);

    // Add lighting
    waterColor += lighting * 0.3;

    fragColor = vec4(waterColor, 0.8);
}
