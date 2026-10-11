#version 450
layout(location = 0) in vec2 vUv;
layout(set = 0, binding = 0) uniform sampler2D uTex;
layout(location = 0) out vec4 frag;
void main() { frag = texture(uTex, vUv); }
