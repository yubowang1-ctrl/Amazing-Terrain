#version 330 core

layout(location = 0) in vec3 a_pos;

out vec3 v_dir;

uniform mat4 uView;
uniform mat4 uProj;

void main()
{
    v_dir = a_pos; // cube vertex direction

    gl_Position = uProj * uView * vec4(a_pos, 1.0);
}
