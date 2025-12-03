#version 330 core

in vec3 v_worldPos;
in vec3 v_worldNormal;

out vec4 fragColor;

uniform vec3 uEye;

uniform vec3 uSunDir;       // FROM light TO scene
uniform vec3 uSunColor;
uniform vec3 uAmbientColor;

uniform vec3  uFogColor;
uniform float uFogDensity;

struct Material {
    vec3 ka;
    vec3 kd;
    vec3 ks;
    float shininess;
};

uniform Material u_mat;

void main()
{
    vec3 N = normalize(v_worldNormal);
    vec3 V = normalize(uEye - v_worldPos);
    vec3 L = normalize(-uSunDir);  // light comes from -dir

    float NdotL = max(dot(N, L), 0.0);
    vec3 H      = normalize(L + V);
    float spec  = pow(max(dot(N, H), 0.0), u_mat.shininess);

    vec3 ambient  = u_mat.kd * uAmbientColor;
    vec3 diffuse  = u_mat.kd * NdotL * uSunColor;
    vec3 specular = u_mat.ks * spec    * uSunColor;


    vec3 color = ambient + diffuse + specular;

    // simple distance fog: using world-space distance
    float dist = length(uEye - v_worldPos);
    float fog  = 1.0 - exp(-uFogDensity * dist);
    fog = clamp(fog, 0.0, 1.0);

    color = mix(color, uFogColor, fog);

    fragColor = vec4(color, 1.0);
}
