#version 330 core

in vec2 v_uv;
in vec3 v_nearCorner;  // Frustum corner at near plane (interpolated)

out vec4 fragColor;

uniform sampler2D uSceneColor;
uniform sampler2D uSceneDepth;
uniform sampler3D uColorLUT;     // 3D LUT for color grading

uniform float     uNear;
uniform float     uFar;

// Camera position and view direction for fog
uniform vec3 uCameraPos;
uniform vec3 uCameraView;

// Fog parameters
uniform vec3  uFogColor;
uniform float uFogDensity;
uniform float uFogHeightFalloff;  // Height-based fog density falloff
uniform float uFogStart;          // Distance where fog starts
uniform bool  uEnableFog;
uniform bool  uEnableHeightFog;

// Exposure: unit stop (0 = 1x, 1 = 2x)
uniform float uExposure;

// Lift/Gamma/Gain + Tint
uniform vec3  uLift;
uniform vec3  uGamma;
uniform vec3  uGain;
uniform vec3  uTint;
uniform float uGradeStrength; // 0..1
uniform int   uGradePreset;   // 0=none,1=cold,2=warm,3=rainy

// Color LUT parameters
uniform bool  uEnableColorLUT;
uniform float uLUTSize;       // Size of the LUT (e.g., 32.0 for 32x32x32)

float linearizeDepth(float depth01, float nearP, float farP)
{
    float z = depth01 * 2.0 - 1.0;
    return (2.0 * nearP * farP) / (farP + nearP - z * (farP - nearP));
}

// Reconstruct world position from depth
vec3 reconstructWorldPos(float depth01, vec3 nearCorner)
{
    float linearZ = linearizeDepth(depth01, uNear, uFar);
    
    // Scale the ray from camera through near plane corner
    float t = linearZ / uNear;
    vec3 viewRay = nearCorner;
    
    return uCameraPos + viewRay * t;
}

// ============================================================================
// FIXED: Calculate fog factor with smooth transitions (no hard lines!)
// ============================================================================
float calculateHeightFog(vec3 worldPos)
{
    vec3 rayStart = uCameraPos;
    vec3 rayEnd = worldPos;
    float rayLength = length(rayEnd - rayStart);
    
    // ========================================
    // FIX: Use smoothstep instead of hard cutoff!
    // ========================================
    // Smooth transition zone: from fogStart to fogStart + fadeDistance
    float fadeDistance = max(5.0, uFogStart * 0.5);  // Transition over 50% of start distance or at least 5 units
    float fogStartFactor = smoothstep(uFogStart, uFogStart + fadeDistance, rayLength);
    
    if (fogStartFactor < 0.001) {
        return 0.0;  // Too close, no fog
    }
    
    // Calculate fog as before
    float adjustedLength = rayLength - uFogStart;
    
    if (!uEnableHeightFog) {
        // Simple distance-based fog
        float fog = 1.0 - exp(-uFogDensity * adjustedLength);
        fog = clamp(fog, 0.0, 1.0);
        
        // Apply smooth fade-in based on distance
        return fog * fogStartFactor;
    }
    
    // Height-based exponential fog
    // Integrate exponential fog density along the view ray
    float yStart = rayStart.y;
    float yEnd = rayEnd.y;
    
    // Avoid division by zero
    float deltaY = abs(yEnd - yStart);
    
    float fogAmount;
    if (deltaY < 0.001) {
        // Ray is mostly horizontal
        fogAmount = adjustedLength * exp(-yStart * uFogHeightFalloff);
    } else {
        // Full height integration
        float fogIntegral = (exp(-yStart * uFogHeightFalloff) - exp(-yEnd * uFogHeightFalloff));
        fogAmount = (adjustedLength / deltaY) * fogIntegral / uFogHeightFalloff;
    }
    
    float fog = 1.0 - exp(-uFogDensity * abs(fogAmount));
    fog = clamp(fog, 0.0, 1.0);
    
    // Apply smooth fade-in based on distance
    return fog * fogStartFactor;
}

// Apply 3D LUT color grading
vec3 applyColorLUT(vec3 color)
{
    if (!uEnableColorLUT) {
        return color;
    }
    
    // Clamp input color to [0, 1] range
    vec3 clampedColor = clamp(color, 0.0, 1.0);
    
    // Scale and offset to sample from the center of texels, not edges
    float scale = (uLUTSize - 1.0) / uLUTSize;
    float offset = 1.0 / (2.0 * uLUTSize);
    
    vec3 lutCoord = clampedColor * scale + vec3(offset);
    
    // Sample the 3D LUT
    return texture(uColorLUT, lutCoord).rgb;
}

void main()
{
    vec3 sceneColor = texture(uSceneColor, v_uv).rgb;
    float depth = texture(uSceneDepth, v_uv).r;
    
    // ============================================================================
    // 1. Apply fog if enabled
    // ============================================================================
    if (uEnableFog && depth < 1.0) {
        vec3 worldPos = reconstructWorldPos(depth, v_nearCorner);
        float fogFactor = calculateHeightFog(worldPos);
        
        // Mix scene color with fog color based on fog factor
        sceneColor = mix(sceneColor, uFogColor, fogFactor);
    }
    
    // ============================================================================
    // 2. Apply color grading
    // ============================================================================
    
    // Exposure adjustment
    vec3 color = sceneColor * pow(2.0, uExposure);
    
    // Lift/Gamma/Gain
    vec3 lift  = uLift;
    vec3 gamma = uGamma;
    vec3 gain  = uGain;
    
    // Preset adjustments
    if (uGradePreset == 1) {
        // Cold blue (snow mountain)
        lift  = vec3(-0.03, -0.02, 0.05);
        gamma = vec3(1.05, 1.00, 0.90);
        gain  = vec3(0.95, 0.98, 1.08);
    } else if (uGradePreset == 3) {
        // Rainy (desaturated cool)
        lift  = vec3(0.0, 0.0, 0.02);
        gamma = vec3(1.15, 1.10, 1.05);
        gain  = vec3(0.85, 0.88, 0.95);
    }
    
    // Apply lift/gamma/gain
    color = pow(max(color + lift, 0.0), 1.0 / gamma) * gain;
    
    // Tint
    color *= uTint;
    
    // Blend with original based on strength
    color = mix(sceneColor, color, uGradeStrength);
    
    // Apply 3D LUT color grading
    color = applyColorLUT(color);
    
    // ============================================================================
    // 3. Output
    // ============================================================================
    fragColor = vec4(color, 1.0);
}
