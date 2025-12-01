// #version 330 core

// layout(location=0) in vec3 vertex;
// layout(location=1) in vec3 normal;
// layout(location=2) in vec3 inColor;

// out vec4 vert;
// out vec4 norm;
// out vec3 color;
// out vec3 lightDir;

// uniform mat4 uProj, uView, uModel;

// void main(){
//     mat4 mv = uView * uModel;
//     vert  = mv * vec4(vertex, 1.0);
//     norm  = transpose(inverse(mv)) * vec4(normal, 0.0);
//     color = inColor;
//     lightDir = normalize(vec3(mv * vec4(1, 0, 1, 0)));
//     gl_Position = uProj * mv * vec4(vertex, 1.0);
// }

#version 330 core

layout(location=0) in vec3 vertex;
layout(location=1) in vec3 normal;
layout(location=2) in vec3 inTex;   // 现在这就是 (u,v,0)

out vec3 v_worldPos;
out vec3 v_worldNormal;
out vec2 v_uv;

uniform mat4 uProj;
uniform mat4 uView;
uniform mat4 uModel;

void main()
{
    // 先算世界坐标（包含你的 R*S*T）:contentReference[oaicite:4]{index=4}
    vec4 world = uModel * vec4(vertex, 1.0);
    v_worldPos = world.xyz;

    // normal matrix
    mat3 Nmat = transpose(inverse(mat3(uModel)));
    v_worldNormal = normalize(Nmat * normal);

    // 把 attribute2 的 xy 当成 UV
    v_uv = inTex.xy;

    gl_Position = uProj * uView * world;
}
