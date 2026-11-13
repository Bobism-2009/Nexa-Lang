// gfx3d.shadow. The light's picture of the room, then the camera's, with
// whatever the light could not see drawn darker. A program that never calls
// it does not run this, so its source stays as written.
inline void gfx3dApplyShadow(std::string& s) {
    auto put = [&](const std::string& from, const std::string& to) {
        size_t at = s.find(from);
        if (at == std::string::npos) return;
        s.replace(at, from.size(), to);
    };
    auto all = [&](const std::string& from, const std::string& to) {
        size_t at = 0;
        while ((at = s.find(from, at)) != std::string::npos) {
            s.replace(at, from.size(), to);
            at += to.size();
        }
    };

    // The built-in shaders. A program's own gfx3d.shader is left alone: it
    // has no uShadowOn, so these lines are not in it.
    all("    \"uniform mat4 uMVP;\\n\"\n",
        "    \"uniform mat4 uMVP;\\n\"\n"
        "    \"uniform mat4 uShadowVP;\\n\"\n");
    all("    \"varying vec3 vCol;\\n\"\n",
        "    \"varying vec3 vCol;\\n\"\n"
        "    \"varying vec4 vLight;\\n\"\n");
    all("    \"out vec3 vCol;\\n\"\n",
        "    \"out vec3 vCol;\\n\"\n"
        "    \"out vec4 vLight;\\n\"\n");
    all("    \"in vec3 vCol;\\n\"\n",
        "    \"in vec3 vCol;\\n\"\n"
        "    \"in vec4 vLight;\\n\"\n");
    all("    \"  gl_Position = uMVP * vec4(aPos, 1.0);\\n\"\n",
        "    \"  vLight = uShadowVP * vec4(aPos, 1.0);\\n\"\n"
        "    \"  gl_Position = uMVP * vec4(aPos, 1.0);\\n\"\n");
    all("    \"uniform float uTexOn;\\n\"\n",
        "    \"uniform float uTexOn;\\n\"\n"
        "    \"uniform float uShadowOn;\\n\"\n"
        "    \"uniform float uShadowLine;\\n\"\n"
        "    \"uniform sampler2D uShadowTex;\\n\"\n"
        "    \"uniform float uShadowSx;\\n\"\n"
        "    \"uniform float uShadowSy;\\n\"\n");
    // The light's picture stores the near side. A face pointing away from
    // the light is already dark from the shading, so only a face the light
    // shines on is compared. WebGL 1 needs the derivative extension.
    const char* dark =
        "    \"  if (uShadowOn < -0.5) {\\n\"\n"
        "    \"    float z = gl_FragCoord.z;\\n\"\n"
        "    \"    vec3 e = fract(vec3(1.0, 255.0, 65025.0) * z);\\n\"\n"
        "    \"    e.xy -= e.yz * (1.0 / 255.0);\\n\"\n"
        "    \"    c = e;\\n\"\n"
        "    \"  } else if (uShadowOn > 0.5 && vLight.w > 0.0) {\\n\"\n"
        "    \"    vec3 p = vLight.xyz / vLight.w;\\n\"\n"
        "    \"    float su = (p.x * 0.5 + 0.5) * uShadowSx;\\n\"\n"
        "    \"    float sv = (p.y * 0.5 + 0.5) * uShadowSy;\\n\"\n"
        "    \"    float sz = p.z * 0.5 + 0.5;\\n\"\n"
        "    \"    vec3 n = cross(dFdx(p), dFdy(p));\\n\"\n"
        "    \"    float slope = abs(dFdx(sz)) + abs(dFdy(sz));\\n\"\n"
        "    \"    float bias = 0.005 + slope;\\n\"\n"
        "    \"    if (bias > 0.012) bias = 0.012;\\n\"\n"
        "    \"    vec2 uv = vec2(su, sv);\\n\"\n"
        "    \"    vec2 dx = dFdx(uv);\\n\"\n"
        "    \"    vec2 dy = dFdy(uv);\\n\"\n";
    // One sample in the middle of a shadow stays dark. A sample on the
    // edge takes its neighbours too, so the outline fades instead of
    // stepping. A grid line has no face; where it runs through a shadow
    // it is left out, so the line does not cut across the dark.
    // A shadow's middle stays solid. Only the rim, where a neighbour a
    // few texels away disagrees, is averaged, so a close view does not
    // show the map's steps. The body of a lit curve never reaches that.
    auto shade = [](const char* tex) {
        return std::string(
            "    \"    if ((n.z > 0.0 || uShadowLine > 0.5) && su > 0.0 && su < uShadowSx && sv > 0.0 && sv < uShadowSy) {\\n\"\n"
            "    \"      vec3 e = ") + tex + "(uShadowTex, uv).rgb;\\n\"\n"
            "    \"      float diff = sz - bias - dot(e, vec3(1.0, 1.0/255.0, 1.0/65025.0));\\n\"\n"
            "    \"      if (uShadowLine > 0.5) {\\n\"\n"
            "    \"        if (diff > -0.006) discard;\\n\"\n"
            "    \"      } else {\\n\"\n"
            "    \"        float occ = 0.0;\\n\"\n"
            "    \"        float cnt = 0.0;\\n\"\n"
            "    \"        for (int j = -2; j <= 2; j++) {\\n\"\n"
            "    \"          for (int i = -2; i <= 2; i++) {\\n\"\n"
            "    \"            vec2 q = uv + dx * float(i) + dy * float(j);\\n\"\n"
            "    \"            if (q.x > 0.0 && q.x < uShadowSx && q.y > 0.0 && q.y < uShadowSy) {\\n\"\n"
            "    \"              e = " + tex + "(uShadowTex, q).rgb;\\n\"\n"
            "    \"              occ += (sz - bias - dot(e, vec3(1.0, 1.0/255.0, 1.0/65025.0)) > 0.008) ? 1.0 : 0.0;\\n\"\n"
            "    \"              cnt += 1.0;\\n\"\n"
            "    \"            }\\n\"\n"
            "    \"          }\\n\"\n"
            "    \"        }\\n\"\n"
            "    \"        float w = cnt > 0.0 ? occ / cnt : 0.0;\\n\"\n"
            "    \"        c *= mix(1.0, 0.42, w);\\n\"\n"
            "    \"      }\\n\"\n"
            "    \"    }\\n\"\n"
            "    \"  }\\n\"\n";
    };
    std::string shade2 = shade("texture2D");
    std::string shade1 = shade("texture");
    all("    \"precision mediump float;\\n\"\n"
        "    \"uniform sampler2D uTex;\\n\"\n"
        "    \"uniform float uTexOn;\\n\"\n",
        "    \"#extension GL_OES_standard_derivatives : enable\\n\"\n"
        "    \"precision mediump float;\\n\"\n"
        "    \"uniform sampler2D uTex;\\n\"\n"
        "    \"uniform float uTexOn;\\n\"\n");
    all("    \"  gl_FragColor = vec4(c, 1.0);\\n\"\n",
        std::string(dark) + shade2 +
        "    \"  gl_FragColor = vec4(c, 1.0);\\n\"\n");
    all("    \"  fragColor = vec4(c, 1.0);\\n\"\n",
        std::string(dark) + shade1 +
        "    \"  fragColor = vec4(c, 1.0);\\n\"\n");

    put("static void __nexa_g3_apply_camera(void) {\n",
        "static int __nexa_g3_shadow_on = 0;\n"
        "static int __nexa_g3_shadow_have = 0;\n"
        "static int __nexa_g3_shadow_biased = 0;\n"
        "static int __nexa_g3_shadow_lines = 0;\n"
        "static float __nexa_g3_shadow_vp[16];\n"
        "static float __nexa_g3_shadow_lp[16];\n"
        "static const float* __nexa_g3_shadow_model = nullptr;\n"
        "static float __nexa_g3_shadow_sx = 1.0f, __nexa_g3_shadow_sy = 1.0f;\n"
        "static unsigned __nexa_g3_shadow_tex = 0;\n"
        "static int __nexa_g3_shadow_tw = 0, __nexa_g3_shadow_th = 0;\n"
        "static unsigned __nexa_g3_shadow_cast = 0, __nexa_g3_shadow_recv = 0;\n"
        "static int __nexa_g3_shadow_tried = 0;\n"
        "static int __nexa_g3_shadow_u_vp = -1, __nexa_g3_shadow_u_tex = -1;\n"
        "static int __nexa_g3_shadow_u_sx = -1, __nexa_g3_shadow_u_sy = -1, __nexa_g3_shadow_u_line = -1;\n"
        "static int __nexa_g3_shadow_u_to = -1;\n"
        "static int __nexa_g3_shadow_bound = 0;\n"
        "static void __nexa_g3_shadow_look(void);\n"
        "static void __nexa_g3_shadow_guard(void);\n"
        "static void __nexa_g3_shadow_lp_now(void);\n"
        "static void __nexa_g3_shadow_use(void);\n"
        "static void __nexa_g3_shadow_use33(void);\n"
        "static void __nexa_g3_shadow_bind(void);\n"
        "static void __nexa_g3_shadow_normals(const float* n);\n"
        "static void __nexa_g3_shadow_make(void);\n"
        "static void __nexa_g3_shadow_copy(void);\n"
        "static void __nexa_g3_shadow_unbind(void);\n"
        "\n"
        "static void __nexa_g3_apply_camera(void) {\n");
    put("    __nexa_g3_look_at(__nexa_g3.view, __nexa_g3.eye, __nexa_g3.target);\n",
        "    if (__nexa_g3_shadow_on) __nexa_g3_shadow_look();\n"
        "    else __nexa_g3_look_at(__nexa_g3.view, __nexa_g3.eye, __nexa_g3.target);\n");
    put("    __nexa_g3_platform_camera();\n",
        "    __nexa_g3_platform_camera();\n"
        "    __nexa_g3_shadow_guard();\n");

    all("static int __nexa_g3_mesh_submit(__nexa_G3Mesh& m, __nexa_G3Mesh::Lit& L, const float* M) {\n",
        "static int __nexa_g3_mesh_submit(__nexa_G3Mesh& m, __nexa_G3Mesh::Lit& L, const float* M) {\n"
        "    __nexa_g3_shadow_model = M;\n"
        "    __nexa_g3_shadow_lines = 0;\n"
        "    __nexa_g3_shadow_lp_now();\n");
    put("static void __nexa_g3_batch_submit(void) {\n#if defined(_WIN32)\n    if (__nexa_g3.vulkan) {\n",
        "static void __nexa_g3_batch_submit(void) {\n"
        "    __nexa_g3_shadow_model = nullptr;\n"
        "    __nexa_g3_shadow_lines = __nexa_g3_prim;\n"
        "    __nexa_g3_shadow_lp_now();\n"
        "    if (__nexa_g3_shadow_on && __nexa_g3_prim) return;\n"
        "#if defined(_WIN32)\n    if (__nexa_g3.vulkan) {\n");
    put("static void __nexa_g3_batch_submit(void) {\n    if (!__nexa_g3_prog || !__nexa_g3_vbo) return;\n",
        "static void __nexa_g3_batch_submit(void) {\n"
        "    __nexa_g3_shadow_model = nullptr;\n"
        "    __nexa_g3_shadow_lines = __nexa_g3_prim;\n"
        "    __nexa_g3_shadow_lp_now();\n"
        "    if (__nexa_g3_shadow_on && __nexa_g3_prim) return;\n"
        "    if (!__nexa_g3_prog || !__nexa_g3_vbo) return;\n");
    put("    if (__nexa_g3.core33) return __nexa_g3_core_mesh(m, L, M);\n",
        "    if (__nexa_g3.core33) return __nexa_g3_core_mesh(m, L, M);\n"
        "    __nexa_g3_shadow_bind();\n");
    put("    if (__nexa_g3.core33) { __nexa_g3_core_batch(); return; }\n",
        "    if (__nexa_g3.core33) { __nexa_g3_core_batch(); return; }\n"
        "    __nexa_g3_shadow_bind();\n");
    // The sphere's own normals, so the shadow follows the smooth shade
    // instead of cutting off on each flat triangle. Lines have none.
    put("    __nexa_gl.ColorPointer(3, NEXA_GL_FLOAT, 0, col);\n",
        "    __nexa_gl.ColorPointer(3, NEXA_GL_FLOAT, 0, col);\n"
        "    __nexa_g3_shadow_normals(m.nrm.empty() ? nullptr : m.nrm.data());\n");
    put("    __nexa_gl.DrawArrays(NEXA_GL_TRIANGLES, 0, (__nexa_GLsizei)(m.pos.size() / 3));\n"
        "    if (cut) {\n"
        "        __nexa_gl.Disable(NEXA_GL_ALPHA_TEST);\n",
        "    __nexa_gl.DrawArrays(NEXA_GL_TRIANGLES, 0, (__nexa_GLsizei)(m.pos.size() / 3));\n"
        "    __nexa_g3_shadow_normals(nullptr);\n"
        "    if (cut) {\n"
        "        __nexa_gl.Disable(NEXA_GL_ALPHA_TEST);\n");

    all("    __nexa_g33.UniformMatrix4fv(__nexa_g33_u_mvp, 1, 0, mvp);\n",
        "    __nexa_g33.UniformMatrix4fv(__nexa_g33_u_mvp, 1, 0, mvp);\n"
        "    __nexa_g3_shadow_use33();\n");
    all("    glUniformMatrix4fv(__nexa_g3_u_mvp, 1, 0, mvp);\n",
        "    glUniformMatrix4fv(__nexa_g3_u_mvp, 1, 0, mvp);\n"
        "    __nexa_g3_shadow_use();\n");

    put("static __nexa_GLint __nexa_g33_u_mvp = -1;\n",
        "static __nexa_GLint __nexa_g33_u_mvp = -1;\n"
        "static __nexa_GLint __nexa_g33_u_shadowVP = -1, __nexa_g33_u_shadowOn = -1, __nexa_g33_u_shadowLine = -1;\n"
        "static __nexa_GLint __nexa_g33_u_shadowTex = -1, __nexa_g33_u_shadowSx = -1, __nexa_g33_u_shadowSy = -1;\n");
    put("static __nexa_GLint  __nexa_g3_u_mvp = -1;\n",
        "static __nexa_GLint  __nexa_g3_u_mvp = -1;\n"
        "static __nexa_GLint  __nexa_g3_u_shadowVP = -1, __nexa_g3_u_shadowOn = -1, __nexa_g3_u_shadowLine = -1;\n"
        "static __nexa_GLint  __nexa_g3_u_shadowTex = -1, __nexa_g3_u_shadowSx = -1, __nexa_g3_u_shadowSy = -1;\n");
    put("    __nexa_g33_u_mvp = __nexa_g33.GetUniformLocation(__nexa_g33_prog, \"uMVP\");\n",
        "    __nexa_g33_u_mvp = __nexa_g33.GetUniformLocation(__nexa_g33_prog, \"uMVP\");\n"
        "    __nexa_g33_u_shadowVP = __nexa_g33.GetUniformLocation(__nexa_g33_prog, \"uShadowVP\");\n"
        "    __nexa_g33_u_shadowOn = __nexa_g33.GetUniformLocation(__nexa_g33_prog, \"uShadowOn\");\n"
        "    __nexa_g33_u_shadowLine = __nexa_g33.GetUniformLocation(__nexa_g33_prog, \"uShadowLine\");\n"
        "    __nexa_g33_u_shadowTex = __nexa_g33.GetUniformLocation(__nexa_g33_prog, \"uShadowTex\");\n"
        "    __nexa_g33_u_shadowSx = __nexa_g33.GetUniformLocation(__nexa_g33_prog, \"uShadowSx\");\n"
        "    __nexa_g33_u_shadowSy = __nexa_g33.GetUniformLocation(__nexa_g33_prog, \"uShadowSy\");\n");
    put("    __nexa_g3_u_mvp = glGetUniformLocation(__nexa_g3_prog, \"uMVP\");\n",
        "    __nexa_g3_u_mvp = glGetUniformLocation(__nexa_g3_prog, \"uMVP\");\n"
        "    __nexa_g3_u_shadowVP = glGetUniformLocation(__nexa_g3_prog, \"uShadowVP\");\n"
        "    __nexa_g3_u_shadowOn = glGetUniformLocation(__nexa_g3_prog, \"uShadowOn\");\n"
        "    __nexa_g3_u_shadowLine = glGetUniformLocation(__nexa_g3_prog, \"uShadowLine\");\n"
        "    __nexa_g3_u_shadowTex = glGetUniformLocation(__nexa_g3_prog, \"uShadowTex\");\n"
        "    __nexa_g3_u_shadowSx = glGetUniformLocation(__nexa_g3_prog, \"uShadowSx\");\n"
        "    __nexa_g3_u_shadowSy = glGetUniformLocation(__nexa_g3_prog, \"uShadowSy\");\n");
    put("    if (utex >= 0) __nexa_g33.Uniform1i(utex, 0);\n",
        "    if (utex >= 0) __nexa_g33.Uniform1i(utex, 0);\n"
        "    if (__nexa_g33_u_shadowTex >= 0) __nexa_g33.Uniform1i(__nexa_g33_u_shadowTex, 1);\n");
    put("    glUniform1i(__nexa_g3_u_tex, 0);\n",
        "    glUniform1i(__nexa_g3_u_tex, 0);\n"
        "    if (__nexa_g3_u_shadowTex >= 0) glUniform1i(__nexa_g3_u_shadowTex, 1);\n");

    // The glass is fixed-function. A shadow program left bound would hide it.
    put("static void __nexa_g3_mirror_pane(void) {\n",
        "static void __nexa_g3_mirror_pane(void) {\n"
        "    __nexa_g3_shadow_unbind();\n");
    put("    if (__nexa_g3_u_mirrorOn >= 0) glUniform1f(__nexa_g3_u_mirrorOn, 1.0f);\n",
        "    if (__nexa_g3_u_mirrorOn >= 0) glUniform1f(__nexa_g3_u_mirrorOn, 1.0f);\n"
        "    if (__nexa_g3_u_shadowOn >= 0) glUniform1f(__nexa_g3_u_shadowOn, 0.0f);\n");
    put("        if (__nexa_g33_u_mirrorOn >= 0) __nexa_g33.Uniform1f(__nexa_g33_u_mirrorOn, 1.0f);\n",
        "        if (__nexa_g33_u_mirrorOn >= 0) __nexa_g33.Uniform1f(__nexa_g33_u_mirrorOn, 1.0f);\n"
        "        if (__nexa_g33_u_shadowOn >= 0) __nexa_g33.Uniform1f(__nexa_g33_u_shadowOn, 0.0f);\n");

    {
        std::string extra =
            "void glActiveTexture(__nexa_GLenum);\n"
            "void glPolygonOffset(__nexa_GLfloat, __nexa_GLfloat);\n";
        if (s.find("glCopyTexSubImage2D") == std::string::npos) {
            extra += "void glCopyTexSubImage2D(__nexa_GLenum, __nexa_GLint, __nexa_GLint, __nexa_GLint, __nexa_GLint, __nexa_GLint, __nexa_GLsizei, __nexa_GLsizei);\n";
        }
        put("void glUniform1f(__nexa_GLint, __nexa_GLfloat);\n",
            "void glUniform1f(__nexa_GLint, __nexa_GLfloat);\n" + extra);
    }

    // Vulkan: a depth picture on the simple draw path. The scene renderer's
    // shadow map is a different vertex and a different light, so this one is
    // its own image and is spliced in only here.
    put("inline int begin(float r, float g, float b, int w, int h) {\n    Dev& d = dev();\n    if (!d.device) return 0;\n",
        "inline int begin(float r, float g, float b, int w, int h) {\n    Dev& d = dev();\n    if (!d.device) return 0;\n"
        "    if (__nexa_vk_shadow_begin(d, r, g, b, w, h)) return 1;\n");
    put("    if (!d.framing || !xyzrgb || nvert <= 0 || !mvp) return;\n",
        "    if (!d.framing || !xyzrgb || nvert <= 0 || !mvp) return;\n"
        "    if (__nexa_vk_shadow_draw(d, xyzrgb, nvert, lines, twoSided, mvp)) return;\n");
    put("    if (d.device && d.DeviceWaitIdle) d.DeviceWaitIdle(d.device);\n    scene_down(d);\n",
        "    if (d.device && d.DeviceWaitIdle) d.DeviceWaitIdle(d.device);\n"
        "    __nexa_vk_shadow_down(d);\n    scene_down(d);\n");
    put("inline void shadow_end() { detail::shadow_end(); }\n",
        "inline void shadow_end() { detail::shadow_end(); }\n"
        "inline int shadow_hold_end() { return detail::__nexa_vk_shadow_hold_end(); }\n");

    std::string vk = R"NEXA_SH(
static const uint32_t __nexa_sh_dvs[] = {
    119734787, 65536, 851979, 35, 0, 131089, 1, 393227, 1, 1280527431, 1685353262, 808793134,
    0, 196622, 0, 1, 458767, 0, 4, 1852399981, 0, 13, 25, 196611,
    2, 450, 655364, 1197427783, 1279741775, 1885560645, 1953718128, 1600482425, 1701734764, 1919509599, 1769235301, 25974,
    524292, 1197427783, 1279741775, 1852399429, 1685417059, 1768185701, 1952671090, 6649449, 262149, 4, 1852399981, 0,
    393221, 11, 1348430951, 1700164197, 2019914866, 0, 393222, 11, 0, 1348430951, 1953067887, 7237481,
    458758, 11, 1, 1348430951, 1953393007, 1702521171, 0, 458758, 11, 2, 1130327143, 1148217708,
    1635021673, 6644590, 458758, 11, 3, 1130327143, 1147956341, 1635021673, 6644590, 196613, 13, 0,
    196613, 17, 17232, 262150, 17, 0, 7370349, 196613, 19, 25456, 262149, 25,
    1936674913, 0, 196679, 11, 2, 327752, 11, 0, 11, 0, 327752, 11,
    1, 11, 1, 327752, 11, 2, 11, 3, 327752, 11, 3, 11,
    4, 196679, 17, 2, 262216, 17, 0, 5, 327752, 17, 0, 7,
    16, 327752, 17, 0, 35, 0, 262215, 25, 30, 0, 131091, 2,
    196641, 3, 2, 196630, 6, 32, 262167, 7, 6, 4, 262165, 8,
    32, 0, 262187, 8, 9, 1, 262172, 10, 6, 9, 393246, 11,
    7, 6, 10, 10, 262176, 12, 3, 11, 262203, 12, 13, 3,
    262165, 14, 32, 1, 262187, 14, 15, 0, 262168, 16, 7, 4,
    196638, 17, 16, 262176, 18, 9, 17, 262203, 18, 19, 9, 262176,
    20, 9, 16, 262167, 23, 6, 3, 262176, 24, 1, 23, 262203,
    24, 25, 1, 262187, 6, 27, 1065353216, 262176, 33, 3, 7, 327734,
    2, 4, 0, 3, 131320, 5, 327745, 20, 21, 19, 15, 262205,
    16, 22, 21, 262205, 23, 26, 25, 327761, 6, 28, 26, 0,
    327761, 6, 29, 26, 1, 327761, 6, 30, 26, 2, 458832, 7,
    31, 28, 29, 30, 27, 327825, 7, 32, 22, 31, 327745, 33,
    34, 13, 15, 196670, 34, 32, 65789, 65592
};
static const uint32_t __nexa_sh_cvs[] = {
    119734787, 65536, 851979, 49, 0, 131089, 1, 393227, 1, 1280527431, 1685353262, 808793134,
    0, 196622, 0, 1, 655375, 0, 4, 1852399981, 0, 13, 25, 36,
    37, 39, 196611, 2, 450, 655364, 1197427783, 1279741775, 1885560645, 1953718128, 1600482425, 1701734764,
    1919509599, 1769235301, 25974, 524292, 1197427783, 1279741775, 1852399429, 1685417059, 1768185701, 1952671090, 6649449, 262149,
    4, 1852399981, 0, 393221, 11, 1348430951, 1700164197, 2019914866, 0, 393222, 11, 0,
    1348430951, 1953067887, 7237481, 458758, 11, 1, 1348430951, 1953393007, 1702521171, 0, 458758, 11,
    2, 1130327143, 1148217708, 1635021673, 6644590, 458758, 11, 3, 1130327143, 1147956341, 1635021673, 6644590,
    196613, 13, 0, 196613, 17, 17232, 262150, 17, 0, 7370349, 327686, 17,
    1, 1751607660, 116, 196613, 19, 25456, 262149, 25, 1936674913, 0, 262149, 36,
    1819231094, 0, 262149, 37, 1819231073, 0, 262149, 39, 1734954102, 29800, 196679, 11,
    2, 327752, 11, 0, 11, 0, 327752, 11, 1, 11, 1, 327752,
    11, 2, 11, 3, 327752, 11, 3, 11, 4, 196679, 17, 2,
    262216, 17, 0, 5, 327752, 17, 0, 7, 16, 327752, 17, 0,
    35, 0, 262216, 17, 1, 5, 327752, 17, 1, 7, 16, 327752,
    17, 1, 35, 64, 262215, 25, 30, 0, 262215, 36, 30, 0,
    262215, 37, 30, 1, 262215, 39, 30, 1, 131091, 2, 196641, 3,
    2, 196630, 6, 32, 262167, 7, 6, 4, 262165, 8, 32, 0,
    262187, 8, 9, 1, 262172, 10, 6, 9, 393246, 11, 7, 6,
    10, 10, 262176, 12, 3, 11, 262203, 12, 13, 3, 262165, 14,
    32, 1, 262187, 14, 15, 0, 262168, 16, 7, 4, 262174, 17,
    16, 16, 262176, 18, 9, 17, 262203, 18, 19, 9, 262176, 20,
    9, 16, 262167, 23, 6, 3, 262176, 24, 1, 23, 262203, 24,
    25, 1, 262187, 6, 27, 1065353216, 262176, 33, 3, 7, 262176, 35,
    3, 23, 262203, 35, 36, 3, 262203, 24, 37, 1, 262203, 33,
    39, 3, 262187, 14, 40, 1, 327734, 2, 4, 0, 3, 131320,
    5, 327745, 20, 21, 19, 15, 262205, 16, 22, 21, 262205, 23,
    26, 25, 327761, 6, 28, 26, 0, 327761, 6, 29, 26, 1,
    327761, 6, 30, 26, 2, 458832, 7, 31, 28, 29, 30, 27,
    327825, 7, 32, 22, 31, 327745, 33, 34, 13, 15, 196670, 34,
    32, 262205, 23, 38, 37, 196670, 36, 38, 327745, 20, 41, 19,
    40, 262205, 16, 42, 41, 262205, 23, 43, 25, 327761, 6, 44,
    43, 0, 327761, 6, 45, 43, 1, 327761, 6, 46, 43, 2,
    458832, 7, 47, 44, 45, 46, 27, 327825, 7, 48, 42, 47,
    196670, 39, 48, 65789, 65592
};
static const uint32_t __nexa_sh_cfs[] = {
    119734787, 65536, 851979, 246, 0, 131089, 1, 393227, 1, 1280527431, 1685353262, 808793134,
    0, 196622, 0, 1, 524303, 4, 4, 1852399981, 0, 12, 236, 238,
    196624, 4, 7, 196611, 2, 450, 655364, 1197427783, 1279741775, 1885560645, 1953718128, 1600482425,
    1701734764, 1919509599, 1769235301, 25974, 524292, 1197427783, 1279741775, 1852399429, 1685417059, 1768185701, 1952671090, 6649449,
    262149, 4, 1852399981, 0, 196613, 8, 7629164, 262149, 12, 1734954102, 29800, 196613,
    25, 112, 196613, 34, 30325, 196613, 41, 122, 196613, 47, 30820, 196613,
    50, 31076, 196613, 53, 110, 262149, 59, 1886350451, 101, 262149, 67, 1935763810,
    0, 262149, 77, 1919508840, 0, 196613, 119, 100, 262149, 123, 1634227061, 7827300,
    262149, 128, 1717987684, 0, 196613, 144, 6513519, 196613, 145, 7630435, 196613, 148,
    106, 196613, 158, 105, 196613, 166, 113, 196613, 221, 119, 262149, 236,
    1734439526, 0, 262149, 238, 1819231094, 0, 262215, 12, 30, 1, 262215, 123,
    33, 0, 262215, 123, 34, 0, 262215, 236, 30, 0, 262215, 238,
    30, 0, 131091, 2, 196641, 3, 2, 196630, 6, 32, 262176, 7,
    7, 6, 262187, 6, 9, 1065353216, 262167, 10, 6, 4, 262176, 11,
    1, 10, 262203, 11, 12, 1, 262165, 13, 32, 0, 262187, 13,
    14, 3, 262176, 15, 1, 6, 262187, 6, 18, 0, 131092, 19,
    262167, 23, 6, 3, 262176, 24, 7, 23, 262167, 32, 6, 2,
    262176, 33, 7, 32, 262187, 6, 37, 1056964608, 262187, 13, 42, 2,
    262187, 6, 68, 990057071, 262187, 6, 72, 1011129254, 262176, 76, 7, 19,
    262187, 6, 81, 841731191, 262187, 13, 90, 0, 262187, 13, 103, 1,
    589849, 120, 6, 1, 0, 0, 0, 1, 0, 196635, 121, 120,
    262176, 122, 0, 121, 262203, 122, 123, 0, 262187, 6, 138, 3150224294,
    262165, 146, 32, 1, 262176, 147, 7, 146, 262187, 146, 149, 4294967294,
    262187, 146, 156, 2, 262187, 6, 209, 1006834287, 262187, 146, 217, 1,
    262187, 6, 232, 1054280253, 262176, 235, 3, 10, 262203, 235, 236, 3,
    262176, 237, 1, 23, 262203, 237, 238, 1, 327734, 2, 4, 0,
    3, 131320, 5, 262203, 7, 8, 7, 262203, 24, 25, 7, 262203,
    33, 34, 7, 262203, 7, 41, 7, 262203, 33, 47, 7, 262203,
    33, 50, 7, 262203, 24, 53, 7, 262203, 7, 59, 7, 262203,
    7, 67, 7, 262203, 76, 77, 7, 262203, 7, 119, 7, 262203,
    7, 128, 7, 262203, 7, 144, 7, 262203, 7, 145, 7, 262203,
    147, 148, 7, 262203, 147, 158, 7, 262203, 33, 166, 7, 262203,
    7, 221, 7, 262203, 7, 224, 7, 196670, 8, 9, 327745, 15,
    16, 12, 14, 262205, 6, 17, 16, 327866, 19, 20, 17, 18,
    196855, 22, 0, 262394, 20, 21, 22, 131320, 21, 262205, 10, 26,
    12, 524367, 23, 27, 26, 26, 0, 1, 2, 327745, 15, 28,
    12, 14, 262205, 6, 29, 28, 393296, 23, 30, 29, 29, 29,
    327816, 23, 31, 27, 30, 196670, 25, 31, 262205, 23, 35, 25,
    458831, 32, 36, 35, 35, 0, 1, 327822, 32, 38, 36, 37,
    327760, 32, 39, 37, 37, 327809, 32, 40, 38, 39, 196670, 34,
    40, 327745, 7, 43, 25, 42, 262205, 6, 44, 43, 327813, 6,
    45, 44, 37, 327809, 6, 46, 45, 37, 196670, 41, 46, 262205,
    32, 48, 34, 262351, 32, 49, 48, 196670, 47, 49, 262205, 32,
    51, 34, 262352, 32, 52, 51, 196670, 50, 52, 262205, 23, 54,
    25, 262351, 23, 55, 54, 262205, 23, 56, 25, 262352, 23, 57,
    56, 458764, 23, 58, 1, 68, 55, 57, 196670, 53, 58, 262205,
    6, 60, 41, 262351, 6, 61, 60, 393228, 6, 62, 1, 4,
    61, 262205, 6, 63, 41, 262352, 6, 64, 63, 393228, 6, 65,
    1, 4, 64, 327809, 6, 66, 62, 65, 196670, 59, 66, 262205,
    6, 69, 59, 327809, 6, 70, 68, 69, 196670, 67, 70, 262205,
    6, 71, 67, 327866, 19, 73, 71, 72, 196855, 75, 0, 262394,
    73, 74, 75, 131320, 74, 196670, 67, 72, 131321, 75, 131320, 75,
    262205, 23, 78, 53, 262205, 23, 79, 53, 327828, 6, 80, 78,
    79, 327864, 19, 82, 80, 81, 196670, 77, 82, 327745, 7, 83,
    53, 42, 262205, 6, 84, 83, 327864, 19, 85, 84, 18, 262205,
    19, 86, 77, 327846, 19, 87, 85, 86, 196855, 89, 0, 262394,
    87, 88, 89, 131320, 88, 327745, 7, 91, 34, 90, 262205, 6,
    92, 91, 327866, 19, 93, 92, 18, 131321, 89, 131320, 89, 458997,
    19, 94, 87, 75, 93, 88, 196855, 96, 0, 262394, 94, 95,
    96, 131320, 95, 327745, 7, 97, 34, 90, 262205, 6, 98, 97,
    327864, 19, 99, 98, 9, 131321, 96, 131320, 96, 458997, 19, 100,
    94, 89, 99, 95, 196855, 102, 0, 262394, 100, 101, 102, 131320,
    101, 327745, 7, 104, 34, 103, 262205, 6, 105, 104, 327866, 19,
    106, 105, 18, 131321, 102, 131320, 102, 458997, 19, 107, 100, 96,
    106, 101, 196855, 109, 0, 262394, 107, 108, 109, 131320, 108, 327745,
    7, 110, 34, 103, 262205, 6, 111, 110, 327864, 19, 112, 111,
    9, 131321, 109, 131320, 109, 458997, 19, 113, 107, 102, 112, 108,
    262205, 6, 114, 41, 327864, 19, 115, 114, 9, 327847, 19, 116,
    113, 115, 196855, 118, 0, 262394, 116, 117, 118, 131320, 117, 262205,
    121, 124, 123, 262205, 32, 125, 34, 327767, 10, 126, 124, 125,
    327761, 6, 127, 126, 0, 196670, 119, 127, 262205, 6, 129, 41,
    262205, 6, 130, 67, 327811, 6, 131, 129, 130, 262205, 6, 132,
    119, 327811, 6, 133, 131, 132, 196670, 128, 133, 262205, 19, 134,
    77, 196855, 136, 0, 262394, 134, 135, 143, 131320, 135, 262205, 6,
    137, 128, 327866, 19, 139, 137, 138, 196855, 141, 0, 262394, 139,
    140, 141, 131320, 140, 65788, 131320, 141, 131321, 136, 131320, 143, 196670,
    144, 18, 196670, 145, 18, 196670, 148, 149, 131321, 150, 131320, 150,
    262390, 152, 153, 0, 131321, 154, 131320, 154, 262205, 146, 155, 148,
    327859, 19, 157, 155, 156, 262394, 157, 151, 152, 131320, 151, 196670,
    158, 149, 131321, 159, 131320, 159, 262390, 161, 162, 0, 131321, 163,
    131320, 163, 262205, 146, 164, 158, 327859, 19, 165, 164, 156, 262394,
    165, 160, 161, 131320, 160, 262205, 32, 167, 34, 262205, 32, 168,
    47, 262205, 146, 169, 158, 262255, 6, 170, 169, 327822, 32, 171,
    168, 170, 327809, 32, 172, 167, 171, 262205, 32, 173, 50, 262205,
    146, 174, 148, 262255, 6, 175, 174, 327822, 32, 176, 173, 175,
    327809, 32, 177, 172, 176, 196670, 166, 177, 327745, 7, 178, 166,
    90, 262205, 6, 179, 178, 327866, 19, 180, 179, 18, 196855, 182,
    0, 262394, 180, 181, 182, 131320, 181, 327745, 7, 183, 166, 90,
    262205, 6, 184, 183, 327864, 19, 185, 184, 9, 131321, 182, 131320,
    182, 458997, 19, 186, 180, 160, 185, 181, 196855, 188, 0, 262394,
    186, 187, 188, 131320, 187, 327745, 7, 189, 166, 103, 262205, 6,
    190, 189, 327866, 19, 191, 190, 18, 131321, 188, 131320, 188, 458997,
    19, 192, 186, 182, 191, 187, 196855, 194, 0, 262394, 192, 193,
    194, 131320, 193, 327745, 7, 195, 166, 103, 262205, 6, 196, 195,
    327864, 19, 197, 196, 9, 131321, 194, 131320, 194, 458997, 19, 198,
    192, 188, 197, 193, 196855, 200, 0, 262394, 198, 199, 200, 131320,
    199, 262205, 6, 201, 41, 262205, 6, 202, 67, 327811, 6, 203,
    201, 202, 262205, 121, 204, 123, 262205, 32, 205, 166, 327767, 10,
    206, 204, 205, 327761, 6, 207, 206, 0, 327811, 6, 208, 203,
    207, 327866, 19, 210, 208, 209, 393385, 6, 211, 210, 9, 18,
    262205, 6, 212, 144, 327809, 6, 213, 212, 211, 196670, 144, 213,
    262205, 6, 214, 145, 327809, 6, 215, 214, 9, 196670, 145, 215,
    131321, 200, 131320, 200, 131321, 162, 131320, 162, 262205, 146, 216, 158,
    327808, 146, 218, 216, 217, 196670, 158, 218, 131321, 159, 131320, 161,
    131321, 153, 131320, 153, 262205, 146, 219, 148, 327808, 146, 220, 219,
    217, 196670, 148, 220, 131321, 150, 131320, 152, 262205, 6, 222, 145,
    327866, 19, 223, 222, 18, 196855, 226, 0, 262394, 223, 225, 230,
    131320, 225, 262205, 6, 227, 144, 262205, 6, 228, 145, 327816, 6,
    229, 227, 228, 196670, 224, 229, 131321, 226, 131320, 230, 196670, 224,
    18, 131321, 226, 131320, 226, 262205, 6, 231, 224, 196670, 221, 231,
    262205, 6, 233, 221, 524300, 6, 234, 1, 46, 9, 232, 233,
    196670, 8, 234, 131321, 136, 131320, 136, 131321, 118, 131320, 118, 131321,
    22, 131320, 22, 262205, 23, 239, 238, 262205, 6, 240, 8, 327822,
    23, 241, 239, 240, 327761, 6, 242, 241, 0, 327761, 6, 243,
    241, 1, 327761, 6, 244, 241, 2, 458832, 10, 245, 242, 243,
    244, 9, 196670, 236, 245, 65789, 65592
};
struct __nexa_VkSh {
    Img img;
    uint32_t fmt = 0;
    int layout = 0;
    int passOn = 0;
    int hold = 0;
    uint64_t pass = 0, fb = 0;
    uint64_t vs = 0, fs = 0, cvs = 0, cfs = 0;
    uint64_t pipeD = 0, pipeC = 0, pipeC2 = 0, pipeCL = 0;
    uint64_t layD = 0, layC = 0, setLay = 0, pool = 0, set = 0, samp = 0;
    uint64_t forPass = 0;
};
inline __nexa_VkSh& __nexa_vk_sh() { static __nexa_VkSh s; return s; }

inline void __nexa_vk_shadow_down(Dev& d) {
    __nexa_VkSh& s = __nexa_vk_sh();
    if (s.pipeD && d.DestroyPipeline) d.DestroyPipeline(d.device, s.pipeD, nullptr);
    if (s.pipeC && d.DestroyPipeline) d.DestroyPipeline(d.device, s.pipeC, nullptr);
    if (s.pipeC2 && d.DestroyPipeline) d.DestroyPipeline(d.device, s.pipeC2, nullptr);
    if (s.pipeCL && d.DestroyPipeline) d.DestroyPipeline(d.device, s.pipeCL, nullptr);
    s.pipeD = s.pipeC = s.pipeC2 = s.pipeCL = 0;
    if (s.vs && d.DestroyShaderModule) d.DestroyShaderModule(d.device, s.vs, nullptr);
    if (s.fs && d.DestroyShaderModule) d.DestroyShaderModule(d.device, s.fs, nullptr);
    if (s.cvs && d.DestroyShaderModule) d.DestroyShaderModule(d.device, s.cvs, nullptr);
    if (s.cfs && d.DestroyShaderModule) d.DestroyShaderModule(d.device, s.cfs, nullptr);
    s.vs = s.fs = s.cvs = s.cfs = 0;
    if (s.fb && d.DestroyFramebuffer) d.DestroyFramebuffer(d.device, s.fb, nullptr);
    s.fb = 0;
    if (s.pass && d.DestroyRenderPass) d.DestroyRenderPass(d.device, s.pass, nullptr);
    s.pass = 0;
    free_img(d, s.img);
    if (s.samp && d.DestroySampler) d.DestroySampler(d.device, s.samp, nullptr);
    s.samp = 0;
    if (s.pool && d.DestroyDescriptorPool) d.DestroyDescriptorPool(d.device, s.pool, nullptr);
    s.pool = 0; s.set = 0;
    if (s.setLay && d.DestroyDescriptorSetLayout) d.DestroyDescriptorSetLayout(d.device, s.setLay, nullptr);
    s.setLay = 0;
    if (s.layD && d.DestroyPipelineLayout) d.DestroyPipelineLayout(d.device, s.layD, nullptr);
    if (s.layC && d.DestroyPipelineLayout) d.DestroyPipelineLayout(d.device, s.layC, nullptr);
    s.layD = s.layC = 0;
    s.fmt = 0; s.layout = 0; s.passOn = 0; s.hold = 0; s.forPass = 0;
}

inline int __nexa_vk_shadow_ensure(Dev& d) {
    __nexa_VkSh& s = __nexa_vk_sh();
    if (!s.img.image) {
        uint32_t fmts[3] = { 126, 129, 124 };
        for (uint32_t f : fmts) {
            uint32_t props[4] = {};
            d.GetPhysicalDeviceFormatProperties(d.phys, f, props);
            if ((props[1] & 0x201u) == 0x201u) { s.fmt = f; break; }
        }
        if (!s.fmt) return 0;
        if (!make_image(d, s.img, s.fmt, 1024, 1024, 1, 0x20u | 0x4u, 2)) return 0;
        struct Att { uint32_t flags, format, samples, load, store, sload, sstore, initial, final; } at{};
        at.format = s.fmt; at.samples = 1; at.load = 1; at.store = 0; at.sload = 2; at.sstore = 1; at.initial = 3; at.final = 5;
        struct Ref { uint32_t attachment, layout; } depth{ 0, 3 };
        struct Sub { uint32_t flags, bind, inCount; const void* in; uint32_t colorCount; const void* colors; const void* resolve; const Ref* depth; uint32_t preserve; const void* p; } sub{};
        sub.depth = &depth;
        struct RP { uint32_t sType; const void* pNext; uint32_t flags; uint32_t ac; const Att* a; uint32_t sc; const Sub* s; uint32_t dc; const void* dep; } rp{};
        rp.sType = 38; rp.ac = 1; rp.a = &at; rp.sc = 1; rp.s = &sub;
        if (d.CreateRenderPass(d.device, &rp, nullptr, &s.pass) != 0) { __nexa_vk_shadow_down(d); return 0; }
        if (!make_fb(d, s.pass, s.img.view, 0, 1024, 1024, &s.fb)) { __nexa_vk_shadow_down(d); return 0; }
        if (!shader_mod(d, __nexa_sh_dvs, sizeof(__nexa_sh_dvs) / 4, &s.vs) ||
            !shader_mod(d, __nexa_sh_cvs, sizeof(__nexa_sh_cvs) / 4, &s.cvs) ||
            !shader_mod(d, __nexa_sh_cfs, sizeof(__nexa_sh_cfs) / 4, &s.cfs)) { __nexa_vk_shadow_down(d); return 0; }
        struct Range { uint32_t stage, offset, size; } pc{ 1, 0, 64 };
        struct PL { uint32_t sType; const void* pNext; uint32_t flags; uint32_t setCount; const void* sets; uint32_t pcCount; const Range* pc; } pl{};
        pl.sType = 30; pl.pcCount = 1; pl.pc = &pc;
        if (d.CreatePipelineLayout(d.device, &pl, nullptr, &s.layD) != 0) { __nexa_vk_shadow_down(d); return 0; }
        At attr = { 0, 0, 106, 0 };
        if (!pipe_ex(d, s.vs, 0, 3, 0, 0, 1, 1, 0, 1, 1, 1.5f, 2.0f, 0, 0, 24, &attr, 1, 0, s.layD, s.pass, &s.pipeD)) {
            __nexa_vk_shadow_down(d); return 0;
        }
        uint32_t binds[3] = { 0, 1, 0x10 };
        if (!layout_of(d, binds, 1, 1, 0, 128, 0, 0, 0, 0, &s.setLay, &s.layC)) { __nexa_vk_shadow_down(d); return 0; }
        struct Samp { uint32_t sType; const void* pNext; uint32_t flags; uint32_t mag, minf, mip, u, v, w; float bias; uint32_t anisoEn; float maxAniso; uint32_t cmpEn, cmp; float minLod, maxLod; uint32_t border, unnorm; } samp{};
        samp.sType = 31; samp.u = samp.v = samp.w = 2;
        if (d.CreateSampler(d.device, &samp, nullptr, &s.samp) != 0) { __nexa_vk_shadow_down(d); return 0; }
        struct PS { uint32_t type, count; } ps{ 1, 1 };
        struct DP { uint32_t sType; const void* pNext; uint32_t flags; uint32_t maxSets; uint32_t n; const PS* p; } dp{};
        dp.sType = 33; dp.maxSets = 1; dp.n = 1; dp.p = &ps;
        if (d.CreateDescriptorPool(d.device, &dp, nullptr, &s.pool) != 0) { __nexa_vk_shadow_down(d); return 0; }
        struct DA { uint32_t sType; const void* pNext; uint64_t pool; uint32_t count; const uint64_t* layouts; } da{};
        da.sType = 34; da.pool = s.pool; da.count = 1; da.layouts = &s.setLay;
        if (d.AllocateDescriptorSets(d.device, &da, &s.set) != 0) { __nexa_vk_shadow_down(d); return 0; }
        write_img(d, s.set, 0, s.samp, s.img.view);
        s.layout = 0;
    }
    if (d.pass && s.forPass != d.pass) {
        if (s.pipeC && d.DestroyPipeline) d.DestroyPipeline(d.device, s.pipeC, nullptr);
        if (s.pipeC2 && d.DestroyPipeline) d.DestroyPipeline(d.device, s.pipeC2, nullptr);
        if (s.pipeCL && d.DestroyPipeline) d.DestroyPipeline(d.device, s.pipeCL, nullptr);
        s.pipeC = s.pipeC2 = s.pipeCL = 0;
        if (!make_pipe(d, s.cvs, s.cfs, 3, 2, 1, 0, d.pass, 24, 0, s.layC, &s.pipeC) ||
            !make_pipe(d, s.cvs, s.cfs, 3, 0, 1, 0, d.pass, 24, 0, s.layC, &s.pipeC2) ||
            !make_pipe(d, s.cvs, s.cfs, 1, 0, 1, 0, d.pass, 24, 0, s.layC, &s.pipeCL)) {
            s.pipeC = s.pipeC2 = s.pipeCL = 0;
            return 1;
        }
        s.forPass = d.pass;
    }
    return 1;
}

inline void __nexa_vk_shadow_vp(Dev& d) {
    struct Vp { float x, y, w, h, mind, maxd; } vp{};
    vp.w = 1024.0f; vp.h = 1024.0f; vp.maxd = 1.0f;
    d.CmdSetViewport(d.cmd, 0, 1, &vp);
    struct Sc { int32_t x, y; uint32_t w, h; } sc{ 0, 0, 1024u, 1024u };
    d.CmdSetScissor(d.cmd, 0, 1, &sc);
}

inline int __nexa_vk_shadow_begin(Dev& d, float r, float g, float b, int w, int h) {
    __nexa_VkSh& s = __nexa_vk_sh();
    if (!__nexa_g3_shadow_on && !s.hold) return 0;
    if (w < 1 || h < 1) return 0;
    if (s.hold) {
        __nexa_vk_shadow_ensure(d);
        struct Clear { float c[4]; } clears[2] = {};
        clears[0].c[0] = r; clears[0].c[1] = g; clears[0].c[2] = b; clears[0].c[3] = 1;
        clears[1].c[0] = 1;
        struct Off { int32_t x, y; };
        struct Ext { uint32_t w, h; };
        struct Area { Off o; Ext e; };
        struct Begin { uint32_t sType; const void* pNext; uint64_t pass, fb; Area area; uint32_t cc; const Clear* c; } begin{};
        begin.sType = 43; begin.pass = d.pass; begin.fb = d.fbs[d.imageIndex];
        begin.area.e = { (uint32_t)d.swapW, (uint32_t)d.swapH }; begin.cc = 2; begin.c = clears;
        d.CmdBeginRenderPass(d.cmd, &begin, 0);
        set_viewport(d);
        s.hold = 0;
        d.framing = 1;
        scene_note_simple();
        return 1;
    }
    if (d.framing) return 1;
    if (!__nexa_vk_shadow_ensure(d) || !s.pipeD) { __nexa_g3_shadow_on = 0; return 0; }
    if (d.WaitForFences(d.device, 1, &d.fence, 1, ~0ull) != 0) return 1;
    if (d.dirty || w != d.swapW || h != d.swapH) {
        if (!rebuild_swapchain(d, w, h)) { __nexa_g3_shadow_on = 0; return 0; }
        s.forPass = 0;
        if (!__nexa_vk_shadow_ensure(d)) { __nexa_g3_shadow_on = 0; return 0; }
    }
    d.ResetCommandPool(d.device, d.pool, 0);
    int32_t acq = d.AcquireNextImageKHR(d.device, d.swapchain, ~0ull, d.semImg, 0, &d.imageIndex);
    if (acq == -1000001004) {
        d.dirty = 1;
        if (!rebuild_swapchain(d, w, h)) return 1;
        s.forPass = 0;
        __nexa_vk_shadow_ensure(d);
        acq = d.AcquireNextImageKHR(d.device, d.swapchain, ~0ull, d.semImg, 0, &d.imageIndex);
    }
    if (acq == 1000001003) { d.dirty = 1; acq = 0; }
    if (acq != 0) return 1;
    struct BI { uint32_t sType; const void* pNext; uint32_t flags; const void* inherit; } bi{};
    bi.sType = 42; bi.flags = 1;
    if (d.BeginCommandBuffer(d.cmd, &bi) != 0) return 1;
    if (s.layout != 3) {
        barrier(d, s.img.image, 2, 1, 0x200, 0, 0x400, (uint32_t)s.layout, 3);
        s.layout = 3;
    }
    struct Clear { float c[4]; } cv{};
    cv.c[0] = 1.0f;
    struct Off { int32_t x, y; };
    struct Ext { uint32_t w, h; };
    struct Area { Off o; Ext e; };
    struct Begin { uint32_t sType; const void* pNext; uint64_t pass, fb; Area area; uint32_t cc; const Clear* c; } bp{};
    bp.sType = 43; bp.pass = s.pass; bp.fb = s.fb; bp.area.e = { 1024u, 1024u }; bp.cc = 1; bp.c = &cv;
    d.CmdBeginRenderPass(d.cmd, &bp, 0);
    __nexa_vk_shadow_vp(d);
    s.passOn = 1;
    d.framing = 1;
    d.vused = 0;
    return 1;
}

inline int __nexa_vk_shadow_hold_end() {
    Dev& d = dev();
    __nexa_VkSh& s = __nexa_vk_sh();
    if (!s.passOn) return 0;
    d.CmdEndRenderPass(d.cmd);
    s.passOn = 0;
    s.layout = 5;
    struct MB { uint32_t sType; const void* pNext; uint32_t srcA, dstA; } mb{};
    mb.sType = 46; mb.srcA = 0x400; mb.dstA = 0x20;
    d.CmdPipelineBarrier(d.cmd, 0x200, 0x80, 0, 1, &mb, 0, nullptr, 0, nullptr);
    s.hold = 1;
    d.framing = 0;
    return 1;
}

inline int __nexa_vk_shadow_draw(Dev& d, const float* xyzrgb, int nvert, int lines, int twoSided, const float* mvp) {
    __nexa_VkSh& s = __nexa_vk_sh();
    if (!__nexa_g3_shadow_on && !__nexa_g3_shadow_have) return 0;
    if (__nexa_g3_shadow_on) {
        if (!s.passOn || !s.pipeD) return 0;
        if (lines) return 1;
        uint64_t bytes = (uint64_t)nvert * 6ull * 4ull;
        if (d.vused + bytes > d.vcap) return 1;
        std::memcpy((char*)d.vmap + d.vused, xyzrgb, (size_t)bytes);
        float clip[16];
        for (int c = 0; c < 4; ++c) {
            float x = mvp[c * 4 + 0], y = mvp[c * 4 + 1], z = mvp[c * 4 + 2], w = mvp[c * 4 + 3];
            clip[c * 4 + 0] = x; clip[c * 4 + 1] = y; clip[c * 4 + 2] = 0.5f * z + 0.5f * w; clip[c * 4 + 3] = w;
        }
        d.CmdBindPipeline(d.cmd, 0, s.pipeD);
        d.CmdPushConstants(d.cmd, s.layD, 1, 0, 64, clip);
        uint64_t off = d.vused;
        d.CmdBindVertexBuffers(d.cmd, 0, 1, &d.vbuf, &off);
        d.CmdDraw(d.cmd, (uint32_t)nvert, 1, 0, 0);
        d.vused += bytes;
        return 1;
    }
    if (lines) {
        if (!s.pipeCL) return 0;
    } else if (!s.pipeC) return 0;
    /*NEXA_SHADOW_CUSTOM*/
    uint64_t bytes = (uint64_t)nvert * 6ull * 4ull;
    if (d.vused + bytes > d.vcap) return 0;
    std::memcpy((char*)d.vmap + d.vused, xyzrgb, (size_t)bytes);
    float pc[32];
    for (int c = 0; c < 4; ++c) {
        float x = mvp[c * 4 + 0], y = mvp[c * 4 + 1], z = mvp[c * 4 + 2], w = mvp[c * 4 + 3];
        pc[c * 4 + 0] = x; pc[c * 4 + 1] = y; pc[c * 4 + 2] = 0.5f * z + 0.5f * w; pc[c * 4 + 3] = w;
    }
    for (int i = 0; i < 16; ++i) pc[16 + i] = __nexa_g3_shadow_lp[i];
    uint64_t pipe = lines ? s.pipeCL : (twoSided ? s.pipeC2 : s.pipeC);
    d.CmdBindPipeline(d.cmd, 0, pipe);
    d.CmdBindDescriptorSets(d.cmd, 0, s.layC, 0, 1, &s.set, 0, nullptr);
    d.CmdPushConstants(d.cmd, s.layC, 1, 0, 128, pc);
    uint64_t off = d.vused;
    d.CmdBindVertexBuffers(d.cmd, 0, 1, &d.vbuf, &off);
    d.CmdDraw(d.cmd, (uint32_t)nvert, 1, 0, 0);
    d.vused += bytes;
    return 1;
}
)NEXA_SH";
    if (s.find("d.customOn") != std::string::npos) {
        const std::string mark = "/*NEXA_SHADOW_CUSTOM*/";
        size_t at = vk.find(mark);
        if (at != std::string::npos) vk.replace(at, mark.size(), "if (d.customOn) return 0;");
    }
    put("inline void shutdown() {\n    keep_epoch()++;\n",
        vk + "inline void shutdown() {\n    keep_epoch()++;\n");

    put("static void __nexa_gfx3d_present(void) {\n",
        R"NEXA_SH(
static void __nexa_g3_shadow_lp_now(void) {
    if (__nexa_g3_shadow_model)
        __nexa_g3_mul4(__nexa_g3_shadow_lp, __nexa_g3_shadow_vp, __nexa_g3_shadow_model);
    else
        for (int i = 0; i < 16; i++) __nexa_g3_shadow_lp[i] = __nexa_g3_shadow_vp[i];
}

static void __nexa_g3_shadow_look(void) {
    float tx = __nexa_g3.target[0], ty = __nexa_g3.target[1], tz = __nexa_g3.target[2];
    float ex = __nexa_g3.eye[0] - tx, ey = __nexa_g3.eye[1] - ty, ez = __nexa_g3.eye[2] - tz;
    float span = std::sqrt(ex * ex + ey * ey + ez * ez);
    if (span < 4.0f) span = 4.0f;
    if (span > 40.0f) span = 40.0f;
    float eye[3] = { tx + __nexa_g3_lx * span, ty + __nexa_g3_ly * span, tz + __nexa_g3_lz * span };
    float at[3] = { tx, ty, tz };
    __nexa_g3_look_at(__nexa_g3.view, eye, at);
    float n = 0.05f, f = span * 2.2f;
    float* m = __nexa_g3.proj;
    for (int i = 0; i < 16; i++) m[i] = 0.0f;
    m[0] = 1.0f / span;
    m[5] = 1.0f / span;
    m[10] = -2.0f / (f - n);
    m[14] = -(f + n) / (f - n);
    m[15] = 1.0f;
    if (__nexa_g3.vulkan || !__nexa_gl.ClearColor || !__nexa_gl.Clear) return;
    __nexa_gl.ClearColor(1.0f, 1.0f, 1.0f, 1.0f);
    __nexa_gl.Clear(NEXA_GL_COLOR_BUFFER_BIT | NEXA_GL_DEPTH_BUFFER_BIT);
}

static void __nexa_g3_shadow_guard(void) {
    if (__nexa_g3.vulkan) return;
#if defined(NEXA_WASM)
    if (__nexa_g3_shadow_on) { glEnable(0x8037u); glPolygonOffset(1.5f, 2.0f); __nexa_g3_shadow_biased = 1; }
    else if (__nexa_g3_shadow_biased) { glDisable(0x8037u); __nexa_g3_shadow_biased = 0; }
#else
    typedef void (NEXA_GLAPI *Off)(float, float);
    static Off off = nullptr;
    static int got = 0;
    if (!got) { off = (Off)__nexa_g3_glproc("glPolygonOffset"); got = 1; }
    if (!__nexa_gl.Enable) return;
    if (__nexa_g3_shadow_on && off) {
        __nexa_gl.Enable(0x8037u);
        off(1.5f, 2.0f);
        __nexa_g3_shadow_biased = 1;
    } else if (__nexa_g3_shadow_biased) {
        __nexa_gl.Disable(0x8037u);
        __nexa_g3_shadow_biased = 0;
    }
#endif
}

static void __nexa_g3_shadow_use(void) {
#if defined(NEXA_WASM)
    if (__nexa_g3_u_shadowOn < 0) return;
    float on = __nexa_g3_shadow_on ? -1.0f : (__nexa_g3_shadow_have ? 1.0f : 0.0f);
    /*NEXA_MIRROR_SHADOW*/
    glUniform1f(__nexa_g3_u_shadowOn, on);
    if (on > 0.5f && __nexa_g3_u_shadowVP >= 0) {
        __nexa_g3_shadow_lp_now();
        glUniformMatrix4fv(__nexa_g3_u_shadowVP, 1, 0, __nexa_g3_shadow_lp);
        if (__nexa_g3_u_shadowSx >= 0) glUniform1f(__nexa_g3_u_shadowSx, __nexa_g3_shadow_sx);
        if (__nexa_g3_u_shadowSy >= 0) glUniform1f(__nexa_g3_u_shadowSy, __nexa_g3_shadow_sy);
        if (__nexa_g3_u_shadowLine >= 0) glUniform1f(__nexa_g3_u_shadowLine, __nexa_g3_shadow_lines ? 1.0f : 0.0f);
        glActiveTexture(0x84C1u);
        glBindTexture(NEXA_GL_TEXTURE_2D, __nexa_g3_shadow_tex);
        glActiveTexture(0x84C0u);
    }
#else
    (void)0;
#endif
}

static void __nexa_g3_shadow_use33(void) {
#if !defined(NEXA_WASM)
    if (__nexa_g3.vulkan || __nexa_g33_u_shadowOn < 0 || !__nexa_g33.Uniform1f) return;
    float on = __nexa_g3_shadow_on ? -1.0f : (__nexa_g3_shadow_have ? 1.0f : 0.0f);
    /*NEXA_MIRROR_SHADOW*/
    __nexa_g33.Uniform1f(__nexa_g33_u_shadowOn, on);
    if (on > 0.5f && __nexa_g33_u_shadowVP >= 0 && __nexa_g33.UniformMatrix4fv) {
        __nexa_g3_shadow_lp_now();
        __nexa_g33.UniformMatrix4fv(__nexa_g33_u_shadowVP, 1, 0, __nexa_g3_shadow_lp);
        if (__nexa_g33_u_shadowSx >= 0) __nexa_g33.Uniform1f(__nexa_g33_u_shadowSx, __nexa_g3_shadow_sx);
        if (__nexa_g33_u_shadowSy >= 0) __nexa_g33.Uniform1f(__nexa_g33_u_shadowSy, __nexa_g3_shadow_sy);
        if (__nexa_g33_u_shadowLine >= 0) __nexa_g33.Uniform1f(__nexa_g33_u_shadowLine, __nexa_g3_shadow_lines ? 1.0f : 0.0f);
        typedef void (NEXA_GLAPI *Act)(unsigned int);
        static Act act = nullptr;
        static int got = 0;
        if (!got) { act = (Act)__nexa_g3_glproc("glActiveTexture"); got = 1; }
        if (act && __nexa_gl.BindTexture) {
            act(0x84C1u);
            __nexa_gl.BindTexture(NEXA_GL_TEXTURE_2D, __nexa_g3_shadow_tex);
            act(0x84C0u);
        }
    }
#endif
}

static void __nexa_g3_shadow_unbind(void) {
#if !defined(NEXA_WASM)
    if (__nexa_g3.core33 || __nexa_g3.vulkan || !__nexa_g3_shadow_bound) return;
    typedef void (NEXA_GLAPI *Use)(unsigned int);
    static Use use = nullptr;
    static int got = 0;
    if (!got) { use = (Use)__nexa_g3_glproc("glUseProgram"); got = 1; }
    if (use) use(0);
    __nexa_g3_shadow_bound = 0;
#endif
}

static void __nexa_g3_shadow_make(void) {
#if !defined(NEXA_WASM)
    if (__nexa_g3_shadow_tried || __nexa_g3.core33 || __nexa_g3.vulkan) return;
    __nexa_g3_shadow_tried = 1;
    typedef unsigned int (NEXA_GLAPI *CreateSh)(unsigned int);
    typedef void (NEXA_GLAPI *Source)(unsigned int, int, const char* const*, const int*);
    typedef void (NEXA_GLAPI *Shiv)(unsigned int, unsigned int, int*);
    typedef void (NEXA_GLAPI *Del)(unsigned int);
    typedef unsigned int (NEXA_GLAPI *CreatePr)(void);
    typedef void (NEXA_GLAPI *Att)(unsigned int, unsigned int);
    typedef void (NEXA_GLAPI *One)(unsigned int);
    typedef int (NEXA_GLAPI *Loc)(unsigned int, const char*);
    typedef void (NEXA_GLAPI *Mat)(int, int, unsigned char, const float*);
    typedef void (NEXA_GLAPI *U1i)(int, int);
    typedef void (NEXA_GLAPI *U1f)(int, float);
    typedef void (NEXA_GLAPI *Use)(unsigned int);
    static CreateSh csh; static Source src; static Shiv siv; static Del del;
    static CreatePr cpr; static Att att; static One comp, link;
    static Shiv piv; static Loc loc; static Mat mat; static U1i u1i; static U1f u1f; static Use use;
    static int got = 0;
    if (!got) {
        csh = (CreateSh)__nexa_g3_glproc("glCreateShader");
        src = (Source)__nexa_g3_glproc("glShaderSource");
        comp = (One)__nexa_g3_glproc("glCompileShader");
        siv = (Shiv)__nexa_g3_glproc("glGetShaderiv");
        del = (Del)__nexa_g3_glproc("glDeleteShader");
        cpr = (CreatePr)__nexa_g3_glproc("glCreateProgram");
        att = (Att)__nexa_g3_glproc("glAttachShader");
        link = (One)__nexa_g3_glproc("glLinkProgram");
        piv = (Shiv)__nexa_g3_glproc("glGetProgramiv");
        loc = (Loc)__nexa_g3_glproc("glGetUniformLocation");
        mat = (Mat)__nexa_g3_glproc("glUniformMatrix4fv");
        u1i = (U1i)__nexa_g3_glproc("glUniform1i");
        u1f = (U1f)__nexa_g3_glproc("glUniform1f");
        use = (Use)__nexa_g3_glproc("glUseProgram");
        got = 1;
    }
    if (!csh || !src || !comp || !siv || !cpr || !att || !link || !piv || !loc || !use) return;
    auto stage = [&](unsigned kind, const char* text) -> unsigned {
        unsigned sh = csh(kind);
        src(sh, 1, &text, nullptr);
        comp(sh);
        int ok = 0;
        siv(sh, NEXA_GL_COMPILE_STATUS, &ok);
        if (!ok) { if (del) del(sh); return 0; }
        return sh;
    };
    auto linkp = [&](unsigned vs, unsigned fs) -> unsigned {
        if (!vs || !fs) return 0;
        unsigned p = cpr();
        att(p, vs); att(p, fs); link(p);
        int ok = 0;
        piv(p, NEXA_GL_LINK_STATUS, &ok);
        if (del) { del(vs); del(fs); }
        if (!ok) return 0;
        return p;
    };
    const char* cvs =
        "#version 120\n"
        "void main() { gl_Position = ftransform(); }\n";
    const char* cfs =
        "#version 120\n"
        "void main() {\n"
        "  float z = gl_FragCoord.z;\n"
        "  vec3 e = fract(vec3(1.0, 255.0, 65025.0) * z);\n"
        "  e.xy -= e.yz * (1.0 / 255.0);\n"
        "  gl_FragColor = vec4(e, 1.0);\n"
        "}\n";
    const char* rvs =
        "#version 120\n"
        "uniform mat4 uLightVP;\n"
        "uniform vec3 uToLight;\n"
        "varying vec4 vLight;\n"
        "varying vec3 vCol;\n"
        "varying float vFace;\n"
        "void main() {\n"
        "  gl_Position = ftransform();\n"
        "  vLight = uLightVP * gl_Vertex;\n"
        "  vCol = gl_Color.rgb;\n"
        "  vFace = dot(gl_NormalMatrix * gl_Normal, uToLight);\n"
        "}\n";
    const char* rfs =
        "#version 120\n"
        "uniform sampler2D uShadow;\n"
        "uniform float uSx;\n"
        "uniform float uSy;\n"
        "uniform float uLine;\n"
        "varying vec4 vLight;\n"
        "varying vec3 vCol;\n"
        "varying float vFace;\n"
        "void main() {\n"
        "  vec3 c = vCol;\n"
        "  if (vLight.w > 0.0) {\n"
        "    vec3 p = vLight.xyz / vLight.w;\n"
        "    float su = (p.x * 0.5 + 0.5) * uSx;\n"
        "    float sv = (p.y * 0.5 + 0.5) * uSy;\n"
        "    float sz = p.z * 0.5 + 0.5;\n"
        "    float slope = abs(dFdx(sz)) + abs(dFdy(sz));\n"
        "    float bias = 0.005 + slope;\n"
        "    if (bias > 0.012) bias = 0.012;\n"
        "    vec2 uv = vec2(su, sv);\n"
        "    vec2 dx = dFdx(uv);\n"
        "    vec2 dy = dFdy(uv);\n"
        "    if ((vFace > 0.0 || uLine > 0.5) && su > 0.0 && su < uSx && sv > 0.0 && sv < uSy) {\n"
        "      vec3 e = texture2D(uShadow, uv).rgb;\n"
        "      float diff = sz - bias - dot(e, vec3(1.0, 1.0/255.0, 1.0/65025.0));\n"
        "      if (uLine > 0.5) {\n"
        "        if (diff > -0.006) discard;\n"
        "      } else {\n"
        "        float occ = 0.0;\n"
        "        float cnt = 0.0;\n"
        "        for (int j = -2; j <= 2; j++) {\n"
        "          for (int i = -2; i <= 2; i++) {\n"
        "            vec2 q = uv + dx * float(i) + dy * float(j);\n"
        "            if (q.x > 0.0 && q.x < uSx && q.y > 0.0 && q.y < uSy) {\n"
        "              e = texture2D(uShadow, q).rgb;\n"
        "              occ += (sz - bias - dot(e, vec3(1.0, 1.0/255.0, 1.0/65025.0)) > 0.008) ? 1.0 : 0.0;\n"
        "              cnt += 1.0;\n"
        "            }\n"
        "          }\n"
        "        }\n"
        "        float w = cnt > 0.0 ? occ / cnt : 0.0;\n"
        "        float face = vFace * 5.0;\n"
        "        if (face < 0.0) face = 0.0;\n"
        "        if (face > 1.0) face = 1.0;\n"
        "        c *= mix(1.0, mix(1.0, 0.42, w), face);\n"
        "      }\n"
        "    }\n"
        "  }\n"
        "  gl_FragColor = vec4(c, 1.0);\n"
        "}\n";
    __nexa_g3_shadow_cast = linkp(stage(NEXA_GL_VERTEX_SHADER, cvs), stage(NEXA_GL_FRAGMENT_SHADER, cfs));
    __nexa_g3_shadow_recv = linkp(stage(NEXA_GL_VERTEX_SHADER, rvs), stage(NEXA_GL_FRAGMENT_SHADER, rfs));
    if (__nexa_g3_shadow_recv && loc) {
        __nexa_g3_shadow_u_vp = loc(__nexa_g3_shadow_recv, "uLightVP");
        __nexa_g3_shadow_u_tex = loc(__nexa_g3_shadow_recv, "uShadow");
        __nexa_g3_shadow_u_sx = loc(__nexa_g3_shadow_recv, "uSx");
        __nexa_g3_shadow_u_sy = loc(__nexa_g3_shadow_recv, "uSy");
        __nexa_g3_shadow_u_line = loc(__nexa_g3_shadow_recv, "uLine");
        __nexa_g3_shadow_u_to = loc(__nexa_g3_shadow_recv, "uToLight");
        if (use && u1i && __nexa_g3_shadow_u_tex >= 0) {
            use(__nexa_g3_shadow_recv);
            u1i(__nexa_g3_shadow_u_tex, 1);
            use(0);
        }
        (void)mat; (void)u1f;
    }
#endif
}

static void __nexa_g3_shadow_normals(const float* n) {
#if !defined(NEXA_WASM)
    if (__nexa_g3.core33 || __nexa_g3.vulkan) return;
    if (!__nexa_gl.EnableClientState || !__nexa_gl.DisableClientState) return;
    if (!n || !__nexa_g3_shadow_have || __nexa_g3_shadow_on) {
        __nexa_gl.DisableClientState(0x8075u);
        return;
    }
    typedef void (NEXA_GLAPI *NP)(unsigned int, int, const void*);
    static NP np = nullptr;
    static int got = 0;
    if (!got) { np = (NP)__nexa_g3_glproc("glNormalPointer"); got = 1; }
    if (!np) return;
    __nexa_gl.EnableClientState(0x8075u);
    np(0x1406u, 0, n);
#else
    (void)n;
#endif
}

static void __nexa_g3_shadow_bind(void) {
#if !defined(NEXA_WASM)
    if (__nexa_g3.core33 || __nexa_g3.vulkan) return;
    __nexa_g3_shadow_make();
    __nexa_g3_shadow_normals(nullptr);
    typedef void (NEXA_GLAPI *Use)(unsigned int);
    typedef void (NEXA_GLAPI *Mat)(int, int, unsigned char, const float*);
    typedef void (NEXA_GLAPI *U1f)(int, float);
    typedef void (NEXA_GLAPI *U3f)(int, float, float, float);
    typedef void (NEXA_GLAPI *Act)(unsigned int);
    static Use use = nullptr; static Mat mat = nullptr; static U1f u1f = nullptr; static U3f u3f = nullptr; static Act act = nullptr;
    static int got = 0;
    if (!got) {
        use = (Use)__nexa_g3_glproc("glUseProgram");
        mat = (Mat)__nexa_g3_glproc("glUniformMatrix4fv");
        u1f = (U1f)__nexa_g3_glproc("glUniform1f");
        u3f = (U3f)__nexa_g3_glproc("glUniform3f");
        act = (Act)__nexa_g3_glproc("glActiveTexture");
        got = 1;
    }
    if (!use) return;
    if (__nexa_g3_shadow_on && __nexa_g3_shadow_cast) {
        use(__nexa_g3_shadow_cast);
        __nexa_g3_shadow_bound = 1;
        return;
    }
    if (__nexa_g3_shadow_have && __nexa_g3_shadow_recv && !__nexa_g3_tex_cur /*NEXA_MIRROR_BIND*/) {
        use(__nexa_g3_shadow_recv);
        __nexa_g3_shadow_bound = 1;
        if (mat && __nexa_g3_shadow_u_vp >= 0)
            mat(__nexa_g3_shadow_u_vp, 1, 0, __nexa_g3_shadow_lp);
        if (u1f && __nexa_g3_shadow_u_sx >= 0) u1f(__nexa_g3_shadow_u_sx, __nexa_g3_shadow_sx);
        if (u1f && __nexa_g3_shadow_u_sy >= 0) u1f(__nexa_g3_shadow_u_sy, __nexa_g3_shadow_sy);
        if (u1f && __nexa_g3_shadow_u_line >= 0) u1f(__nexa_g3_shadow_u_line, __nexa_g3_shadow_lines ? 1.0f : 0.0f);
        if (u3f && __nexa_g3_shadow_u_to >= 0) {
            const float* V = __nexa_g3.view;
            u3f(__nexa_g3_shadow_u_to,
                V[0] * __nexa_g3_lx + V[4] * __nexa_g3_ly + V[8] * __nexa_g3_lz,
                V[1] * __nexa_g3_lx + V[5] * __nexa_g3_ly + V[9] * __nexa_g3_lz,
                V[2] * __nexa_g3_lx + V[6] * __nexa_g3_ly + V[10] * __nexa_g3_lz);
        }
        if (act && __nexa_gl.BindTexture) {
            act(0x84C1u);
            __nexa_gl.BindTexture(NEXA_GL_TEXTURE_2D, __nexa_g3_shadow_tex);
            act(0x84C0u);
        }
        return;
    }
    if (__nexa_g3_shadow_bound) { use(0); __nexa_g3_shadow_bound = 0; }
#endif
}

static int __nexa_g3_shadow_pot(int n) {
    int p = 1;
    while (p < n && p < 4096) p *= 2;
    return p;
}

static void __nexa_g3_shadow_copy(void) {
    if (__nexa_g3.vulkan) return;
#if !defined(NEXA_WASM)
    if (!__nexa_g3.core33 && !__nexa_g3_shadow_cast) { __nexa_g3_shadow_have = 0; return; }
#endif
    int w = __nexa_g3.w, h = __nexa_g3.h;
    if (w < 1 || h < 1 || w > 4096 || h > 4096) { __nexa_g3_shadow_have = 0; return; }
    int tw = __nexa_g3_shadow_pot(w), th = __nexa_g3_shadow_pot(h);
#if defined(NEXA_WASM)
    if (!__nexa_g3_shadow_tex || tw != __nexa_g3_shadow_tw || th != __nexa_g3_shadow_th) {
        if (!__nexa_g3_shadow_tex) glGenTextures(1, &__nexa_g3_shadow_tex);
        glBindTexture(NEXA_GL_TEXTURE_2D, __nexa_g3_shadow_tex);
        glTexImage2D(NEXA_GL_TEXTURE_2D, 0, (__nexa_GLint)NEXA_GL_RGBA, tw, th, 0,
                     NEXA_GL_RGBA, NEXA_GL_UNSIGNED_BYTE, nullptr);
        glTexParameteri(NEXA_GL_TEXTURE_2D, NEXA_GL_TEXTURE_MIN_FILTER, (__nexa_GLint)NEXA_GL_NEAREST);
        glTexParameteri(NEXA_GL_TEXTURE_2D, NEXA_GL_TEXTURE_MAG_FILTER, (__nexa_GLint)NEXA_GL_NEAREST);
        glTexParameteri(NEXA_GL_TEXTURE_2D, NEXA_GL_TEXTURE_WRAP_S, 0x812F);
        glTexParameteri(NEXA_GL_TEXTURE_2D, NEXA_GL_TEXTURE_WRAP_T, 0x812F);
        __nexa_g3_shadow_tw = tw; __nexa_g3_shadow_th = th;
    } else {
        glBindTexture(NEXA_GL_TEXTURE_2D, __nexa_g3_shadow_tex);
    }
    glCopyTexSubImage2D(NEXA_GL_TEXTURE_2D, 0, 0, 0, 0, 0, w, h);
#else
    if (!__nexa_gl.GenTextures || !__nexa_gl.BindTexture || !__nexa_gl.TexImage2D || !__nexa_gl.TexParameteri) {
        __nexa_g3_shadow_have = 0; return;
    }
    typedef void (NEXA_GLAPI *CopySub)(unsigned int, int, int, int, int, int, int, int);
    static CopySub copysub = nullptr;
    static int got = 0;
    if (!got) { copysub = (CopySub)__nexa_g3_glproc("glCopyTexSubImage2D"); got = 1; }
    if (!copysub) { __nexa_g3_shadow_have = 0; return; }
    if (!__nexa_g3_shadow_tex || tw != __nexa_g3_shadow_tw || th != __nexa_g3_shadow_th) {
        if (!__nexa_g3_shadow_tex) __nexa_gl.GenTextures(1, &__nexa_g3_shadow_tex);
        __nexa_gl.BindTexture(NEXA_GL_TEXTURE_2D, __nexa_g3_shadow_tex);
        __nexa_gl.TexImage2D(NEXA_GL_TEXTURE_2D, 0, (__nexa_GLint)NEXA_GL_RGBA, tw, th, 0,
                             NEXA_GL_RGBA, NEXA_GL_UNSIGNED_BYTE, nullptr);
        __nexa_gl.TexParameteri(NEXA_GL_TEXTURE_2D, NEXA_GL_TEXTURE_MIN_FILTER, (__nexa_GLint)NEXA_GL_NEAREST);
        __nexa_gl.TexParameteri(NEXA_GL_TEXTURE_2D, NEXA_GL_TEXTURE_MAG_FILTER, (__nexa_GLint)NEXA_GL_NEAREST);
        __nexa_gl.TexParameteri(NEXA_GL_TEXTURE_2D, NEXA_GL_TEXTURE_WRAP_S, 0x812F);
        __nexa_gl.TexParameteri(NEXA_GL_TEXTURE_2D, NEXA_GL_TEXTURE_WRAP_T, 0x812F);
        __nexa_g3_shadow_tw = tw; __nexa_g3_shadow_th = th;
    } else {
        __nexa_gl.BindTexture(NEXA_GL_TEXTURE_2D, __nexa_g3_shadow_tex);
    }
    copysub(NEXA_GL_TEXTURE_2D, 0, 0, 0, 0, 0, w, h);
#endif
    __nexa_g3_shadow_sx = (float)w / (float)tw;
    __nexa_g3_shadow_sy = (float)h / (float)th;
    __nexa_g3_shadow_have = 1;
}

static void __nexa_gfx3d_present(void) {
)NEXA_SH");

    if (s.find("__nexa_g3_mirror_on") != std::string::npos) {
        const std::string mark = "/*NEXA_MIRROR_SHADOW*/";
        size_t at = 0;
        while ((at = s.find(mark, at)) != std::string::npos) {
            s.replace(at, mark.size(), "if (__nexa_g3_mirror_on) on = 0.0f;");
            at += 38;
        }
        const std::string bind = "/*NEXA_MIRROR_BIND*/";
        at = s.find(bind);
        if (at != std::string::npos)
            s.replace(at, bind.size(), "&& !__nexa_g3_mirror_on");
    }

    put("static int __nexa_gfx3d_vsync_get(void) {\n    return __nexa_g3.vsync;\n}\n",
        "static int __nexa_gfx3d_vsync_get(void) {\n    return __nexa_g3.vsync;\n}\n"
        "\n"
        "static int __nexa_gfx3d_shadow(void) {\n"
        "    if (!__nexa_g3.ready) return 0;\n"
        "    if (!__nexa_g3_shadow_on) {\n"
        "        __nexa_g3_shadow_on = 1;\n"
        "#if !defined(NEXA_WASM)\n"
        "        if (!__nexa_g3.vulkan && !__nexa_g3.core33) __nexa_g3_shadow_make();\n"
        "#endif\n"
        "        return 1;\n"
        "    }\n"
        "    __nexa_g3_mul4(__nexa_g3_shadow_vp, __nexa_g3.proj, __nexa_g3.view);\n"
        "    __nexa_g3_shadow_on = 0;\n"
        "    __nexa_g3_shadow_unbind();\n"
        "#if defined(_WIN32)\n"
        "    if (__nexa_g3.vulkan) {\n"
        "        __nexa_g3_shadow_have = nexa_vk14::shadow_hold_end() ? 1 : 0;\n"
        "        __nexa_g3_shadow_sx = 1.0f;\n"
        "        __nexa_g3_shadow_sy = 1.0f;\n"
        "        return 0;\n"
        "    }\n"
        "#endif\n"
        "    __nexa_g3_shadow_copy();\n"
        "    return 0;\n"
        "}\n");
}
