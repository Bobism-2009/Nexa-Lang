#version 450
layout(location = 0) in vec3 aPos;
layout(location = 1) in vec3 aNrm;
layout(location = 2) in vec3 aCol;
layout(set = 0, binding = 0) uniform Scene {
    vec4 lpos[16];
    vec4 ldif[16];
    vec4 lspot[16];
    vec4 lsp[16];
    vec4 ambFill;
    vec4 fog;
    ivec4 info;
} u;
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
layout(location = 0) out vec3 vFront;
layout(location = 1) out vec3 vBack;
layout(location = 2) out vec3 vSunF;
layout(location = 3) out vec3 vSunB;
layout(location = 4) out vec4 vLight;
layout(location = 5) out float vFog;
layout(location = 6) out float vAlpha;
vec3 shade(vec3 n, vec3 pe, vec3 mat, int skip) {
    vec3 direct = vec3(0.0);
    for (int i = 0; i < 16; i++) {
        if (i >= u.info.x) break;
        if (i == skip) continue;
        vec3 toL;
        float att = 1.0;
        if (u.lpos[i].w == 0.0) {
            toL = normalize(u.lpos[i].xyz);
        } else {
            vec3 L = u.lpos[i].xyz - pe;
            float dist = length(L);
            if (dist < 1e-5) toL = vec3(0.0, 0.0, 1.0);
            else { toL = L / dist; att = 1.0 / (1.0 + u.lsp[i].z * dist * dist); }
        }
        float spot = 1.0;
        if (u.lsp[i].x < 1.5) {
            float s = dot(-toL, normalize(u.lspot[i].xyz));
            if (s < u.lsp[i].x) spot = 0.0;
            else spot = pow(max(s, 0.0), u.lsp[i].y);
        }
        direct += u.ldif[i].xyz * max(dot(n, toL), 0.0) * att * spot;
    }
    vec3 lit = d.ambFill.xyz + direct;
    if (d.ambFill.w > 0.5) {
        vec3 c = clamp(direct, 0.0, 1.0);
        lit = c + d.ambFill.xyz * (vec3(1.0) - c);
    }
    return clamp(lit * mat, 0.0, 1.0);
}
void main() {
    vec4 eye = d.mv * vec4(aPos, 1.0);
    gl_Position = d.mvp * vec4(aPos, 1.0);
    gl_Position.xy += vec2(d.flags.w, d.extra.x) * gl_Position.w;
    gl_Position.z = gl_Position.z * 0.5 + gl_Position.w * 0.5;
    vLight = d.lightMvp * vec4(aPos, 1.0);
    vFog = 0.0;
    if (u.fog.w > 0.001) {
        float dist = length(eye.xyz);
        float start = u.fog.w * 0.25;
        vFog = clamp((dist - start) / max(u.fog.w - start, 0.001), 0.0, 1.0);
    }
    vAlpha = d.color.a;
    if (d.flags.x < 0.5) {
        vec3 col = (d.flags.y > 0.5) ? aCol : d.color.rgb;
        vFront = col;
        vBack = col;
        vSunF = vec3(0.0);
        vSunB = vec3(0.0);
    } else {
        vec3 n = normalize(mat3(d.nmat) * aNrm);
        vFront = shade(n, eye.xyz, d.color.rgb, -1);
        vSunF = vFront - shade(n, eye.xyz, d.color.rgb, u.info.y);
        if (d.flags.z > 0.5) {
            vBack = shade(-n, eye.xyz, d.color.rgb, -1);
            vSunB = vBack - shade(-n, eye.xyz, d.color.rgb, u.info.y);
        } else {
            vBack = vFront;
            vSunB = vSunF;
        }
    }
}
