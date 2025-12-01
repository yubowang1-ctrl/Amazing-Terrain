#version 330 core

in vec3 v_worldPos;
in vec3 v_worldNormal;
in vec2 v_uv;

out vec4 fragColor;

uniform bool wireshade;

// Sun + ambient lighting
uniform vec3 uEye;
uniform vec3 uSunDir;      // FROM light TO scene
uniform vec3 uSunColor;
uniform vec3 uAmbientColor;

// 5 albedo sets: grass, low rock, high rock, beach (lake bed), snow
uniform sampler2D uGrassAlbedo;
uniform sampler2D uRockAlbedo;      // low rock / rock_beach
uniform sampler2D uRockHighAlbedo;  // mountain rock
uniform sampler2D uBeachAlbedo;
uniform sampler2D uSnowAlbedo;

// Tangent-space normal maps
uniform sampler2D uGrassNormal;
uniform sampler2D uRockNormal;
uniform sampler2D uRockHighNormal;
uniform sampler2D uBeachNormal;
uniform sampler2D uSnowNormal;

// Global scalar for normal-map strength (0 = flat, 1 = full)
uniform float uNormalStrength;

// Roughness
uniform sampler2D uGrassRough;
uniform sampler2D uRockRough;
uniform sampler2D uRockHighRough;
uniform sampler2D uBeachRough;
uniform sampler2D uSnowRough;

// Fog
uniform vec3  uFogColor;    // should roughly match sky horizon color
uniform float uFogDensity;  // e.g. 0.02

// Height normalization
uniform float uSeaHeight;    // world-space sea level
uniform float uHeightScale;  // approximate world-space mountain height range

// 1. Base layer weights: Grass / Rock / Beach

// Compute base grass/rock weights in [0,1]
//   hNorm: 0..1 from sea level to mountain peak
//   slope: 0 = flat, 1 = vertical
vec2 computeGrassRockWeights(float hNorm, float slope)
{
    // Low near sea: more "rock beach"
    float rockBeach = 1.0 - smoothstep(0.02, 0.12, hNorm);

    // Grass appears from fairly low up to mid-high altitude
    float grassBand = smoothstep(0.05, 0.80, hNorm);

    // Steep slopes: rock; mid slopes can still have some grass
    float rockSlope = smoothstep(0.75, 0.90, slope);

    float wRock  = max(rockBeach, rockSlope);
    float wGrass = grassBand * (1.0 - 0.7 * rockSlope);

    // Globally bias toward grass a bit, rock a bit less
    wGrass *= 1.4;
    wRock  *= 0.7;

    vec2 w = vec2(wGrass, wRock);
    float s = w.x + w.y + 1e-5;
    return w / s;
}

// Base 3-way weights: (grass, rock, beach)
vec3 computeLayerWeights(float hNorm, float slope, float worldY)
{
    vec2 wr    = computeGrassRockWeights(hNorm, slope);
    float wGrass = wr.x;
    float wRock  = wr.y;

    // "Water depth" below sea level: we only care about a thin band
    float depthBelowSea = clamp(
        (uSeaHeight - worldY) / max(uHeightScale * 0.2, 1e-4),
        0.0, 1.0);

    // Flatness: 0 = steep wall, 1 = flat
    float flatness = 1.0 - slope;
    float flatMask = smoothstep(0.6, 0.9, flatness);

    // Only very low altitudes should get beach
    float nearSea = 1.0 - smoothstep(0.08, 0.15, hNorm);

    // Beach: flat + near sea, kept fairly weak
    float beachMask = depthBelowSea * flatMask * nearSea;
    beachMask *= 0.35;

    float wBeach = beachMask;

    // Beach steals some weight from grass + rock
    float landFactor = 1.0 - wBeach;
    wGrass *= landFactor;
    wRock  *= landFactor;

    vec3 w = vec3(wGrass, wRock, wBeach);
    float s = w.x + w.y + w.z + 1e-5;
    return w / s;
}

// 2. Helpers

// Sample tangent-space normal map and convert to world space.
//   amplitude: how much bump to keep from the texture (0..1)
//   blend: how strongly we trust the map vs. geometric normal (0..1)
//   -> This is where we "tame" the normal maps so they don't look too crazy.
vec3 sampleTangentNormal(
    sampler2D normalMap,
    vec2      uv,
    vec3      N_geom,
    float     amplitude,
    float     blend)
{
    // Build an approximate TBN from world up and the geometric normal.
    vec3 up = vec3(0.0, 1.0, 0.0);
    if (abs(dot(up, N_geom)) > 0.8)
        up = vec3(1.0, 0.0, 0.0);

    vec3 T = normalize(cross(up, N_geom));
    vec3 B = normalize(cross(N_geom, T));

    // Sample map in tangent space
    vec3 nTS = texture(normalMap, uv).xyz * 2.0 - 1.0;

    // Scale X/Y (tangent plane) by amplitude * global strength
    float amp = clamp(amplitude * uNormalStrength, 0.0, 2.0);
    nTS.xy *= amp;

    // Re-normalize in tangent space to avoid weird stretching
    float len2 = clamp(dot(nTS.xy, nTS.xy), 0.0, 1.0);
    nTS.z = sqrt(1.0 - len2);

    // Transform to world space
    mat3 TBN = mat3(T, B, N_geom);
    vec3 nWorld = normalize(TBN * nTS);

    // Soft blend between geometric normal and map normal:
    //   blend = 0 -> purely geometric
    //   blend = 1 -> purely normal map
    float b = clamp(blend, 0.0, 1.0);
    return normalize(mix(N_geom, nWorld, b));
}

// Simple tri-planar color sampling using world position and normal
vec3 triplanarSampleColor(sampler2D tex, vec3 worldPos, vec3 normal, float tiling)
{
    vec3 n = abs(normalize(normal));
    float sum = n.x + n.y + n.z + 1e-5;
    n /= sum;

    vec2 uvX = worldPos.zy * tiling; // YZ plane
    vec2 uvY = worldPos.xz * tiling; // XZ plane
    vec2 uvZ = worldPos.xy * tiling; // XY plane

    vec3 cX = texture(tex, uvX).rgb;
    vec3 cY = texture(tex, uvY).rgb;
    vec3 cZ = texture(tex, uvZ).rgb;

    return cX * n.x + cY * n.y + cZ * n.z;
}

// Main

void main()
{
    // Wireframe debug: show height as grayscale
    if (wireshade) {
        float hNormDbg = clamp((v_worldPos.y - uSeaHeight) / max(uHeightScale, 1e-4),
                               0.0, 1.0);
        fragColor = vec4(vec3(hNormDbg), 1.0);
        return;
    }

    vec3 N_geom = normalize(v_worldNormal);    // geometric normal (for slopes / weights)
    vec3 V      = normalize(uEye - v_worldPos);
    vec3 L      = normalize(-uSunDir);

    float hNorm = clamp((v_worldPos.y - uSeaHeight) / max(uHeightScale, 1e-4), 0.0, 1.0);
    float slope = clamp(1.0 - dot(N_geom, vec3(0.0, 1.0, 0.0)), 0.0, 1.0);

    // 3.1 Base 3-way weights: Grass / Rock / Beach
    vec3 wBase = computeLayerWeights(hNorm, slope, v_worldPos.y);
    float wGrassBase = wBase.x;
    float wRockBase  = wBase.y;
    float wBeach     = wBase.z;

    // 3.2 Split rock into low rock vs. high rock
    float rockHighH     = smoothstep(0.40, 0.85, hNorm);
    float rockHighSlope = smoothstep(0.50, 0.90, slope);
    float rockHighMask  = rockHighH * rockHighSlope;

    float wRockHigh = wRockBase * rockHighMask;
    float wRockLow  = wRockBase * (1.0 - rockHighMask);

    // Start with 4 layers
    float wGrass    = wGrassBase;
    float wRockLowN = wRockLow;
    float wRockHighN= wRockHigh;
    float wBeachN   = wBeach;

    // 3.3 Snow layer: plateau snow line + extra peak snow
    // 1) Plateau snow: mid-high altitude, not too steep
    float snowHeightPlateau = smoothstep(0.55, 0.85, hNorm);
    float snowFlatPlateau   = smoothstep(0.15, 0.55, 1.0 - slope);
    float snowPlateauMask   = snowHeightPlateau * snowFlatPlateau;

    // 2) Peak snow: very top region, even on steeper ridges
    float snowPeakHeight = smoothstep(0.80, 0.98, hNorm);
    float snowPeakMask   = snowPeakHeight;

    float snowMask = snowPlateauMask + 0.6 * snowPeakMask;
    snowMask = clamp(snowMask, 0.0, 1.0);

    // Avoid snow right at lake/shoreline
    snowMask *= clamp(1.0 - wBeachN * 4.0, 0.0, 1.0);

    // Snow steals weight only from grass + high rock
    float source   = wGrass + wRockHighN + 1e-5;
    float wSnowRaw = snowMask * source;

    float snowFromGrass    = wSnowRaw * (wGrass     / source);
    float snowFromRockHigh = wSnowRaw * (wRockHighN / source);

    wGrass     -= snowFromGrass;
    wRockHighN -= snowFromRockHigh;
    float wSnow = snowFromGrass + snowFromRockHigh;

    // Normalize final 5-layer weights
    float wSum = wGrass + wRockLowN + wRockHighN + wBeachN + wSnow + 1e-5;
    wGrass    /= wSum;
    wRockLowN /= wSum;
    wRockHighN/= wSum;
    wBeachN   /= wSum;
    wSnow     /= wSum;

    // 3.4 Albedo mixing (5 layers)
    vec3 grassAlbedo = texture(uGrassAlbedo, v_uv).rgb;

    const float ROCK_TILE    = 0.12;
    const float ROCK_H_TILE  = 0.15;
    const float BEACH_TILE   = 0.10;
    const float SNOW_TILE    = 0.10;

    vec3 rockLowAlbedo  = triplanarSampleColor(uRockAlbedo,     v_worldPos, N_geom, ROCK_TILE);
    vec3 rockHighAlbedo = triplanarSampleColor(uRockHighAlbedo, v_worldPos, N_geom, ROCK_H_TILE);
    vec3 beachAlbedo    = triplanarSampleColor(uBeachAlbedo,    v_worldPos, N_geom, BEACH_TILE);
    vec3 snowAlbedo     = triplanarSampleColor(uSnowAlbedo,     v_worldPos, N_geom, SNOW_TILE);

    vec3 albedo = grassAlbedo    * wGrass
                + rockLowAlbedo  * wRockLowN
                + rockHighAlbedo * wRockHighN
                + beachAlbedo    * wBeachN
                + snowAlbedo     * wSnow;

    // 3.5 Roughness mixing
    float rGrass    = texture(uGrassRough, v_uv).r;
    float rRockLow  = texture(uRockRough,      v_uv).r;
    float rRockHigh = texture(uRockHighRough,  v_uv).r;
    float rBeach    = texture(uBeachRough,     v_uv).r;
    float rSnow     = texture(uSnowRough,      v_uv).r;

    // Make grass rougher (almost matte), snow slightly smoother
    rGrass = clamp(rGrass + 0.2, 0.0, 1.0);
    rSnow  = clamp(rSnow  - 0.1, 0.1, 0.8);

    float rough = rGrass    * wGrass
                + rRockLow  * wRockLowN
                + rRockHigh * wRockHighN
                + rBeach    * wBeachN
                + rSnow     * wSnow;
    rough = clamp(rough, 0.04, 0.85);

    // 3.6 Normal mixing (5 layers)
    // per-layer amplitudes and blends – tuned to be less noisy & more natural
    vec3 nGrass    = sampleTangentNormal(uGrassNormal,    v_uv, N_geom, 0.6, 0.6);
    vec3 nRockLow  = sampleTangentNormal(uRockNormal,     v_uv, N_geom, 0.9, 0.85);
    vec3 nRockHigh = sampleTangentNormal(uRockHighNormal, v_uv, N_geom, 1.1, 0.9);
    vec3 nBeach    = sampleTangentNormal(uBeachNormal,    v_uv, N_geom, 0.8, 0.8);
    vec3 nSnow     = sampleTangentNormal(uSnowNormal,     v_uv, N_geom, 0.5, 0.6);

    vec3 N = normalize(nGrass    * wGrass +
                       nRockLow  * wRockLowN +
                       nRockHigh * wRockHighN +
                       nBeach    * wBeachN +
                       nSnow     * wSnow);

    // 3.7 Blinn–Phong lighting with roughness-driven specular
    float gloss      = pow(1.0 - rough, 0.8);       // smoother -> larger gloss
    float specPower  = mix(8.0, 50.0, gloss);       // grass ~8, rocks/snow up to ~50
    float specAmount = mix(0.02, 0.28, gloss);

    // When grass dominates, kill most spec; snow keeps a bit
    float nonGrass = wRockLowN + wRockHighN + wBeachN + wSnow * 0.9;
    specAmount *= nonGrass;

    float NdotL = max(dot(N, L), 0.0);
    vec3  H     = normalize(L + V);
    float spec  = pow(max(dot(N, H), 0.0), specPower);

    vec3 diffuse  = albedo * NdotL * uSunColor;
    vec3 specular = spec * uSunColor * specAmount;
    vec3 ambient  = albedo * uAmbientColor;

    vec3 color = ambient + diffuse + specular;

    // 3.8 Exponential distance fog in world space
    float dist = length(uEye - v_worldPos);
    float fog  = 1.0 - exp(-uFogDensity * dist);
    fog = clamp(fog, 0.0, 1.0);

    color = mix(color, uFogColor, fog);

    fragColor = vec4(color, 1.0);
}
