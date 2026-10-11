#version 450
layout(location = 0) in vec3 aPos;
layout(push_constant) uniform PC { mat4 mvp; } pc;
void main() {
    gl_Position = pc.mvp * vec4(aPos, 1.0);
    gl_Position.z = gl_Position.z * 0.5 + gl_Position.w * 0.5;
}
