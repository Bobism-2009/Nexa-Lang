#version 450
layout(location = 0) in vec3 aPos;
layout(push_constant) uniform PC { mat4 view; vec4 fog; } pc;
layout(location = 0) out vec3 vDir;
void main() {
    vec4 clip = pc.view * vec4(aPos, 1.0);
    gl_Position = clip.xyww;
    gl_Position.z = gl_Position.z * 0.5 + gl_Position.w * 0.5;
    vDir = aPos;
}
