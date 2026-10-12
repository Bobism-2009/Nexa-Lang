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
    if (p.z > 1.0 || p.x < 0.0 || p.x > 1.0 || p.y < 0.0 || p.y > 1.0) return 1.0;
    const vec2 kOff[16] = vec2[](
        vec2(-0.94201624, -0.39906216), vec2(0.94558609, -0.76890725),
        vec2(-0.094184101, -0.92938870), vec2(0.34495938, 0.29387760),
        vec2(-0.91588581, 0.45771432), vec2(-0.81544232, -0.87912464),
        vec2(-0.38277543, 0.27676845), vec2(0.97484398, 0.75648379),
        vec2(0.44323325, -0.97511554), vec2(0.53742981, -0.47373420),
        vec2(-0.26496911, -0.41893023), vec2(0.79197514, 0.19048895),
        vec2(-0.24188840, 0.99706507), vec2(-0.81409955, 0.91437590),
        vec2(0.19984126, 0.78641367), vec2(0.14383161, -0.14100790)
    );
    float s = 0.0;
    float scale = 48.0 / 4096.0;
    for (int i = 0; i < 16; i++) {
        float d = texture(uShadow, p.xy + kOff[i] * scale).r;
        s += (p.z - 0.0012 > d) ? 0.0 : 1.0;
    }
    return s / 16.0;
}
void main() {
    vec3 col = gl_FrontFacing ? vFront : vBack;
    vec3 sun = gl_FrontFacing ? vSunF : vSunB;
    if (u.info.z > 0) col -= sun * (1.0 - shadow_pcf(vLight));
    col = mix(clamp(col, 0.0, 1.0), u.fog.rgb, vFog);
    if (vAlpha > 1.5) {
        frag = vec4(vFront, 1.0);
        return;
    }
    frag = vec4(col, vAlpha);
}
