// gfx3d.glint. A tight highlight where the sun or the lamp bounces toward
// the camera. A program that never calls it does not run this, so its source
// stays as written. Applied after the lamp: the sun's highlight is added
// before a shadow darkens the colour, and the lamp's is added after, the
// same way the lamp's own light is.
inline void gfx3dApplyGlint(std::string& s) {
    auto put = [&](const std::string& from, const std::string& to) -> bool {
        const size_t at = s.find(from);
        if (at == std::string::npos) return false;
        s.replace(at, from.size(), to);
        return true;
    };
    auto all = [&](const std::string& from, const std::string& to) {
        size_t at = 0;
        while ((at = s.find(from, at)) != std::string::npos) {
            s.replace(at, from.size(), to);
            at += to.size();
        }
    };

    // 256 keeps the lobe to a few degrees. A flat face has one normal, so
    // only the part of it that aims the bounce at the camera lights up.
    const char* kPow = "256.0";
    const std::string sun =
        std::string("        \"  if (uGlint > 0.0) {\\n\"\n") +
        "        \"    vec3 sgeo = cross(dFdx(vLampPos), dFdy(vLampPos));\\n\"\n"
        "        \"    vec3 sn = (uMesh > 0.5) ? vLampN : sgeo;\\n\"\n"
        "        \"    float snl = dot(sn, sn);\\n\"\n"
        "        \"    if (snl > 0.0000001) {\\n\"\n"
        "        \"      sn = sn * (1.0 / sqrt(snl));\\n\"\n"
        "        \"      vec3 sv = -vLampPos;\\n\"\n"
        "        \"      float svl = dot(sv, sv);\\n\"\n"
        "        \"      vec3 sl = uSun;\\n\"\n"
        "        \"      float sll = dot(sl, sl);\\n\"\n"
        "        \"      if (svl > 0.0000001 && sll > 0.0000001) {\\n\"\n"
        "        \"        sv = sv * (1.0 / sqrt(svl));\\n\"\n"
        "        \"        sl = sl * (1.0 / sqrt(sll));\\n\"\n"
        "        \"        float snd = dot(sn, sl);\\n\"\n"
        "        \"        if (uSide > 0.5) snd = abs(snd);\\n\"\n"
        "        \"        else if (snd < 0.0) snd = 0.0;\\n\"\n"
        "        \"        vec3 shv = sl + sv;\\n\"\n"
        "        \"        float shl = dot(shv, shv);\\n\"\n"
        "        \"        if (snd > 0.0 && shl > 0.0000001) {\\n\"\n"
        "        \"          shv = shv * (1.0 / sqrt(shl));\\n\"\n"
        "        \"          float snh = abs(dot(sn, shv));\\n\"\n"
        "        \"          c += uSunCol * (uGlint * snd * pow(snh, " + kPow + "));\\n\"\n"
        "        \"        }\\n\"\n"
        "        \"      }\\n\"\n"
        "        \"    }\\n\"\n"
        "        \"  }\\n\"\n";
    const std::string guns =
        "        \"uniform float uGlint;\\n\"\n"
        "        \"uniform vec3 uSun;\\n\"\n"
        "        \"uniform vec3 uSunCol;\\n\"\n";

    // The shadow shader has no view-space position until the lamp added one.
    // A glint with a shadow and no lamp has to add it itself, and must not
    // add it a second time when the lamp already did.
    if (s.find("vLampPos") == std::string::npos) {
        all("        \"varying float vFace;\\n\"\n",
            "        \"varying float vFace;\\n\"\n"
            "        \"varying vec3 vLampPos;\\n\"\n"
            "        \"varying vec3 vLampN;\\n\"\n");
        put("        \"  vFace = dot(gl_NormalMatrix * gl_Normal, uToLight);\\n\"\n",
            "        \"  vFace = dot(gl_NormalMatrix * gl_Normal, uToLight);\\n\"\n"
            "        \"  vLampPos = (gl_ModelViewMatrix * gl_Vertex).xyz;\\n\"\n"
            "        \"  vLampN = gl_NormalMatrix * gl_Normal;\\n\"\n");
    }
    if (s.find("uniform float uMesh") != std::string::npos) {
        all("        \"uniform float uMesh;\\n\"\n",
            "        \"uniform float uMesh;\\n\"\n" + guns);
    } else if (s.find("uniform float uLine") != std::string::npos) {
        put("        \"uniform float uLine;\\n\"\n",
            "        \"uniform float uLine;\\n\"\n" + guns +
            "        \"uniform float uSide;\\n\"\n"
            "        \"uniform float uMesh;\\n\"\n");
    }
    // The lamp's own shader. The sun's highlight joins the colour here; there
    // is no shadow multiply in this program, so nothing darkens it.
    put("        \"  vec3 c = vCol;\\n\"\n"
        "        \"  vec3 geo = cross(dFdx(vLampPos), dFdy(vLampPos));\\n\"\n",
        "        \"  vec3 c = vCol;\\n\"\n" + sun +
        "        \"  vec3 geo = cross(dFdx(vLampPos), dFdy(vLampPos));\\n\"\n");
    // The shadow shader. This highlight is in c before the darken, so a
    // shadow takes the sun's glint with it.
    put("        \"  vec3 c = vCol;\\n\"\n"
        "        \"  if (vLight.w > 0.0) {\\n\"\n",
        "        \"  vec3 c = vCol;\\n\"\n" + sun +
        "        \"  if (vLight.w > 0.0) {\\n\"\n");
    // The lamp's highlight, after the shadow darken when that shader is the
    // one drawing. Same reach falloff as the lamp's light.
    all("        \"        c += uLampCol * nd * t * t;\\n\"\n",
        "        \"        c += uLampCol * nd * t * t;\\n\"\n"
        "        \"        if (uGlint > 0.0) {\\n\"\n"
        "        \"          vec3 lv = -vLampPos;\\n\"\n"
        "        \"          float lvl = dot(lv, lv);\\n\"\n"
        "        \"          if (lvl > 0.0000001) {\\n\"\n"
        "        \"            lv = lv * (1.0 / sqrt(lvl));\\n\"\n"
        "        \"            vec3 ld = d / dist;\\n\"\n"
        "        \"            vec3 lh = ld + lv;\\n\"\n"
        "        \"            float lhl = dot(lh, lh);\\n\"\n"
        "        \"            if (lhl > 0.0000001) {\\n\"\n"
        "        \"              lh = lh * (1.0 / sqrt(lhl));\\n\"\n"
        "        \"              float lnh = abs(dot(n, lh));\\n\"\n"
        "        \"              c += uLampCol * (uGlint * nd * pow(lnh, " + std::string(kPow) + ") * t * t);\\n\"\n"
        "        \"            }\\n\"\n"
        "        \"          }\\n\"\n"
        "        \"        }\\n\"\n");

    put("static void __nexa_g3_apply_camera(void) {\n",
        "static int __nexa_g3_glint_on = 0;\n"
        "static int __nexa_g3_glint_lines = 0;\n"
        "static int __nexa_g3_glint_mesh = 1;\n"
        "static int __nexa_g3_glint_side = 0;\n"
        "static int __nexa_g3_glint_bound = 0;\n"
        "static int __nexa_g3_glint_tried = 0;\n"
        "static float __nexa_g3_glint = 0;\n"
        "static unsigned __nexa_g3_glint_prog = 0;\n"
        "static unsigned __nexa_g3_glint_locp = 0;\n"
        "static int __nexa_g3_glint_u[5] = {-1, -1, -1, -1, -1};\n"
        "static std::vector<float> __nexa_g3_glint_col;\n"
        "static void __nexa_g3_glint_pack(float* v, int stride, int col, int n,\n"
        "    const float* pos, const float* nrm, const float* M, int two);\n"
        "static void __nexa_g3_glint_batch(void);\n"
        "static void __nexa_g3_glint_bind(void);\n"
        "static void __nexa_g3_glint_normals(const float* n);\n"
        "static void __nexa_g3_apply_camera(void) {\n");

    put("static int __nexa_gfx3d_vsync_get(void) {\n    return __nexa_g3.vsync;\n}\n",
        std::string("static int __nexa_gfx3d_vsync_get(void) {\n    return __nexa_g3.vsync;\n}\n") +
        R"NEXA_GLINT(
static void __nexa_g3_glint_add(float* rgb, float nx, float ny, float nz,
                                float lx, float ly, float lz,
                                float vx, float vy, float vz,
                                float cr, float cg, float cb, float fall, int two) {
    float nd = nx * lx + ny * ly + nz * lz;
    if (two) { if (nd < 0.0f) nd = -nd; }
    else if (nd < 0.0f) nd = 0.0f;
    if (!(nd > 0.0f) || !(fall > 0.0f)) return;
    float hx = lx + vx, hy = ly + vy, hz = lz + vz;
    float hl = std::sqrt(hx * hx + hy * hy + hz * hz);
    if (hl < 1e-8f) return;
    hx /= hl; hy /= hl; hz /= hl;
    float nh = nx * hx + ny * hy + nz * hz;
    if (nh < 0.0f) nh = -nh;
    float s = nh;
    s *= s; s *= s; s *= s; s *= s; s *= s; s *= s; s *= s; s *= s;
    float w = __nexa_g3_glint * nd * s * fall;
    rgb[0] += cr * w;
    rgb[1] += cg * w;
    rgb[2] += cb * w;
    if (rgb[0] > 1.0f) rgb[0] = 1.0f;
    if (rgb[1] > 1.0f) rgb[1] = 1.0f;
    if (rgb[2] > 1.0f) rgb[2] = 1.0f;
}

static void __nexa_g3_glint_one(float* rgb, float wx, float wy, float wz,
                                float nx, float ny, float nz, int two) {
    if (!__nexa_g3_glint_on || __nexa_g3_glint <= 0.0f || !rgb) return;
    float nlen = std::sqrt(nx * nx + ny * ny + nz * nz);
    if (nlen < 1e-8f) return;
    nx /= nlen; ny /= nlen; nz /= nlen;
    float vx = __nexa_g3.eye[0] - wx, vy = __nexa_g3.eye[1] - wy, vz = __nexa_g3.eye[2] - wz;
    float vl = std::sqrt(vx * vx + vy * vy + vz * vz);
    if (vl < 1e-8f) return;
    vx /= vl; vy /= vl; vz /= vl;
    float lx = __nexa_g3_lx, ly = __nexa_g3_ly, lz = __nexa_g3_lz;
    float ll = std::sqrt(lx * lx + ly * ly + lz * lz);
    if (ll > 1e-8f) { lx /= ll; ly /= ll; lz /= ll; }
    __nexa_g3_glint_add(rgb, nx, ny, nz, lx, ly, lz, vx, vy, vz,
                        __nexa_g3_lr, __nexa_g3_lg, __nexa_g3_lb, 1.0f, two);
    /*NEXA_GLINT_LAMP*/
}

static void __nexa_g3_glint_world(const float* M, float x, float y, float z,
                                  float nx, float ny, float nz,
                                  float* wx, float* wy, float* wz,
                                  float* ox, float* oy, float* oz) {
    if (!M) {
        *wx = x; *wy = y; *wz = z;
        *ox = nx; *oy = ny; *oz = nz;
        return;
    }
    *wx = M[0] * x + M[4] * y + M[8] * z + M[12];
    *wy = M[1] * x + M[5] * y + M[9] * z + M[13];
    *wz = M[2] * x + M[6] * y + M[10] * z + M[14];
    *ox = M[0] * nx + M[4] * ny + M[8] * nz;
    *oy = M[1] * nx + M[5] * ny + M[9] * nz;
    *oz = M[2] * nx + M[6] * ny + M[10] * nz;
}

static void __nexa_g3_glint_pack(float* v, int stride, int col, int n,
                                 const float* pos, const float* nrm, const float* M, int two) {
    if (!__nexa_g3_glint_on || __nexa_g3_tex_cur || !v || !pos || n <= 0) return;
    for (int i = 0; i < n; i++) {
        float nx = nrm ? nrm[i * 3] : 0.0f;
        float ny = nrm ? nrm[i * 3 + 1] : 1.0f;
        float nz = nrm ? nrm[i * 3 + 2] : 0.0f;
        float wx, wy, wz, ox, oy, oz;
        __nexa_g3_glint_world(M, pos[i * 3], pos[i * 3 + 1], pos[i * 3 + 2],
                              nx, ny, nz, &wx, &wy, &wz, &ox, &oy, &oz);
        __nexa_g3_glint_one(v + i * stride + col, wx, wy, wz, ox, oy, oz, two);
    }
}

static void __nexa_g3_glint_batch(void) {
    if (!__nexa_g3_glint_on || __nexa_g3_glint <= 0.0f || __nexa_g3_prim || __nexa_g3_tex_cur) return;
    for (int i = 0; i + 2 < __nexa_g3_vn; i += 3) {
        float* a = &__nexa_g3_vb[i * 6];
        float* b = &__nexa_g3_vb[(i + 1) * 6];
        float* c = &__nexa_g3_vb[(i + 2) * 6];
        float ux = b[0] - a[0], uy = b[1] - a[1], uz = b[2] - a[2];
        float vx = c[0] - a[0], vy = c[1] - a[1], vz = c[2] - a[2];
        float nx = uy * vz - uz * vy, ny = uz * vx - ux * vz, nz = ux * vy - uy * vx;
        __nexa_g3_glint_one(a + 3, a[0], a[1], a[2], nx, ny, nz, __nexa_g3_two_sided);
        __nexa_g3_glint_one(b + 3, b[0], b[1], b[2], nx, ny, nz, __nexa_g3_two_sided);
        __nexa_g3_glint_one(c + 3, c[0], c[1], c[2], nx, ny, nz, __nexa_g3_two_sided);
    }
}

static void __nexa_g3_glint_make(void) {
#if !defined(NEXA_WASM)
    if (__nexa_g3_glint_tried || __nexa_g3.core33 || __nexa_g3.vulkan) return;
    __nexa_g3_glint_tried = 1;
    typedef unsigned int (NEXA_GLAPI *CreateSh)(unsigned int);
    typedef void (NEXA_GLAPI *Source)(unsigned int, int, const char* const*, const int*);
    typedef void (NEXA_GLAPI *Shiv)(unsigned int, unsigned int, int*);
    typedef void (NEXA_GLAPI *Del)(unsigned int);
    typedef unsigned int (NEXA_GLAPI *CreatePr)(void);
    typedef void (NEXA_GLAPI *Att)(unsigned int, unsigned int);
    typedef void (NEXA_GLAPI *One)(unsigned int);
    static CreateSh csh; static Source src; static Shiv siv; static Del del;
    static CreatePr cpr; static Att att; static One comp, link; static Shiv piv;
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
        got = 1;
    }
    if (!csh || !src || !comp || !siv || !cpr || !att || !link || !piv) return;
    const char* vs =
        "#version 120\n"
        "varying vec3 vCol;\n"
        "varying vec3 vLampPos;\n"
        "varying vec3 vLampN;\n"
        "void main() {\n"
        "  gl_Position = ftransform();\n"
        "  vCol = gl_Color.rgb;\n"
        "  vLampPos = (gl_ModelViewMatrix * gl_Vertex).xyz;\n"
        "  vLampN = gl_NormalMatrix * gl_Normal;\n"
        "}\n";
    const char* fs =
        "#version 120\n"
        "uniform float uGlint;\n"
        "uniform vec3 uSun;\n"
        "uniform vec3 uSunCol;\n"
        "uniform float uSide;\n"
        "uniform float uMesh;\n"
        "varying vec3 vCol;\n"
        "varying vec3 vLampPos;\n"
        "varying vec3 vLampN;\n"
        "void main() {\n"
        "  vec3 c = vCol;\n"
        "  if (uGlint > 0.0) {\n"
        "    vec3 sgeo = cross(dFdx(vLampPos), dFdy(vLampPos));\n"
        "    vec3 sn = (uMesh > 0.5) ? vLampN : sgeo;\n"
        "    float snl = dot(sn, sn);\n"
        "    if (snl > 0.0000001) {\n"
        "      sn = sn * (1.0 / sqrt(snl));\n"
        "      vec3 sv = -vLampPos;\n"
        "      float svl = dot(sv, sv);\n"
        "      vec3 sl = uSun;\n"
        "      float sll = dot(sl, sl);\n"
        "      if (svl > 0.0000001 && sll > 0.0000001) {\n"
        "        sv = sv * (1.0 / sqrt(svl));\n"
        "        sl = sl * (1.0 / sqrt(sll));\n"
        "        float snd = dot(sn, sl);\n"
        "        if (uSide > 0.5) snd = abs(snd);\n"
        "        else if (snd < 0.0) snd = 0.0;\n"
        "        vec3 shv = sl + sv;\n"
        "        float shl = dot(shv, shv);\n"
        "        if (snd > 0.0 && shl > 0.0000001) {\n"
        "          shv = shv * (1.0 / sqrt(shl));\n"
        "          float snh = abs(dot(sn, shv));\n"
        "          c += uSunCol * (uGlint * snd * pow(snh, 256.0));\n"
        "        }\n"
        "      }\n"
        "    }\n"
        "  }\n"
        "  gl_FragColor = vec4(c, 1.0);\n"
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
    __nexa_g3_glint_prog = p;
#else
    (void)0;
#endif
}

static void __nexa_g3_glint_normals(const float* n) {
#if !defined(NEXA_WASM)
    if (__nexa_g3.core33 || __nexa_g3.vulkan || !__nexa_g3_glint_on) return;
    if (!__nexa_gl.EnableClientState || !__nexa_gl.DisableClientState) return;
    if (!n) {
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

static void __nexa_g3_glint_bind(void) {
#if !defined(NEXA_WASM)
    if (__nexa_g3.core33 || __nexa_g3.vulkan) return;
    __nexa_g3_glint_make();
    typedef void (NEXA_GLAPI *Use)(unsigned int);
    typedef void (NEXA_GLAPI *U1f)(int, float);
    typedef void (NEXA_GLAPI *U3f)(int, float, float, float);
    typedef int (NEXA_GLAPI *Loc)(unsigned int, const char*);
    static Use use = nullptr; static U1f u1f = nullptr; static U3f u3f = nullptr; static Loc loc = nullptr;
    static int got = 0;
    if (!got) {
        use = (Use)__nexa_g3_glproc("glUseProgram");
        u1f = (U1f)__nexa_g3_glproc("glUniform1f");
        u3f = (U3f)__nexa_g3_glproc("glUniform3f");
        loc = (Loc)__nexa_g3_glproc("glGetUniformLocation");
        got = 1;
    }
    if (!use) return;
    int want = __nexa_g3_glint_on && __nexa_g3_glint > 0.0f
               && !__nexa_g3_glint_lines && !__nexa_g3_tex_cur;
    const float* V = __nexa_g3.view;
    float sx = V[0] * __nexa_g3_lx + V[4] * __nexa_g3_ly + V[8] * __nexa_g3_lz;
    float sy = V[1] * __nexa_g3_lx + V[5] * __nexa_g3_ly + V[9] * __nexa_g3_lz;
    float sz = V[2] * __nexa_g3_lx + V[6] * __nexa_g3_ly + V[10] * __nexa_g3_lz;
    unsigned prog = 0;
    int held = 0;
    /*NEXA_GLINT_HOLD*/
    if (!prog && want && __nexa_g3_glint_prog) prog = __nexa_g3_glint_prog;
    if (!prog) {
        if (__nexa_g3_glint_bound) { use(0); __nexa_g3_glint_bound = 0; }
        return;
    }
    if (!held) { use(prog); __nexa_g3_glint_bound = 1; }
    if (loc && __nexa_g3_glint_locp != prog) {
        __nexa_g3_glint_locp = prog;
        __nexa_g3_glint_u[0] = loc(prog, "uGlint");
        __nexa_g3_glint_u[1] = loc(prog, "uSun");
        __nexa_g3_glint_u[2] = loc(prog, "uSunCol");
        __nexa_g3_glint_u[3] = loc(prog, "uSide");
        __nexa_g3_glint_u[4] = loc(prog, "uMesh");
    }
    float g = want ? __nexa_g3_glint : 0.0f;
    if (u1f && __nexa_g3_glint_u[0] >= 0) u1f(__nexa_g3_glint_u[0], g);
    if (u3f && __nexa_g3_glint_u[1] >= 0) u3f(__nexa_g3_glint_u[1], sx, sy, sz);
    if (u3f && __nexa_g3_glint_u[2] >= 0) u3f(__nexa_g3_glint_u[2], __nexa_g3_lr, __nexa_g3_lg, __nexa_g3_lb);
    if (u1f && __nexa_g3_glint_u[3] >= 0) u1f(__nexa_g3_glint_u[3], __nexa_g3_glint_side ? 1.0f : 0.0f);
    if (u1f && __nexa_g3_glint_u[4] >= 0) u1f(__nexa_g3_glint_u[4], __nexa_g3_glint_mesh ? 1.0f : 0.0f);
#else
    (void)0;
#endif
}

static int __nexa_gfx3d_glint_set(int level) {
    if (level < 0) level = 0;
    if (level > 255) level = 255;
    __nexa_g3_glint = (float)level / 255.0f;
    __nexa_g3_glint_on = level > 0 ? 1 : 0;
    return level;
}

static int __nexa_gfx3d_glint(void) {
    __nexa_gfx3d_glint_set(255);
    return 1;
}
)NEXA_GLINT");

    if (s.find("__nexa_g3_lamp_on") != std::string::npos) {
        const std::string mark = "/*NEXA_GLINT_LAMP*/";
        const size_t at = s.find(mark);
        if (at != std::string::npos) {
            s.replace(at, mark.size(),
                "if (__nexa_g3_lamp_on && __nexa_g3_lamp_reach > 0.0f) {\n"
                "        float dx = __nexa_g3_lamp_x - wx, dy = __nexa_g3_lamp_y - wy, dz = __nexa_g3_lamp_z - wz;\n"
                "        float dist2 = dx * dx + dy * dy + dz * dz;\n"
                "        float reach = __nexa_g3_lamp_reach;\n"
                "        if (dist2 < reach * reach && dist2 > 1e-12f) {\n"
                "            float dist = std::sqrt(dist2);\n"
                "            float t = 1.0f - dist / reach;\n"
                "            dx /= dist; dy /= dist; dz /= dist;\n"
                "            __nexa_g3_glint_add(rgb, nx, ny, nz, dx, dy, dz, vx, vy, vz,\n"
                "                __nexa_g3_lamp_r, __nexa_g3_lamp_g, __nexa_g3_lamp_b, t * t, two);\n"
                "        }\n"
                "    }");
        }
    }
    {
        std::string hold;
        if (s.find("__nexa_g3_shadow_bound") != std::string::npos) {
            hold +=
                "if (__nexa_g3_shadow_bound && __nexa_g3_shadow_on) return;\n"
                "    if (__nexa_g3_shadow_bound && __nexa_g3_shadow_recv) {\n"
                "        prog = __nexa_g3_shadow_recv;\n"
                "        held = 1;\n"
                "    }\n"
                "    ";
        }
        if (s.find("__nexa_g3_lamp_bound") != std::string::npos) {
            hold +=
                "if (!prog && __nexa_g3_lamp_bound && __nexa_g3_lamp_prog) {\n"
                "        prog = __nexa_g3_lamp_prog;\n"
                "        held = 1;\n"
                "    }\n"
                "    ";
        }
        const std::string mark = "/*NEXA_GLINT_HOLD*/";
        const size_t at = s.find(mark);
        if (at != std::string::npos && !hold.empty()) s.replace(at, mark.size(), hold);
    }

    const std::string mesh =
        "    __nexa_g3_glint_lines = 0;\n"
        "    __nexa_g3_glint_mesh = m.nrm.empty() ? 0 : 1;\n"
        "    __nexa_g3_glint_side = m.two_sided;\n"
        "    __nexa_g3_glint_bind();\n";
    const std::string batch =
        "    __nexa_g3_glint_lines = __nexa_g3_prim;\n"
        "    __nexa_g3_glint_mesh = 0;\n"
        "    __nexa_g3_glint_side = __nexa_g3_two_sided;\n"
        "    __nexa_g3_glint_bind();\n";
    if (s.find("__nexa_g3_lamp_bind();") != std::string::npos) {
        all("    __nexa_g3_lamp_bind();\n",
            "    __nexa_g3_lamp_bind();\n"
            "    __nexa_g3_glint_lines = __nexa_g3_lamp_lines;\n"
            "    __nexa_g3_glint_mesh = __nexa_g3_lamp_mesh;\n"
            "    __nexa_g3_glint_side = __nexa_g3_lamp_side;\n"
            "    __nexa_g3_glint_bind();\n");
    } else {
        const std::string meshCore = "    if (__nexa_g3.core33) return __nexa_g3_core_mesh(m, L, M);\n";
        const std::string batchCore = "    if (__nexa_g3.core33) { __nexa_g3_core_batch(); return; }\n";
        const bool sh = s.find("__nexa_g3_shadow_bind();") != std::string::npos;
        const bool fog = s.find("__nexa_g3_fog_bind();") != std::string::npos;
        if (sh) {
            put(meshCore + "    __nexa_g3_shadow_bind();\n",
                meshCore + "    __nexa_g3_shadow_bind();\n" + mesh);
            put(batchCore + "    __nexa_g3_shadow_bind();\n",
                batchCore + "    __nexa_g3_shadow_bind();\n" + batch);
        } else if (fog) {
            put(meshCore + "    __nexa_g3_fog_bind();\n",
                meshCore + "    __nexa_g3_fog_bind();\n" + mesh);
            put(batchCore + "    __nexa_g3_fog_bind();\n",
                batchCore + "    __nexa_g3_fog_bind();\n" + batch);
        } else {
            put(meshCore, meshCore + mesh);
            put(batchCore, batchCore + batch);
        }
    }

    if (s.find("__nexa_g3_lamp_normals(nullptr);") != std::string::npos) {
        put("    __nexa_g3_lamp_normals(m.nrm.empty() ? nullptr : m.nrm.data());\n",
            "    __nexa_g3_lamp_normals(m.nrm.empty() ? nullptr : m.nrm.data());\n"
            "    __nexa_g3_glint_normals(m.nrm.empty() ? nullptr : m.nrm.data());\n");
        put("    __nexa_g3_lamp_normals(nullptr);\n",
            "    __nexa_g3_lamp_normals(nullptr);\n"
            "    __nexa_g3_glint_normals(nullptr);\n");
    } else if (s.find("__nexa_g3_shadow_normals(nullptr);") != std::string::npos) {
        put("    __nexa_g3_shadow_normals(m.nrm.empty() ? nullptr : m.nrm.data());\n",
            "    __nexa_g3_shadow_normals(m.nrm.empty() ? nullptr : m.nrm.data());\n"
            "    __nexa_g3_glint_normals(m.nrm.empty() ? nullptr : m.nrm.data());\n");
        put("    __nexa_g3_shadow_normals(nullptr);\n",
            "    __nexa_g3_shadow_normals(nullptr);\n"
            "    __nexa_g3_glint_normals(nullptr);\n");
    } else {
        put("    __nexa_gl.ColorPointer(3, NEXA_GL_FLOAT, 0, col);\n",
            "    __nexa_gl.ColorPointer(3, NEXA_GL_FLOAT, 0, col);\n"
            "    __nexa_g3_glint_normals(m.nrm.empty() ? nullptr : m.nrm.data());\n");
        put("    __nexa_gl.DrawArrays(NEXA_GL_TRIANGLES, 0, (__nexa_GLsizei)(m.pos.size() / 3));\n"
            "    if (cut) {\n",
            "    __nexa_gl.DrawArrays(NEXA_GL_TRIANGLES, 0, (__nexa_GLsizei)(m.pos.size() / 3));\n"
            "    __nexa_g3_glint_normals(nullptr);\n"
            "    if (cut) {\n");
    }

    const std::string packMesh =
        "m.pos.data(), m.nrm.empty() ? nullptr : m.nrm.data(), M, m.two_sided);\n";
    if (s.find("__nexa_g3_lamp_pack(") != std::string::npos) {
        all("        __nexa_g3_lamp_pack(__nexa_g3_lamp_col.data(), 3, 0, (int)(m.pos.size() / 3),\n"
            "            " + packMesh,
            "        __nexa_g3_lamp_pack(__nexa_g3_lamp_col.data(), 3, 0, (int)(m.pos.size() / 3),\n"
            "            " + packMesh +
            "        __nexa_g3_glint_pack(__nexa_g3_lamp_col.data(), 3, 0, (int)(m.pos.size() / 3),\n"
            "            " + packMesh);
        all("        __nexa_g3_lamp_pack(tmp.data(), 6, 3, (int)n, " + packMesh,
            "        __nexa_g3_lamp_pack(tmp.data(), 6, 3, (int)n, " + packMesh +
            "        __nexa_g3_glint_pack(tmp.data(), 6, 3, (int)n, " + packMesh);
        all("            __nexa_g3_lamp_pack(tmp.data(), 8, 3, (int)n, " + packMesh,
            "            __nexa_g3_lamp_pack(tmp.data(), 8, 3, (int)n, " + packMesh +
            "            __nexa_g3_glint_pack(tmp.data(), 8, 3, (int)n, " + packMesh);
        put("        __nexa_g3_lamp_batch();\n"
            "        nexa_vk14::draw(__nexa_g3_vb, __nexa_g3_vn, __nexa_g3_prim, __nexa_g3_two_sided, mvp);\n",
            "        __nexa_g3_lamp_batch();\n"
            "        __nexa_g3_glint_batch();\n"
            "        nexa_vk14::draw(__nexa_g3_vb, __nexa_g3_vn, __nexa_g3_prim, __nexa_g3_two_sided, mvp);\n");
        put("static void __nexa_g3_core_batch(void) {\n"
            "    __nexa_g3_lamp_batch();\n",
            "static void __nexa_g3_core_batch(void) {\n"
            "    __nexa_g3_lamp_batch();\n"
            "    __nexa_g3_glint_batch();\n");
        put("    __nexa_g3_lamp_batch();\n"
            "    glDrawArrays(__nexa_g3_prim ? NEXA_GL_LINES : NEXA_GL_TRIANGLES, 0, __nexa_g3_vn);\n",
            "    __nexa_g3_lamp_batch();\n"
            "    __nexa_g3_glint_batch();\n"
            "    glDrawArrays(__nexa_g3_prim ? NEXA_GL_LINES : NEXA_GL_TRIANGLES, 0, __nexa_g3_vn);\n");
    } else {
        put("    if (L.gl_stale) {\n"
            "        __nexa_g33.BufferData(NEXA_GL_ARRAY_BUFFER, (__nexa_GLsizeiptr)(L.col.size() * sizeof(float)),\n"
            "                              L.col.data(), NEXA_GL_DYNAMIC_DRAW);\n"
            "        L.gl_stale = 0;\n"
            "    }\n",
            "    if (__nexa_g3_glint_on) {\n"
            "        __nexa_g3_glint_col = L.col;\n"
            "        __nexa_g3_glint_pack(__nexa_g3_glint_col.data(), 3, 0, (int)(m.pos.size() / 3),\n"
            "            " + packMesh +
            "        __nexa_g33.BufferData(NEXA_GL_ARRAY_BUFFER, (__nexa_GLsizeiptr)(__nexa_g3_glint_col.size() * sizeof(float)),\n"
            "                              __nexa_g3_glint_col.data(), NEXA_GL_DYNAMIC_DRAW);\n"
            "        L.gl_stale = 0;\n"
            "    } else if (L.gl_stale) {\n"
            "        __nexa_g33.BufferData(NEXA_GL_ARRAY_BUFFER, (__nexa_GLsizeiptr)(L.col.size() * sizeof(float)),\n"
            "                              L.col.data(), NEXA_GL_DYNAMIC_DRAW);\n"
            "        L.gl_stale = 0;\n"
            "    }\n");
        put("    if (L.gl_stale) {\n"
            "        glBufferData(NEXA_GL_ARRAY_BUFFER, (__nexa_GLsizeiptr)(L.col.size() * sizeof(float)),\n"
            "                     L.col.data(), NEXA_GL_DYNAMIC_DRAW);\n"
            "        L.gl_stale = 0;\n"
            "    }\n",
            "    if (__nexa_g3_glint_on) {\n"
            "        __nexa_g3_glint_col = L.col;\n"
            "        __nexa_g3_glint_pack(__nexa_g3_glint_col.data(), 3, 0, (int)(m.pos.size() / 3),\n"
            "            " + packMesh +
            "        glBufferData(NEXA_GL_ARRAY_BUFFER, (__nexa_GLsizeiptr)(__nexa_g3_glint_col.size() * sizeof(float)),\n"
            "                     __nexa_g3_glint_col.data(), NEXA_GL_DYNAMIC_DRAW);\n"
            "        L.gl_stale = 0;\n"
            "    } else if (L.gl_stale) {\n"
            "        glBufferData(NEXA_GL_ARRAY_BUFFER, (__nexa_GLsizeiptr)(L.col.size() * sizeof(float)),\n"
            "                     L.col.data(), NEXA_GL_DYNAMIC_DRAW);\n"
            "        L.gl_stale = 0;\n"
            "    }\n");
        put("        nexa_vk14::draw(tmp.data(), (int)n, 0, m.two_sided, mvp);\n",
            "        __nexa_g3_glint_pack(tmp.data(), 6, 3, (int)n, " + packMesh +
            "        nexa_vk14::draw(tmp.data(), (int)n, 0, m.two_sided, mvp);\n");
        put("            nexa_vk14::draw_tex(tmp.data(), (int)n, t.px.data(), t.w, t.h, t.cutout, m.two_sided || t.cutout, mvp);\n",
            "            __nexa_g3_glint_pack(tmp.data(), 8, 3, (int)n, " + packMesh +
            "            nexa_vk14::draw_tex(tmp.data(), (int)n, t.px.data(), t.w, t.h, t.cutout, m.two_sided || t.cutout, mvp);\n");
        put("        nexa_vk14::draw(__nexa_g3_vb, __nexa_g3_vn, __nexa_g3_prim, __nexa_g3_two_sided, mvp);\n",
            "        __nexa_g3_glint_batch();\n"
            "        nexa_vk14::draw(__nexa_g3_vb, __nexa_g3_vn, __nexa_g3_prim, __nexa_g3_two_sided, mvp);\n");
        put("static void __nexa_g3_core_batch(void) {\n",
            "static void __nexa_g3_core_batch(void) {\n"
            "    __nexa_g3_glint_batch();\n");
        put("    glDrawArrays(__nexa_g3_prim ? NEXA_GL_LINES : NEXA_GL_TRIANGLES, 0, __nexa_g3_vn);\n",
            "    __nexa_g3_glint_batch();\n"
            "    glDrawArrays(__nexa_g3_prim ? NEXA_GL_LINES : NEXA_GL_TRIANGLES, 0, __nexa_g3_vn);\n");
    }
}
