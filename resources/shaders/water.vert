#version 330 core

layout(location = 0) in vec3 a_pos;
layout(location = 1) in vec3 a_nor;
layout(location = 2) in vec3 a_uvPacked;

out vec3 v_worldPos;
out vec3 v_worldNormal;
out vec2 v_uv;

uniform mat4 uProj;
uniform mat4 uView;
uniform mat4 uModel;

void main()
{
    vec4 world = uModel * vec4(a_pos, 1.0);
    v_worldPos = world.xyz;

    mat3 Nmat = transpose(inverse(mat3(uModel)));
    v_worldNormal = normalize(Nmat * a_nor);

    v_uv = a_uvPacked.xy;

    gl_Position = uProj * uView * world;
}
