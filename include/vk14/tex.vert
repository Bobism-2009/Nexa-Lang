#version 450
layout(location = 0) in vec3 aPos;
layout(location = 1) in vec3 aCol;
layout(location = 2) in vec2 aUv;
layout(push_constant) uniform PC { mat4 mvp; } pc;
layout(location = 0) out vec3 vCol;
layout(location = 1) out vec2 vUv;
void main() {
    vec4 c = pc.mvp * vec4(aPos, 1.0);
    c.z = c.z * 0.5 + c.w * 0.5;
    gl_Position = c;
    vCol = aCol;
    vUv = aUv;
}
