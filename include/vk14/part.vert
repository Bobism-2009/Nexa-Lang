#version 450
layout(location = 0) in vec3 aPos;
layout(location = 1) in vec2 aUv;
layout(location = 2) in vec4 aColor;
layout(set = 0, binding = 0) uniform Cam {
    mat4 view;
    mat4 proj;
    vec4 fog;
} u;
layout(location = 0) out vec2 vUv;
layout(location = 1) out vec4 vColor;
layout(location = 2) out float vFog;
void main() {
    vec4 eye = u.view * vec4(aPos, 1.0);
    gl_Position = u.proj * eye;
    gl_Position.z = gl_Position.z * 0.5 + gl_Position.w * 0.5;
    vUv = aUv;
    vColor = aColor;
    vFog = 0.0;
    if (u.fog.w > 0.001) {
        float dist = length(eye.xyz);
        float start = u.fog.w * 0.25;
        vFog = clamp((dist - start) / max(u.fog.w - start, 0.001), 0.0, 1.0);
    }
}
