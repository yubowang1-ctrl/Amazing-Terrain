#version 330 core

layout(location = 0) in vec3 a_pos;
layout(location = 2) in vec3 a_uvPacked; // xy = uv

out vec2 v_uv;

void main()
{
    v_uv = a_uvPacked.xy;
    gl_Position = vec4(a_pos.xy, 0.0, 1.0);
}
