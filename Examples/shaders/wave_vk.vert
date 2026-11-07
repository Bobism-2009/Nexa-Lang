// The wave vertex shader, in the dialect Vulkan takes.
//
// Same effect as wave.vert, written for the other renderer. NexaC compiles this
// to wave_vk.vert.spv as part of the build -- it offers to install
// glslangValidator the first time it needs to -- and std/gfx3d loads the .spv
// it finds beside this file.
//
// What the Vulkan renderer hands a shader is smaller than what OpenGL does:
// position and colour per vertex, and one push constant. No uv, no sampler.
#version 450

layout(location = 0) in vec3 aPos;
layout(location = 1) in vec3 aCol;

// uMVP is camera * model, as on the other renderer. uUser is where
// gfx3d.shader_set writes: a name takes the next free float the first time it
// is set, so the program's first shader_set is uUser[0].x -- here, uTime.
layout(push_constant) uniform Nexa {
    mat4 uMVP;
    vec4 uUser[4];
} nexa;

layout(location = 0) out vec3 vCol;

void main() {
    float uTime = nexa.uUser[0].x;

    // Ride a wave along x, so the shape visibly moves without the program
    // changing a single vertex.
    vec3 p = aPos;
    p.y += sin(p.x * 2.0 + uTime) * 0.15;

    vCol = aCol;
    gl_Position = nexa.uMVP * vec4(p, 1.0);
}
