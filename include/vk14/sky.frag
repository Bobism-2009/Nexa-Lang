#version 450
layout(location = 0) in vec3 vDir;
layout(push_constant) uniform PC { mat4 view; vec4 fog; } pc;
layout(set = 0, binding = 0) uniform sampler2D uFace0;
layout(set = 0, binding = 1) uniform sampler2D uFace1;
layout(set = 0, binding = 2) uniform sampler2D uFace2;
layout(set = 0, binding = 3) uniform sampler2D uFace3;
layout(set = 0, binding = 4) uniform sampler2D uFace4;
layout(set = 0, binding = 5) uniform sampler2D uFace5;
layout(location = 0) out vec4 frag;
void main() {
    vec3 d = normalize(vDir);
    vec3 a = abs(d);
    vec2 uv;
    vec3 c;
    if (a.x >= a.y && a.x >= a.z) {
        if (d.x > 0.0) { uv = vec2(-d.z, d.y) / a.x; c = texture(uFace0, uv * 0.5 + 0.5).rgb; }
        else           { uv = vec2( d.z, d.y) / a.x; c = texture(uFace1, uv * 0.5 + 0.5).rgb; }
    } else if (a.y >= a.z) {
        if (d.y > 0.0) { uv = vec2( d.x,-d.z) / a.y; c = texture(uFace2, uv * 0.5 + 0.5).rgb; }
        else           { uv = vec2( d.x, d.z) / a.y; c = texture(uFace3, uv * 0.5 + 0.5).rgb; }
    } else {
        if (d.z > 0.0) { uv = vec2( d.x, d.y) / a.z; c = texture(uFace4, uv * 0.5 + 0.5).rgb; }
        else           { uv = vec2(-d.x, d.y) / a.z; c = texture(uFace5, uv * 0.5 + 0.5).rgb; }
    }
    float horizon = pow(clamp(1.0 - abs(d.y), 0.0, 1.0), 1.4);
    c = mix(c, pc.fog.rgb, horizon * pc.fog.w);
    frag = vec4(c, 1.0);
}
