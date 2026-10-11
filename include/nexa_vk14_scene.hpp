#include "nexa_vk14_spv.inc"

// Offscreen scene that matches the OpenGL 3.3 view: lit meshes, one shadow,
// fog, materials, sky, particles, and a resolve into the swapchain.

struct Img {
    uint64_t image = 0, mem = 0, view = 0;
    int w = 0, h = 0;
    const void* key = nullptr;
    int keyW = 0, keyH = 0;
};

struct Stage {
    uint64_t buf = 0, mem = 0;
    void* map = nullptr;
    uint64_t cap = 0, used = 0;
};

struct Gpu {
    int booted = 0, on = 0, simpleOpen = 0;
    int inColor = 0, inShadow = 0, needLoad = 0, drewColor = 0, shadowReady = 0;
    uint64_t sh[13] = {};
    uint64_t layoutMesh = 0, layoutMat = 0, layoutSky = 0, layoutPart = 0, layoutBlit = 0, layoutTex = 0, layoutDepth = 0;
    uint64_t slMesh = 0, slMat = 0, slSky = 0, slPart = 0, slBlit = 0, slTex = 0;
    uint64_t setMesh = 0, setSky = 0, setBlit = 0;
    uint64_t pool = 0;
    uint64_t sampLin = 0, sampNear = 0, sampSh = 0, sampRep = 0;
    uint64_t sceneBuf = 0, sceneMem = 0, drawBuf = 0, drawMem = 0, partBuf = 0, partMem = 0;
    void* sceneMap = nullptr;
    void* drawMap = nullptr;
    void* partMap = nullptr;
    int drawN = 0;
    uint64_t pipeMesh[10] = {};
    uint64_t pipeMat[2] = {};
    uint64_t pipeSky = 0, pipePart = 0, pipeShadow = 0, pipeBlit = 0, pipeTex[2] = {};
    uint64_t passC = 0, passL = 0, passSh = 0, passSwapL = 0;
    uint64_t fbC = 0, fbL = 0, fbSh = 0;
    std::vector<uint64_t> fbSwapL;
    Img color, resolve, depth, shadow, white;
    uint32_t samples = 1, reqSamples = 0, depthFmt = 0, depthAspect = 2;
    int rw = 0, rh = 0;
    int colorLayout = 0, depthLayout = 0, resolveLayout = 0, shadowLayout = 0, whiteLayout = 0;
    float cr = 0, cg = 0, cb = 0, d0 = 0, d1 = 1;
    Stage stage;
    std::vector<Stage> extra;
    std::vector<Img> textures;
    std::vector<uint64_t> meshSets, matSets, partSets, texSets;
    int meshN = 0, matN = 0, partN = 0, texN = 0;
    uint64_t matViews[128][5] = {};
    int matSlot[128] = {};
    int matReady[128] = {};
    uint64_t partView[64] = {};
    int partReady[64] = {};
    uint64_t skySeen[6] = {};
    int skyReady = 0;
};

inline Gpu& gpu() {
    static Gpu g;
    return g;
}

inline void free_img(Dev& d, Img& im) {
    if (im.view && d.DestroyImageView) d.DestroyImageView(d.device, im.view, nullptr);
    if (im.image && d.DestroyImage) d.DestroyImage(d.device, im.image, nullptr);
    if (im.mem && d.FreeMemory) d.FreeMemory(d.device, im.mem, nullptr);
    im = Img{};
}

inline void free_stage(Dev& d, Stage& s) {
    if (s.map && d.UnmapMemory) d.UnmapMemory(d.device, s.mem);
    if (s.buf && d.DestroyBuffer) d.DestroyBuffer(d.device, s.buf, nullptr);
    if (s.mem && d.FreeMemory) d.FreeMemory(d.device, s.mem, nullptr);
    s = Stage{};
}

inline int make_image(Dev& d, Img& im, uint32_t fmt, uint32_t w, uint32_t h, uint32_t samples, uint32_t usage, uint32_t aspect) {
    struct Info {
        uint32_t sType; const void* pNext; uint32_t flags; uint32_t type; uint32_t format;
        uint32_t ew, eh, ed; uint32_t mips, layers, samples, tiling, usage, sharing, qcount;
        const void* q; uint32_t initial;
    } img{};
    img.sType = 14; img.type = 1; img.format = fmt; img.ew = w; img.eh = h; img.ed = 1;
    img.mips = 1; img.layers = 1; img.samples = samples; img.tiling = 0; img.usage = usage;
    if (d.CreateImage(d.device, &img, nullptr, &im.image) != 0 || !im.image) return 0;
    if (!image_mem(d, im.image, 1u, &im.mem)) { free_img(d, im); return 0; }
    if (!make_view(d, im.image, fmt, aspect, &im.view)) { free_img(d, im); return 0; }
    im.w = (int)w; im.h = (int)h;
    return 1;
}

inline void barrier(Dev& d, uint64_t image, uint32_t aspect, uint32_t srcS, uint32_t dstS,
                    uint32_t srcA, uint32_t dstA, uint32_t oldL, uint32_t neu) {
    struct Range { uint32_t aspect, mip, levels, layer, layers; };
    struct Barr {
        uint32_t sType; const void* pNext; uint32_t srcA, dstA, oldL, neu, srcQ, dstQ;
        uint64_t image; Range range;
    } b{};
    b.sType = 45; b.srcA = srcA; b.dstA = dstA; b.oldL = oldL; b.neu = neu; b.srcQ = ~0u; b.dstQ = ~0u;
    b.image = image; b.range = { aspect, 0, 1, 0, 1 };
    d.CmdPipelineBarrier(d.cmd, srcS, dstS, 0, 0, nullptr, 0, nullptr, 1, &b);
}

struct At { uint32_t loc, bind, fmt, off; };

inline int pipe_ex(Dev& d, uint64_t vs, uint64_t fs, uint32_t topo, uint32_t cull, uint32_t front,
                   int depthTest, int depthWrite, int blend, uint32_t samples, int bias,
                   float bconst, float bslope, int stencil, uint32_t colorMask, uint32_t stride,
                   const At* attrs, int nattr, int colorCount, uint64_t layout, uint64_t pass, uint64_t* out) {
    struct Stage { uint32_t sType; const void* pNext; uint32_t flags; uint32_t stage; uint64_t module; const char* name; const void* spec; };
    Stage stages[2] = {};
    stages[0].sType = 18; stages[0].stage = 1; stages[0].module = vs; stages[0].name = "main";
    int nstage = 1;
    if (fs) {
        stages[1].sType = 18; stages[1].stage = 0x10; stages[1].module = fs; stages[1].name = "main";
        nstage = 2;
    }
    struct Bind { uint32_t binding, stride, rate; };
    struct Attr { uint32_t location, binding, format, offset; };
    Bind bind{ 0, stride, 0 };
    Attr attr[4] = {};
    if (nattr > 4) nattr = 4;
    for (int i = 0; i < nattr; ++i) attr[i] = { attrs[i].loc, attrs[i].bind, attrs[i].fmt, attrs[i].off };
    struct VI { uint32_t sType; const void* pNext; uint32_t flags; uint32_t bc; const Bind* b; uint32_t ac; const Attr* a; } vi{};
    vi.sType = 19; vi.bc = 1; vi.b = &bind; vi.ac = (uint32_t)nattr; vi.a = attr;
    struct IA { uint32_t sType; const void* pNext; uint32_t flags; uint32_t topo; uint32_t restart; } ia{};
    ia.sType = 20; ia.topo = topo;
    struct VP { uint32_t sType; const void* pNext; uint32_t flags; uint32_t vc; const void* v; uint32_t sc; const void* s; } vp{};
    vp.sType = 22; vp.vc = 1; vp.sc = 1;
    struct RS { uint32_t sType; const void* pNext; uint32_t flags; uint32_t depthClamp; uint32_t discard; uint32_t poly; uint32_t cull; uint32_t front; uint32_t biasEn; float bconst, bclamp, bslope, lineWidth; } rs{};
    rs.sType = 23; rs.cull = cull; rs.front = front; rs.biasEn = bias ? 1u : 0u; rs.bconst = bconst; rs.bslope = bslope; rs.lineWidth = 1.0f;
    struct MS { uint32_t sType; const void* pNext; uint32_t flags; uint32_t samples; uint32_t sampleShading; float minSample; const void* mask; uint32_t a2c; uint32_t a2o; } ms{};
    ms.sType = 24; ms.samples = samples ? samples : 1u;
    struct St { uint32_t failOp, passOp, depthFailOp, compareOp, compareMask, writeMask, reference; };
    struct DS { uint32_t sType; const void* pNext; uint32_t flags; uint32_t test; uint32_t write; uint32_t cmp; uint32_t bounds; uint32_t stencil; St front; St back; float minB, maxB; } ds{};
    ds.sType = 25; ds.test = depthTest ? 1u : 0u; ds.write = depthWrite ? 1u : 0u; ds.cmp = 3; ds.maxB = 1.0f;
    if (stencil) {
        ds.stencil = 1;
        ds.front.compareOp = stencil == 2 ? 5u : 7u;
        ds.front.passOp = stencil == 1 ? 2u : 0u;
        ds.front.compareMask = 0xFFu;
        ds.front.writeMask = stencil == 1 ? 0xFFu : 0u;
        ds.front.reference = 1;
        ds.back = ds.front;
    }
    struct BA { uint32_t en, srcC, dstC, opC, srcA, dstA, opA, mask; } ba{};
    ba.en = blend ? 1u : 0u; ba.srcC = 6; ba.dstC = 7; ba.srcA = 6; ba.dstA = 7; ba.mask = colorMask;
    struct CB { uint32_t sType; const void* pNext; uint32_t flags; uint32_t logicEn; uint32_t logicOp; uint32_t ac; const BA* a; float c[4]; } cb{};
    cb.sType = 26; cb.ac = colorCount > 0 ? 1u : 0u; cb.a = colorCount > 0 ? &ba : nullptr;
    uint32_t dyns[2] = { 0, 1 };
    struct DY { uint32_t sType; const void* pNext; uint32_t flags; uint32_t n; const uint32_t* p; } dy{};
    dy.sType = 27; dy.n = 2; dy.p = dyns;
    struct GP { uint32_t sType; const void* pNext; uint32_t flags; uint32_t stageCount; const Stage* stages; const VI* vi; const IA* ia; const void* tess; const VP* vp; const RS* rs; const MS* ms; const DS* ds; const CB* cb; const DY* dy; uint64_t layout; uint64_t pass; uint32_t subpass; uint64_t base; int32_t baseIndex; } gp{};
    gp.sType = 28; gp.stageCount = (uint32_t)nstage; gp.stages = stages; gp.vi = &vi; gp.ia = &ia; gp.vp = &vp; gp.rs = &rs; gp.ms = &ms; gp.ds = &ds; gp.cb = &cb; gp.dy = &dy; gp.layout = layout; gp.pass = pass;
    return d.CreateGraphicsPipelines(d.device, 0, 1, &gp, nullptr, out) == 0 && *out;
}

inline int make_pass(Dev& d, uint32_t colorFmt, uint32_t depthFmt, uint32_t samples, int load, int stencil, uint64_t* pass) {
    struct Att { uint32_t flags, format, samples, load, store, sload, sstore, initial, final; };
    Att at[2] = {};
    at[0].format = colorFmt; at[0].samples = samples; at[0].load = load ? 1u : 0u; at[0].store = 0;
    at[0].sload = 2; at[0].sstore = 1; at[0].initial = 2; at[0].final = 2;
    at[1].format = depthFmt; at[1].samples = samples; at[1].load = load ? 1u : 0u; at[1].store = 0;
    at[1].sload = load ? 1u : 0u; at[1].sstore = 0; at[1].initial = 3; at[1].final = 3;
    if (!stencil) { at[1].sload = 2; at[1].sstore = 1; }
    struct Ref { uint32_t attachment, layout; };
    Ref color{ 0, 2 }, depth{ 1, 3 };
    struct Sub { uint32_t flags, bind, inCount; const void* in; uint32_t colorCount; const Ref* colors; const void* resolve; const Ref* depth; uint32_t preserve; const void* p; } sub{};
    sub.colorCount = 1; sub.colors = &color; sub.depth = &depth;
    struct RP { uint32_t sType; const void* pNext; uint32_t flags; uint32_t ac; const Att* a; uint32_t sc; const Sub* s; uint32_t dc; const void* dep; } rp{};
    rp.sType = 38; rp.ac = 2; rp.a = at; rp.sc = 1; rp.s = &sub;
    return d.CreateRenderPass(d.device, &rp, nullptr, pass) == 0 && *pass;
}

inline int make_fb(Dev& d, uint64_t pass, uint64_t color, uint64_t depth, uint32_t w, uint32_t h, uint64_t* fb) {
    uint64_t att[2] = { color, depth };
    struct FB { uint32_t sType; const void* pNext; uint32_t flags; uint64_t pass; uint32_t n; const uint64_t* a; uint32_t w, h, layers; } info{};
    info.sType = 37; info.pass = pass; info.n = depth ? 2u : 1u; info.a = att; info.w = w; info.h = h; info.layers = 1;
    return d.CreateFramebuffer(d.device, &info, nullptr, fb) == 0 && *fb;
}

inline void begin_pass(Dev& d, uint64_t pass, uint64_t fb, uint32_t w, uint32_t h, int clears, float r, float g, float b) {
    struct Clear { float c[4]; } cv[2] = {};
    cv[0].c[0] = r; cv[0].c[1] = g; cv[0].c[2] = b; cv[0].c[3] = 1;
    cv[1].c[0] = 1;
    struct Off { int32_t x, y; };
    struct Ext { uint32_t w, h; };
    struct Area { Off o; Ext e; };
    struct Begin { uint32_t sType; const void* pNext; uint64_t pass, fb; Area area; uint32_t cc; const Clear* c; } bp{};
    bp.sType = 43; bp.pass = pass; bp.fb = fb; bp.area.e = { w, h }; bp.cc = (uint32_t)clears; bp.c = cv;
    d.CmdBeginRenderPass(d.cmd, &bp, 0);
}

inline void vp_scene(Dev& d) {
    Gpu& g = gpu();
    struct Vp { float x, y, w, h, mind, maxd; } vp{};
    vp.y = (float)g.rh; vp.w = (float)g.rw; vp.h = -(float)g.rh; vp.mind = g.d0; vp.maxd = g.d1;
    d.CmdSetViewport(d.cmd, 0, 1, &vp);
    struct Sc { int32_t x, y; uint32_t w, h; } sc{};
    sc.w = (uint32_t)g.rw; sc.h = (uint32_t)g.rh;
    d.CmdSetScissor(d.cmd, 0, 1, &sc);
}

inline void vp_shadow(Dev& d) {
    struct Vp { float x, y, w, h, mind, maxd; } vp{};
    vp.w = 2048; vp.h = 2048; vp.maxd = 1;
    d.CmdSetViewport(d.cmd, 0, 1, &vp);
    struct Sc { int32_t x, y; uint32_t w, h; } sc{ 0, 0, 2048, 2048 };
    d.CmdSetScissor(d.cmd, 0, 1, &sc);
}

inline int layout_of(Dev& d, const uint32_t* binds, int nbinds, uint32_t pcStage, uint32_t pcOff, uint32_t pcSize, int pc2, uint32_t pc2Stage, uint32_t pc2Off, uint32_t pc2Size, uint64_t* set, uint64_t* lay) {
    struct Bind { uint32_t binding, type, count, stages; const void* samplers; };
    Bind b[8] = {};
    for (int i = 0; i < nbinds && i < 8; ++i) {
        b[i].binding = binds[i * 3]; b[i].type = binds[i * 3 + 1]; b[i].count = 1; b[i].stages = binds[i * 3 + 2];
    }
    struct SL { uint32_t sType; const void* pNext; uint32_t flags; uint32_t n; const Bind* b; } sl{};
    sl.sType = 32; sl.n = (uint32_t)nbinds; sl.b = b;
    if (d.CreateDescriptorSetLayout(d.device, &sl, nullptr, set) != 0) return 0;
    struct Range { uint32_t stage, offset, size; } pc[2] = {};
    int npc = 0;
    if (pcSize) { pc[npc++] = { pcStage, pcOff, pcSize }; }
    if (pc2 && pc2Size) { pc[npc++] = { pc2Stage, pc2Off, pc2Size }; }
    struct PL { uint32_t sType; const void* pNext; uint32_t flags; uint32_t setCount; const uint64_t* sets; uint32_t pcCount; const Range* pc; } pl{};
    pl.sType = 30; pl.setCount = 1; pl.sets = set; pl.pcCount = (uint32_t)npc; pl.pc = npc ? pc : nullptr;
    return d.CreatePipelineLayout(d.device, &pl, nullptr, lay) == 0;
}

inline int alloc_sets(Dev& d, uint64_t sl, int n, std::vector<uint64_t>& out) {
    if (n < 1) return 1;
    std::vector<uint64_t> layouts((size_t)n, sl);
    out.assign((size_t)n, 0);
    struct DA { uint32_t sType; const void* pNext; uint64_t pool; uint32_t count; const uint64_t* layouts; } da{};
    da.sType = 34; da.pool = gpu().pool; da.count = (uint32_t)n; da.layouts = layouts.data();
    return d.AllocateDescriptorSets(d.device, &da, out.data()) == 0;
}

struct BI { uint64_t buffer; uint64_t offset; uint64_t range; };
struct II { uint64_t sampler; uint64_t view; uint32_t layout; };
struct Wr { uint32_t sType; const void* pNext; uint64_t set; uint32_t binding, element, count, type; const II* image; const BI* buf; const void* texel; };

inline void write_buf_at(Dev& d, uint64_t set, uint32_t binding, uint32_t type, uint64_t buf, uint64_t offset, uint64_t range) {
    BI bi{ buf, offset, range };
    Wr wr{};
    wr.sType = 35; wr.set = set; wr.binding = binding; wr.count = 1; wr.type = type; wr.buf = &bi;
    d.UpdateDescriptorSets(d.device, 1, &wr, 0, nullptr);
}

inline void write_buf(Dev& d, uint64_t set, uint32_t binding, uint32_t type, uint64_t buf, uint64_t range) {
    write_buf_at(d, set, binding, type, buf, 0, range);
}

inline void write_img(Dev& d, uint64_t set, uint32_t binding, uint64_t samp, uint64_t view) {
    II ii{ samp, view, 5 };
    Wr wr{};
    wr.sType = 35; wr.set = set; wr.binding = binding; wr.count = 1; wr.type = 1; wr.image = &ii;
    d.UpdateDescriptorSets(d.device, 1, &wr, 0, nullptr);
}

inline int grow_stage(Dev& d, Stage& s, uint64_t need) {
    if (s.cap >= need && s.buf) return 1;
    free_stage(d, s);
    uint64_t cap = need < (8ull << 20) ? (8ull << 20) : need;
    if (!buffer_alloc(d, cap, 0x1, &s.buf, &s.mem, &s.map)) return 0;
    s.cap = cap; s.used = 0;
    return 1;
}

inline int stage_bytes(Dev& d, const void* src, uint64_t n, uint64_t* buf, uint64_t* off) {
    Gpu& g = gpu();
    uint64_t aligned = (n + 15ull) & ~15ull;
    if (g.stage.used + aligned <= g.stage.cap && g.stage.map) {
        std::memcpy((char*)g.stage.map + g.stage.used, src, (size_t)n);
        *buf = g.stage.buf; *off = g.stage.used; g.stage.used += aligned; return 1;
    }
    if (g.stage.used == 0 && grow_stage(d, g.stage, aligned)) {
        std::memcpy(g.stage.map, src, (size_t)n);
        *buf = g.stage.buf; *off = 0; g.stage.used = aligned; return 1;
    }
    Stage extra;
    if (!grow_stage(d, extra, aligned)) return 0;
    std::memcpy(extra.map, src, (size_t)n);
    *buf = extra.buf; *off = 0;
    g.extra.push_back(extra);
    return 1;
}

inline uint64_t upload_rgba(Dev& d, const void* key, const unsigned char* px, int w, int h, int comp) {
    Gpu& g = gpu();
    if (!px || w < 1 || h < 1) return g.white.view;
    for (const Img& im : g.textures) if (im.key == key && im.keyW == w && im.keyH == h && im.view) return im.view;
    if ((int)g.textures.size() >= 256) return g.white.view;
    std::vector<unsigned char> rgba;
    const unsigned char* src = px;
    if (comp == 3) {
        rgba.resize((size_t)w * h * 4);
        for (int i = 0; i < w * h; ++i) {
            rgba[(size_t)i * 4] = px[(size_t)i * 3];
            rgba[(size_t)i * 4 + 1] = px[(size_t)i * 3 + 1];
            rgba[(size_t)i * 4 + 2] = px[(size_t)i * 3 + 2];
            rgba[(size_t)i * 4 + 3] = 255;
        }
        src = rgba.data();
    }
    Img im;
    if (!make_image(d, im, 37, (uint32_t)w, (uint32_t)h, 1, 0x2 | 0x4, 1)) return g.white.view;
    uint64_t buf = 0, off = 0;
    if (!stage_bytes(d, src, (uint64_t)w * h * 4, &buf, &off)) { free_img(d, im); return g.white.view; }
    barrier(d, im.image, 1, 1, 0x1000, 0, 0x1000, 0, 7);
    struct Subr { uint32_t aspect, mip, layer, count; };
    struct Copy { uint64_t off; uint32_t row, height; Subr sub; int32_t x, y, z; uint32_t w, h, dep; } copy{};
    copy.off = off; copy.sub = { 1, 0, 0, 1 }; copy.w = (uint32_t)w; copy.h = (uint32_t)h; copy.dep = 1;
    d.CmdCopyBufferToImage(d.cmd, buf, im.image, 7, 1, &copy);
    barrier(d, im.image, 1, 0x1000, 0x80, 0x1000, 0x20, 7, 5);
    im.key = key; im.keyW = w; im.keyH = h;
    uint64_t view = im.view;
    g.textures.push_back(im);
    return view;
}

inline void close_pass(Dev& d) {
    Gpu& g = gpu();
    if (!g.inColor && !g.inShadow && !g.simpleOpen) return;
    d.CmdEndRenderPass(d.cmd);
    if (g.inColor) g.needLoad = 1;
    g.inColor = 0; g.inShadow = 0; g.simpleOpen = 0;
}

inline void prep_color(Dev& d) {
    Gpu& g = gpu();
    if (g.colorLayout != 2) {
        barrier(d, g.color.image, 1, 1, 0x400, 0, 0x100, (uint32_t)g.colorLayout, 2);
        g.colorLayout = 2;
    }
    if (g.depthLayout != 3) {
        barrier(d, g.depth.image, g.depthAspect, 1, 0x200, 0, 0x400, (uint32_t)g.depthLayout, 3);
        g.depthLayout = 3;
    }
}

inline void destroy_target(Dev& d) {
    Gpu& g = gpu();
    if (g.fbC && d.DestroyFramebuffer) d.DestroyFramebuffer(d.device, g.fbC, nullptr);
    if (g.fbL && d.DestroyFramebuffer) d.DestroyFramebuffer(d.device, g.fbL, nullptr);
    g.fbC = g.fbL = 0;
    for (int i = 0; i < 10; ++i) if (g.pipeMesh[i] && d.DestroyPipeline) { d.DestroyPipeline(d.device, g.pipeMesh[i], nullptr); g.pipeMesh[i] = 0; }
    for (int i = 0; i < 2; ++i) if (g.pipeMat[i] && d.DestroyPipeline) { d.DestroyPipeline(d.device, g.pipeMat[i], nullptr); g.pipeMat[i] = 0; }
    if (g.pipeSky && d.DestroyPipeline) { d.DestroyPipeline(d.device, g.pipeSky, nullptr); g.pipeSky = 0; }
    if (g.pipePart && d.DestroyPipeline) { d.DestroyPipeline(d.device, g.pipePart, nullptr); g.pipePart = 0; }
    if (g.passC && d.DestroyRenderPass) { d.DestroyRenderPass(d.device, g.passC, nullptr); g.passC = 0; }
    if (g.passL && d.DestroyRenderPass) { d.DestroyRenderPass(d.device, g.passL, nullptr); g.passL = 0; }
    free_img(d, g.color); free_img(d, g.resolve); free_img(d, g.depth);
    g.rw = g.rh = 0; g.colorLayout = g.depthLayout = g.resolveLayout = 0;
}

inline int make_scene_pipes(Dev& d) {
    Gpu& g = gpu();
    const At meshAt[] = { {0, 0, 106, 0}, {1, 0, 106, 12}, {2, 0, 106, 24} };
    const At matAt[] = { {0, 0, 106, 0}, {1, 0, 106, 12}, {2, 0, 109, 24}, {3, 0, 103, 40} };
    const At skyAt[] = { {0, 0, 106, 0} };
    const At partAt[] = { {0, 0, 106, 0}, {1, 0, 103, 12}, {2, 0, 109, 20} };
    struct Kind { int cull, blend, depthT, depthW, bias, stencil; uint32_t mask; float bc, bs; uint32_t topo; };
    const Kind kinds[10] = {
        { 0, 0, 1, 1, 0, 0, 0xF, 0, 0, 3 },
        { 0, 0, 1, 1, 0, 0, 0xF, 0, 0, 3 },
        { 0, 1, 1, 1, 0, 0, 0xF, 0, 0, 3 },
        { 0, 1, 1, 1, 0, 0, 0xF, 0, 0, 3 },
        { 0, 1, 1, 0, 1, 0, 0xF, -1, -1, 3 },
        { 0, 0, 0, 0, 0, 0, 0xF, 0, 0, 3 },
        { 0, 1, 0, 0, 0, 0, 0xF, 0, 0, 3 },
        { 0, 1, 1, 0, 0, 0, 0xF, 0, 0, 1 },
        { 0, 0, 1, 0, 0, 1, 0, 0, 0, 3 },
        { 0, 0, 0, 0, 0, 2, 0xF, 0, 0, 3 },
    };
    for (int i = 0; i < 10; ++i) {
        const Kind& k = kinds[i];
        if (!pipe_ex(d, g.sh[0], g.sh[1], k.topo, (uint32_t)k.cull, 0, k.depthT, k.depthW, k.blend, g.samples,
                     k.bias, k.bc, k.bs, k.stencil, k.mask, 36, meshAt, 3, 1, g.layoutMesh, g.passC, &g.pipeMesh[i])) { fail("mesh pipe"); return 0; }
    }
    if (!pipe_ex(d, g.sh[3], g.sh[4], 3, 0, 1, 1, 1, 0, g.samples, 0, 0, 0, 0, 0xF, 48, matAt, 4, 1, g.layoutMat, g.passC, &g.pipeMat[0])) { fail("mat pipe"); return 0; }
    if (!pipe_ex(d, g.sh[3], g.sh[4], 3, 0, 1, 1, 1, 1, g.samples, 0, 0, 0, 0, 0xF, 48, matAt, 4, 1, g.layoutMat, g.passC, &g.pipeMat[1])) { fail("mat pipe blend"); return 0; }
    if (!pipe_ex(d, g.sh[5], g.sh[6], 3, 0, 1, 0, 0, 0, g.samples, 0, 0, 0, 0, 0xF, 12, skyAt, 1, 1, g.layoutSky, g.passC, &g.pipeSky)) { fail("sky pipe"); return 0; }
    if (!pipe_ex(d, g.sh[7], g.sh[8], 3, 0, 1, 1, 0, 1, g.samples, 0, 0, 0, 0, 0xF, 36, partAt, 3, 1, g.layoutPart, g.passC, &g.pipePart)) { fail("part pipe"); return 0; }
    return 1;
}

inline int ensure_target(Dev& d, int w, int h, int samples) {
    Gpu& g = gpu();
    if (w < 1) w = 1;
    if (h < 1) h = 1;
    uint32_t want = samples < 2 ? 1u : (uint32_t)samples;
    if (g.color.image && g.rw == w && g.rh == h && g.reqSamples == want) return 1;
    destroy_target(d);
    uint32_t tried = want;
    for (;;) {
        uint32_t usage = tried > 1 ? (0x10u | 0x1u) : (0x10u | 0x4u);
        if (make_image(d, g.color, 37, (uint32_t)w, (uint32_t)h, tried, usage, 1)) break;
        if (tried == 8) tried = 4;
        else if (tried == 4) tried = 2;
        else if (tried == 2) tried = 1;
        else { fail("color image"); return 0; }
    }
    g.samples = tried;
    if (g.samples > 1) {
        if (!make_image(d, g.resolve, 37, (uint32_t)w, (uint32_t)h, 1, 0x2 | 0x4, 1)) { fail("resolve image"); return 0; }
    }
    if (!make_image(d, g.depth, g.depthFmt, (uint32_t)w, (uint32_t)h, g.samples, 0x20, g.depthAspect)) { fail("depth image"); return 0; }
    int stencil = g.depthAspect == 6;
    if (!make_pass(d, 37, g.depthFmt, g.samples, 1, stencil, &g.passC)) { fail("color pass"); return 0; }
    if (!make_pass(d, 37, g.depthFmt, g.samples, 0, stencil, &g.passL)) { fail("load pass"); return 0; }
    if (!make_fb(d, g.passC, g.color.view, g.depth.view, (uint32_t)w, (uint32_t)h, &g.fbC)) { fail("color fb"); return 0; }
    if (!make_fb(d, g.passL, g.color.view, g.depth.view, (uint32_t)w, (uint32_t)h, &g.fbL)) { fail("load fb"); return 0; }
    if (!make_scene_pipes(d)) return 0;
    g.rw = w; g.rh = h; g.reqSamples = want;
    g.colorLayout = 0; g.depthLayout = 0; g.resolveLayout = 0;
    return 1;
}

inline int ensure_shadow(Dev& d) {
    Gpu& g = gpu();
    if (g.shadow.image && g.pipeShadow) return 1;
    if (!make_image(d, g.shadow, 126, 2048, 2048, 1, 0x20 | 0x4, 2)) { fail("shadow image"); return 0; }
    struct Att { uint32_t flags, format, samples, load, store, sload, sstore, initial, final; } at{};
    at.format = 126; at.samples = 1; at.load = 1; at.store = 0; at.sload = 2; at.sstore = 1; at.initial = 3; at.final = 5;
    struct Ref { uint32_t attachment, layout; } depth{ 0, 3 };
    struct Sub { uint32_t flags, bind, inCount; const void* in; uint32_t colorCount; const void* colors; const void* resolve; const Ref* depth; uint32_t preserve; const void* p; } sub{};
    sub.depth = &depth;
    struct RP { uint32_t sType; const void* pNext; uint32_t flags; uint32_t ac; const Att* a; uint32_t sc; const Sub* s; uint32_t dc; const void* dep; } rp{};
    rp.sType = 38; rp.ac = 1; rp.a = &at; rp.sc = 1; rp.s = &sub;
    if (d.CreateRenderPass(d.device, &rp, nullptr, &g.passSh) != 0) { fail("shadow pass"); return 0; }
    if (!make_fb(d, g.passSh, g.shadow.view, 0, 2048, 2048, &g.fbSh)) { fail("shadow fb"); return 0; }
    const At atv[] = { {0, 0, 106, 0} };
    if (!pipe_ex(d, g.sh[2], 0, 3, 0, 1, 1, 1, 0, 1, 1, 2.0f, 1.5f, 0, 0, 36, atv, 1, 0, g.layoutDepth, g.passSh, &g.pipeShadow)) { fail("shadow pipe"); return 0; }
    g.shadowLayout = 0;
    return 1;
}

inline void destroy_pipes_swap(Dev& d) {
    Gpu& g = gpu();
    if (g.pipeBlit && d.DestroyPipeline) { d.DestroyPipeline(d.device, g.pipeBlit, nullptr); g.pipeBlit = 0; }
    for (int i = 0; i < 2; ++i) if (g.pipeTex[i] && d.DestroyPipeline) { d.DestroyPipeline(d.device, g.pipeTex[i], nullptr); g.pipeTex[i] = 0; }
    for (uint64_t fb : g.fbSwapL) if (fb && d.DestroyFramebuffer) d.DestroyFramebuffer(d.device, fb, nullptr);
    g.fbSwapL.clear();
    if (g.passSwapL && d.DestroyRenderPass) { d.DestroyRenderPass(d.device, g.passSwapL, nullptr); g.passSwapL = 0; }
}

inline void scene_drop_swapchain(Dev& d) { destroy_pipes_swap(d); }

inline void scene_swapchain(Dev& d) {
    Gpu& g = gpu();
    destroy_pipes_swap(d);
    if (!g.booted || !d.pass || d.views.empty()) return;
    struct Att { uint32_t flags, format, samples, load, store, sload, sstore, initial, final; } at[2] = {};
    at[0].format = d.colorFmt; at[0].samples = 1; at[0].load = 0; at[0].store = 0; at[0].sload = 2; at[0].sstore = 1;
    at[0].initial = 1000001002; at[0].final = 1000001002;
    at[1].format = d.depthFmt; at[1].samples = 1; at[1].load = 0; at[1].store = 0; at[1].sload = 2; at[1].sstore = 1;
    at[1].initial = 3; at[1].final = 3;
    struct Ref { uint32_t attachment, layout; };
    Ref color{ 0, 2 }, depth{ 1, 3 };
    struct Sub { uint32_t flags, bind, inCount; const void* in; uint32_t colorCount; const Ref* colors; const void* resolve; const Ref* depth; uint32_t preserve; const void* p; } sub{};
    sub.colorCount = 1; sub.colors = &color; sub.depth = &depth;
    struct RP { uint32_t sType; const void* pNext; uint32_t flags; uint32_t ac; const Att* a; uint32_t sc; const Sub* s; uint32_t dc; const void* dep; } rp{};
    rp.sType = 38; rp.ac = 2; rp.a = at; rp.sc = 1; rp.s = &sub;
    if (d.CreateRenderPass(d.device, &rp, nullptr, &g.passSwapL) != 0) return;
    g.fbSwapL.resize(d.views.size());
    for (size_t i = 0; i < d.views.size(); ++i) {
        if (!make_fb(d, g.passSwapL, d.views[i], d.depthView, (uint32_t)d.swapW, (uint32_t)d.swapH, &g.fbSwapL[i])) return;
    }
    const At blitAt[] = { {0, 0, 103, 0}, {1, 0, 103, 8} };
    const At texAt[] = { {0, 0, 106, 0}, {1, 0, 106, 12}, {2, 0, 103, 24} };
    if (!pipe_ex(d, g.sh[9], g.sh[10], 3, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0xF, 16, blitAt, 2, 1, g.layoutBlit, d.pass, &g.pipeBlit)) fail("blit pipe");
    pipe_ex(d, g.sh[11], g.sh[12], 3, 2, 0, 1, 1, 0, 1, 0, 0, 0, 0, 0xF, 32, texAt, 3, 1, g.layoutTex, d.pass, &g.pipeTex[0]);
    pipe_ex(d, g.sh[11], g.sh[12], 3, 0, 0, 1, 1, 0, 1, 0, 0, 0, 0, 0xF, 32, texAt, 3, 1, g.layoutTex, d.pass, &g.pipeTex[1]);
}

inline void scene_down(Dev& d) {
    Gpu& g = gpu();
    if (d.device) {
        destroy_pipes_swap(d);
        destroy_target(d);
        if (g.pipeShadow && d.DestroyPipeline) d.DestroyPipeline(d.device, g.pipeShadow, nullptr);
        if (g.fbSh && d.DestroyFramebuffer) d.DestroyFramebuffer(d.device, g.fbSh, nullptr);
        if (g.passSh && d.DestroyRenderPass) d.DestroyRenderPass(d.device, g.passSh, nullptr);
        free_img(d, g.shadow); free_img(d, g.white);
        for (Img& im : g.textures) free_img(d, im);
        free_stage(d, g.stage);
        for (Stage& s : g.extra) free_stage(d, s);
        if (g.sceneMap && d.UnmapMemory) d.UnmapMemory(d.device, g.sceneMem);
        if (g.drawMap && d.UnmapMemory) d.UnmapMemory(d.device, g.drawMem);
        if (g.partMap && d.UnmapMemory) d.UnmapMemory(d.device, g.partMem);
        if (g.sceneBuf && d.DestroyBuffer) d.DestroyBuffer(d.device, g.sceneBuf, nullptr);
        if (g.sceneMem && d.FreeMemory) d.FreeMemory(d.device, g.sceneMem, nullptr);
        if (g.drawBuf && d.DestroyBuffer) d.DestroyBuffer(d.device, g.drawBuf, nullptr);
        if (g.drawMem && d.FreeMemory) d.FreeMemory(d.device, g.drawMem, nullptr);
        if (g.partBuf && d.DestroyBuffer) d.DestroyBuffer(d.device, g.partBuf, nullptr);
        if (g.partMem && d.FreeMemory) d.FreeMemory(d.device, g.partMem, nullptr);
        if (g.pool && d.DestroyDescriptorPool) d.DestroyDescriptorPool(d.device, g.pool, nullptr);
        uint64_t sls[6] = { g.slMesh, g.slMat, g.slSky, g.slPart, g.slBlit, g.slTex };
        for (uint64_t sl : sls) if (sl && d.DestroyDescriptorSetLayout) d.DestroyDescriptorSetLayout(d.device, sl, nullptr);
        uint64_t lays[7] = { g.layoutMesh, g.layoutMat, g.layoutSky, g.layoutPart, g.layoutBlit, g.layoutTex, g.layoutDepth };
        for (uint64_t lay : lays) if (lay && d.DestroyPipelineLayout) d.DestroyPipelineLayout(d.device, lay, nullptr);
        for (uint64_t sh : g.sh) if (sh && d.DestroyShaderModule) d.DestroyShaderModule(d.device, sh, nullptr);
        if (g.sampLin && d.DestroySampler) d.DestroySampler(d.device, g.sampLin, nullptr);
        if (g.sampNear && d.DestroySampler) d.DestroySampler(d.device, g.sampNear, nullptr);
        if (g.sampSh && d.DestroySampler) d.DestroySampler(d.device, g.sampSh, nullptr);
        if (g.sampRep && d.DestroySampler) d.DestroySampler(d.device, g.sampRep, nullptr);
    }
    g = Gpu{};
}

inline void prime_mesh_sets(Dev& d);

inline int scene_boot(Dev& d) {
    Gpu& g = gpu();
    if (g.booted) return 1;
    const uint32_t* words[13] = { kSpvMeshVs, kSpvMeshFs, kSpvDepthVs, kSpvMatVs, kSpvMatFs, kSpvSkyVs, kSpvSkyFs, kSpvPartVs, kSpvPartFs, kSpvBlitVs, kSpvBlitFs, kSpvTexVs, kSpvTexFs };
    const size_t bytes[13] = { sizeof(kSpvMeshVs), sizeof(kSpvMeshFs), sizeof(kSpvDepthVs), sizeof(kSpvMatVs), sizeof(kSpvMatFs), sizeof(kSpvSkyVs), sizeof(kSpvSkyFs), sizeof(kSpvPartVs), sizeof(kSpvPartFs), sizeof(kSpvBlitVs), sizeof(kSpvBlitFs), sizeof(kSpvTexVs), sizeof(kSpvTexFs) };
    for (int i = 0; i < 13; ++i) if (!shader_mod(d, words[i], bytes[i] / 4, &g.sh[i])) { fail("scene shader"); scene_down(d); return 0; }
    auto samp = [&](uint64_t* out, uint32_t filter, uint32_t addr, uint32_t border) {
        struct Samp { uint32_t sType; const void* pNext; uint32_t flags; uint32_t mag, minf, mip, u, v, w; float bias; uint32_t anisoEn; float maxAniso; uint32_t cmpEn, cmp; float minLod, maxLod; uint32_t border, unnorm; } s{};
        s.sType = 31; s.mag = s.minf = filter; s.mip = filter ? 1u : 0u; s.u = s.v = s.w = addr; s.border = border;
        return d.CreateSampler(d.device, &s, nullptr, out) == 0;
    };
    if (!samp(&g.sampLin, 1, 2, 0) || !samp(&g.sampNear, 0, 2, 0) || !samp(&g.sampSh, 0, 3, 2) || !samp(&g.sampRep, 1, 0, 0)) { fail("scene sampler"); scene_down(d); return 0; }
    const uint32_t meshB[] = { 0, 6, 0x11, 1, 6, 0x11, 2, 1, 0x10 };
    const uint32_t matB[] = { 0, 6, 0x11, 1, 6, 0x11, 2, 1, 0x10, 3, 1, 0x10, 4, 1, 0x10, 5, 1, 0x10, 6, 1, 0x10, 7, 1, 0x10 };
    const uint32_t skyB[] = { 0, 1, 0x10, 1, 1, 0x10, 2, 1, 0x10, 3, 1, 0x10, 4, 1, 0x10, 5, 1, 0x10 };
    const uint32_t partB[] = { 0, 6, 0x11, 1, 1, 0x10 };
    const uint32_t oneB[] = { 0, 1, 0x10 };
    if (!layout_of(d, meshB, 3, 0, 0, 0, 0, 0, 0, 0, &g.slMesh, &g.layoutMesh)) { fail("mesh layout"); scene_down(d); return 0; }
    if (!layout_of(d, matB, 8, 0, 0, 0, 0, 0, 0, 0, &g.slMat, &g.layoutMat)) { fail("mat layout"); scene_down(d); return 0; }
    if (!layout_of(d, skyB, 6, 0x11, 0, 80, 0, 0, 0, 0, &g.slSky, &g.layoutSky)) { fail("sky layout"); scene_down(d); return 0; }
    if (!layout_of(d, partB, 2, 0, 0, 0, 0, 0, 0, 0, &g.slPart, &g.layoutPart)) { fail("part layout"); scene_down(d); return 0; }
    if (!layout_of(d, oneB, 1, 0, 0, 0, 0, 0, 0, 0, &g.slBlit, &g.layoutBlit)) { fail("blit layout"); scene_down(d); return 0; }
    if (!layout_of(d, oneB, 1, 1, 0, 64, 1, 0x10, 64, 4, &g.slTex, &g.layoutTex)) { fail("tex layout"); scene_down(d); return 0; }
    struct Range { uint32_t stage, offset, size; } pc{ 1, 0, 64 };
    struct PL2 { uint32_t sType; const void* pNext; uint32_t flags; uint32_t setCount; const void* sets; uint32_t pcCount; const Range* pc; } pl2{};
    pl2.sType = 30; pl2.pcCount = 1; pl2.pc = &pc;
    if (d.CreatePipelineLayout(d.device, &pl2, nullptr, &g.layoutDepth) != 0) { fail("depth layout"); scene_down(d); return 0; }
    struct PS { uint32_t type, count; } ps[3] = { { 6, 3000 }, { 7, 8 }, { 1, 2200 } };
    struct DP { uint32_t sType; const void* pNext; uint32_t flags; uint32_t maxSets; uint32_t n; const PS* p; } dp{};
    dp.sType = 33; dp.maxSets = 1600; dp.n = 3; dp.p = ps;
    if (d.CreateDescriptorPool(d.device, &dp, nullptr, &g.pool) != 0) { fail("scene pool"); scene_down(d); return 0; }
    if (!alloc_sets(d, g.slMesh, 1024, g.meshSets)) { fail("mesh sets"); scene_down(d); return 0; }
    g.setMesh = g.meshSets.empty() ? 0 : g.meshSets[0];
    if (!alloc_sets(d, g.slMat, 128, g.matSets)) { fail("mat sets"); scene_down(d); return 0; }
    if (!alloc_sets(d, g.slSky, 1, g.partSets)) { fail("sky set"); scene_down(d); return 0; }
    g.setSky = g.partSets.empty() ? 0 : g.partSets[0];
    g.partSets.clear();
    if (!alloc_sets(d, g.slPart, 64, g.partSets)) { fail("part sets"); scene_down(d); return 0; }
    if (!alloc_sets(d, g.slBlit, 1, g.texSets)) { fail("blit set"); scene_down(d); return 0; }
    g.setBlit = g.texSets.empty() ? 0 : g.texSets[0];
    g.texSets.clear();
    if (!alloc_sets(d, g.slTex, 128, g.texSets)) { fail("tex sets"); scene_down(d); return 0; }
    if (!buffer_alloc(d, 1072, 0x10, &g.sceneBuf, &g.sceneMem, &g.sceneMap)) { fail("scene ubo"); scene_down(d); return 0; }
    if (!buffer_alloc(d, 4096ull * 512ull, 0x10, &g.drawBuf, &g.drawMem, &g.drawMap)) { fail("draw ubo"); scene_down(d); return 0; }
    if (!buffer_alloc(d, 256, 0x10, &g.partBuf, &g.partMem, &g.partMap)) { fail("part ubo"); scene_down(d); return 0; }
    if (!grow_stage(d, g.stage, 8ull << 20)) { fail("stage"); scene_down(d); return 0; }
    uint32_t props[4] = {};
    d.GetPhysicalDeviceFormatProperties(d.phys, 129, props);
    if (props[1] & 0x200u) { g.depthFmt = 129; g.depthAspect = 6; }
    else {
        d.GetPhysicalDeviceFormatProperties(d.phys, 130, props);
        if (props[1] & 0x200u) { g.depthFmt = 130; g.depthAspect = 6; }
        else { g.depthFmt = 126; g.depthAspect = 2; }
    }
    if (!make_image(d, g.white, 37, 1, 1, 1, 0x2 | 0x4, 1)) { fail("white"); scene_down(d); return 0; }
    if (!ensure_shadow(d) || !ensure_target(d, 16, 16, 1)) { scene_down(d); return 0; }
    prime_mesh_sets(d);
    g.booted = 1;
    return 1;
}

inline int scene_ok() { return gpu().booted && gpu().pipeMesh[0] && gpu().pipeBlit; }

inline int scene_on() { return gpu().on; }
inline int scene_simple_open() { return gpu().simpleOpen; }
inline void scene_note_simple() {
    Dev& d = dev();
    Gpu& g = gpu();
    g.on = 0; g.simpleOpen = 1; g.texN = 0; g.stage.used = 0;
    for (Stage& s : g.extra) free_stage(d, s);
    g.extra.clear();
}
inline void scene_note_done() {
    Gpu& g = gpu();
    g.on = 0; g.simpleOpen = 0; g.inColor = 0; g.inShadow = 0;
}

inline int open_frame(Dev& d, int w, int h) {
    if (!d.device || d.framing) return 0;
    if (w < 1 || h < 1) return 0;
    if (d.WaitForFences(d.device, 1, &d.fence, 1, ~0ull) != 0) return 0;
    if (d.dirty || w != d.swapW || h != d.swapH) {
        if (!rebuild_swapchain(d, w, h)) return 0;
    }
    d.ResetCommandPool(d.device, d.pool, 0);
    int32_t acq = d.AcquireNextImageKHR(d.device, d.swapchain, ~0ull, d.semImg, 0, &d.imageIndex);
    if (acq == -1000001004) {
        d.dirty = 1;
        if (!rebuild_swapchain(d, w, h)) return 0;
        acq = d.AcquireNextImageKHR(d.device, d.swapchain, ~0ull, d.semImg, 0, &d.imageIndex);
    }
    if (acq == 1000001003) { d.dirty = 1; acq = 0; }
    if (acq != 0) return 0;
    struct BI2 { uint32_t sType; const void* pNext; uint32_t flags; const void* inherit; } bi{};
    bi.sType = 42; bi.flags = 1;
    if (d.BeginCommandBuffer(d.cmd, &bi) != 0) return 0;
    d.framing = 1; d.vused = 0;
    return 1;
}

inline void write_mesh_set(Dev& d) {
    Gpu& g = gpu();
    write_buf(d, g.setMesh, 0, 6, g.sceneBuf, 1072);
    write_buf(d, g.setMesh, 1, 6, g.drawBuf, 512);
    write_img(d, g.setMesh, 2, g.sampSh, g.shadow.view ? g.shadow.view : g.white.view);
}

inline int scene_begin(int ww, int wh, int rw, int rh, int samples, float red, float green, float blue) {
    Dev& d = dev();
    Gpu& g = gpu();
    if (!g.booted) return 0;
    if (!ensure_target(d, rw, rh, samples) && !g.color.image) return 0;
    if (!open_frame(d, ww, wh)) return 0;
    g.on = 1; g.simpleOpen = 0; g.inColor = 0; g.inShadow = 0; g.needLoad = 0; g.drewColor = 0;
    g.shadowReady = 0; g.drawN = 0; g.meshN = 0; g.matN = 0; g.partN = 0; g.texN = 0; g.stage.used = 0;
    g.cr = red; g.cg = green; g.cb = blue; g.d0 = 0; g.d1 = 1;
    for (Stage& s : g.extra) free_stage(d, s);
    g.extra.clear();
    if (g.whiteLayout != 5) {
        unsigned char px[4] = { 255, 255, 255, 255 };
        uint64_t buf = 0, off = 0;
        if (stage_bytes(d, px, 4, &buf, &off)) {
            barrier(d, g.white.image, 1, 1, 0x1000, 0, 0x1000, (uint32_t)g.whiteLayout, 7);
            struct Subr { uint32_t aspect, mip, layer, count; };
            struct Copy { uint64_t off; uint32_t row, height; Subr sub; int32_t x, y, z; uint32_t w, h, dep; } copy{};
            copy.off = off; copy.sub = { 1, 0, 0, 1 }; copy.w = 1; copy.h = 1; copy.dep = 1;
            d.CmdCopyBufferToImage(d.cmd, buf, g.white.image, 7, 1, &copy);
            barrier(d, g.white.image, 1, 0x1000, 0x80, 0x1000, 0x20, 7, 5);
            g.whiteLayout = 5;
        }
    }
    return 1;
}

inline void scene_uniforms(const void* bytes, int n) {
    Gpu& g = gpu();
    if (!g.sceneMap || !bytes || n < 1) return;
    if (n > 1072) n = 1072;
    std::memcpy(g.sceneMap, bytes, (size_t)n);
}

inline void ensure_shadow_readable(Dev& d) {
    Gpu& g = gpu();
    if (g.shadowReady || !g.shadow.image) return;
    if (g.shadowLayout != 3) {
        barrier(d, g.shadow.image, 2, 1, 0x200, 0, 0x400, (uint32_t)g.shadowLayout, 3);
        g.shadowLayout = 3;
    }
    begin_pass(d, g.passSh, g.fbSh, 2048, 2048, 1, 1, 1, 1);
    d.CmdEndRenderPass(d.cmd);
    g.shadowLayout = 5; g.shadowReady = 1;
}

inline void ensure_color(Dev& d) {
    Gpu& g = gpu();
    if (g.inColor) return;
    if (g.inShadow) { d.CmdEndRenderPass(d.cmd); g.inShadow = 0; g.shadowLayout = 5; g.shadowReady = 1; }
    ensure_shadow_readable(d);
    prep_color(d);
    uint64_t pass = g.needLoad ? g.passL : g.passC;
    uint64_t fb = g.needLoad ? g.fbL : g.fbC;
    begin_pass(d, pass, fb, (uint32_t)g.rw, (uint32_t)g.rh, 2, g.cr, g.cg, g.cb);
    vp_scene(d);
    g.inColor = 1; g.drewColor = 1; g.needLoad = 0;
}

inline uint64_t vert_upload(Dev& d, const void* data, uint64_t bytes) {
    if (!data || !bytes || d.vused + bytes > d.vcap) return ~0ull;
    std::memcpy((char*)d.vmap + d.vused, data, (size_t)bytes);
    uint64_t off = d.vused;
    d.vused = (d.vused + bytes + 15ull) & ~15ull;
    return off;
}

inline int draw_slot(const void* draw) {
    Gpu& g = gpu();
    if (!draw || g.drawN >= 4096 || !g.drawMap) return -1;
    std::memcpy((char*)g.drawMap + (size_t)g.drawN * 512, draw, 320);
    return g.drawN++;
}

inline int tex_hit(const void* key, int w, int h) {
    if (!key || w < 1 || h < 1) return 1;
    Gpu& g = gpu();
    for (const Img& im : g.textures)
        if (im.key == key && im.keyW == w && im.keyH == h && im.view) return 1;
    return 0;
}

inline void prime_mesh_sets(Dev& d) {
    Gpu& g = gpu();
    uint64_t sh = g.shadow.view ? g.shadow.view : g.white.view;
    for (size_t i = 0; i < g.meshSets.size(); ++i) {
        write_buf(d, g.meshSets[i], 0, 6, g.sceneBuf, 1072);
        write_buf_at(d, g.meshSets[i], 1, 6, g.drawBuf, (uint64_t)i * 512ull, 512);
        write_img(d, g.meshSets[i], 2, g.sampSh, sh);
    }
}

inline uint64_t vert_keep(const void* data, uint64_t bytes) {
    Dev& d = dev();
    if (!data || !bytes || !d.smap || d.sused + bytes > d.scap) return ~0ull;
    std::memcpy((char*)d.smap + d.sused, data, (size_t)bytes);
    uint64_t off = d.sused;
    d.sused = (d.sused + bytes + 15ull) & ~15ull;
    return off;
}

inline int shadow_begin() {
    Dev& d = dev();
    Gpu& g = gpu();
    if (!g.on || !g.pipeShadow) return 0;
    if (g.inColor) { close_pass(d); }
    if (g.shadowLayout != 3) {
        barrier(d, g.shadow.image, 2, 1, 0x200, 0, 0x400, (uint32_t)g.shadowLayout, 3);
        g.shadowLayout = 3;
    }
    begin_pass(d, g.passSh, g.fbSh, 2048, 2048, 1, 1, 1, 1);
    vp_shadow(d);
    d.CmdBindPipeline(d.cmd, 0, g.pipeShadow);
    g.inShadow = 1; g.shadowReady = 0;
    return 1;
}

inline void shadow_draw_buf(uint64_t buf, uint64_t off, int nvert, const float* mvp) {
    Dev& d = dev();
    Gpu& g = gpu();
    if (!g.inShadow || nvert <= 0 || !mvp || off == ~0ull || !buf) return;
    d.CmdPushConstants(d.cmd, g.layoutDepth, 1, 0, 64, mvp);
    d.CmdBindVertexBuffers(d.cmd, 0, 1, &buf, &off);
    d.CmdDraw(d.cmd, (uint32_t)nvert, 1, 0, 0);
}

inline void shadow_draw(const float* xyz, int nvert, const float* mvp) {
    Dev& d = dev();
    if (!xyz || nvert <= 0) return;
    shadow_draw_buf(d.vbuf, vert_upload(d, xyz, (uint64_t)nvert * 36ull), nvert, mvp);
}

inline void shadow_draw_kept(uint64_t off, int nvert, const float* mvp) {
    shadow_draw_buf(dev().sbuf, off, nvert, mvp);
}

inline void shadow_end() {
    Dev& d = dev();
    Gpu& g = gpu();
    if (!g.inShadow) return;
    d.CmdEndRenderPass(d.cmd);
    g.inShadow = 0; g.shadowLayout = 5; g.shadowReady = 1;
    struct MB { uint32_t sType; const void* pNext; uint32_t srcA, dstA; } mb{};
    mb.sType = 46; mb.srcA = 0x400; mb.dstA = 0x20;
    d.CmdPipelineBarrier(d.cmd, 0x200, 0x80, 0, 1, &mb, 0, nullptr, 0, nullptr);
}

inline void mesh_draw_buf(uint64_t buf, uint64_t off, int nvert, const void* draw, int kind) {
    Dev& d = dev();
    Gpu& g = gpu();
    if (!g.on || nvert <= 0 || !draw || kind < 0 || kind > 9 || !g.pipeMesh[kind] || off == ~0ull || !buf) return;
    ensure_color(d);
    int slot = draw_slot(draw);
    if (slot < 0 || slot >= (int)g.meshSets.size()) return;
    uint64_t set = g.meshSets[(size_t)slot];
    d.CmdBindPipeline(d.cmd, 0, g.pipeMesh[kind]);
    d.CmdBindDescriptorSets(d.cmd, 0, g.layoutMesh, 0, 1, &set, 0, nullptr);
    d.CmdBindVertexBuffers(d.cmd, 0, 1, &buf, &off);
    d.CmdDraw(d.cmd, (uint32_t)nvert, 1, 0, 0);
}

inline void mesh_draw(const float* xyz, int nvert, const void* draw, int kind) {
    Dev& d = dev();
    if (!xyz || nvert <= 0) return;
    mesh_draw_buf(d.vbuf, vert_upload(d, xyz, (uint64_t)nvert * 36ull), nvert, draw, kind);
}

inline void mesh_draw_kept(uint64_t off, int nvert, const void* draw, int kind) {
    mesh_draw_buf(dev().sbuf, off, nvert, draw, kind);
}

struct Pix { const unsigned char* p; int w, h; };

inline void mat_draw_buf(uint64_t buf, uint64_t off, int nvert, const void* draw, int blend, const Pix maps[5]) {
    Dev& d = dev();
    Gpu& g = gpu();
    if (!g.on || nvert <= 0 || !draw || off == ~0ull || !buf || g.matN >= (int)g.matSets.size() || g.matN >= 128) return;
    int fresh = 0;
    if (maps) {
        for (int i = 0; i < 5; ++i)
            if (maps[i].p && maps[i].w > 0 && maps[i].h > 0 && !tex_hit(maps[i].p, maps[i].w, maps[i].h)) fresh = 1;
    }
    if (fresh) close_pass(d);
    uint64_t views[5];
    for (int i = 0; i < 5; ++i) {
        views[i] = (maps && maps[i].p && maps[i].w > 0) ? upload_rgba(d, maps[i].p, maps[i].p, maps[i].w, maps[i].h, 3) : g.white.view;
        if (!views[i]) views[i] = g.white.view;
    }
    int slot = draw_slot(draw);
    if (slot < 0) return;
    int mi = g.matN++;
    uint64_t set = g.matSets[(size_t)mi];
    int same = g.matReady[mi] && g.matSlot[mi] == slot;
    if (same) {
        for (int i = 0; i < 5; ++i) if (g.matViews[mi][i] != views[i]) same = 0;
    }
    if (!same) {
        write_buf(d, set, 0, 6, g.sceneBuf, 1072);
        write_buf_at(d, set, 1, 6, g.drawBuf, (uint64_t)slot * 512ull, 512);
        write_img(d, set, 2, g.sampSh, g.shadow.view ? g.shadow.view : g.white.view);
        for (int i = 0; i < 5; ++i) {
            write_img(d, set, (uint32_t)(3 + i), g.sampLin, views[i]);
            g.matViews[mi][i] = views[i];
        }
        g.matSlot[mi] = slot;
        g.matReady[mi] = 1;
    }
    ensure_color(d);
    d.CmdBindPipeline(d.cmd, 0, g.pipeMat[blend ? 1 : 0]);
    d.CmdBindDescriptorSets(d.cmd, 0, g.layoutMat, 0, 1, &set, 0, nullptr);
    d.CmdBindVertexBuffers(d.cmd, 0, 1, &buf, &off);
    d.CmdDraw(d.cmd, (uint32_t)nvert, 1, 0, 0);
}

inline void mat_draw(const float* xyz, int nvert, const void* draw, int blend, const Pix maps[5]) {
    Dev& d = dev();
    if (!xyz || nvert <= 0) return;
    mat_draw_buf(d.vbuf, vert_upload(d, xyz, (uint64_t)nvert * 48ull), nvert, draw, blend, maps);
}

inline void mat_draw_kept(uint64_t off, int nvert, const void* draw, int blend, const Pix maps[5]) {
    mat_draw_buf(dev().sbuf, off, nvert, draw, blend, maps);
}

inline void sky_draw_buf(uint64_t buf, uint64_t off, int nvert, const void* pc, const Pix faces[6]) {
    Dev& d = dev();
    Gpu& g = gpu();
    if (!g.on || nvert <= 0 || !pc || !faces || !g.pipeSky || off == ~0ull || !buf) return;
    int fresh = 0;
    for (int i = 0; i < 6; ++i)
        if (faces[i].p && faces[i].w > 0 && faces[i].h > 0 && !tex_hit(faces[i].p, faces[i].w, faces[i].h)) fresh = 1;
    if (fresh) close_pass(d);
    uint64_t views[6];
    for (int i = 0; i < 6; ++i) {
        uint64_t view = upload_rgba(d, faces[i].p, faces[i].p, faces[i].w, faces[i].h, 3);
        views[i] = view ? view : g.white.view;
    }
    int same = g.skyReady;
    if (same) for (int i = 0; i < 6; ++i) if (g.skySeen[i] != views[i]) same = 0;
    if (!same) {
        for (int i = 0; i < 6; ++i) {
            write_img(d, g.setSky, (uint32_t)i, g.sampLin, views[i]);
            g.skySeen[i] = views[i];
        }
        g.skyReady = 1;
    }
    ensure_color(d);
    d.CmdBindPipeline(d.cmd, 0, g.pipeSky);
    d.CmdBindDescriptorSets(d.cmd, 0, g.layoutSky, 0, 1, &g.setSky, 0, nullptr);
    d.CmdPushConstants(d.cmd, g.layoutSky, 0x11, 0, 80, pc);
    d.CmdBindVertexBuffers(d.cmd, 0, 1, &buf, &off);
    d.CmdDraw(d.cmd, (uint32_t)nvert, 1, 0, 0);
}

inline void sky_draw(const float* xyz, int nvert, const void* pc, const Pix faces[6]) {
    Dev& d = dev();
    if (!xyz || nvert <= 0) return;
    sky_draw_buf(d.vbuf, vert_upload(d, xyz, (uint64_t)nvert * 12ull), nvert, pc, faces);
}

inline void sky_draw_kept(uint64_t off, int nvert, const void* pc, const Pix faces[6]) {
    sky_draw_buf(dev().sbuf, off, nvert, pc, faces);
}

inline void part_draw(const float* xyz, int nvert, const unsigned char* rgba, int w, int h, const void* cam) {
    Dev& d = dev();
    Gpu& g = gpu();
    if (!g.on || !xyz || nvert <= 0 || !cam || g.partN >= (int)g.partSets.size() || g.partN >= 64 || !g.pipePart) return;
    if (rgba && w > 0 && h > 0 && !tex_hit(rgba, w, h)) close_pass(d);
    uint64_t view = upload_rgba(d, rgba, rgba, w, h, 4);
    if (!view) view = g.white.view;
    if (g.partMap) std::memcpy(g.partMap, cam, 144);
    int pi = g.partN++;
    uint64_t set = g.partSets[(size_t)pi];
    if (!g.partReady[pi] || g.partView[pi] != view) {
        write_buf(d, set, 0, 6, g.partBuf, 144);
        write_img(d, set, 1, g.sampLin, view);
        g.partView[pi] = view;
        g.partReady[pi] = 1;
    }
    ensure_color(d);
    uint64_t off = vert_upload(d, xyz, (uint64_t)nvert * 36ull);
    if (off == ~0ull) return;
    d.CmdBindPipeline(d.cmd, 0, g.pipePart);
    d.CmdBindDescriptorSets(d.cmd, 0, g.layoutPart, 0, 1, &set, 0, nullptr);
    d.CmdBindVertexBuffers(d.cmd, 0, 1, &d.vbuf, &off);
    d.CmdDraw(d.cmd, (uint32_t)nvert, 1, 0, 0);
}

inline void depth_range(float a, float b) {
    Dev& d = dev();
    Gpu& g = gpu();
    g.d0 = a; g.d1 = b;
    if (g.inColor) vp_scene(d);
}

inline void draw_tex(const float* xyz, int nvert, const unsigned char* rgba, int w, int h, int cutout, int twoSided, const float* mvp) {
    Dev& d = dev();
    Gpu& g = gpu();
    if (g.on || !d.framing || !xyz || nvert <= 0 || !rgba || !mvp || g.texN >= (int)g.texSets.size()) return;
    int pipe = (twoSided || cutout) ? 1 : 0;
    if (!g.pipeTex[pipe]) return;
    close_pass(d);
    uint64_t view = upload_rgba(d, rgba, rgba, w, h, 4);
    uint64_t set = g.texSets[(size_t)g.texN++];
    write_img(d, set, 0, g.sampRep, view ? view : g.white.view);
    if (g.passSwapL && d.imageIndex < g.fbSwapL.size() && g.fbSwapL[d.imageIndex]) {
        begin_pass(d, g.passSwapL, g.fbSwapL[d.imageIndex], (uint32_t)d.swapW, (uint32_t)d.swapH, 2, 0, 0, 0);
        set_viewport(d);
        g.simpleOpen = 1;
    } else return;
    uint64_t off = vert_upload(d, xyz, (uint64_t)nvert * 32ull);
    if (off == ~0ull) return;
    float cut = cutout ? 1.0f : 0.0f;
    d.CmdBindPipeline(d.cmd, 0, g.pipeTex[pipe]);
    d.CmdBindDescriptorSets(d.cmd, 0, g.layoutTex, 0, 1, &set, 0, nullptr);
    d.CmdPushConstants(d.cmd, g.layoutTex, 1, 0, 64, mvp);
    d.CmdPushConstants(d.cmd, g.layoutTex, 0x10, 64, 4, &cut);
    d.CmdBindVertexBuffers(d.cmd, 0, 1, &d.vbuf, &off);
    d.CmdDraw(d.cmd, (uint32_t)nvert, 1, 0, 0);
}

inline void scene_finish(Dev& d) {
    Gpu& g = gpu();
    if (!g.on) return;
    if (!g.drewColor) ensure_color(d);
    if (g.inColor || g.inShadow) {
        d.CmdEndRenderPass(d.cmd);
        g.inColor = 0; g.inShadow = 0;
    }
    uint64_t src = g.color.image;
    if (g.samples > 1 && g.resolve.image && d.CmdResolveImage) {
        if (g.colorLayout != 6) {
            barrier(d, g.color.image, 1, 0x400, 0x1000, 0x100, 0x800, (uint32_t)g.colorLayout, 6);
            g.colorLayout = 6;
        }
        if (g.resolveLayout != 7) {
            barrier(d, g.resolve.image, 1, 1, 0x1000, 0, 0x1000, (uint32_t)g.resolveLayout, 7);
        }
        struct Layers { uint32_t aspect, mip, base, count; };
        struct Off { int32_t x, y, z; };
        struct Ext { uint32_t w, h, dep; };
        struct Res { Layers src; Off so; Layers dst; Off dO; Ext e; } res{};
        res.src = { 1, 0, 0, 1 }; res.dst = { 1, 0, 0, 1 }; res.e = { (uint32_t)g.rw, (uint32_t)g.rh, 1 };
        d.CmdResolveImage(d.cmd, g.color.image, 6, g.resolve.image, 7, 1, &res);
        barrier(d, g.resolve.image, 1, 0x1000, 0x80, 0x1000, 0x20, 7, 5);
        g.resolveLayout = 5;
        src = g.resolve.image;
    } else if (g.colorLayout != 5) {
        barrier(d, g.color.image, 1, 0x400, 0x80, 0x100, 0x20, (uint32_t)g.colorLayout, 5);
        g.colorLayout = 5;
        src = g.color.image;
    }
    uint64_t view = (g.samples > 1 && g.resolve.view) ? g.resolve.view : g.color.view;
    if (g.pipeBlit && g.setBlit && view && d.imageIndex < d.fbs.size()) {
        uint64_t samp = (g.rw == d.swapW && g.rh == d.swapH && g.sampNear) ? g.sampNear : g.sampLin;
        write_img(d, g.setBlit, 0, samp, view);
        begin_pass(d, d.pass, d.fbs[d.imageIndex], (uint32_t)d.swapW, (uint32_t)d.swapH, 2, 0, 0, 0);
        set_viewport(d);
        d.CmdBindPipeline(d.cmd, 0, g.pipeBlit);
        d.CmdBindDescriptorSets(d.cmd, 0, g.layoutBlit, 0, 1, &g.setBlit, 0, nullptr);
        uint64_t off = 0;
        d.CmdBindVertexBuffers(d.cmd, 0, 1, &d.quad, &off);
        d.CmdDraw(d.cmd, 6, 1, 0, 0);
        d.CmdEndRenderPass(d.cmd);
    }
    (void)src;
}
