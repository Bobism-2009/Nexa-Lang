#version 450
layout(location = 0) in vec3 vEye;
layout(location = 1) in vec4 vLight;
layout(location = 2) in float vFog;
layout(location = 3) in vec3 vN;
layout(location = 4) in vec3 vT;
layout(location = 5) in vec3 vB;
layout(location = 6) in vec2 vUv;
layout(location = 7) in float vAlpha;
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
layout(set = 0, binding = 2) uniform sampler2D uShadow;
layout(set = 0, binding = 3) uniform sampler2D uBase;
layout(set = 0, binding = 4) uniform sampler2D uNrm;
layout(set = 0, binding = 5) uniform sampler2D uMetal;
layout(set = 0, binding = 6) uniform sampler2D uRough;
layout(set = 0, binding = 7) uniform sampler2D uHeight;
layout(location = 0) out vec4 frag;
float shadow_pcf(vec4 clip) {
    if (clip.w <= 0.0) return 1.0;
    vec3 p = clip.xyz / clip.w * 0.5 + 0.5;
    if (p.z > 1.0) return 1.0;
    float s = 0.0;
    vec2 o = vec2(1.5 / 2048.0);
    for (int y = -1; y <= 1; y++) {
        for (int x = -1; x <= 1; x++) {
            float dep = texture(uShadow, p.xy + vec2(float(x), float(y)) * o).r;
            s += (p.z - 0.002 > dep) ? 0.0 : 1.0;
        }
    }
    return s / 9.0;
}
vec3 shade(vec3 n, vec3 pe, vec3 albedo, float metal, float rough) {
    vec3 direct = vec3(0.0);
    vec3 spec = vec3(0.0);
    vec3 V = normalize(-pe);
    float shin = mix(96.0, 6.0, clamp(rough, 0.0, 1.0));
    vec3 F0 = mix(vec3(0.04), albedo, metal);
    for (int i = 0; i < 16; i++) {
        if (i >= u.info.x) break;
        vec3 toL;
        float att = 1.0;
        if (u.lpos[i].w == 0.0) toL = normalize(u.lpos[i].xyz);
        else {
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
        float ndl = max(dot(n, toL), 0.0);
        float sh = 1.0;
        if (u.info.z > 0 && i == u.info.y) sh = shadow_pcf(vLight);
        float lamp = ndl * att * spot * sh;
        direct += u.ldif[i].xyz * lamp;
        vec3 H = normalize(toL + V);
        spec += u.ldif[i].xyz * pow(max(dot(n, H), 0.0), shin) * att * spot * sh;
    }
    vec3 lit = d.ambFill.xyz + direct;
    if (d.ambFill.w > 0.5) {
        vec3 c = clamp(direct, 0.0, 1.0);
        lit = c + d.ambFill.xyz * (vec3(1.0) - c);
    }
    vec3 diff = albedo * (1.0 - metal);
    return clamp(lit * diff + spec * F0 * 0.18, 0.0, 1.0);
}
void main() {
    vec2 uv = vUv;
    vec3 nrm = normalize(gl_FrontFacing ? vN : -vN);
    vec3 tan = normalize(gl_FrontFacing ? vT : -vT);
    vec3 bit = normalize(gl_FrontFacing ? vB : -vB);
    int map = int(d.extra.z);
    vec3 albedo = ((map & 1) != 0) ? texture(uBase, uv).rgb : d.color.rgb;
    if ((map & 2) != 0) {
        vec3 tn = texture(uNrm, uv).rgb * 2.0 - 1.0;
        nrm = normalize(tan * tn.x + bit * tn.y + nrm * tn.z);
    }
    if ((map & 16) != 0) {
        float h = texture(uHeight, uv).r;
        vec2 dx = dFdx(uv);
        vec2 dy = dFdy(uv);
        float hx = texture(uHeight, uv + dx).r - h;
        float hy = texture(uHeight, uv + dy).r - h;
        nrm = normalize(nrm - tan * hx * 4.0 - bit * hy * 4.0);
    }
    float metal = ((map & 4) != 0) ? texture(uMetal, uv).r : 0.0;
    float rough = ((map & 8) != 0) ? texture(uRough, uv).r : 0.55;
    vec3 col = (d.flags.x < 0.5) ? albedo : shade(nrm, vEye, albedo, metal, rough);
    col = mix(col, u.fog.rgb, vFog);
    frag = vec4(col, vAlpha);
}
