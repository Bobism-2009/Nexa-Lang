// The fragment half of the wave shader.
//
// uTexOn is 1 while a gfx3d.texture is bound and 0 otherwise, which is how the
// built-in shader decides whether to sample -- worth keeping, or a textured
// model drawn with this would come out untextured.
precision mediump float;

uniform sampler2D uTex;
uniform float uTexOn;
uniform float uTime;

varying vec3 vCol;
varying vec2 vUV;

void main() {
    vec3 c = vCol;
    if (uTexOn > 0.5) {
        vec4 t = texture2D(uTex, vUV);
        if (t.a < 0.5) discard;
        c *= t.rgb;
    }
    // A slow pulse, so it is obvious the shader is the one drawing.
    c *= 0.75 + 0.25 * sin(uTime * 2.0);
    gl_FragColor = vec4(c, 1.0);
}
