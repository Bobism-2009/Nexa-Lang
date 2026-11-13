// gfx3d.lamp. A light with a place: full at that spot, gone at its reach,
// added on top of the sun. A program that never calls it does not run this,
// so its source stays as written. Applied after the shadow, so the shade
// can stay bright.
inline void gfx3dApplyLamp(std::string& s) {
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

    // The fixed-function shadow shader owns the program while it is receiving.
    // The lamp is added after that darken, so a spot in the shade can be bright.
    all("        \"varying float vFace;\\n\"\n",
        "        \"varying float vFace;\\n\"\n"
        "        \"varying vec3 vLampPos;\\n\"\n"
        "        \"varying vec3 vLampN;\\n\"\n");
    put("        \"uniform float uLine;\\n\"\n",
        "        \"uniform float uLine;\\n\"\n"
        "        \"uniform vec3 uLamp;\\n\"\n"
        "        \"uniform vec3 uLampCol;\\n\"\n"
        "        \"uniform float uReach;\\n\"\n"
        "        \"uniform float uOn;\\n\"\n"
        "        \"uniform float uSide;\\n\"\n"
        "        \"uniform float uMesh;\\n\"\n");
    put("        \"  vFace = dot(gl_NormalMatrix * gl_Normal, uToLight);\\n\"\n",
        "        \"  vFace = dot(gl_NormalMatrix * gl_Normal, uToLight);\\n\"\n"
        "        \"  vLampPos = (gl_ModelViewMatrix * gl_Vertex).xyz;\\n\"\n"
        "        \"  vLampN = gl_NormalMatrix * gl_Normal;\\n\"\n");
    put("        \"  }\\n\"\n"
        "        \"  gl_FragColor = vec4(c, 1.0);\\n\"\n",
        "        \"  }\\n\"\n"
        "        \"  vec3 geo = cross(dFdx(vLampPos), dFdy(vLampPos));\\n\"\n"
        "        \"  if (uOn > 0.5 && uReach > 0.0) {\\n\"\n"
        "        \"    vec3 d = uLamp - vLampPos;\\n\"\n"
        "        \"    float dist2 = dot(d, d);\\n\"\n"
        "        \"    if (dist2 < uReach * uReach && dist2 > 0.0000001) {\\n\"\n"
        "        \"      float dist = sqrt(dist2);\\n\"\n"
        "        \"      float t = 1.0 - dist / uReach;\\n\"\n"
        "        \"      vec3 n = (uMesh > 0.5) ? vLampN : geo;\\n\"\n"
        "        \"      float nl = dot(n, n);\\n\"\n"
        "        \"      if (nl > 0.0000001) {\\n\"\n"
        "        \"        n = n * (1.0 / sqrt(nl));\\n\"\n"
        "        \"        float nd = dot(n, d) / dist;\\n\"\n"
        "        \"        if (uSide > 0.5) nd = abs(nd);\\n\"\n"
        "        \"        else if (nd < 0.0) nd = 0.0;\\n\"\n"
        "        \"        c += uLampCol * nd * t * t;\\n\"\n"
        "        \"      }\\n\"\n"
        "        \"    }\\n\"\n"
        "        \"  }\\n\"\n"
        "        \"  gl_FragColor = vec4(c, 1.0);\\n\"\n");

    // Vulkan and core add the lamp at each corner. A box face is two triangles,
    // so a lamp in the middle of the floor would never meet a corner. Cut each
    // face into a grid; the sun on a flat face does not change.
    {
        const std::string mark = "// Each face shows the whole texture";
        const size_t comment = s.find(mark);
        const size_t start = (comment == std::string::npos)
            ? std::string::npos
            : s.rfind("for (int f = 0; f < 6; f++)", comment);
        if (start != std::string::npos) {
            size_t i = s.find('{', start);
            int depth = 0;
            size_t end = std::string::npos;
            for (size_t k = i; i != std::string::npos && k < s.size(); k++) {
                if (s[k] == '{') depth++;
                else if (s[k] == '}') {
                    depth--;
                    if (depth == 0) { end = k + 1; break; }
                }
            }
            if (end != std::string::npos) {
                s.replace(start, end - start, R"LAMPBOX(for (int f = 0; f < 6; f++) {
            const int id0 = face[f][0], id1 = face[f][1], id2 = face[f][2], id3 = face[f][5];
            float c0[3] = {sx[id0] * 0.5f, sy[id0] * 0.5f, sz[id0] * 0.5f};
            float c1[3] = {sx[id1] * 0.5f, sy[id1] * 0.5f, sz[id1] * 0.5f};
            float c2[3] = {sx[id2] * 0.5f, sy[id2] * 0.5f, sz[id2] * 0.5f};
            float c3[3] = {sx[id3] * 0.5f, sy[id3] * 0.5f, sz[id3] * 0.5f};
            float ux = c1[0] - c0[0], uy = c1[1] - c0[1], uz = c1[2] - c0[2];
            float vx = c2[0] - c0[0], vy = c2[1] - c0[1], vz = c2[2] - c0[2];
            float nx = uy * vz - uz * vy, ny = uz * vx - ux * vz, nz = ux * vy - uy * vx;
            float len = std::sqrt(nx * nx + ny * ny + nz * nz);
            if (len < 1e-8f) continue;
            nx /= len; ny /= len; nz /= len;
            const int N = 8;
            for (int iy = 0; iy < N; iy++) {
                for (int ix = 0; ix < N; ix++) {
                    float u0 = ix / (float)N, u1 = (ix + 1) / (float)N;
                    float v0 = iy / (float)N, v1 = (iy + 1) / (float)N;
                    float uu[4] = {u0, u1, u1, u0};
                    float vv[4] = {v0, v0, v1, v1};
                    float p[4][3];
                    for (int k = 0; k < 4; k++) {
                        float u = uu[k], v = vv[k];
                        float a = (1.0f - u) * (1.0f - v), b = u * (1.0f - v);
                        float c = u * v, d = (1.0f - u) * v;
                        p[k][0] = c0[0] * a + c1[0] * b + c2[0] * c + c3[0] * d;
                        p[k][1] = c0[1] * a + c1[1] * b + c2[1] * c + c3[1] * d;
                        p[k][2] = c0[2] * a + c1[2] * b + c2[2] * c + c3[2] * d;
                    }
                    static const int tri[2][3] = {{0, 1, 2}, {0, 2, 3}};
                    for (int t = 0; t < 2; t++) {
                        for (int k = 0; k < 3; k++) {
                            int q = tri[t][k];
                            m.pos.push_back(p[q][0]); m.pos.push_back(p[q][1]); m.pos.push_back(p[q][2]);
                            m.nrm.push_back(nx); m.nrm.push_back(ny); m.nrm.push_back(nz);
                            m.uv.push_back(uu[q]); m.uv.push_back(1.0f - vv[q]);
                        }
                    }
                }
            }
        })LAMPBOX");
            }
        }
    }

    put("static void __nexa_g3_apply_camera(void) {\n",
        "static int __nexa_g3_lamp_on = 0;\n"
        "static int __nexa_g3_lamp_lines = 0;\n"
        "static int __nexa_g3_lamp_mesh = 1;\n"
        "static int __nexa_g3_lamp_side = 0;\n"
        "static int __nexa_g3_lamp_bound = 0;\n"
        "static int __nexa_g3_lamp_tried = 0;\n"
        "static float __nexa_g3_lamp_x = 0, __nexa_g3_lamp_y = 0, __nexa_g3_lamp_z = 0;\n"
        "static float __nexa_g3_lamp_reach = 0;\n"
        "static float __nexa_g3_lamp_r = 1, __nexa_g3_lamp_g = 1, __nexa_g3_lamp_b = 1;\n"
        "static unsigned __nexa_g3_lamp_prog = 0;\n"
        "static unsigned __nexa_g3_lamp_sprog = 0;\n"
        "static int __nexa_g3_lamp_u[6] = {-1, -1, -1, -1, -1, -1};\n"
        "static int __nexa_g3_lamp_su[6] = {-1, -1, -1, -1, -1, -1};\n"
        "static std::vector<float> __nexa_g3_lamp_col;\n"
        "static void __nexa_g3_lamp_pack(float* v, int stride, int col, int n,\n"
        "    const float* pos, const float* nrm, const float* M, int two);\n"
        "static void __nexa_g3_lamp_batch(void);\n"
        "static void __nexa_g3_lamp_bind(void);\n"
        "static void __nexa_g3_lamp_normals(const float* n);\n"
        "static void __nexa_g3_apply_camera(void) {\n");

    put("static int __nexa_gfx3d_vsync_get(void) {\n    return __nexa_g3.vsync;\n}\n",
        std::string("static int __nexa_gfx3d_vsync_get(void) {\n    return __nexa_g3.vsync;\n}\n") +
        R"NEXA_LAMP(
static void __nexa_g3_lamp_one(float* rgb, float wx, float wy, float wz,
                               float nx, float ny, float nz, int two) {
    if (!__nexa_g3_lamp_on || __nexa_g3_lamp_reach <= 0.0f) return;
    float dx = __nexa_g3_lamp_x - wx, dy = __nexa_g3_lamp_y - wy, dz = __nexa_g3_lamp_z - wz;
    float dist2 = dx * dx + dy * dy + dz * dz;
    float reach = __nexa_g3_lamp_reach;
    if (dist2 >= reach * reach || dist2 < 1e-12f) return;
    float dist = std::sqrt(dist2);
    float t = 1.0f - dist / reach;
    float nlen = std::sqrt(nx * nx + ny * ny + nz * nz);
    if (nlen < 1e-8f) return;
    nx /= nlen; ny /= nlen; nz /= nlen;
    float nd = (nx * dx + ny * dy + nz * dz) / dist;
    if (two) { if (nd < 0.0f) nd = -nd; }
    else if (nd < 0.0f) nd = 0.0f;
    float w = nd * t * t;
    rgb[0] += __nexa_g3_lamp_r * w;
    rgb[1] += __nexa_g3_lamp_g * w;
    rgb[2] += __nexa_g3_lamp_b * w;
    if (rgb[0] > 1.0f) rgb[0] = 1.0f;
    if (rgb[1] > 1.0f) rgb[1] = 1.0f;
    if (rgb[2] > 1.0f) rgb[2] = 1.0f;
}

static void __nexa_g3_lamp_world(const float* M, float x, float y, float z,
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

static void __nexa_g3_lamp_pack(float* v, int stride, int col, int n,
                                const float* pos, const float* nrm, const float* M, int two) {
    if (!__nexa_g3_lamp_on || !v || !pos || n <= 0) return;
    for (int i = 0; i < n; i++) {
        float nx = nrm ? nrm[i * 3] : 0.0f;
        float ny = nrm ? nrm[i * 3 + 1] : 1.0f;
        float nz = nrm ? nrm[i * 3 + 2] : 0.0f;
        float wx, wy, wz, ox, oy, oz;
        __nexa_g3_lamp_world(M, pos[i * 3], pos[i * 3 + 1], pos[i * 3 + 2],
                             nx, ny, nz, &wx, &wy, &wz, &ox, &oy, &oz);
        __nexa_g3_lamp_one(v + i * stride + col, wx, wy, wz, ox, oy, oz, two);
    }
}

static void __nexa_g3_lamp_batch(void) {
    if (!__nexa_g3_lamp_on || __nexa_g3_lamp_reach <= 0.0f || __nexa_g3_prim) return;
    for (int i = 0; i + 2 < __nexa_g3_vn; i += 3) {
        float* a = &__nexa_g3_vb[i * 6];
        float* b = &__nexa_g3_vb[(i + 1) * 6];
        float* c = &__nexa_g3_vb[(i + 2) * 6];
        float ux = b[0] - a[0], uy = b[1] - a[1], uz = b[2] - a[2];
        float vx = c[0] - a[0], vy = c[1] - a[1], vz = c[2] - a[2];
        float nx = uy * vz - uz * vy, ny = uz * vx - ux * vz, nz = ux * vy - uy * vx;
        __nexa_g3_lamp_one(a + 3, a[0], a[1], a[2], nx, ny, nz, __nexa_g3_two_sided);
        __nexa_g3_lamp_one(b + 3, b[0], b[1], b[2], nx, ny, nz, __nexa_g3_two_sided);
        __nexa_g3_lamp_one(c + 3, c[0], c[1], c[2], nx, ny, nz, __nexa_g3_two_sided);
    }
}

static void __nexa_g3_lamp_locs(unsigned prog, int* u,
                                int (NEXA_GLAPI *loc)(unsigned, const char*)) {
    if (!loc || !u) return;
    u[0] = loc(prog, "uLamp");
    u[1] = loc(prog, "uLampCol");
    u[2] = loc(prog, "uReach");
    u[3] = loc(prog, "uOn");
    u[4] = loc(prog, "uSide");
    u[5] = loc(prog, "uMesh");
}

static void __nexa_g3_lamp_make(void) {
#if !defined(NEXA_WASM)
    if (__nexa_g3_lamp_tried || __nexa_g3.core33 || __nexa_g3.vulkan) return;
    __nexa_g3_lamp_tried = 1;
    typedef unsigned int (NEXA_GLAPI *CreateSh)(unsigned int);
    typedef void (NEXA_GLAPI *Source)(unsigned int, int, const char* const*, const int*);
    typedef void (NEXA_GLAPI *Shiv)(unsigned int, unsigned int, int*);
    typedef void (NEXA_GLAPI *Del)(unsigned int);
    typedef unsigned int (NEXA_GLAPI *CreatePr)(void);
    typedef void (NEXA_GLAPI *Att)(unsigned int, unsigned int);
    typedef void (NEXA_GLAPI *One)(unsigned int);
    typedef int (NEXA_GLAPI *Loc)(unsigned int, const char*);
    static CreateSh csh; static Source src; static Shiv siv; static Del del;
    static CreatePr cpr; static Att att; static One comp, link; static Shiv piv; static Loc loc;
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
        "uniform vec3 uLamp;\n"
        "uniform vec3 uLampCol;\n"
        "uniform float uReach;\n"
        "uniform float uOn;\n"
        "uniform float uSide;\n"
        "uniform float uMesh;\n"
        "varying vec3 vCol;\n"
        "varying vec3 vLampPos;\n"
        "varying vec3 vLampN;\n"
        "void main() {\n"
        "  vec3 c = vCol;\n"
        "  vec3 geo = cross(dFdx(vLampPos), dFdy(vLampPos));\n"
        "  if (uOn > 0.5 && uReach > 0.0) {\n"
        "    vec3 d = uLamp - vLampPos;\n"
        "    float dist2 = dot(d, d);\n"
        "    if (dist2 < uReach * uReach && dist2 > 0.0000001) {\n"
        "      float dist = sqrt(dist2);\n"
        "      float t = 1.0 - dist / uReach;\n"
        "      vec3 n = (uMesh > 0.5) ? vLampN : geo;\n"
        "      float nl = dot(n, n);\n"
        "      if (nl > 0.0000001) {\n"
        "        n = n * (1.0 / sqrt(nl));\n"
        "        float nd = dot(n, d) / dist;\n"
        "        if (uSide > 0.5) nd = abs(nd);\n"
        "        else if (nd < 0.0) nd = 0.0;\n"
        "        c += uLampCol * nd * t * t;\n"
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
    __nexa_g3_lamp_prog = p;
    __nexa_g3_lamp_locs(p, __nexa_g3_lamp_u, (int (NEXA_GLAPI *)(unsigned, const char*))loc);
#else
    (void)0;
#endif
}

static void __nexa_g3_lamp_normals(const float* n) {
#if !defined(NEXA_WASM)
    if (__nexa_g3.core33 || __nexa_g3.vulkan || !__nexa_g3_lamp_on) return;
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

static void __nexa_g3_lamp_bind(void) {
#if !defined(NEXA_WASM)
    if (__nexa_g3.core33 || __nexa_g3.vulkan) return;
    __nexa_g3_lamp_make();
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
    int on = __nexa_g3_lamp_on && __nexa_g3_lamp_reach > 0.0f
             && !__nexa_g3_lamp_lines && !__nexa_g3_tex_cur;
    const float* V = __nexa_g3.view;
    float x = __nexa_g3_lamp_x, y = __nexa_g3_lamp_y, z = __nexa_g3_lamp_z;
    float ex = V[0] * x + V[4] * y + V[8] * z + V[12];
    float ey = V[1] * x + V[5] * y + V[9] * z + V[13];
    float ez = V[2] * x + V[6] * y + V[10] * z + V[14];
    unsigned prog = 0;
    int recv = 0;
    /*NEXA_LAMP_SHADOW*/
    if (!prog && on && __nexa_g3_lamp_prog) prog = __nexa_g3_lamp_prog;
    if (!prog) {
        if (__nexa_g3_lamp_bound) { use(0); __nexa_g3_lamp_bound = 0; }
        return;
    }
    if (!recv) { use(prog); __nexa_g3_lamp_bound = 1; }
    int* u = __nexa_g3_lamp_u;
    if (recv) {
        if (__nexa_g3_lamp_sprog != prog && loc) {
            __nexa_g3_lamp_sprog = prog;
            __nexa_g3_lamp_locs(prog, __nexa_g3_lamp_su, loc);
        }
        u = __nexa_g3_lamp_su;
    }
    if (u3f && u[0] >= 0) u3f(u[0], ex, ey, ez);
    if (u3f && u[1] >= 0) u3f(u[1], __nexa_g3_lamp_r, __nexa_g3_lamp_g, __nexa_g3_lamp_b);
    if (u1f && u[2] >= 0) u1f(u[2], __nexa_g3_lamp_reach);
    if (u1f && u[3] >= 0) u1f(u[3], on ? 1.0f : 0.0f);
    if (u1f && u[4] >= 0) u1f(u[4], __nexa_g3_lamp_side ? 1.0f : 0.0f);
    if (u1f && u[5] >= 0) u1f(u[5], __nexa_g3_lamp_mesh ? 1.0f : 0.0f);
#endif
}

static int __nexa_gfx3d_lamp(double x, double y, double z, double reach,
                             int r = 255, int g = 255, int b = 255) {
    if (!(reach > 0.0)) {
        __nexa_g3_lamp_on = 0;
        return 0;
    }
    if (r < 0) r = 0; if (r > 255) r = 255;
    if (g < 0) g = 0; if (g > 255) g = 255;
    if (b < 0) b = 0; if (b > 255) b = 255;
    __nexa_g3_lamp_x = (float)x;
    __nexa_g3_lamp_y = (float)y;
    __nexa_g3_lamp_z = (float)z;
    __nexa_g3_lamp_reach = (float)reach;
    __nexa_g3_lamp_r = (float)r / 255.0f;
    __nexa_g3_lamp_g = (float)g / 255.0f;
    __nexa_g3_lamp_b = (float)b / 255.0f;
    __nexa_g3_lamp_on = 1;
    return 1;
}

static int __nexa_gfx3d_lamp_off(void) {
    __nexa_g3_lamp_on = 0;
    return 0;
}
)NEXA_LAMP");

    if (s.find("__nexa_g3_shadow_bound") != std::string::npos) {
        const std::string mark = "/*NEXA_LAMP_SHADOW*/";
        const size_t at = s.find(mark);
        if (at != std::string::npos) {
            s.replace(at, mark.size(),
                "if (__nexa_g3_shadow_bound && __nexa_g3_shadow_on) return;\n"
                "    if (__nexa_g3_shadow_bound && __nexa_g3_shadow_recv) {\n"
                "        prog = __nexa_g3_shadow_recv;\n"
                "        recv = 1;\n"
                "    }\n"
                "    ");
        }
    }

    const std::string lampMesh =
        "    __nexa_g3_lamp_lines = 0;\n"
        "    __nexa_g3_lamp_mesh = m.nrm.empty() ? 0 : 1;\n"
        "    __nexa_g3_lamp_side = m.two_sided;\n"
        "    __nexa_g3_lamp_bind();\n";
    const std::string lampBatch =
        "    __nexa_g3_lamp_lines = __nexa_g3_prim;\n"
        "    __nexa_g3_lamp_mesh = 0;\n"
        "    __nexa_g3_lamp_side = __nexa_g3_two_sided;\n"
        "    __nexa_g3_lamp_bind();\n";
    const std::string meshCore = "    if (__nexa_g3.core33) return __nexa_g3_core_mesh(m, L, M);\n";
    const std::string batchCore = "    if (__nexa_g3.core33) { __nexa_g3_core_batch(); return; }\n";
    const bool sh = s.find("__nexa_g3_shadow_bind();") != std::string::npos;
    const bool fog = s.find("__nexa_g3_fog_bind();") != std::string::npos;
    if (sh) {
        put(meshCore + "    __nexa_g3_shadow_bind();\n",
            meshCore + "    __nexa_g3_shadow_bind();\n" + lampMesh);
        put(batchCore + "    __nexa_g3_shadow_bind();\n",
            batchCore + "    __nexa_g3_shadow_bind();\n" + lampBatch);
    } else if (fog) {
        put(meshCore + "    __nexa_g3_fog_bind();\n",
            meshCore + "    __nexa_g3_fog_bind();\n" + lampMesh);
        put(batchCore + "    __nexa_g3_fog_bind();\n",
            batchCore + "    __nexa_g3_fog_bind();\n" + lampBatch);
    } else {
        put(meshCore, meshCore + lampMesh);
        put(batchCore, batchCore + lampBatch);
    }

    if (s.find("__nexa_g3_shadow_normals(nullptr);") != std::string::npos) {
        put("    __nexa_g3_shadow_normals(m.nrm.empty() ? nullptr : m.nrm.data());\n",
            "    __nexa_g3_shadow_normals(m.nrm.empty() ? nullptr : m.nrm.data());\n"
            "    __nexa_g3_lamp_normals(m.nrm.empty() ? nullptr : m.nrm.data());\n");
        put("    __nexa_g3_shadow_normals(nullptr);\n",
            "    __nexa_g3_shadow_normals(nullptr);\n"
            "    __nexa_g3_lamp_normals(nullptr);\n");
    } else {
        put("    __nexa_gl.ColorPointer(3, NEXA_GL_FLOAT, 0, col);\n",
            "    __nexa_gl.ColorPointer(3, NEXA_GL_FLOAT, 0, col);\n"
            "    __nexa_g3_lamp_normals(m.nrm.empty() ? nullptr : m.nrm.data());\n");
        put("    __nexa_gl.DrawArrays(NEXA_GL_TRIANGLES, 0, (__nexa_GLsizei)(m.pos.size() / 3));\n"
            "    if (cut) {\n",
            "    __nexa_gl.DrawArrays(NEXA_GL_TRIANGLES, 0, (__nexa_GLsizei)(m.pos.size() / 3));\n"
            "    __nexa_g3_lamp_normals(nullptr);\n"
            "    if (cut) {\n");
    }

    put("    if (L.gl_stale) {\n"
        "        __nexa_g33.BufferData(NEXA_GL_ARRAY_BUFFER, (__nexa_GLsizeiptr)(L.col.size() * sizeof(float)),\n"
        "                              L.col.data(), NEXA_GL_DYNAMIC_DRAW);\n"
        "        L.gl_stale = 0;\n"
        "    }\n",
        "    if (__nexa_g3_lamp_on) {\n"
        "        __nexa_g3_lamp_col = L.col;\n"
        "        __nexa_g3_lamp_pack(__nexa_g3_lamp_col.data(), 3, 0, (int)(m.pos.size() / 3),\n"
        "            m.pos.data(), m.nrm.empty() ? nullptr : m.nrm.data(), M, m.two_sided);\n"
        "        __nexa_g33.BufferData(NEXA_GL_ARRAY_BUFFER, (__nexa_GLsizeiptr)(__nexa_g3_lamp_col.size() * sizeof(float)),\n"
        "                              __nexa_g3_lamp_col.data(), NEXA_GL_DYNAMIC_DRAW);\n"
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
        "    if (__nexa_g3_lamp_on) {\n"
        "        __nexa_g3_lamp_col = L.col;\n"
        "        __nexa_g3_lamp_pack(__nexa_g3_lamp_col.data(), 3, 0, (int)(m.pos.size() / 3),\n"
        "            m.pos.data(), m.nrm.empty() ? nullptr : m.nrm.data(), M, m.two_sided);\n"
        "        glBufferData(NEXA_GL_ARRAY_BUFFER, (__nexa_GLsizeiptr)(__nexa_g3_lamp_col.size() * sizeof(float)),\n"
        "                     __nexa_g3_lamp_col.data(), NEXA_GL_DYNAMIC_DRAW);\n"
        "        L.gl_stale = 0;\n"
        "    } else if (L.gl_stale) {\n"
        "        glBufferData(NEXA_GL_ARRAY_BUFFER, (__nexa_GLsizeiptr)(L.col.size() * sizeof(float)),\n"
        "                     L.col.data(), NEXA_GL_DYNAMIC_DRAW);\n"
        "        L.gl_stale = 0;\n"
        "    }\n");

    put("        nexa_vk14::draw(tmp.data(), (int)n, 0, m.two_sided, mvp);\n",
        "        __nexa_g3_lamp_pack(tmp.data(), 6, 3, (int)n, m.pos.data(), m.nrm.empty() ? nullptr : m.nrm.data(), M, m.two_sided);\n"
        "        nexa_vk14::draw(tmp.data(), (int)n, 0, m.two_sided, mvp);\n");
    put("            nexa_vk14::draw_tex(tmp.data(), (int)n, t.px.data(), t.w, t.h, t.cutout, m.two_sided || t.cutout, mvp);\n",
        "            __nexa_g3_lamp_pack(tmp.data(), 8, 3, (int)n, m.pos.data(), m.nrm.empty() ? nullptr : m.nrm.data(), M, m.two_sided);\n"
        "            nexa_vk14::draw_tex(tmp.data(), (int)n, t.px.data(), t.w, t.h, t.cutout, m.two_sided || t.cutout, mvp);\n");
    put("        nexa_vk14::draw(__nexa_g3_vb, __nexa_g3_vn, __nexa_g3_prim, __nexa_g3_two_sided, mvp);\n",
        "        __nexa_g3_lamp_batch();\n"
        "        nexa_vk14::draw(__nexa_g3_vb, __nexa_g3_vn, __nexa_g3_prim, __nexa_g3_two_sided, mvp);\n");
    put("static void __nexa_g3_core_batch(void) {\n",
        "static void __nexa_g3_core_batch(void) {\n"
        "    __nexa_g3_lamp_batch();\n");
    put("    glDrawArrays(__nexa_g3_prim ? NEXA_GL_LINES : NEXA_GL_TRIANGLES, 0, __nexa_g3_vn);\n",
        "    __nexa_g3_lamp_batch();\n"
        "    glDrawArrays(__nexa_g3_prim ? NEXA_GL_LINES : NEXA_GL_TRIANGLES, 0, __nexa_g3_vn);\n");
}
