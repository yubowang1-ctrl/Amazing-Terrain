#version 330 core

layout(location = 0) in vec3 os_pos;
layout(location = 1) in vec3 os_norm;
layout(location = 2) in vec3 os_uv;

out vec3 ws_pos;
out vec3 ws_norm;
out vec2 uv;

uniform mat4 model_matrix;
uniform mat4 view_matrix;
uniform mat4 proj_matrix;

void main() {
    ws_pos = vec3(model_matrix * vec4(os_pos, 1.0));

    mat3 model_normal_matrix = transpose(inverse(mat3(model_matrix)));
    ws_norm = model_normal_matrix * normalize(os_norm);

    uv = os_uv.xy;

    gl_Position = proj_matrix * view_matrix * vec4(ws_pos, 1.0);
}

