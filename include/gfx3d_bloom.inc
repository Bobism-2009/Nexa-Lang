// gfx3d.bloom. Bright pixels bleed a soft glow onto what sits next to them.
// A program that never calls it does not run this, so its source stays as
// written. The picture is already drawn; this runs once, just before it is
// shown, and a dim pixel is left alone.
inline void gfx3dApplyBloom(std::string& s) {
    auto put = [&](const std::string& from, const std::string& to) -> bool {
        const size_t at = s.find(from);
        if (at == std::string::npos) return false;
        s.replace(at, from.size(), to);
        return true;
    };

    put("static void __nexa_g3_apply_camera(void) {\n",
        "static int __nexa_g3_bloom_on = 0;\n"
        "static int __nexa_g3_bloom_tried = 0;\n"
        "static float __nexa_g3_bloom = 0;\n"
        "static unsigned __nexa_g3_bloom_prog = 0;\n"
        "static unsigned __nexa_g3_bloom_scene = 0;\n"
        "static unsigned __nexa_g3_bloom_a = 0;\n"
        "static unsigned __nexa_g3_bloom_b = 0;\n"
        "static int __nexa_g3_bloom_sw = 0, __nexa_g3_bloom_sh = 0;\n"
        "static int __nexa_g3_bloom_aw = 0, __nexa_g3_bloom_ah = 0;\n"
        "static int __nexa_g3_bloom_bw = 0, __nexa_g3_bloom_bh = 0;\n"
        "static int __nexa_g3_bloom_u_mode = -1, __nexa_g3_bloom_u_texel = -1, __nexa_g3_bloom_u_str = -1;\n"
        "static void __nexa_g3_bloom_present(void);\n"
        "static void __nexa_g3_apply_camera(void) {\n");

    put("static int __nexa_gfx3d_vsync_get(void) {\n    return __nexa_g3.vsync;\n}\n",
        std::string("static int __nexa_gfx3d_vsync_get(void) {\n    return __nexa_g3.vsync;\n}\n") +
        R"NEXA_BLOOM(
static void __nexa_g3_bloom_make(void) {
#if !defined(NEXA_WASM)
    if (__nexa_g3_bloom_tried || __nexa_g3.core33 || __nexa_g3.vulkan) return;
    __nexa_g3_bloom_tried = 1;
    typedef unsigned int (NEXA_GLAPI *CreateSh)(unsigned int);
    typedef void (NEXA_GLAPI *Source)(unsigned int, int, const char* const*, const int*);
    typedef void (NEXA_GLAPI *Shiv)(unsigned int, unsigned int, int*);
    typedef void (NEXA_GLAPI *Del)(unsigned int);
    typedef unsigned int (NEXA_GLAPI *CreatePr)(void);
    typedef void (NEXA_GLAPI *Att)(unsigned int, unsigned int);
    typedef void (NEXA_GLAPI *One)(unsigned int);
    typedef int (NEXA_GLAPI *Loc)(unsigned int, const char*);
    static CreateSh csh; static Source src; static Shiv siv; static Del del;
    static CreatePr cpr; static Att att; static One comp, link; static Shiv piv;
    static Loc loc;
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
        got = 1;
    }
    if (!csh || !src || !comp || !siv || !cpr || !att || !link || !piv || !loc) return;
    const char* vs =
        "#version 120\n"
        "varying vec2 vUv;\n"
        "void main() {\n"
        "  gl_Position = ftransform();\n"
        "  vUv = gl_MultiTexCoord0.xy;\n"
        "}\n";
    const char* fs =
        "#version 120\n"
        "uniform sampler2D uTex;\n"
        "uniform sampler2D uBloom;\n"
        "uniform float uMode;\n"
        "uniform vec2 uTexel;\n"
        "uniform float uStrength;\n"
        "varying vec2 vUv;\n"
        "void main() {\n"
        "  if (uMode < 0.5) {\n"
        "    vec3 c = texture2D(uTex, vUv).rgb;\n"
        "    float b = max(c.r, max(c.g, c.b));\n"
        "    float k = smoothstep(0.72, 1.0, b);\n"
        "    gl_FragColor = vec4(c * k * 2.0, 1.0);\n"
        "  } else if (uMode < 1.5) {\n"
        "    vec3 c = texture2D(uTex, vUv).rgb * 0.227027;\n"
        "    c += texture2D(uTex, vUv + uTexel).rgb * 0.1945946;\n"
        "    c += texture2D(uTex, vUv - uTexel).rgb * 0.1945946;\n"
        "    c += texture2D(uTex, vUv + uTexel * 2.0).rgb * 0.1216216;\n"
        "    c += texture2D(uTex, vUv - uTexel * 2.0).rgb * 0.1216216;\n"
        "    c += texture2D(uTex, vUv + uTexel * 3.0).rgb * 0.054054;\n"
        "    c += texture2D(uTex, vUv - uTexel * 3.0).rgb * 0.054054;\n"
        "    c += texture2D(uTex, vUv + uTexel * 4.0).rgb * 0.016216;\n"
        "    c += texture2D(uTex, vUv - uTexel * 4.0).rgb * 0.016216;\n"
        "    gl_FragColor = vec4(c, 1.0);\n"
        "  } else {\n"
        "    vec3 scene = texture2D(uTex, vUv).rgb;\n"
        "    vec3 glow = texture2D(uBloom, vUv).rgb;\n"
        "    gl_FragColor = vec4(scene + glow * uStrength, 1.0);\n"
        "  }\n"
        "}\n";
    auto stage = [&](unsigned kind, const char* text) -> unsigned {
        unsigned sh = csh(kind);
        if (!sh) return 0;
        src(sh, 1, &text, nullptr);
        comp(sh);
        int ok = 0;
        siv(sh, NEXA_GL_COMPILE_STATUS, &ok);
        if (!ok) { if (del) del(sh); return 0; }
        return sh;
    };
    unsigned v = stage(NEXA_GL_VERTEX_SHADER, vs);
    unsigned f = stage(NEXA_GL_FRAGMENT_SHADER, fs);
    if (!v || !f) {
        if (del) { if (v) del(v); if (f) del(f); }
        return;
    }
    unsigned p = cpr();
    att(p, v); att(p, f); link(p);
    if (del) { del(v); del(f); }
    int ok = 0;
    piv(p, NEXA_GL_LINK_STATUS, &ok);
    if (!ok) return;
    __nexa_g3_bloom_prog = p;
    __nexa_g3_bloom_u_mode = loc(p, "uMode");
    __nexa_g3_bloom_u_texel = loc(p, "uTexel");
    __nexa_g3_bloom_u_str = loc(p, "uStrength");
#else
    (void)0;
#endif
}

static void __nexa_g3_bloom_present(void) {
#if !defined(NEXA_WASM)
    if (!__nexa_g3_bloom_on || __nexa_g3_bloom <= 0.0f) return;
    if (__nexa_g3.vulkan || __nexa_g3.core33) return;
    if (!__nexa_gl.GenTextures || !__nexa_gl.BindTexture || !__nexa_gl.TexImage2D ||
        !__nexa_gl.TexParameteri || !__nexa_gl.Viewport || !__nexa_gl.Begin ||
        !__nexa_gl.End || !__nexa_gl.Vertex3f || !__nexa_gl.TexCoord2f ||
        !__nexa_gl.MatrixMode || !__nexa_gl.LoadMatrixf || !__nexa_gl.Disable ||
        !__nexa_gl.Enable) return;
    __nexa_g3_bloom_make();
    if (!__nexa_g3_bloom_prog) return;
    typedef void (NEXA_GLAPI *Use)(unsigned int);
    typedef void (NEXA_GLAPI *U1f)(int, float);
    typedef void (NEXA_GLAPI *U2f)(int, float, float);
    typedef void (NEXA_GLAPI *U1i)(int, int);
    typedef void (NEXA_GLAPI *Act)(unsigned int);
    typedef void (NEXA_GLAPI *CopySub)(unsigned int, int, int, int, int, int, int, int);
    typedef void (NEXA_GLAPI *DepthMask)(unsigned char);
    static Use use; static U1f u1f; static U2f u2f; static U1i u1i; static Act act;
    static CopySub copysub; static DepthMask depthmask;
    static int got = 0;
    if (!got) {
        use = (Use)__nexa_g3_glproc("glUseProgram");
        u1f = (U1f)__nexa_g3_glproc("glUniform1f");
        u2f = (U2f)__nexa_g3_glproc("glUniform2f");
        u1i = (U1i)__nexa_g3_glproc("glUniform1i");
        act = (Act)__nexa_g3_glproc("glActiveTexture");
        copysub = (CopySub)__nexa_g3_glproc("glCopyTexSubImage2D");
        depthmask = (DepthMask)__nexa_g3_glproc("glDepthMask");
        got = 1;
    }
    if (!use || !u1f || !u2f || !copysub) return;
    int w = __nexa_g3.w, h = __nexa_g3.h;
    if (w < 4 || h < 4 || w > 4096 || h > 4096) return;
    int bw = w / 4; if (bw < 2) bw = 2;
    int bh = h / 4; if (bh < 2) bh = 2;
    if (act) act(0x84C0u);
    auto tex = [&](unsigned* id, int* tw, int* th, int ww, int hh) {
        if (!*id || *tw != ww || *th != hh) {
            if (!*id) __nexa_gl.GenTextures(1, id);
            __nexa_gl.BindTexture(NEXA_GL_TEXTURE_2D, *id);
            __nexa_gl.TexParameteri(NEXA_GL_TEXTURE_2D, NEXA_GL_TEXTURE_MIN_FILTER, (__nexa_GLint)NEXA_GL_LINEAR);
            __nexa_gl.TexParameteri(NEXA_GL_TEXTURE_2D, NEXA_GL_TEXTURE_MAG_FILTER, (__nexa_GLint)NEXA_GL_LINEAR);
            __nexa_gl.TexParameteri(NEXA_GL_TEXTURE_2D, NEXA_GL_TEXTURE_WRAP_S, 0x812F);
            __nexa_gl.TexParameteri(NEXA_GL_TEXTURE_2D, NEXA_GL_TEXTURE_WRAP_T, 0x812F);
            __nexa_gl.TexImage2D(NEXA_GL_TEXTURE_2D, 0, (__nexa_GLint)NEXA_GL_RGBA, ww, hh, 0,
                                 NEXA_GL_RGBA, NEXA_GL_UNSIGNED_BYTE, nullptr);
            *tw = ww; *th = hh;
        } else {
            __nexa_gl.BindTexture(NEXA_GL_TEXTURE_2D, *id);
        }
    };
    tex(&__nexa_g3_bloom_scene, &__nexa_g3_bloom_sw, &__nexa_g3_bloom_sh, w, h);
    copysub(NEXA_GL_TEXTURE_2D, 0, 0, 0, 0, 0, w, h);
    tex(&__nexa_g3_bloom_a, &__nexa_g3_bloom_aw, &__nexa_g3_bloom_ah, bw, bh);
    tex(&__nexa_g3_bloom_b, &__nexa_g3_bloom_bw, &__nexa_g3_bloom_bh, bw, bh);

    use(__nexa_g3_bloom_prog);
    typedef int (NEXA_GLAPI *Loc)(unsigned int, const char*);
    static Loc loc = nullptr;
    static int gotloc = 0;
    if (!gotloc) { loc = (Loc)__nexa_g3_glproc("glGetUniformLocation"); gotloc = 1; }
    if (u1i && loc) {
        int ut = loc(__nexa_g3_bloom_prog, "uTex");
        int ub = loc(__nexa_g3_bloom_prog, "uBloom");
        if (ut >= 0) u1i(ut, 0);
        if (ub >= 0) u1i(ub, 1);
    }
    __nexa_gl.Disable(NEXA_GL_DEPTH_TEST);
    __nexa_gl.Disable(NEXA_GL_CULL_FACE);
    __nexa_gl.Disable(NEXA_GL_BLEND);
    if (depthmask) depthmask(0);
    const float I[16] = {1,0,0,0, 0,1,0,0, 0,0,1,0, 0,0,0,1};
    __nexa_gl.MatrixMode(NEXA_GL_PROJECTION);
    __nexa_gl.LoadMatrixf(I);
    __nexa_gl.MatrixMode(NEXA_GL_MODELVIEW);
    __nexa_gl.LoadMatrixf(I);
    auto quad = [&]() {
        __nexa_gl.Begin(NEXA_GL_TRIANGLES);
        __nexa_gl.TexCoord2f(0.0f, 1.0f); __nexa_gl.Vertex3f(-1.0f,  1.0f, 0.0f);
        __nexa_gl.TexCoord2f(0.0f, 0.0f); __nexa_gl.Vertex3f(-1.0f, -1.0f, 0.0f);
        __nexa_gl.TexCoord2f(1.0f, 0.0f); __nexa_gl.Vertex3f( 1.0f, -1.0f, 0.0f);
        __nexa_gl.TexCoord2f(0.0f, 1.0f); __nexa_gl.Vertex3f(-1.0f,  1.0f, 0.0f);
        __nexa_gl.TexCoord2f(1.0f, 0.0f); __nexa_gl.Vertex3f( 1.0f, -1.0f, 0.0f);
        __nexa_gl.TexCoord2f(1.0f, 1.0f); __nexa_gl.Vertex3f( 1.0f,  1.0f, 0.0f);
        __nexa_gl.End();
    };
    __nexa_gl.Viewport(0, 0, bw, bh);
    if (act) act(0x84C0u);
    __nexa_gl.BindTexture(NEXA_GL_TEXTURE_2D, __nexa_g3_bloom_scene);
    if (u1f && __nexa_g3_bloom_u_mode >= 0) u1f(__nexa_g3_bloom_u_mode, 0.0f);
    if (u2f && __nexa_g3_bloom_u_texel >= 0) u2f(__nexa_g3_bloom_u_texel, 0.0f, 0.0f);
    quad();
    __nexa_gl.BindTexture(NEXA_GL_TEXTURE_2D, __nexa_g3_bloom_a);
    copysub(NEXA_GL_TEXTURE_2D, 0, 0, 0, 0, 0, bw, bh);
    // Two wide passes. One tight blur dies on the silhouette of a big bright shape.
    for (int pass = 0; pass < 2; pass++) {
        __nexa_gl.BindTexture(NEXA_GL_TEXTURE_2D, __nexa_g3_bloom_a);
        if (u1f && __nexa_g3_bloom_u_mode >= 0) u1f(__nexa_g3_bloom_u_mode, 1.0f);
        if (u2f && __nexa_g3_bloom_u_texel >= 0) u2f(__nexa_g3_bloom_u_texel, 2.0f / (float)bw, 0.0f);
        quad();
        __nexa_gl.BindTexture(NEXA_GL_TEXTURE_2D, __nexa_g3_bloom_b);
        copysub(NEXA_GL_TEXTURE_2D, 0, 0, 0, 0, 0, bw, bh);
        __nexa_gl.BindTexture(NEXA_GL_TEXTURE_2D, __nexa_g3_bloom_b);
        if (u2f && __nexa_g3_bloom_u_texel >= 0) u2f(__nexa_g3_bloom_u_texel, 0.0f, 2.0f / (float)bh);
        quad();
        __nexa_gl.BindTexture(NEXA_GL_TEXTURE_2D, __nexa_g3_bloom_a);
        copysub(NEXA_GL_TEXTURE_2D, 0, 0, 0, 0, 0, bw, bh);
    }

    __nexa_gl.Viewport(0, 0, w, h);
    if (act) act(0x84C0u);
    __nexa_gl.BindTexture(NEXA_GL_TEXTURE_2D, __nexa_g3_bloom_scene);
    if (act) act(0x84C1u);
    __nexa_gl.BindTexture(NEXA_GL_TEXTURE_2D, __nexa_g3_bloom_a);
    if (act) act(0x84C0u);
    if (u1f && __nexa_g3_bloom_u_mode >= 0) u1f(__nexa_g3_bloom_u_mode, 2.0f);
    if (u1f && __nexa_g3_bloom_u_str >= 0) u1f(__nexa_g3_bloom_u_str, __nexa_g3_bloom);
    quad();

    use(0);
    if (act) act(0x84C0u);
    __nexa_gl.BindTexture(NEXA_GL_TEXTURE_2D, 0);
    __nexa_gl.Disable(NEXA_GL_TEXTURE_2D);
    if (depthmask) depthmask(1);
    __nexa_gl.Enable(NEXA_GL_CULL_FACE);
    __nexa_gl.Enable(NEXA_GL_DEPTH_TEST);
    __nexa_gl.MatrixMode(NEXA_GL_PROJECTION);
    __nexa_gl.LoadMatrixf(__nexa_g3.proj);
    __nexa_gl.MatrixMode(NEXA_GL_MODELVIEW);
    __nexa_gl.LoadMatrixf(__nexa_g3.view);
#else
    (void)0;
#endif
}

static int __nexa_gfx3d_bloom_set(int level) {
    if (level < 0) level = 0;
    if (level > 255) level = 255;
    __nexa_g3_bloom = (float)level / 255.0f;
    __nexa_g3_bloom_on = level > 0 ? 1 : 0;
    return level;
}

static int __nexa_gfx3d_bloom(void) {
    __nexa_gfx3d_bloom_set(255);
    return 1;
}
)NEXA_BLOOM");

    put("    if (!__nexa_g3.ready) return;\n"
        "    // Last thing before the swap, so it is over everything the frame drew.\n"
        "    __nexa_g3_overlay_present();\n",
        "    if (!__nexa_g3.ready) return;\n"
        "    __nexa_g3_bloom_present();\n"
        "    // Last thing before the swap, so it is over everything the frame drew.\n"
        "    __nexa_g3_overlay_present();\n");
}
