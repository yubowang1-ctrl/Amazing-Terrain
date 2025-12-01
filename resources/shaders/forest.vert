#version 330 core

layout(location = 0) in vec3 a_pos;
layout(location = 1) in vec3 a_nor;

// per-instance model matrix, occupying attributes 2, 3, 4, 5
layout(location = 2) in mat4 aModel;

uniform mat4 uView;
uniform mat4 uProj;

out vec3 v_worldPos;
out vec3 v_worldNormal;

void main()
{
    vec4 world = aModel * vec4(a_pos, 1.0);
    v_worldPos    = world.xyz;

    // calculate the normal matrix directly in the shader
    mat3 normalMat = mat3(transpose(inverse(aModel)));
    v_worldNormal  = normalize(normalMat * a_nor);

    gl_Position = uProj * uView * world;
}
