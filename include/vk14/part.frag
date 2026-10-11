#version 450
layout(location = 0) in vec2 vUv;
layout(location = 1) in vec4 vColor;
layout(location = 2) in float vFog;
layout(set = 0, binding = 1) uniform sampler2D uTex;
layout(set = 0, binding = 0) uniform Cam {
    mat4 view;
    mat4 proj;
    vec4 fog;
} u;
layout(location = 0) out vec4 frag;
void main() {
    vec4 t = texture(uTex, vUv);
    vec3 col = mix(t.rgb * vColor.rgb, u.fog.rgb, vFog);
    frag = vec4(col, t.a * vColor.a);
}
