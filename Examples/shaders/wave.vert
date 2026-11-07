// A custom vertex shader for std/gfx3d.
//
// What gfx3d hands every shader, and all it hands them: aPos, aCol and aUV per
// vertex, and uMVP, uTex and uTexOn as uniforms. Declare the ones you use.
// uTime is this shader's own -- gfx3d.shader_set(sh, "uTime", t) writes it.
attribute vec3 aPos;
attribute vec3 aCol;
attribute vec2 aUV;

uniform mat4 uMVP;
uniform float uTime;

varying vec3 vCol;
varying vec2 vUV;

void main() {
    // Ride a wave along x, so the shape visibly moves without the program
    // changing a single vertex.
    vec3 p = aPos;
    p.y += sin(p.x * 2.0 + uTime) * 0.15;

    vCol = aCol;
    vUV = aUV;
    gl_Position = uMVP * vec4(p, 1.0);
}
