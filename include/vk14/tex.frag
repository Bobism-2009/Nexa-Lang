#version 450
layout(location = 0) in vec3 vCol;
layout(location = 1) in vec2 vUv;
layout(push_constant) uniform PC { layout(offset = 64) float cut; } pc;
layout(set = 0, binding = 0) uniform sampler2D uTex;
layout(location = 0) out vec4 frag;
void main() {
    vec4 t = texture(uTex, vUv);
    if (pc.cut > 0.5 && t.a < 0.5) discard;
    frag = vec4(t.rgb * vCol, 1.0);
}
