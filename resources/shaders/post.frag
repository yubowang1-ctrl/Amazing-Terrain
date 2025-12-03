#version 330 core

in vec2 v_uv;
out vec4 fragColor;

uniform sampler2D uSceneColor;
uniform sampler2D uSceneDepth;
uniform float     uNear;
uniform float     uFar;

// eExposure: unit stop (0 = 1x, 1 = 2x)
uniform float uExposure;

// Lift/Gamma/Gain + Tint
uniform vec3  uLift;
uniform vec3  uGamma;
uniform vec3  uGain;
uniform vec3  uTint;
uniform float uGradeStrength; // 0..1
uniform int   uGradePreset;   // 0=none,1=cold,2=warm

float linearizeDepth(float depth01, float nearP, float farP)
{
    float z = depth01 * 2.0 - 1.0;
    return (2.0 * nearP * farP) / (farP + nearP - z * (farP - nearP));
}

// apply a small amount of tonemap
// to the highlighted areas to avoid making the overall area too dark.
vec3 applyTone(vec3 hdr, float viewZ)
{
    // if preset=0 and strength=0, return the value as is.
    if (uGradePreset == 0 && uGradeStrength <= 0.0) {
        return hdr;
    }

    float maxC = max(max(hdr.r, hdr.g), hdr.b);
    float t    = smoothstep(1.0, 4.0, maxC);

    vec3 reinhard = hdr / (hdr + vec3(1.0));
    vec3 color    = mix(hdr, reinhard, t);

    // depth-based fade
    float z01  = clamp((viewZ - uNear) / (uFar - uNear), 0.0, 1.0);
    float fade = smoothstep(0.3, 1.0, z01);

    // default slight "cinematic fade".
    vec3 fogTint      = vec3(0.9, 0.9, 0.95);
    float fadeStrength = 0.05;

    // Rainy days -> Grayer, bluer, and foggier
    if (uGradePreset == 3) {
        fogTint       = vec3(0.65, 0.68, 0.72); // grayish blue
        fadeStrength  = 0.3;                   // distant obj swallowed up by the fog.
    }

    color = mix(color, fogTint, fadeStrength * fade);
    return color;
}


// Adjustable: Lift / Gamma / Gain + Tint
vec3 applyColorGrade(vec3 color)
{
    vec3 lift  = uLift;
    vec3 gamma = uGamma;
    vec3 gain  = uGain;
    vec3 tint  = uTint;

    if (uGradePreset == 1) {
        // Cold Blue Snow Mountain: Bluish, slightly enhance highlights
        tint  = vec3(0.92, 0.98, 1.08);
        lift  = vec3(0.00, 0.00, 0.02);
        gamma = vec3(1.02, 1.01, 0.98);
        gain  = vec3(1.03, 1.02, 1.06);
    }
    else if (uGradePreset == 2) {
        // (Currently unused) Sunset gold: leaning towards orange, with slightly higher contrast.
        tint  = vec3(1.08, 1.02, 0.92);
        lift  = vec3(0.02, 0.01, -0.01);
        gamma = vec3(0.98, 0.99, 1.03);
        gain  = vec3(1.08, 1.05, 0.98);
    }
    else if (uGradePreset == 3) {
        // Rainy day: Overall slightly dark, contrast flattened, desaturated and cooler.
        tint  = vec3(0.80, 0.85, 0.95);   // Slightly blue and slightly gray
        lift  = vec3(0.06, 0.06, 0.07);   // Lift shadows -> Foggy
        gamma = vec3(1.18, 1.15, 1.12);   // Increase gamma -> Suppress midtone contrast
        gain  = vec3(0.85, 0.88, 0.90);   // suprpress highlights
    }

    vec3 graded = color;

    graded += lift;
    graded = max(graded, 0.0);
    graded = pow(graded, gamma);
    graded *= gain;
    graded *= tint;

    float s = clamp(uGradeStrength, 0.0, 1.0);
    return mix(color, graded, s);
}


void main() {
    vec3 sceneColor = texture(uSceneColor, v_uv).rgb;
    float depth01   = texture(uSceneDepth, v_uv).r;
    float viewZ     = linearizeDepth(depth01, uNear, uFar);

    float exposureMul = pow(2.0, uExposure);
    vec3 hdr = sceneColor * exposureMul;

    vec3 color = applyTone(hdr, viewZ);
    color      = applyColorGrade(color);

    fragColor = vec4(color, 1.0);
}
