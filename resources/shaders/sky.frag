#version 330 core

in vec3 v_dir;
out vec4 fragColor;

uniform vec3 uSunDir;   // FROM light TO scene
uniform vec3 uSunColor;

uniform vec3 uSkyTopColor;
uniform vec3 uSkyHorizonColor;
uniform vec3 uSkyBottomColor;

void main()
{
    vec3 d = normalize(v_dir);

    // y component determines: pointing upwards or closer to the horizon.
    float tUp   = clamp(d.y * 0.5 + 0.5, 0.0, 1.0);
    float tHori = smoothstep(-0.1, 0.2, d.y);

    vec3 skyUp   = mix(uSkyHorizonColor, uSkyTopColor, tUp);
    vec3 skyDown = uSkyBottomColor;

    vec3 base = mix(skyDown, skyUp, tHori);

    // Sun: add a small bright disk and halo along the direction of -uSunDir
    float sunAmount = max(dot(-uSunDir, d), 0.0);
    float sunDisc   = pow(sunAmount, 800.0);
    float sunGlow   = pow(sunAmount, 8.0);

    vec3 color = base + uSunColor * (sunGlow * 0.35 + sunDisc * 2.0);

    fragColor = vec4(color, 1.0);
}
