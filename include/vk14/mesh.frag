#version 450
layout(location = 0) in vec3 vFront;
layout(location = 1) in vec3 vBack;
layout(location = 2) in vec3 vSunF;
layout(location = 3) in vec3 vSunB;
layout(location = 4) in vec4 vLight;
layout(location = 5) in float vFog;
layout(location = 6) in float vAlpha;
layout(set = 0, binding = 0) uniform Scene {
    vec4 lpos[16];
    vec4 ldif[16];
    vec4 lspot[16];
    vec4 lsp[16];
    vec4 ambFill;
    vec4 fog;
    ivec4 info;
} u;
layout(set = 0, binding = 2) uniform sampler2D uShadow;
layout(location = 0) out vec4 frag;
float shadow_pcf(vec4 clip) {
    if (clip.w <= 0.0) return 1.0;
    vec3 p = clip.xyz / clip.w * 0.5 + 0.5;
    if (p.z > 1.0) return 1.0;
    float s = 0.0;
    vec2 o = vec2(1.5 / 2048.0);
    for (int y = -1; y <= 1; y++) {
        for (int x = -1; x <= 1; x++) {
            float d = texture(uShadow, p.xy + vec2(float(x), float(y)) * o).r;
            s += (p.z - 0.002 > d) ? 0.0 : 1.0;
        }
    }
    return s / 9.0;
}
void main() {
    vec3 col = gl_FrontFacing ? vFront : vBack;
    vec3 sun = gl_FrontFacing ? vSunF : vSunB;
    if (u.info.z > 0) col -= sun * (1.0 - shadow_pcf(vLight));
    col = mix(clamp(col, 0.0, 1.0), u.fog.rgb, vFog);
    frag = vec4(col, vAlpha);
}
