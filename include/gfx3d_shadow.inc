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
        "    \"uniform sampler2D uShadowTex;\\n\"\n"
        "    \"uniform float uShadowSx;\\n\"\n"
        "    \"uniform float uShadowSy;\\n\"\n");
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
        "    \"    float sz = p.z * 0.5 + 0.5;\\n\"\n";
    const char* sample =
        "    \"    if (su > 0.0 && su < uShadowSx && sv > 0.0 && sv < uShadowSy) {\\n\"\n"
        "    \"      vec3 e = ";
    const char* dark2 =
        "(uShadowTex, vec2(su, sv)).rgb;\\n\"\n"
        "    \"      float d = dot(e, vec3(1.0, 1.0/255.0, 1.0/65025.0));\\n\"\n";
    // WebGL 1 has no dFdx unless an extension is on, and this shader is the
    // only one that window has, so the wasm copy uses a fixed gap.
    const char* tail =
        "    \"      if (sz - 0.008 > d) c *= 0.42;\\n\"\n"
        "    \"    }\\n\"\n"
        "    \"  }\\n\"\n";
    const char* slope =
        "    \"    float slope = abs(dFdx(sz)) + abs(dFdy(sz));\\n\"\n"
        "    \"    float bias = 0.006 + slope;\\n\"\n"
        "    \"    if (bias > 0.02) bias = 0.02;\\n\"\n";
    const char* slopeTail =
        "    \"      if (sz - bias > d) c *= 0.42;\\n\"\n"
        "    \"    }\\n\"\n"
        "    \"  }\\n\"\n";
    all("    \"  gl_FragColor = vec4(c, 1.0);\\n\"\n",
        std::string(dark) + sample + "texture2D" + dark2 + tail +
        "    \"  gl_FragColor = vec4(c, 1.0);\\n\"\n");
    all("    \"  fragColor = vec4(c, 1.0);\\n\"\n",
        std::string(dark) + slope + sample + "texture" + dark2 + slopeTail +
        "    \"  fragColor = vec4(c, 1.0);\\n\"\n");

    put("static void __nexa_g3_apply_camera(void) {\n",
        "static int __nexa_g3_shadow_on = 0;\n"
        "static int __nexa_g3_shadow_have = 0;\n"
        "static int __nexa_g3_shadow_biased = 0;\n"
        "static float __nexa_g3_shadow_vp[16];\n"
        "static float __nexa_g3_shadow_lp[16];\n"
        "static const float* __nexa_g3_shadow_model = nullptr;\n"
        "static float __nexa_g3_shadow_sx = 1.0f, __nexa_g3_shadow_sy = 1.0f;\n"
        "static unsigned __nexa_g3_shadow_tex = 0;\n"
        "static int __nexa_g3_shadow_tw = 0, __nexa_g3_shadow_th = 0;\n"
        "static unsigned __nexa_g3_shadow_cast = 0, __nexa_g3_shadow_recv = 0;\n"
        "static int __nexa_g3_shadow_tried = 0;\n"
        "static int __nexa_g3_shadow_u_vp = -1, __nexa_g3_shadow_u_tex = -1;\n"
        "static int __nexa_g3_shadow_u_sx = -1, __nexa_g3_shadow_u_sy = -1;\n"
        "static int __nexa_g3_shadow_bound = 0;\n"
        "static void __nexa_g3_shadow_look(void);\n"
        "static void __nexa_g3_shadow_guard(void);\n"
        "static void __nexa_g3_shadow_lp_now(void);\n"
        "static void __nexa_g3_shadow_use(void);\n"
        "static void __nexa_g3_shadow_use33(void);\n"
        "static void __nexa_g3_shadow_bind(void);\n"
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
        "    __nexa_g3_shadow_lp_now();\n");
    put("static void __nexa_g3_batch_submit(void) {\n#if defined(_WIN32)\n    if (__nexa_g3.vulkan) {\n",
        "static void __nexa_g3_batch_submit(void) {\n"
        "    __nexa_g3_shadow_model = nullptr;\n"
        "    __nexa_g3_shadow_lp_now();\n"
        "    if (__nexa_g3_shadow_on && __nexa_g3_prim) return;\n"
        "#if defined(_WIN32)\n    if (__nexa_g3.vulkan) {\n");
    put("static void __nexa_g3_batch_submit(void) {\n    if (!__nexa_g3_prog || !__nexa_g3_vbo) return;\n",
        "static void __nexa_g3_batch_submit(void) {\n"
        "    __nexa_g3_shadow_model = nullptr;\n"
        "    __nexa_g3_shadow_lp_now();\n"
        "    if (__nexa_g3_shadow_on && __nexa_g3_prim) return;\n"
        "    if (!__nexa_g3_prog || !__nexa_g3_vbo) return;\n");
    put("    if (__nexa_g3.core33) return __nexa_g3_core_mesh(m, L, M);\n",
        "    if (__nexa_g3.core33) return __nexa_g3_core_mesh(m, L, M);\n"
        "    __nexa_g3_shadow_bind();\n");
    put("    if (__nexa_g3.core33) { __nexa_g3_core_batch(); return; }\n",
        "    if (__nexa_g3.core33) { __nexa_g3_core_batch(); return; }\n"
        "    __nexa_g3_shadow_bind();\n");

    all("    __nexa_g33.UniformMatrix4fv(__nexa_g33_u_mvp, 1, 0, mvp);\n",
        "    __nexa_g33.UniformMatrix4fv(__nexa_g33_u_mvp, 1, 0, mvp);\n"
        "    __nexa_g3_shadow_use33();\n");
    all("    glUniformMatrix4fv(__nexa_g3_u_mvp, 1, 0, mvp);\n",
        "    glUniformMatrix4fv(__nexa_g3_u_mvp, 1, 0, mvp);\n"
        "    __nexa_g3_shadow_use();\n");

    put("static __nexa_GLint __nexa_g33_u_mvp = -1;\n",
        "static __nexa_GLint __nexa_g33_u_mvp = -1;\n"
        "static __nexa_GLint __nexa_g33_u_shadowVP = -1, __nexa_g33_u_shadowOn = -1;\n"
        "static __nexa_GLint __nexa_g33_u_shadowTex = -1, __nexa_g33_u_shadowSx = -1, __nexa_g33_u_shadowSy = -1;\n");
    put("static __nexa_GLint  __nexa_g3_u_mvp = -1;\n",
        "static __nexa_GLint  __nexa_g3_u_mvp = -1;\n"
        "static __nexa_GLint  __nexa_g3_u_shadowVP = -1, __nexa_g3_u_shadowOn = -1;\n"
        "static __nexa_GLint  __nexa_g3_u_shadowTex = -1, __nexa_g3_u_shadowSx = -1, __nexa_g3_u_shadowSy = -1;\n");
    put("    __nexa_g33_u_mvp = __nexa_g33.GetUniformLocation(__nexa_g33_prog, \"uMVP\");\n",
        "    __nexa_g33_u_mvp = __nexa_g33.GetUniformLocation(__nexa_g33_prog, \"uMVP\");\n"
        "    __nexa_g33_u_shadowVP = __nexa_g33.GetUniformLocation(__nexa_g33_prog, \"uShadowVP\");\n"
        "    __nexa_g33_u_shadowOn = __nexa_g33.GetUniformLocation(__nexa_g33_prog, \"uShadowOn\");\n"
        "    __nexa_g33_u_shadowTex = __nexa_g33.GetUniformLocation(__nexa_g33_prog, \"uShadowTex\");\n"
        "    __nexa_g33_u_shadowSx = __nexa_g33.GetUniformLocation(__nexa_g33_prog, \"uShadowSx\");\n"
        "    __nexa_g33_u_shadowSy = __nexa_g33.GetUniformLocation(__nexa_g33_prog, \"uShadowSy\");\n");
    put("    __nexa_g3_u_mvp = glGetUniformLocation(__nexa_g3_prog, \"uMVP\");\n",
        "    __nexa_g3_u_mvp = glGetUniformLocation(__nexa_g3_prog, \"uMVP\");\n"
        "    __nexa_g3_u_shadowVP = glGetUniformLocation(__nexa_g3_prog, \"uShadowVP\");\n"
        "    __nexa_g3_u_shadowOn = glGetUniformLocation(__nexa_g3_prog, \"uShadowOn\");\n"
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
    119734787, 65536, 851979, 120, 0, 131089, 1, 393227, 1, 1280527431, 1685353262, 808793134,
    0, 196622, 0, 1, 524303, 4, 4, 1852399981, 0, 12, 110, 112,
    196624, 4, 7, 196611, 2, 450, 655364, 1197427783, 1279741775, 1885560645, 1953718128, 1600482425,
    1701734764, 1919509599, 1769235301, 25974, 524292, 1197427783, 1279741775, 1852399429, 1685417059, 1768185701, 1952671090, 6649449,
    262149, 4, 1852399981, 0, 196613, 8, 7629164, 262149, 12, 1734954102, 29800, 196613,
    25, 112, 196613, 34, 30325, 196613, 41, 122, 262149, 47, 1886350451, 101,
    262149, 55, 1935763810, 0, 196613, 92, 100, 262149, 96, 1634227061, 7827300, 262149,
    110, 1734439526, 0, 262149, 112, 1819231094, 0, 262215, 12, 30, 1, 262215,
    96, 33, 0, 262215, 96, 34, 0, 262215, 110, 30, 0, 262215,
    112, 30, 0, 131091, 2, 196641, 3, 2, 196630, 6, 32, 262176,
    7, 7, 6, 262187, 6, 9, 1065353216, 262167, 10, 6, 4, 262176,
    11, 1, 10, 262203, 11, 12, 1, 262165, 13, 32, 0, 262187,
    13, 14, 3, 262176, 15, 1, 6, 262187, 6, 18, 0, 131092,
    19, 262167, 23, 6, 3, 262176, 24, 7, 23, 262167, 32, 6,
    2, 262176, 33, 7, 32, 262187, 6, 37, 1056964608, 262187, 13, 42,
    2, 262187, 6, 56, 1002740646, 262187, 6, 60, 1017370378, 262187, 13, 64,
    0, 262187, 13, 76, 1, 589849, 93, 6, 1, 0, 0, 0,
    1, 0, 196635, 94, 93, 262176, 95, 0, 94, 262203, 95, 96,
    0, 262187, 6, 108, 1054280253, 262176, 109, 3, 10, 262203, 109, 110,
    3, 262176, 111, 1, 23, 262203, 111, 112, 1, 327734, 2, 4,
    0, 3, 131320, 5, 262203, 7, 8, 7, 262203, 24, 25, 7,
    262203, 33, 34, 7, 262203, 7, 41, 7, 262203, 7, 47, 7,
    262203, 7, 55, 7, 262203, 7, 92, 7, 196670, 8, 9, 327745,
    15, 16, 12, 14, 262205, 6, 17, 16, 327866, 19, 20, 17,
    18, 196855, 22, 0, 262394, 20, 21, 22, 131320, 21, 262205, 10,
    26, 12, 524367, 23, 27, 26, 26, 0, 1, 2, 327745, 15,
    28, 12, 14, 262205, 6, 29, 28, 393296, 23, 30, 29, 29,
    29, 327816, 23, 31, 27, 30, 196670, 25, 31, 262205, 23, 35,
    25, 458831, 32, 36, 35, 35, 0, 1, 327822, 32, 38, 36,
    37, 327760, 32, 39, 37, 37, 327809, 32, 40, 38, 39, 196670,
    34, 40, 327745, 7, 43, 25, 42, 262205, 6, 44, 43, 327813,
    6, 45, 44, 37, 327809, 6, 46, 45, 37, 196670, 41, 46,
    262205, 6, 48, 41, 262351, 6, 49, 48, 393228, 6, 50, 1,
    4, 49, 262205, 6, 51, 41, 262352, 6, 52, 51, 393228, 6,
    53, 1, 4, 52, 327809, 6, 54, 50, 53, 196670, 47, 54,
    262205, 6, 57, 47, 327809, 6, 58, 56, 57, 196670, 55, 58,
    262205, 6, 59, 55, 327866, 19, 61, 59, 60, 196855, 63, 0,
    262394, 61, 62, 63, 131320, 62, 196670, 55, 60, 131321, 63, 131320,
    63, 327745, 7, 65, 34, 64, 262205, 6, 66, 65, 327866, 19,
    67, 66, 18, 196855, 69, 0, 262394, 67, 68, 69, 131320, 68,
    327745, 7, 70, 34, 64, 262205, 6, 71, 70, 327864, 19, 72,
    71, 9, 131321, 69, 131320, 69, 458997, 19, 73, 67, 63, 72,
    68, 196855, 75, 0, 262394, 73, 74, 75, 131320, 74, 327745, 7,
    77, 34, 76, 262205, 6, 78, 77, 327866, 19, 79, 78, 18,
    131321, 75, 131320, 75, 458997, 19, 80, 73, 69, 79, 74, 196855,
    82, 0, 262394, 80, 81, 82, 131320, 81, 327745, 7, 83, 34,
    76, 262205, 6, 84, 83, 327864, 19, 85, 84, 9, 131321, 82,
    131320, 82, 458997, 19, 86, 80, 75, 85, 81, 262205, 6, 87,
    41, 327864, 19, 88, 87, 9, 327847, 19, 89, 86, 88, 196855,
    91, 0, 262394, 89, 90, 91, 131320, 90, 262205, 94, 97, 96,
    262205, 32, 98, 34, 327767, 10, 99, 97, 98, 327761, 6, 100,
    99, 0, 196670, 92, 100, 262205, 6, 101, 41, 262205, 6, 102,
    55, 327811, 6, 103, 101, 102, 262205, 6, 104, 92, 327866, 19,
    105, 103, 104, 196855, 107, 0, 262394, 105, 106, 107, 131320, 106,
    196670, 8, 108, 131321, 107, 131320, 107, 131321, 91, 131320, 91, 131321,
    22, 131320, 22, 262205, 23, 113, 112, 262205, 6, 114, 8, 327822,
    23, 115, 113, 114, 327761, 6, 116, 115, 0, 327761, 6, 117,
    115, 1, 327761, 6, 118, 115, 2, 458832, 10, 119, 116, 117,
    118, 9, 196670, 110, 119, 65789, 65592
};
struct __nexa_VkSh {
    Img img;
    uint32_t fmt = 0;
    int layout = 0;
    int passOn = 0;
    int hold = 0;
    uint64_t pass = 0, fb = 0;
    uint64_t vs = 0, fs = 0, cvs = 0, cfs = 0;
    uint64_t pipeD = 0, pipeC = 0, pipeC2 = 0;
    uint64_t layD = 0, layC = 0, setLay = 0, pool = 0, set = 0, samp = 0;
    uint64_t forPass = 0;
};
inline __nexa_VkSh& __nexa_vk_sh() { static __nexa_VkSh s; return s; }

inline void __nexa_vk_shadow_down(Dev& d) {
    __nexa_VkSh& s = __nexa_vk_sh();
    if (s.pipeD && d.DestroyPipeline) d.DestroyPipeline(d.device, s.pipeD, nullptr);
    if (s.pipeC && d.DestroyPipeline) d.DestroyPipeline(d.device, s.pipeC, nullptr);
    if (s.pipeC2 && d.DestroyPipeline) d.DestroyPipeline(d.device, s.pipeC2, nullptr);
    s.pipeD = s.pipeC = s.pipeC2 = 0;
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
        if (!pipe_ex(d, s.vs, 0, 3, 2, 0, 1, 1, 0, 1, 1, 2.0f, 4.0f, 0, 0, 24, &attr, 1, 0, s.layD, s.pass, &s.pipeD)) {
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
        s.pipeC = s.pipeC2 = 0;
        if (!make_pipe(d, s.cvs, s.cfs, 3, 2, 1, 0, d.pass, 24, 0, s.layC, &s.pipeC) ||
            !make_pipe(d, s.cvs, s.cfs, 3, 0, 1, 0, d.pass, 24, 0, s.layC, &s.pipeC2)) {
            s.pipeC = s.pipeC2 = 0;
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
    if (lines || !s.pipeC) return 0;
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
    uint64_t pipe = twoSided ? s.pipeC2 : s.pipeC;
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
    if (__nexa_g3_shadow_on) {
        glEnable(0x8037u); glPolygonOffset(2.0f, 4.0f);
        glEnable(0x0B44u); glCullFace(0x0404u);
        __nexa_g3_shadow_biased = 1;
    } else if (__nexa_g3_shadow_biased) {
        glDisable(0x8037u);
        glCullFace(0x0405u);
        __nexa_g3_shadow_biased = 0;
    }
#else
    typedef void (NEXA_GLAPI *Off)(float, float);
    static Off off = nullptr;
    static int got = 0;
    if (!got) { off = (Off)__nexa_g3_glproc("glPolygonOffset"); got = 1; }
    if (!__nexa_gl.Enable) return;
    if (__nexa_g3_shadow_on) {
        if (off) { __nexa_gl.Enable(0x8037u); off(2.0f, 4.0f); }
        if (__nexa_gl.CullFace) { __nexa_gl.Enable(NEXA_GL_CULL_FACE); __nexa_gl.CullFace(0x0404u); }
        __nexa_g3_shadow_biased = 1;
    } else if (__nexa_g3_shadow_biased) {
        if (off) __nexa_gl.Disable(0x8037u);
        if (__nexa_gl.CullFace) __nexa_gl.CullFace(NEXA_GL_BACK);
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
        "varying vec4 vLight;\n"
        "varying vec3 vCol;\n"
        "void main() {\n"
        "  gl_Position = ftransform();\n"
        "  vLight = uLightVP * gl_Vertex;\n"
        "  vCol = gl_Color.rgb;\n"
        "}\n";
    const char* rfs =
        "#version 120\n"
        "uniform sampler2D uShadow;\n"
        "uniform float uSx;\n"
        "uniform float uSy;\n"
        "varying vec4 vLight;\n"
        "varying vec3 vCol;\n"
        "void main() {\n"
        "  vec3 c = vCol;\n"
        "  if (vLight.w > 0.0) {\n"
        "    vec3 p = vLight.xyz / vLight.w;\n"
        "    float su = (p.x * 0.5 + 0.5) * uSx;\n"
        "    float sv = (p.y * 0.5 + 0.5) * uSy;\n"
        "    float sz = p.z * 0.5 + 0.5;\n"
        "    float slope = abs(dFdx(sz)) + abs(dFdy(sz));\n"
        "    float bias = 0.006 + slope;\n"
        "    if (bias > 0.02) bias = 0.02;\n"
        "    if (su > 0.0 && su < uSx && sv > 0.0 && sv < uSy) {\n"
        "      vec3 e = texture2D(uShadow, vec2(su, sv)).rgb;\n"
        "      float d = dot(e, vec3(1.0, 1.0/255.0, 1.0/65025.0));\n"
        "      if (sz - bias > d) c *= 0.42;\n"
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
        if (use && u1i && __nexa_g3_shadow_u_tex >= 0) {
            use(__nexa_g3_shadow_recv);
            u1i(__nexa_g3_shadow_u_tex, 1);
            use(0);
        }
        (void)mat; (void)u1f;
    }
#endif
}

static void __nexa_g3_shadow_bind(void) {
#if !defined(NEXA_WASM)
    if (__nexa_g3.core33 || __nexa_g3.vulkan) return;
    __nexa_g3_shadow_make();
    typedef void (NEXA_GLAPI *Use)(unsigned int);
    typedef void (NEXA_GLAPI *Mat)(int, int, unsigned char, const float*);
    typedef void (NEXA_GLAPI *U1f)(int, float);
    typedef void (NEXA_GLAPI *Act)(unsigned int);
    static Use use = nullptr; static Mat mat = nullptr; static U1f u1f = nullptr; static Act act = nullptr;
    static int got = 0;
    if (!got) {
        use = (Use)__nexa_g3_glproc("glUseProgram");
        mat = (Mat)__nexa_g3_glproc("glUniformMatrix4fv");
        u1f = (U1f)__nexa_g3_glproc("glUniform1f");
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
