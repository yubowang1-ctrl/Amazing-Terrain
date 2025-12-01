#version 330 core

layout(location = 0) in vec3 a_pos;
layout(location = 1) in vec3 a_nor;

uniform mat4 uModel;
uniform mat4 uView;
uniform mat4 uProj;
uniform mat3 uNormalMat;

out vec3 v_worldPos;
out vec3 v_worldNormal;

void main()
{
    vec4 world = uModel * vec4(a_pos, 1.0);
    v_worldPos    = world.xyz;
    v_worldNormal = normalize(uNormalMat * a_nor);

    gl_Position = uProj * uView * world;
}
