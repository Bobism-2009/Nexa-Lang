#version 450
layout(location = 0) in vec3 aPos;
layout(location = 1) in vec3 aNrm;
layout(location = 2) in vec4 aTan;
layout(location = 3) in vec2 aUv;
layout(set = 0, binding = 1) uniform Draw {
    mat4 mvp;
    mat4 mv;
    mat4 lightMvp;
    mat4 nmat;
    vec4 color;
    vec4 flags;
    vec4 extra;
    vec4 ambFill;
} d;
layout(set = 0, binding = 0) uniform Scene {
    vec4 lpos[16];
    vec4 ldif[16];
    vec4 lspot[16];
    vec4 lsp[16];
    vec4 ambFill;
    vec4 fog;
    ivec4 info;
} u;
layout(location = 0) out vec3 vEye;
layout(location = 1) out vec4 vLight;
layout(location = 2) out float vFog;
layout(location = 3) out vec3 vN;
layout(location = 4) out vec3 vT;
layout(location = 5) out vec3 vB;
layout(location = 6) out vec2 vUv;
layout(location = 7) out float vAlpha;
void main() {
    vec4 eye = d.mv * vec4(aPos, 1.0);
    gl_Position = d.mvp * vec4(aPos, 1.0);
    gl_Position.xy += vec2(d.flags.w, d.extra.x) * gl_Position.w;
    gl_Position.z = gl_Position.z * 0.5 + gl_Position.w * 0.5;
    vEye = eye.xyz;
    vFog = 0.0;
    if (u.fog.w > 0.001) {
        float dist = length(eye.xyz);
        float start = u.fog.w * 0.25;
        vFog = clamp((dist - start) / max(u.fog.w - start, 0.001), 0.0, 1.0);
    }
    mat3 N = mat3(d.nmat);
    vN = normalize(N * aNrm);
    vT = normalize(N * aTan.xyz);
    vB = cross(vN, vT) * aTan.w;
    vUv = aUv;
    vLight = d.lightMvp * vec4(aPos, 1.0);
    vAlpha = d.color.a;
}
