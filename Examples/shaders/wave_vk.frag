// The fragment half, in the dialect Vulkan takes.
//
// The push-constant block is declared the same in both stages, which is what
// lets one range serve them both. uTexOn has no counterpart here: the Vulkan
// renderer binds no texture to a custom shader.
#version 450

layout(push_constant) uniform Nexa {
    mat4 uMVP;
    vec4 uUser[4];
} nexa;

layout(location = 0) in vec3 vCol;
layout(location = 0) out vec4 oCol;

void main() {
    float uTime = nexa.uUser[0].x;

    // A slow pulse, so it is obvious the shader is the one drawing.
    vec3 c = vCol * (0.75 + 0.25 * sin(uTime * 2.0));
    oCol = vec4(c, 1.0);
}
