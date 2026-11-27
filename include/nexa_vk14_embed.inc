inline std::string nexa_vk14_embed() {
return R"NEXA_VK14(
#pragma once

// Vulkan 1.4 for std/gfx3d and the Eyescire view. No vulkan.h: the loader is
// vulkan-1.dll and every entry point is fetched by name. A machine that cannot
// open API version 1.4 leaves the device closed so the caller can stay on OpenGL.

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <vector>

namespace nexa_vk14 {

inline const char*& last_error() {
    static const char* e = "";
    return e;
}

namespace detail {

static const uint32_t kApi14 = 0x00404000u;

// The shaders, kVs and the rest, are in nexa_vk14_spv.inc: see spv().

using PFN_void = int32_t(__stdcall*)();

struct Dev {
    HMODULE dll = nullptr;
    void* gi = nullptr;
    void* instance = nullptr;
    void* phys = nullptr;
    void* device = nullptr;
    void* queue = nullptr;
    void* cmd = nullptr;
    uint64_t surface = 0;
    uint64_t swapchain = 0;
    uint64_t pass = 0;
    uint64_t passOv = 0;
    uint64_t layout = 0;
    uint64_t layoutOv = 0;
    uint64_t pipeTri = 0, pipeTwo = 0, pipeLine = 0, pipeOv = 0;
// [nexa:vkshader-begin]
    // gfx3d.shader: the author's own SPIR-V, built into the same three
    // pipelines the built-in shader has -- triangles, two-sided triangles and
    // lines -- and bound in their place while customOn. Dropped from the
    // emission for a program that loads no shader.
    uint64_t pipeCustomTri = 0, pipeCustomTwo = 0, pipeCustomLine = 0;
    uint64_t shCustom[2] = {};
    // The textured pipelines are built from their own pair: a sampler and a uv
    // the untextured ones cannot supply. Equal to shCustom when the shader came
    // in as Vulkan GLSL and has no translated twin.
    uint64_t shCustomTex[2] = {};
    int customOn = 0;
    // What gfx3d.shader_set writes. A Vulkan shader has no uniform to look up
    // by name, so these go up as push constants after the transform and the
    // shader reads them out of the same block.
    float userPC[16] = {};
// [nexa:vkshader-end]
    uint64_t shVs = 0, shFs = 0, shOvVs = 0, shOvFs = 0;
    uint64_t pool = 0;
    uint64_t vbuf = 0, vmem = 0;
    void* vmap = nullptr;
    uint64_t vcap = 0, vused = 0;
    uint64_t sbuf = 0, smem = 0;
    void* smap = nullptr;
    uint64_t scap = 0, sused = 0;
    uint64_t fence = 0, semImg = 0, semDraw = 0;
    uint64_t depthImg = 0, depthMem = 0, depthView = 0;
    uint32_t depthFmt = 0;
    uint32_t colorFmt = 0;
    uint64_t sampler = 0, setLayout = 0, descPool = 0, descSet = 0;
    uint64_t ovImg = 0, ovMem = 0, ovView = 0, ovStage = 0, ovStageMem = 0;
    void* ovMap = nullptr;
    uint64_t ovStageCap = 0;
    int ovW = 0, ovH = 0;
    uint64_t quad = 0, quadMem = 0;
    std::vector<uint64_t> images, views, fbs, ovFbs;
    int framing = 0;
    int swapW = 0, swapH = 0;
    int vsync = 1;
    int dirty = 1;
    uint32_t imageIndex = 0;
    uint32_t qfam = 0;
    uint32_t memCount = 0;
    uint32_t memFlags[32] = {};
    const void* pendOv = nullptr;
    int pendW = 0, pendH = 0;
    uint64_t readBuf = 0, readMem = 0;
    void* readMap = nullptr;
    uint64_t readCap = 0;
    int readW = 0, readH = 0;
    int wantRead = 0;

    using gp_t = void*(__stdcall*)(void*, const char*);
    gp_t GetInstanceProcAddr = nullptr;
    gp_t GetDeviceProcAddr = nullptr;

#define NEXA_VK_FN(name, ...) using name##_t = __VA_ARGS__; name##_t name = nullptr;
    NEXA_VK_FN(EnumerateInstanceVersion, uint32_t(__stdcall*)(uint32_t*))
    NEXA_VK_FN(CreateInstance, int32_t(__stdcall*)(const void*, const void*, void**))
    NEXA_VK_FN(DestroyInstance, void(__stdcall*)(void*, const void*))
    NEXA_VK_FN(EnumeratePhysicalDevices, int32_t(__stdcall*)(void*, uint32_t*, void**))
    NEXA_VK_FN(GetPhysicalDeviceProperties, void(__stdcall*)(void*, void*))
    NEXA_VK_FN(GetPhysicalDeviceQueueFamilyProperties, void(__stdcall*)(void*, uint32_t*, void*))
    NEXA_VK_FN(GetPhysicalDeviceSurfaceSupportKHR, int32_t(__stdcall*)(void*, uint32_t, uint64_t, uint32_t*))
    NEXA_VK_FN(GetPhysicalDeviceSurfaceFormatsKHR, int32_t(__stdcall*)(void*, uint64_t, uint32_t*, void*))
    NEXA_VK_FN(GetPhysicalDeviceSurfaceCapabilitiesKHR, int32_t(__stdcall*)(void*, uint64_t, void*))
    NEXA_VK_FN(GetPhysicalDeviceSurfacePresentModesKHR, int32_t(__stdcall*)(void*, uint64_t, uint32_t*, uint32_t*))
    NEXA_VK_FN(GetPhysicalDeviceMemoryProperties, void(__stdcall*)(void*, void*))
    NEXA_VK_FN(GetPhysicalDeviceFormatProperties, void(__stdcall*)(void*, uint32_t, void*))
    NEXA_VK_FN(EnumerateDeviceExtensionProperties, int32_t(__stdcall*)(void*, const char*, uint32_t*, void*))
    NEXA_VK_FN(CreateWin32SurfaceKHR, int32_t(__stdcall*)(void*, const void*, const void*, uint64_t*))
    NEXA_VK_FN(DestroySurfaceKHR, void(__stdcall*)(void*, uint64_t, const void*))
    NEXA_VK_FN(CreateDevice, int32_t(__stdcall*)(void*, const void*, const void*, void**))
    NEXA_VK_FN(DestroyDevice, void(__stdcall*)(void*, const void*))
    NEXA_VK_FN(GetDeviceQueue, void(__stdcall*)(void*, uint32_t, uint32_t, void**))
    NEXA_VK_FN(DeviceWaitIdle, int32_t(__stdcall*)(void*))
    NEXA_VK_FN(CreateSwapchainKHR, int32_t(__stdcall*)(void*, const void*, const void*, uint64_t*))
    NEXA_VK_FN(DestroySwapchainKHR, void(__stdcall*)(void*, uint64_t, const void*))
    NEXA_VK_FN(GetSwapchainImagesKHR, int32_t(__stdcall*)(void*, uint64_t, uint32_t*, uint64_t*))
    NEXA_VK_FN(CreateImageView, int32_t(__stdcall*)(void*, const void*, const void*, uint64_t*))
    NEXA_VK_FN(DestroyImageView, void(__stdcall*)(void*, uint64_t, const void*))
    NEXA_VK_FN(CreateImage, int32_t(__stdcall*)(void*, const void*, const void*, uint64_t*))
    NEXA_VK_FN(DestroyImage, void(__stdcall*)(void*, uint64_t, const void*))
    NEXA_VK_FN(GetImageMemoryRequirements, void(__stdcall*)(void*, uint64_t, void*))
    NEXA_VK_FN(AllocateMemory, int32_t(__stdcall*)(void*, const void*, const void*, uint64_t*))
    NEXA_VK_FN(FreeMemory, void(__stdcall*)(void*, uint64_t, const void*))
    NEXA_VK_FN(BindImageMemory, int32_t(__stdcall*)(void*, uint64_t, uint64_t, uint64_t))
    NEXA_VK_FN(BindBufferMemory, int32_t(__stdcall*)(void*, uint64_t, uint64_t, uint64_t))
    NEXA_VK_FN(CreateRenderPass, int32_t(__stdcall*)(void*, const void*, const void*, uint64_t*))
    NEXA_VK_FN(DestroyRenderPass, void(__stdcall*)(void*, uint64_t, const void*))
    NEXA_VK_FN(CreateFramebuffer, int32_t(__stdcall*)(void*, const void*, const void*, uint64_t*))
    NEXA_VK_FN(DestroyFramebuffer, void(__stdcall*)(void*, uint64_t, const void*))
    NEXA_VK_FN(CreateShaderModule, int32_t(__stdcall*)(void*, const void*, const void*, uint64_t*))
    NEXA_VK_FN(DestroyShaderModule, void(__stdcall*)(void*, uint64_t, const void*))
    NEXA_VK_FN(CreatePipelineLayout, int32_t(__stdcall*)(void*, const void*, const void*, uint64_t*))
    NEXA_VK_FN(DestroyPipelineLayout, void(__stdcall*)(void*, uint64_t, const void*))
    NEXA_VK_FN(CreateGraphicsPipelines, int32_t(__stdcall*)(void*, uint64_t, uint32_t, const void*, const void*, uint64_t*))
    NEXA_VK_FN(DestroyPipeline, void(__stdcall*)(void*, uint64_t, const void*))
    NEXA_VK_FN(CreateCommandPool, int32_t(__stdcall*)(void*, const void*, const void*, uint64_t*))
    NEXA_VK_FN(DestroyCommandPool, void(__stdcall*)(void*, uint64_t, const void*))
    NEXA_VK_FN(ResetCommandPool, int32_t(__stdcall*)(void*, uint64_t, uint32_t))
    NEXA_VK_FN(AllocateCommandBuffers, int32_t(__stdcall*)(void*, const void*, void**))
    NEXA_VK_FN(BeginCommandBuffer, int32_t(__stdcall*)(void*, const void*))
    NEXA_VK_FN(EndCommandBuffer, int32_t(__stdcall*)(void*))
    NEXA_VK_FN(CmdBeginRenderPass, void(__stdcall*)(void*, const void*, uint32_t))
    NEXA_VK_FN(CmdEndRenderPass, void(__stdcall*)(void*))
    NEXA_VK_FN(CmdBindPipeline, void(__stdcall*)(void*, uint32_t, uint64_t))
    NEXA_VK_FN(CmdBindVertexBuffers, void(__stdcall*)(void*, uint32_t, uint32_t, const uint64_t*, const uint64_t*))
    NEXA_VK_FN(CmdPushConstants, void(__stdcall*)(void*, uint64_t, uint32_t, uint32_t, uint32_t, const void*))
    NEXA_VK_FN(CmdDraw, void(__stdcall*)(void*, uint32_t, uint32_t, uint32_t, uint32_t))
    NEXA_VK_FN(CmdSetViewport, void(__stdcall*)(void*, uint32_t, uint32_t, const void*))
    NEXA_VK_FN(CmdSetScissor, void(__stdcall*)(void*, uint32_t, uint32_t, const void*))
    NEXA_VK_FN(CmdBindDescriptorSets, void(__stdcall*)(void*, uint32_t, uint64_t, uint32_t, uint32_t, const uint64_t*, uint32_t, const uint32_t*))
    NEXA_VK_FN(CmdPipelineBarrier, void(__stdcall*)(void*, uint32_t, uint32_t, uint32_t, uint32_t, const void*, uint32_t, const void*, uint32_t, const void*))
    NEXA_VK_FN(CmdCopyBufferToImage, void(__stdcall*)(void*, uint64_t, uint64_t, uint32_t, uint32_t, const void*))
    NEXA_VK_FN(CmdCopyImageToBuffer, void(__stdcall*)(void*, uint64_t, uint32_t, uint64_t, uint32_t, const void*))
    NEXA_VK_FN(CmdResolveImage, void(__stdcall*)(void*, uint64_t, uint32_t, uint64_t, uint32_t, uint32_t, const void*))
    NEXA_VK_FN(CreateBuffer, int32_t(__stdcall*)(void*, const void*, const void*, uint64_t*))
    NEXA_VK_FN(DestroyBuffer, void(__stdcall*)(void*, uint64_t, const void*))
    NEXA_VK_FN(GetBufferMemoryRequirements, void(__stdcall*)(void*, uint64_t, void*))
    NEXA_VK_FN(MapMemory, int32_t(__stdcall*)(void*, uint64_t, uint64_t, uint64_t, uint32_t, void**))
    NEXA_VK_FN(FlushMappedMemoryRanges, int32_t(__stdcall*)(void*, uint32_t, const void*))
    NEXA_VK_FN(UnmapMemory, void(__stdcall*)(void*, uint64_t))
    NEXA_VK_FN(CreateFence, int32_t(__stdcall*)(void*, const void*, const void*, uint64_t*))
    NEXA_VK_FN(DestroyFence, void(__stdcall*)(void*, uint64_t, const void*))
    NEXA_VK_FN(CreateSemaphore, int32_t(__stdcall*)(void*, const void*, const void*, uint64_t*))
    NEXA_VK_FN(DestroySemaphore, void(__stdcall*)(void*, uint64_t, const void*))
    NEXA_VK_FN(WaitForFences, int32_t(__stdcall*)(void*, uint32_t, const uint64_t*, uint32_t, uint64_t))
    NEXA_VK_FN(ResetFences, int32_t(__stdcall*)(void*, uint32_t, const uint64_t*))
    NEXA_VK_FN(QueueSubmit, int32_t(__stdcall*)(void*, uint32_t, const void*, uint64_t))
    NEXA_VK_FN(QueuePresentKHR, int32_t(__stdcall*)(void*, const void*))
    NEXA_VK_FN(AcquireNextImageKHR, int32_t(__stdcall*)(void*, uint64_t, uint64_t, uint64_t, uint64_t, uint32_t*))
    NEXA_VK_FN(CreateSampler, int32_t(__stdcall*)(void*, const void*, const void*, uint64_t*))
    NEXA_VK_FN(DestroySampler, void(__stdcall*)(void*, uint64_t, const void*))
    NEXA_VK_FN(CreateDescriptorSetLayout, int32_t(__stdcall*)(void*, const void*, const void*, uint64_t*))
    NEXA_VK_FN(DestroyDescriptorSetLayout, void(__stdcall*)(void*, uint64_t, const void*))
    NEXA_VK_FN(CreateDescriptorPool, int32_t(__stdcall*)(void*, const void*, const void*, uint64_t*))
    NEXA_VK_FN(DestroyDescriptorPool, void(__stdcall*)(void*, uint64_t, const void*))
    NEXA_VK_FN(AllocateDescriptorSets, int32_t(__stdcall*)(void*, const void*, uint64_t*))
    NEXA_VK_FN(UpdateDescriptorSets, void(__stdcall*)(void*, uint32_t, const void*, uint32_t, const void*))
#undef NEXA_VK_FN
};

inline Dev& dev() {
    static Dev d;
    return d;
}

inline void fail(const char* msg) { last_error() = msg; }

template <class T>
int load_inst(Dev& d, T& fn, const char* name) {
    fn = (T)d.GetInstanceProcAddr(d.instance, name);
    if (!fn) { fail(name); return 0; }
    return 1;
}

template <class T>
int load_dev(Dev& d, T& fn, const char* name) {
    fn = (T)d.GetDeviceProcAddr(d.device, name);
    if (!fn) { fail(name); return 0; }
    return 1;
}

inline int mem_type(Dev& d, uint32_t bits, uint32_t want) {
    for (uint32_t i = 0; i < d.memCount; ++i) {
        if ((bits & (1u << i)) && (d.memFlags[i] & want) == want) return (int)i;
    }
    return -1;
}

struct MemReq { uint64_t size; uint64_t alignment; uint32_t bits; };

inline int buffer_alloc(Dev& d, uint64_t size, uint32_t usage, uint64_t* buf, uint64_t* mem, void** mapped) {
    struct Buf {
        uint32_t sType; const void* pNext; uint32_t flags; uint64_t size;
        uint32_t usage, sharing, qcount; const void* q;
    } info{};
    info.sType = 12;
    info.size = size;
    info.usage = usage;
    if (d.CreateBuffer(d.device, &info, nullptr, buf) != 0 || !*buf) return 0;
    MemReq req{};
    d.GetBufferMemoryRequirements(d.device, *buf, &req);
    int mt = mem_type(d, req.bits, 1u | 2u | 4u);
    if (mt < 0) mt = mem_type(d, req.bits, 2u | 4u);
    if (mt < 0) return 0;
    struct AI { uint32_t sType; const void* pNext; uint64_t size; uint32_t index; } ai{};
    ai.sType = 5;
    ai.size = req.size;
    ai.index = (uint32_t)mt;
    if (d.AllocateMemory(d.device, &ai, nullptr, mem) != 0 || !*mem) return 0;
    if (d.BindBufferMemory(d.device, *buf, *mem, 0) != 0) return 0;
    if (mapped && d.MapMemory(d.device, *mem, 0, req.size, 0, mapped) != 0) return 0;
    return 1;
}

inline void shutdown();

inline int shader_mod(Dev& d, const uint32_t* words, size_t nwords, uint64_t* out) {
    if (!words) return 0;
    struct Info { uint32_t sType; const void* pNext; uint32_t flags; uint64_t codeSize; const uint32_t* code; } info{};
    info.sType = 16;
    info.codeSize = nwords * 4;
    info.code = words;
    return d.CreateShaderModule(d.device, &info, nullptr, out) == 0 && *out;
}

inline int make_pipe(Dev& d, uint64_t vs, uint64_t fs, uint32_t topology, uint32_t cull,
                     int depth, int blend, uint64_t pass, uint32_t stride, int overlay, uint64_t layout, uint64_t* out) {
    struct Stage { uint32_t sType; const void* pNext; uint32_t flags; uint32_t stage; uint64_t module; const char* name; const void* spec; };
    Stage stages[2] = {};
    stages[0].sType = 18; stages[0].stage = 1; stages[0].module = vs; stages[0].name = "main";
    stages[1].sType = 18; stages[1].stage = 0x10; stages[1].module = fs; stages[1].name = "main";

    struct Bind { uint32_t binding, stride, rate; };
    struct Attr { uint32_t location, binding, format, offset; };
    Bind bind{ 0, stride, 0 };
    Attr attr[2];
    attr[0] = { 0, 0, 106, 0 };
    attr[1] = { 1, 0, overlay ? 103u : 106u, overlay ? 8u : 12u };

    struct VI { uint32_t sType; const void* pNext; uint32_t flags; uint32_t bc; const Bind* b; uint32_t ac; const Attr* a; } vi{};
    vi.sType = 19; vi.bc = 1; vi.b = &bind; vi.ac = 2; vi.a = attr;

    struct IA { uint32_t sType; const void* pNext; uint32_t flags; uint32_t topo; uint32_t restart; } ia{};
    ia.sType = 20; ia.topo = topology;

    struct VP { uint32_t sType; const void* pNext; uint32_t flags; uint32_t vc; const void* v; uint32_t sc; const void* s; } vp{};
    vp.sType = 22; vp.vc = 1; vp.sc = 1;

    struct RS { uint32_t sType; const void* pNext; uint32_t flags; uint32_t depthClamp; uint32_t discard; uint32_t poly; uint32_t cull; uint32_t front; uint32_t biasEn; float bconst, bclamp, bslope, lineWidth; } rs{};
    rs.sType = 23; rs.cull = cull; rs.lineWidth = 1.0f;

    struct MS { uint32_t sType; const void* pNext; uint32_t flags; uint32_t samples; uint32_t sampleShading; float minSample; const void* mask; uint32_t a2c; uint32_t a2o; } ms{};
    ms.sType = 24; ms.samples = 1;

    struct St { uint32_t failOp, passOp, depthFailOp, compareOp, compareMask, writeMask, reference; };
    struct DS { uint32_t sType; const void* pNext; uint32_t flags; uint32_t test; uint32_t write; uint32_t cmp; uint32_t bounds; uint32_t stencil; St front; St back; float minB, maxB; } ds{};
    ds.sType = 25; ds.test = depth ? 1u : 0u; ds.write = depth ? 1u : 0u; ds.cmp = 1; ds.maxB = 1.0f;

    struct BA { uint32_t en, srcC, dstC, opC, srcA, dstA, opA, mask; } ba{};
    ba.en = blend ? 1u : 0u; ba.srcC = 6; ba.dstC = 7; ba.srcA = 6; ba.dstA = 7; ba.mask = 0xF;

    struct CB { uint32_t sType; const void* pNext; uint32_t flags; uint32_t logicEn; uint32_t logicOp; uint32_t ac; const BA* a; float c[4]; } cb{};
    cb.sType = 26; cb.ac = 1; cb.a = &ba;

    uint32_t dyns[2] = { 0, 1 };
    struct DY { uint32_t sType; const void* pNext; uint32_t flags; uint32_t n; const uint32_t* p; } dy{};
    dy.sType = 27; dy.n = 2; dy.p = dyns;

    struct GP { uint32_t sType; const void* pNext; uint32_t flags; uint32_t stageCount; const Stage* stages; const VI* vi; const IA* ia; const void* tess; const VP* vp; const RS* rs; const MS* ms; const DS* ds; const CB* cb; const DY* dy; uint64_t layout; uint64_t pass; uint32_t subpass; uint64_t base; int32_t baseIndex; } gp{};
    gp.sType = 28; gp.stageCount = 2; gp.stages = stages; gp.vi = &vi; gp.ia = &ia; gp.vp = &vp; gp.rs = &rs; gp.ms = &ms; gp.ds = &ds; gp.cb = &cb; gp.dy = &dy; gp.layout = layout; gp.pass = pass;
    return d.CreateGraphicsPipelines(d.device, 0, 1, &gp, nullptr, out) == 0 && *out;
}

inline void destroy_swapchain(Dev& d) {
    if (!d.device) return;
    for (uint64_t fb : d.fbs) if (fb && d.DestroyFramebuffer) d.DestroyFramebuffer(d.device, fb, nullptr);
    for (uint64_t fb : d.ovFbs) if (fb && d.DestroyFramebuffer) d.DestroyFramebuffer(d.device, fb, nullptr);
    d.fbs.clear();
    d.ovFbs.clear();
    for (uint64_t v : d.views) if (v && d.DestroyImageView) d.DestroyImageView(d.device, v, nullptr);
    d.views.clear();
    d.images.clear();
    if (d.depthView && d.DestroyImageView) d.DestroyImageView(d.device, d.depthView, nullptr);
    d.depthView = 0;
    if (d.depthImg && d.DestroyImage) d.DestroyImage(d.device, d.depthImg, nullptr);
    d.depthImg = 0;
    if (d.depthMem && d.FreeMemory) d.FreeMemory(d.device, d.depthMem, nullptr);
    d.depthMem = 0;
    if (d.swapchain && d.DestroySwapchainKHR) d.DestroySwapchainKHR(d.device, d.swapchain, nullptr);
    d.swapchain = 0;
    d.swapW = d.swapH = 0;
}

inline int image_mem(Dev& d, uint64_t image, uint32_t want, uint64_t* mem) {
    MemReq req{};
    d.GetImageMemoryRequirements(d.device, image, &req);
    int mt = mem_type(d, req.bits, want);
    if (mt < 0) mt = mem_type(d, req.bits, 0);
    if (mt < 0) return 0;
    struct AI { uint32_t sType; const void* pNext; uint64_t size; uint32_t index; } ai{};
    ai.sType = 5; ai.size = req.size; ai.index = (uint32_t)mt;
    return d.AllocateMemory(d.device, &ai, nullptr, mem) == 0 && *mem && d.BindImageMemory(d.device, image, *mem, 0) == 0;
}

inline int make_depth(Dev& d, uint32_t w, uint32_t h) {
    uint32_t fmts[3] = { 126, 129, 124 };
    d.depthFmt = 0;
    for (uint32_t f : fmts) {
        uint32_t props[4] = {};
        d.GetPhysicalDeviceFormatProperties(d.phys, f, props);
        if (props[1] & 0x200u) { d.depthFmt = f; break; }
    }
    if (!d.depthFmt) return 0;
    struct Img { uint32_t sType; const void* pNext; uint32_t flags; uint32_t type; uint32_t format; uint32_t ew, eh, ed; uint32_t mips, layers, samples, tiling, usage, sharing, qcount; const void* q; uint32_t initial; } img{};
    img.sType = 14; img.type = 1; img.format = d.depthFmt; img.ew = w; img.eh = h; img.ed = 1; img.mips = 1; img.layers = 1; img.samples = 1; img.usage = 0x20;
    if (d.CreateImage(d.device, &img, nullptr, &d.depthImg) != 0) return 0;
    if (!image_mem(d, d.depthImg, 1u, &d.depthMem)) return 0;
    struct Comp { uint32_t r, g, b, a; };
    struct Range { uint32_t aspect, baseMip, levels, baseLayer, layers; };
    struct View { uint32_t sType; const void* pNext; uint32_t flags; uint64_t image; uint32_t type; uint32_t format; Comp c; Range range; } view{};
    view.sType = 15; view.image = d.depthImg; view.type = 1; view.format = d.depthFmt; view.range.aspect = 2; view.range.levels = 1; view.range.layers = 1;
    return d.CreateImageView(d.device, &view, nullptr, &d.depthView) == 0;
}

inline int make_view(Dev& d, uint64_t image, uint32_t format, uint32_t aspect, uint64_t* out) {
    struct Comp { uint32_t r, g, b, a; };
    struct Range { uint32_t aspect, baseMip, levels, baseLayer, layers; };
    struct View { uint32_t sType; const void* pNext; uint32_t flags; uint64_t image; uint32_t type; uint32_t format; Comp c; Range range; } view{};
    view.sType = 15; view.image = image; view.type = 1; view.format = format; view.range.aspect = aspect; view.range.levels = 1; view.range.layers = 1;
    return d.CreateImageView(d.device, &view, nullptr, out) == 0;
}

inline void scene_drop_swapchain(Dev&);
inline void scene_swapchain(Dev&);

inline int rebuild_swapchain(Dev& d, int wantW, int wantH) {
    if (d.device && d.DeviceWaitIdle) d.DeviceWaitIdle(d.device);
    scene_drop_swapchain(d);
    destroy_swapchain(d);

    struct Ext { uint32_t w, h; };
    struct Caps {
        uint32_t minImageCount, maxImageCount;
        Ext currentExtent, minImageExtent, maxImageExtent;
        uint32_t maxImageArrayLayers, supportedTransforms, currentTransform, supportedCompositeAlpha, supportedUsageFlags;
    } caps{};
    if (d.GetPhysicalDeviceSurfaceCapabilitiesKHR(d.phys, d.surface, &caps) != 0) return 0;
    uint32_t w = caps.currentExtent.w;
    uint32_t h = caps.currentExtent.h;
    if (w == 0xFFFFFFFFu) {
        w = (uint32_t)wantW;
        h = (uint32_t)wantH;
        if (w < caps.minImageExtent.w) w = caps.minImageExtent.w;
        if (h < caps.minImageExtent.h) h = caps.minImageExtent.h;
        if (w > caps.maxImageExtent.w) w = caps.maxImageExtent.w;
        if (h > caps.maxImageExtent.h) h = caps.maxImageExtent.h;
    }
    if (w == 0 || h == 0) return 0;

    uint32_t fc = 0;
    d.GetPhysicalDeviceSurfaceFormatsKHR(d.phys, d.surface, &fc, nullptr);
    if (!fc) return 0;
    std::vector<uint32_t> formats((size_t)fc * 2);
    if (d.GetPhysicalDeviceSurfaceFormatsKHR(d.phys, d.surface, &fc, formats.data()) != 0) return 0;
    uint32_t fmt = formats[0];
    for (uint32_t i = 0; i < fc; ++i) if (formats[i * 2] == 44) { fmt = 44; break; }
    d.colorFmt = fmt;

    uint32_t mc = 0;
    d.GetPhysicalDeviceSurfacePresentModesKHR(d.phys, d.surface, &mc, nullptr);
    std::vector<uint32_t> modes(mc ? mc : 1);
    if (mc) d.GetPhysicalDeviceSurfacePresentModesKHR(d.phys, d.surface, &mc, modes.data());
    uint32_t mode = 2;
    if (!d.vsync) {
        for (uint32_t m : modes) if (m == 0) { mode = 0; break; }
    }

    uint32_t count = caps.minImageCount < 2 ? 2 : caps.minImageCount;
    if (caps.maxImageCount && count > caps.maxImageCount) count = caps.maxImageCount;
    uint32_t alpha = 1;
    if ((caps.supportedCompositeAlpha & 1u) == 0) {
        if (caps.supportedCompositeAlpha & 2u) alpha = 2;
        else if (caps.supportedCompositeAlpha & 4u) alpha = 4;
        else alpha = 8;
    }

    struct Sci {
        uint32_t sType; const void* pNext; uint32_t flags; uint64_t surface; uint32_t minCount; uint32_t format; uint32_t space;
        Ext extent; uint32_t layers, usage, sharing, qcount; const void* qidx; uint32_t pre, composite, present, clipped; uint64_t old;
    } sci{};
    sci.sType = 1000001000; sci.surface = d.surface; sci.minCount = count; sci.format = fmt; sci.extent = { w, h };
    sci.layers = 1; sci.usage = 0x10 | 0x1; sci.pre = caps.currentTransform ? caps.currentTransform : 1; sci.composite = alpha; sci.present = mode; sci.clipped = 1;
    if (d.CreateSwapchainKHR(d.device, &sci, nullptr, &d.swapchain) != 0) return 0;

    uint32_t ic = 0;
    d.GetSwapchainImagesKHR(d.device, d.swapchain, &ic, nullptr);
    d.images.resize(ic);
    if (!ic || d.GetSwapchainImagesKHR(d.device, d.swapchain, &ic, d.images.data()) != 0) return 0;
    d.views.resize(ic);
    for (uint32_t i = 0; i < ic; ++i) if (!make_view(d, d.images[i], fmt, 1, &d.views[i])) return 0;
    if (!make_depth(d, w, h)) return 0;

    struct Att { uint32_t flags, format, samples, load, store, sload, sstore, initial, final; };
    Att atts[2] = {};
    atts[0].format = fmt; atts[0].samples = 1; atts[0].load = 1; atts[0].final = 1000001002;
    atts[1].format = d.depthFmt; atts[1].samples = 1; atts[1].load = 1; atts[1].final = 3;
    struct Ref { uint32_t attachment, layout; };
    Ref color{ 0, 2 };
    Ref depth{ 1, 3 };
    struct Sub { uint32_t flags, bind, inCount; const void* in; uint32_t colorCount; const Ref* colors; const void* resolve; const Ref* depth; uint32_t preserve; const void* p; } sub{};
    sub.bind = 0; sub.colorCount = 1; sub.colors = &color; sub.depth = &depth;
    struct Dep { uint32_t src, dst, srcStage, dstStage, srcAccess, dstAccess, flags; } dep{};
    dep.src = ~0u; dep.dst = 0; dep.srcStage = 0x400 | 0x100; dep.dstStage = 0x400 | 0x100; dep.dstAccess = 0x100 | 0x400;
    struct RP { uint32_t sType; const void* pNext; uint32_t flags; uint32_t ac; const Att* a; uint32_t sc; const Sub* s; uint32_t dc; const Dep* d; } rp{};
    rp.sType = 38; rp.ac = 2; rp.a = atts; rp.sc = 1; rp.s = &sub; rp.dc = 1; rp.d = &dep;
    if (d.pass) { d.DestroyRenderPass(d.device, d.pass, nullptr); d.pass = 0; }
    if (d.CreateRenderPass(d.device, &rp, nullptr, &d.pass) != 0) return 0;

    Att oat{};
    oat.format = fmt; oat.samples = 1; oat.load = 0; oat.store = 0; oat.sload = 2; oat.sstore = 1; oat.initial = 1000001002; oat.final = 1000001002;
    Ref oc{ 0, 2 };
    struct Sub2 { uint32_t flags, bind, inCount; const void* in; uint32_t colorCount; const Ref* colors; const void* resolve; const void* depth; uint32_t preserve; const void* p; } sub2{};
    sub2.colorCount = 1; sub2.colors = &oc;
    struct RP2 { uint32_t sType; const void* pNext; uint32_t flags; uint32_t ac; const Att* a; uint32_t sc; const Sub2* s; uint32_t dc; const void* d; } rp2{};
    rp2.sType = 38; rp2.ac = 1; rp2.a = &oat; rp2.sc = 1; rp2.s = &sub2;
    if (d.passOv) { d.DestroyRenderPass(d.device, d.passOv, nullptr); d.passOv = 0; }
    if (d.CreateRenderPass(d.device, &rp2, nullptr, &d.passOv) != 0) return 0;

    if (d.pipeTri) { d.DestroyPipeline(d.device, d.pipeTri, nullptr); d.pipeTri = 0; }
    if (d.pipeTwo) { d.DestroyPipeline(d.device, d.pipeTwo, nullptr); d.pipeTwo = 0; }
    if (d.pipeLine) { d.DestroyPipeline(d.device, d.pipeLine, nullptr); d.pipeLine = 0; }
    if (d.pipeOv) { d.DestroyPipeline(d.device, d.pipeOv, nullptr); d.pipeOv = 0; }
    if (!make_pipe(d, d.shVs, d.shFs, 3, 2, 1, 0, d.pass, 24, 0, d.layout, &d.pipeTri)) return 0;
    if (!make_pipe(d, d.shVs, d.shFs, 3, 0, 1, 0, d.pass, 24, 0, d.layout, &d.pipeTwo)) return 0;
    if (!make_pipe(d, d.shVs, d.shFs, 1, 0, 1, 0, d.pass, 24, 0, d.layout, &d.pipeLine)) return 0;
    if (!make_pipe(d, d.shOvVs, d.shOvFs, 3, 0, 0, 1, d.passOv, 16, 1, d.layoutOv, &d.pipeOv)) return 0;
// [nexa:vkshader-begin]
    // A custom shader's pipelines belong to the render pass as much as the
    // built-in ones do, so a resize rebuilds them alongside.
    if (d.shCustom[0] && d.shCustom[1]) {
        if (d.pipeCustomTri) { d.DestroyPipeline(d.device, d.pipeCustomTri, nullptr); d.pipeCustomTri = 0; }
        if (d.pipeCustomTwo) { d.DestroyPipeline(d.device, d.pipeCustomTwo, nullptr); d.pipeCustomTwo = 0; }
        if (d.pipeCustomLine) { d.DestroyPipeline(d.device, d.pipeCustomLine, nullptr); d.pipeCustomLine = 0; }
        if (!make_pipe(d, d.shCustom[0], d.shCustom[1], 3, 2, 1, 0, d.pass, 24, 0, d.layout, &d.pipeCustomTri) ||
            !make_pipe(d, d.shCustom[0], d.shCustom[1], 3, 0, 1, 0, d.pass, 24, 0, d.layout, &d.pipeCustomTwo) ||
            !make_pipe(d, d.shCustom[0], d.shCustom[1], 1, 0, 1, 0, d.pass, 24, 0, d.layout, &d.pipeCustomLine)) {
            return 0;
        }
        // scene_swapchain rebuilds the textured pair on its own, since those
        // pipelines belong to the swapchain it owns.
    }
// [nexa:vkshader-end]

    d.fbs.resize(ic);
    d.ovFbs.resize(ic);
    for (uint32_t i = 0; i < ic; ++i) {
        uint64_t att[2] = { d.views[i], d.depthView };
        struct FB { uint32_t sType; const void* pNext; uint32_t flags; uint64_t pass; uint32_t n; const uint64_t* a; uint32_t w, h, layers; } fb{};
        fb.sType = 37; fb.pass = d.pass; fb.n = 2; fb.a = att; fb.w = w; fb.h = h; fb.layers = 1;
        if (d.CreateFramebuffer(d.device, &fb, nullptr, &d.fbs[i]) != 0) return 0;
        struct FB2 { uint32_t sType; const void* pNext; uint32_t flags; uint64_t pass; uint32_t n; const uint64_t* a; uint32_t w, h, layers; } fb2{};
        fb2.sType = 37; fb2.pass = d.passOv; fb2.n = 1; fb2.a = &d.views[i]; fb2.w = w; fb2.h = h; fb2.layers = 1;
        if (d.CreateFramebuffer(d.device, &fb2, nullptr, &d.ovFbs[i]) != 0) return 0;
    }
    d.swapW = (int)w;
    d.swapH = (int)h;
    d.dirty = 0;
    scene_swapchain(d);
    return 1;
}

inline void set_viewport(Dev& d) {
    struct Vp { float x, y, w, h, mind, maxd; } vp{};
    vp.y = (float)d.swapH;
    vp.w = (float)d.swapW;
    vp.h = -(float)d.swapH;
    vp.maxd = 1.0f;
    d.CmdSetViewport(d.cmd, 0, 1, &vp);
    struct Sc { int32_t x, y; uint32_t w, h; } sc{};
    sc.w = (uint32_t)d.swapW;
    sc.h = (uint32_t)d.swapH;
    d.CmdSetScissor(d.cmd, 0, 1, &sc);
}

inline int ensure_overlay(Dev& d, int w, int h) {
    if (w < 1 || h < 1) return 0;
    uint64_t bytes = (uint64_t)w * (uint64_t)h * 4;
    if (d.ovW == w && d.ovH == h && d.ovImg && d.ovStageCap >= bytes) return 1;
    if (d.ovView && d.DestroyImageView) d.DestroyImageView(d.device, d.ovView, nullptr);
    d.ovView = 0;
    if (d.ovImg && d.DestroyImage) d.DestroyImage(d.device, d.ovImg, nullptr);
    d.ovImg = 0;
    if (d.ovMem && d.FreeMemory) d.FreeMemory(d.device, d.ovMem, nullptr);
    d.ovMem = 0;
    if (d.ovMap && d.UnmapMemory) d.UnmapMemory(d.device, d.ovStageMem);
    d.ovMap = nullptr;
    if (d.ovStage && d.DestroyBuffer) d.DestroyBuffer(d.device, d.ovStage, nullptr);
    d.ovStage = 0;
    if (d.ovStageMem && d.FreeMemory) d.FreeMemory(d.device, d.ovStageMem, nullptr);
    d.ovStageMem = 0;
    d.ovW = d.ovH = 0;

    struct Img { uint32_t sType; const void* pNext; uint32_t flags; uint32_t type; uint32_t format; uint32_t ew, eh, ed; uint32_t mips, layers, samples, tiling, usage, sharing, qcount; const void* q; uint32_t initial; } img{};
    img.sType = 14; img.type = 1; img.format = 44; img.ew = (uint32_t)w; img.eh = (uint32_t)h; img.ed = 1; img.mips = 1; img.layers = 1; img.samples = 1; img.usage = 0x2 | 0x4;
    if (d.CreateImage(d.device, &img, nullptr, &d.ovImg) != 0) return 0;
    if (!image_mem(d, d.ovImg, 1u, &d.ovMem)) return 0;
    if (!make_view(d, d.ovImg, 44, 1, &d.ovView)) return 0;
    if (!buffer_alloc(d, bytes, 0x01, &d.ovStage, &d.ovStageMem, &d.ovMap)) return 0;
    d.ovStageCap = bytes;
    d.ovW = w;
    d.ovH = h;

    struct II { uint64_t sampler, view; uint32_t layout; };
    struct Wr { uint32_t sType; const void* pNext; uint64_t set; uint32_t binding, element, count, type; const II* image; const void* buf; const void* texel; } wr{};
    II ii{ d.sampler, d.ovView, 5 };
    wr.sType = 35; wr.set = d.descSet; wr.count = 1; wr.type = 1; wr.image = &ii;
    d.UpdateDescriptorSets(d.device, 1, &wr, 0, nullptr);
    return 1;
}

inline void draw_overlay(Dev& d, const void* bgra, int w, int h) {
    if (!bgra || w < 1 || h < 1 || !d.cmd) return;
    if (!ensure_overlay(d, w, h)) return;
    uint64_t bytes = (uint64_t)w * (uint64_t)h * 4;
    std::memcpy(d.ovMap, bgra, (size_t)bytes);

    struct Range { uint32_t aspect, mip, levels, layer, layers; };
    struct Barr { uint32_t sType; const void* pNext; uint32_t srcA, dstA, oldL, newL, srcQ, dstQ; uint64_t image; Range range; };
    Barr b{};
    b.sType = 45; b.oldL = 0; b.newL = 7; b.srcQ = ~0u; b.dstQ = ~0u; b.image = d.ovImg; b.dstA = 0x1000; b.range = { 1, 0, 1, 0, 1 };
    d.CmdPipelineBarrier(d.cmd, 0x1, 0x1000, 0, 0, nullptr, 0, nullptr, 1, &b);

    struct Subr { uint32_t aspect, mip, layer, count; };
    struct Copy { uint64_t off; uint32_t row, height; Subr sub; int32_t x, y, z; uint32_t w, h, d; } copy{};
    copy.sub = { 1, 0, 0, 1 }; copy.w = (uint32_t)w; copy.h = (uint32_t)h; copy.d = 1;
    d.CmdCopyBufferToImage(d.cmd, d.ovStage, d.ovImg, 7, 1, &copy);

    b.srcA = 0x1000; b.dstA = 0x20; b.oldL = 7; b.newL = 5;
    d.CmdPipelineBarrier(d.cmd, 0x1000, 0x80, 0, 0, nullptr, 0, nullptr, 1, &b);

    struct Clear { float c[4]; };
    struct Off { int32_t x, y; };
    struct Ext { uint32_t w, h; };
    struct Area { Off o; Ext e; };
    struct Begin { uint32_t sType; const void* pNext; uint64_t pass, fb; Area area; uint32_t cc; const Clear* c; } begin{};
    begin.sType = 43; begin.pass = d.passOv; begin.fb = d.ovFbs[d.imageIndex];
    begin.area.e = { (uint32_t)d.swapW, (uint32_t)d.swapH };
    d.CmdBeginRenderPass(d.cmd, &begin, 0);
    set_viewport(d);
    d.CmdBindPipeline(d.cmd, 0, d.pipeOv);
    d.CmdBindDescriptorSets(d.cmd, 0, d.layoutOv, 0, 1, &d.descSet, 0, nullptr);
    uint64_t off = 0;
    d.CmdBindVertexBuffers(d.cmd, 0, 1, &d.quad, &off);
    d.CmdDraw(d.cmd, 6, 1, 0, 0);
    d.CmdEndRenderPass(d.cmd);
}

// The renderer's seventeen shaders, compiled from include/vk14 and packed by
// tools/pack_vk14_spv.py: debug names out, the words split into byte planes,
// DEFLATEd. 12476 words of SPIR-V in 12129 bytes. Edit the GLSL and repack; this
// file is written, not edited. Needs __nexa_inflate (InflateRuntime.hpp),
// which NexaC writes ahead of the renderer.
static const unsigned char kSpvZ[12129 + 1] =
"\xed\x5c\x09\x7c\x13\xc5\xf7\xdf\x4d\xd2\xd9\xa6\xc9\x36\x6d\x93\x36\x4d\x9b\xa4\x9b\x16\x7a\x6d\x28\x85\x96\x1e\x2c\xb3\x74\xa1\xa5\xe5\x6a\x52\x8e\x96\x72\x96"
"\xbb\xdc\xf7\x51\xee\x52\x0b\xe5\x2a\xa2\xa8\x78\x20\x02\xa2\x5c\xa2\x02\x2a\x56\x04\xbc\x11\x01\x6f\x44\x45\xbc\x2f\x7e\x2a\xa2\x62\x93\xaa\xf8\x7f\x9b\x12\x48"
"\x93\xf4\xe0\xf0\xf8\xfd\x7f\x19\x78\xd9\x9d\xdd\x99\x37\x6f\xde\xcc\xbc\xf9\xbe\x37\xfb\xa9\x94\x50\x54\x13\xc1\xa4\x82\xcc\x4d\x4a\x22\x02\x09\x52\x45\xc8\x26"
"\x11\x27\x4f\x5f\xa2\x4c\xed\x32\x39\xcc\xf7\xcc\x6d\x85\x82\x72\x5b\x03\xc5\x02\xc5\x01\x25\x48\xf2\x12\x88\x18\x22\x2f\x81\x14\x7f\x24\xe2\x8f\x54\xfc\x91\x89"
"\x3f\x7e\x31\x41\x79\x09\x28\x86\xc9\x65\x4d\x44\x2e\x1b\x4d\xe4\x6e\x95\xe4\x6d\x25\xfc\x80\xa8\x20\xf8\x81\x22\x5b\x49\xc8\x91\x62\x8e\x8c\xc9\xca\xdb\x2a\x81"
"\x9c\x44\xcc\x49\x62\x96\xe4\x6d\x95\x42\x4e\x2a\xe6\xa4\x31\x47\xf2\xb6\x8a\x1c\xb7\x8a\x1c\xb7\x02\xc7\xbc\xad\x54\x4c\x72\xee\x36\x13\x99\xbb\x0d\xd8\x9e\x34"
"\x12\xb9\xa7\x24\x79\xa7\x08\x05\x91\x77\x0a\x84\xcf\x3b\x25\x51\x48\xf3\x4e\x49\x15\xb2\xdc\x4b\x46\x59\x2e\x65\xf4\xcb\x35\x19\x51\x6e\x3b\xa3\x24\x37\x13\x8a"
"\x72\x46\x32\x17\xc3\x3d\x6f\x94\xe6\xf6\x34\x92\x6a\x89\x49\x2a\x09\x43\x8c\x96\x42\x52\xc6\x9f\xa2\x42\xe5\x0c\xc9\x04\x50\x72\x93\x82\xf2\xf7\xf7\x0f\x60\x91"
"\x9a\x30\x53\x1a\xb5\x5a\xcd\xca\xc3\x08\x56\x6e\x08\xd2\x18\xb5\x26\x24\x0b\x8d\x66\x08\x36\x3a\x26\x48\xdf\xca\x14\xa3\x6f\x0d\x14\x0b\x14\x67\x8a\xd1\xc6\xcb"
"\x65\xc6\x84\x56\xad\x63\xe3\x4c\xa6\x78\x26\x51\x92\xc0\x25\xb2\x12\x56\x6e\x46\x6c\x74\x1b\x82\x49\x92\xc8\x19\x8e\x42\x2c\xc2\x50\xb9\xb3\x94\xc9\x92\x20\x26"
"\x4f\x62\x62\x51\xf1\x6e\x33\x35\x44\xad\xc6\xac\x7c\xa4\x94\x8d\x1e\x2d\x61\xd1\x74\x68\xac\x4c\xc2\x46\xaf\x22\x59\xf9\x5a\x32\x7c\x8b\x49\x66\xdc\xba\x05\x92"
"\x09\x12\xf3\xa0\x64\x2b\xf7\xe0\x36\xe0\xfb\x10\xc5\xa2\x3d\x20\xe0\x33\x18\x63\xe6\x28\x65\x62\x8e\x49\xb6\x30\x27\x48\x8a\x3b\x71\x92\xd4\xbf\x8b\x56\x19\x4f"
"\x99\xd0\xbb\xef\x32\xef\x49\x4f\x71\xef\x9d\x96\x32\x9f\x4a\x4d\xac\xfc\x73\x3f\xed\x37\x48\xc2\x9c\x93\x22\xee\xd3\x4b\x52\xee\x1c\x25\x65\x51\xc0\x14\x16\x85"
"\x12\xdc\x39\x93\x94\x95\x47\xcb\xb8\x13\xed\x48\x26\x43\x4a\x71\x19\x99\x52\x2e\x83\x03\xc2\x40\xbc\x34\xbc\x2b\x25\xe5\x4e\xf4\x04\x89\x2c\x7f\xa6\x49\x64\x84"
"\xd4\xe6\xc7\x1d\x7d\x95\xe2\xb8\x40\xa0\x10\x8a\xf3\x4f\x04\x4a\x02\xca\x02\xb2\x02\xf5\x01\xea\x4f\x71\x01\xc5\x70\x1d\x0c\x34\x14\x68\x38\xe4\x47\xc3\x75\x1a"
"\xd0\x74\xa0\x59\x90\x2f\x83\xeb\x22\xa0\xc5\x40\x4b\x21\xbf\x8c\x12\x8e\xbd\xb6\x6d\x2d\xde\x72\xfc\x35\x4c\xbd\x7e\xb2\x00\xbd\xf1\x3a\x51\x80\xde\x7c\x9d\x2c"
"\x40\x6f\xbd\x2e\xb1\x9a\xde\x7e\xe3\xcd\xb7\xf0\x3a\xd3\x3b\xc7\xdf\xe6\x5f\x7d\x47\x38\xf6\xfe\xb6\x30\xbc\xe5\x83\xf7\x31\xf5\x21\x14\x3d\xf3\x21\x14\xfd\xe8"
"\x43\x28\x7a\xf6\x43\x28\xfa\xf1\x99\x8f\xce\x42\xd1\x4f\x3e\xf8\x58\xf8\xf4\xb3\xd3\x61\xfc\x67\x9f\x08\x59\x5f\x6c\xfb\xbc\x33\x46\x5f\x7e\x21\x64\x7d\xb5\xcd"
"\xdc\x06\xa3\xaf\xbf\xb2\x7e\xf3\xed\x97\x5f\x0b\xe7\xfe\x73\x3a\x0c\x5e\x7c\xf7\x9f\xea\x6f\xbe\xff\xf6\x3b\xe1\xd3\x1f\x4e\x87\x61\xd3\xf9\x1f\x2c\xdf\xfc\x78"
"\xfe\x3c\x41\x96\x7f\x73\xe1\xc7\xef\x85\x73\x3f\x9d\x0e\x6b\x53\x80\x7e\xbe\x40\xf0\x3f\xfd\x2c\x9c\xfb\xe5\x74\xd8\xaa\x02\x74\xf1\x02\xc9\xff\x72\x51\x38\xf7"
"\xeb\xe9\xb0\xd1\x18\xd5\xfe\x5a\x89\x6c\xb5\x7b\x84\x73\x76\x07\xbb\x3a\x7b\x25\xfa\xad\x6e\x4f\x39\xfa\xdd\xf6\x9b\x70\xee\x0f\x28\xc2\xff\xf1\xbb\x70\xec\xcf"
"\x6d\x65\x78\x0b\xf1\x27\xa6\x48\x10\x5a\x42\x82\xd0\x52\x12\x84\x96\x91\x20\xb4\x9f\x44\x2a\x03\xa1\x11\xe1\xc7\x5f\x42\x3c\xa5\x16\xb2\xfc\x59\x51\x64\xb9\x7f"
"\x8d\x51\x21\x0f\xa8\xa5\x89\x3a\x85\x92\xb6\x29\xb1\x49\xf5\xaa\x85\x0a\x52\xa9\x08\x52\xa2\x44\xc1\x64\x97\x20\x3e\x30\x58\xc8\x52\x3b\x0a\x6b\xd4\x95\x28\x4c"
"\x13\xca\x87\x84\x61\xa4\x0d\xc4\x28\x3c\xa4\x02\xe9\xb4\xe1\x42\x56\x84\xe3\x75\x64\x04\x46\x7a\x78\x64\x88\xd4\x2b\x91\x91\x8c\x37\x04\x54\xa1\x28\x9d\x51\x89"
"\x18\x92\x8d\x52\x63\x9e\x62\xec\xb4\x8d\x16\xb2\x62\xb6\x45\x43\xe9\x56\x31\xbc\xa9\x95\x90\xd5\x7a\xdb\xe7\xa0\xaf\xd8\xd6\x07\x8c\x71\xb1\x7b\x6a\x13\x88\xba"
"\xb8\xf8\xce\xb6\x78\x21\x8b\xdd\xf6\xf9\x2a\x8c\xcc\x6c\x8d\xb1\x8d\x79\x4f\x6d\x32\x51\xd7\xa6\x6d\x8a\xad\x2d\xa6\xda\xb7\xe3\x93\xda\xdb\x93\x6d\x29\x42\x5e"
"\xea\xb6\x68\x6c\xea\x90\x6a\xa1\xd2\x3a\x74\x00\x71\xf9\xa4\x34\x78\x9e\x8c\xa9\xf4\x24\x3e\x31\x1d\x53\x1d\x13\xf9\xcc\x8e\x98\xea\x94\xc8\x73\x9d\x78\xac\xe1"
"\x79\x8d\x3d\xc1\xd6\x59\x38\x26\x6c\x1b\x89\xb7\x74\x11\x0a\x4c\xd9\x5d\x08\x0b\x95\x93\x9d\x0d\x75\x0b\x4c\xdd\xba\x90\x16\x2a\xb7\x5b\x37\x47\x26\xaf\x8b\xc4"
"\x42\x75\xcf\xcb\x83\x8c\xb5\x6b\x8f\x9c\xdc\xee\x98\xea\xd5\x73\x1d\xd5\xbb\x47\x2f\x25\x95\x4f\xe6\xf4\xe6\xb3\xf2\x31\x55\x90\xc5\x5b\x0b\xb0\xa9\x2f\xe8\xab"
"\x5f\xdf\xbe\xa2\x00\x7d\xfa\x09\x79\x85\xa2\x50\x45\x85\x16\x6a\x40\x51\x91\xf8\xac\xff\x00\xbe\xd8\x92\x49\x0d\x0c\xb2\xf6\xe9\x5f\xcc\x67\x0e\xc4\xd4\xa0\x4c"
"\x4c\x0d\xc9\xe2\x07\x0f\xc1\xa6\x61\x50\xb7\x64\xd8\x30\xb1\xdc\xd0\x12\x21\x6f\x84\x58\x77\xe4\x08\x0b\x35\x6a\xe4\x48\xf1\xd9\xf0\x51\x42\xd2\x18\xd6\xbc\x0a"
"\xcb\xc7\x8e\xe1\x47\x8f\xcd\xa4\x4a\x83\x06\x0f\x1d\x3e\xba\x82\x1a\x37\xa8\x94\xc7\xe3\x84\xac\xf1\xdb\x3e\x87\xd9\x31\x61\x7c\x8d\x71\xe2\x84\x3d\xb5\x93\x89"
"\xba\x89\x93\x56\xd9\x26\x61\x6a\x4a\xd6\x62\x6a\xea\x14\x7e\xda\x54\x6c\x9a\x01\x4d\xcc\x9c\x31\x43\x64\x37\x7d\xa6\x90\x37\x5b\x6c\x62\xce\x6c\x0b\x35\x77\xce"
"\x1c\xf1\xd9\xac\xb9\x7c\x19\x88\x37\x2f\x68\xda\xf4\x59\x65\x3c\x37\x0f\x53\xf3\x39\x4c\x2d\x80\xfa\x0b\x17\xf0\x8b\x16\x62\xd3\x12\xa8\x5f\xbe\x64\x89\x58\x76"
"\x71\xb9\x90\x57\x21\xd6\xbf\xa5\xc2\x42\x55\xde\x72\x8b\xf8\x6c\x69\xa5\x90\xb4\xdc\x21\x62\xd5\x72\x7e\x59\x55\x26\xb5\x22\x68\xd1\xe2\xa5\xcb\x2a\xa8\x95\xf3"
"\x57\xf0\xfc\x4a\xfb\x64\xdb\x2a\x4c\xad\xce\xe4\xb9\xd5\x98\x5a\x83\x79\x7e\x0d\x3c\x99\x0c\xe3\x90\xf0\x47\x46\x1a\x15\x44\x28\xd2\xfd\x95\xe9\xfe\x74\xba\x7f"
"\x60\x7a\x80\xca\x16\xcc\xf9\xc3\x9a\x0f\x08\x85\xb5\xdf\x09\xd6\x6d\x37\xa0\x7c\xb8\x87\xf5\xce\x4d\x06\x9a\x0d\xf9\xcd\x40\x7b\x29\x3e\x44\xc3\x87\x86\xd9\xb5"
"\x36\xed\xaf\xba\x08\xc2\x1e\x69\x8b\xc4\x72\x7d\xe8\xe3\xc6\x28\xbd\xa1\x2e\x2a\x5c\x67\x0b\xc7\x72\x26\x54\x48\x6a\xcb\xc2\x5a\x94\x27\xb7\x7d\xd4\xd8\x8e\x49"
"\xae\x4d\x21\xea\xda\xb5\x4f\xb1\xb5\xb7\xeb\x6c\x29\x58\xde\x21\x14\xcb\xd3\x54\x3b\x8d\xe9\x1d\xd2\x6a\x33\x89\xba\xf4\x8c\x4c\x5b\x86\x3d\xc2\x96\xc9\x77\xc2"
"\x58\xce\x87\x0a\x59\x02\x1b\xc6\xc3\x34\xed\x22\xec\x37\x76\xed\xa2\xae\xcd\x21\xea\xba\x66\xf7\xb6\x65\x63\x79\x6e\xa8\x90\xd7\x9d\x0d\xcb\xc5\xa6\x1e\xdd\x2d"
"\x54\xcf\x1e\x3d\xc4\xa5\x42\xf5\x22\x73\x7a\xf2\xdd\x7a\xd9\x73\x6c\xbd\xb1\xdc\x02\x45\xac\x6c\x98\x05\x9b\x0a\xac\x16\xaa\x4f\x41\x01\x14\xc1\x54\x5f\xba\x82"
"\xea\xd7\xa7\x2f\x9f\xdf\x0f\x53\x85\xf9\x4a\x54\x44\x76\x29\xe4\xfb\x17\x61\x34\xa0\xff\x01\xe3\xc0\x01\xc5\xb5\x83\x89\xba\x81\x83\x86\xda\x06\xf1\xdd\x86\xd8"
"\x07\xdb\x86\x62\x6a\x58\x3e\x46\x25\xfd\xad\xd4\xf0\x92\x92\x92\x2a\x6a\xc4\xb0\xe1\x7c\xb7\x11\x58\x3e\x0a\xa4\x1b\xc3\x8e\x1c\x05\x43\x3f\x76\x0c\x46\xa5\xfd"
"\x2b\xd1\xb8\xb1\xa5\x18\x8d\x87\x9b\x09\xe3\xc6\x97\xa3\x89\x78\x42\x15\x9a\x84\x27\xf2\x9d\x26\x01\x9f\xc1\x20\x53\x0e\x3f\x19\xba\x35\x05\x2a\x4e\x65\x47\x4e"
"\x81\x05\x37\x6d\xea\x01\xe3\x8c\x69\xd3\x6b\x67\x11\x75\x33\x66\xce\xb2\xcd\xc4\xd4\x9c\x6e\x8b\x61\x52\x60\xf9\x3c\x90\x7d\x3e\x5b\x36\x0f\x9b\x16\xcc\xb7\xc0"
"\x2c\x58\xe0\xe8\xde\x22\x32\x67\xe1\x7a\xb4\x78\xee\x22\x7e\xf6\x62\x8c\x96\xcc\xc6\xf2\x72\x60\xb6\x94\x1d\x59\x0e\xcc\x2a\x96\x1e\x30\xde\xb2\xa4\xa2\x76\x19"
"\x51\x77\x4b\xe5\x72\x5b\x25\x3f\x59\x6d\x5f\x66\x5b\x8e\x51\xd5\x6c\x25\x5a\x41\xc6\x57\xa9\xb1\x7c\x25\x14\x5f\xcd\x8e\x5c\x09\x8b\x7a\xcd\x6a\x25\xaa\x26\x23"
"\x56\xac\xe1\x27\x57\x43\xb9\x65\xf6\x59\xb6\x59\x58\x7e\x2b\xb4\xbb\x8e\x5d\x7b\x2b\x36\xdd\xb6\xce\x42\xdd\x7e\xdb\x6d\x0e\x9d\xad\x57\x62\xea\x8e\x6e\xeb\xd1"
"\x9d\xeb\xef\x50\xa2\xbb\xc8\xf8\x3b\xd5\xd5\xd4\x86\xdb\xef\xc2\xe8\xee\x4e\xd5\xd4\x3d\x1b\xee\xc6\xe8\xde\xc9\xd5\xd4\x7d\xf7\xdc\x8b\xa9\x8d\x21\xe5\xd4\xfd"
"\x1b\xef\xe3\x43\xee\x87\x71\x8c\xc0\xf2\x4d\xa1\x4b\xe4\x0f\x6c\x5a\xcb\x87\x3e\x00\x13\x45\x27\xe4\x3d\xbc\xed\x21\x6c\xda\xfe\xb0\x85\xda\xb1\x7d\xbb\x83\xf7"
"\x4e\xa8\xb0\x6b\xc7\x4e\x7e\xf3\x2e\x21\x6b\xf7\xb6\x87\x60\xb0\x1f\xd9\x5d\x63\x7c\xf4\x91\x3d\xb5\x8f\x13\x75\x8f\x3e\xf6\xb8\xed\x31\x4c\xed\x0b\xb1\x52\xfb"
"\x61\xaf\xb6\x52\x4f\xc0\x86\xa8\xa4\x9e\x24\xd9\x7d\xfb\x9f\xe0\xf7\x3e\x89\xa9\xa7\xf6\x0a\x79\x07\x44\x9e\x4f\x1f\xb0\x50\x35\x4f\x3f\xed\xe0\x79\x70\x6f\x05"
"\xf5\xec\x33\x07\x2b\xa9\x43\x35\xcf\x96\x53\x87\x9f\x3a\xc4\x6f\x3e\x6c\x7f\xdc\xf6\x38\xa6\x8e\x6c\xc6\xd4\x73\x81\x95\xd4\xf3\x47\x9e\xb3\x52\x2f\x38\x58\xbe"
"\xe8\x60\xf9\x12\xc9\x3e\xff\xc2\x8b\x97\x5e\xca\x90\x12\x8a\x4b\x0d\xc0\x92\x0c\xc0\xd2\xd0\xfd\x4f\x3d\x7d\xe8\xc8\xb1\x77\xbe\x3a\x77\x21\x48\x46\xe5\x0e\x35"
"\x49\x72\x87\x02\x44\xd9\x10\x9e\xbb\x5f\x11\x9c\xfb\x14\xc0\x8f\xa7\x01\x7e\x1c\x02\xf8\x71\x04\xe0\xc7\x0b\x00\xa1\x5e\x04\x7a\x09\xe8\x65\xa0\xa3\x92\xbc\xa3"
"\x22\x2e\x3a\x2a\xc2\xa9\xa3\x22\x9c\x3a\x2a\xc2\xa9\xa3\x22\xf8\x39\x2a\x82\x9f\xa3\x22\x9c\x3a\x06\x70\xea\x18\x30\x7d\x07\xc0\xcd\x57\x00\x6e\xce\x01\xb8\xb9"
"\x60\x24\x5c\x80\x8c\x4c\x04\x32\x26\x39\xf2\x0f\xa5\x01\xa5\xd0\x81\x52\x46\x25\x82\x8e\x10\x42\xa3\x66\x91\x96\xd0\xea\x00\xea\x44\x50\x3a\x16\x45\xc3\xdb\x58"
"\x09\x4b\xb7\x81\x6b\x27\x92\x45\xbd\x89\xd0\x7c\x80\x3f\x16\x2a\x9f\xcd\x2f\x20\xd8\xfc\x01\x41\xba\x41\x88\x24\x08\x82\x24\x22\x07\x0f\x62\x86\x10\x83\xb9\x21"
"\x43\x09\x6d\x09\x82\x3a\x23\x83\xf4\xa3\x4a\x46\xb2\x68\x74\x1d\x8b\xc6\x7c\x62\x2e\x19\x3b\x7a\x0c\x8b\x4a\x7f\x60\xd1\xb8\x48\x73\xc9\xf8\xd2\x71\x2c\x9a\xb0"
"\x81\x45\x13\x27\x98\x4b\x26\x4d\x98\xc8\xa2\xc9\xb7\xb0\x68\x0a\x63\x2e\x99\x3a\x19\xa0\xc6\xb4\x45\x00\x70\xaa\xcc\x25\x30\xa7\x59\x34\xf3\x1d\x16\xcd\xb2\x9a"
"\x4b\x66\xcf\x9c\xc5\xa2\x39\xad\x58\x34\x77\x83\xb9\xa4\x6c\xce\x5c\x16\xcd\x2b\x61\xd1\xfc\xef\xcc\x25\x0b\xe6\xcd\x67\xd1\x42\x78\xb0\xc8\x6c\x2e\x59\xbc\x10"
"\x2a\x2f\x21\x58\x54\xde\xc3\x5c\xb2\x74\x49\x39\x8b\x2a\xfc\x59\x74\x0b\x32\x97\x54\x56\x40\x1b\xcb\x3e\x63\xd1\xf2\x15\xe6\x92\xaa\x65\xcb\x59\xb4\x02\x24\x58"
"\xb9\xdd\x5c\xb2\x6a\xc5\x4a\x16\xad\x86\x76\xd6\xac\x34\x97\x54\xaf\x5e\xc3\xa2\xb5\xc9\x2c\xba\xb5\xd4\x5c\xb2\x6e\xed\xad\x2c\xba\x0d\x8a\xdd\x5e\x62\x2e\x59"
"\x7f\xdb\xed\xe6\x51\x77\x8c\x1d\x3f\x69\xea\x8c\xd9\x65\x0b\x16\x2f\xad\xac\x5a\x55\xbd\x6e\x3d\x73\x17\x35\x8a\xb9\x9b\x02\x59\xb6\xf6\x61\xf3\x77\x91\x22\xe2"
"\x62\xf6\x91\x6a\x6e\xdf\x7e\x92\x79\x92\xd4\x71\x4f\x3e\x45\x72\x4f\x3e\x0d\x74\x08\xe8\x08\xa9\x7f\x81\x1a\xa9\x7f\x11\xe8\x25\xa0\x97\xa9\x91\xda\x57\xf2\x65"
"\xc6\xa3\x2f\xbc\xf8\xd2\xcb\x14\xf5\x0a\xf3\xaa\xe4\x28\xf7\xea\x31\x09\x9b\xff\x1a\x62\x8e\x4b\xf2\x99\xb7\x01\xac\xbd\xfd\x0e\xc9\xe6\x7f\xe2\xc7\x7c\x2a\xa1"
"\x98\x2f\x49\xc4\x7d\xf9\x15\xc9\x7d\x79\x0e\x1a\xfa\x8e\x60\x7e\x04\x18\xf6\xe3\x05\xa9\x13\x6e\x45\xec\xa5\xb8\x88\x83\x60\x56\xdf\xa5\xb0\xfa\x89\xfd\x58\x77"
"\xe0\x29\xac\xab\x79\x7a\x87\xee\x99\x27\x0e\xd4\xf0\x7b\x9f\xc1\xea\x67\xe1\xe1\xe1\x43\x58\xf7\xdc\x91\x1d\xba\xe7\x9f\x3d\xfc\x1c\x7f\xf0\x79\xe1\xf8\x89\x63"
"\xaf\xc5\xe2\xfc\x93\x27\x1e\x51\x03\x9a\xaa\x7d\x93\xa8\x7b\xfd\x8d\x37\x6d\x6f\x60\xdd\x5b\x07\x31\x75\xea\x1d\xfe\xdd\x53\x99\xe8\x3d\xc5\xbb\x15\xe8\xb4\xf6"
"\xbd\x6a\xdd\xfb\x6f\x9d\xc6\xba\x0f\xf6\x56\xe8\x3e\xfc\xe0\x7d\x7e\xef\x87\xf6\x37\x6d\x6f\x62\xdd\x99\xbd\x56\xdd\x47\x21\x21\x21\x56\xdd\x59\xad\x56\xab\xd4"
"\x7d\x4c\xb2\x80\xac\x00\x53\x1d\xfb\x04\x53\x9f\x7f\x66\xd1\x7d\xf1\xf9\xe7\xe2\x9a\x12\xc1\x94\xee\x9b\xaf\xbf\xfe\x5a\xa9\xfb\x96\x4c\xfa\xf8\x8b\x6f\xf8\xbd"
"\xdf\x62\xf4\x9f\x73\x35\xea\xef\xff\xf3\x5d\xed\x79\xa2\xee\xfb\x1f\xce\xdb\x7e\xc0\xba\x9f\x9e\x02\x10\xf5\x13\xa0\x9e\x5f\x7e\x02\xd4\x73\xf1\x27\x89\x95\xfa"
"\xf5\xe7\x5f\x2e\x6a\xf9\x0b\xbf\xfe\x61\x3b\x8f\x75\xb6\xbd\x18\xd9\xcf\x15\xa0\x3a\x1b\x94\xf9\xcd\x06\x65\x7e\xb7\x41\x99\x3f\xea\x7e\xfb\xdd\xce\x5f\xf8\x03"
"\x36\x24\xa4\x20\xe4\xe9\xfe\x01\x36\x25\x17\x11\x49\x71\xaa\x9e\x40\xbd\x28\xce\x02\xb8\x53\x35\x90\xe2\xee\xda\x40\xdd\x21\xa8\x82\x02\x00\xf8\x04\x07\x1d\x54"
"\x6b\x82\x43\x6a\xc3\x88\x3a\x4d\x68\x98\x2d\xf4\x92\xd6\x16\x86\x29\x7d\x80\x45\x67\xd0\xeb\x41\x66\x41\x65\x14\x8b\x45\x19\xad\x3a\x26\x2a\x2a\xaa\x4a\x67\x32"
"\x30\xd5\xba\x18\x53\xb4\x55\xd7\x2a\x3a\x3a\xba\x5c\xd7\x3a\xa6\x15\x1f\xd9\x5a\x50\xc5\x45\xc6\x62\x14\x1f\x57\xa3\x4e\x88\xd7\x6e\x57\x27\x26\xd4\x9a\x89\xba"
"\x44\xd6\x6c\x63\x05\x55\x52\x24\x58\xd6\xb6\x49\x07\xd4\xc9\x6d\x43\xec\x66\x9b\xf9\xa2\xba\x5d\x42\x58\x32\xbb\x5d\xdd\xbe\x5d\x6d\x2a\x51\xd7\x3e\x25\x15\x30"
"\x90\xaa\x83\x58\x2a\xad\x43\x8d\x3a\x3d\x4d\x6b\x4f\xb5\xa5\x5e\x54\x67\xb4\x33\xa7\xa7\x6c\x57\x67\x66\xd4\x72\x44\x5d\x66\x47\xce\xd6\x51\x50\xe1\xc8\x4e\x18"
"\xf1\xf8\x80\xba\x33\x1f\x62\xe7\x6c\xdc\x45\x75\x56\x46\x6a\xe7\x8e\xdb\x01\x06\xd6\x76\x25\xea\x84\x2e\x5d\x6d\x5d\x04\x55\xb6\x58\x2a\x27\xbb\x46\xdd\x2d\x47"
"\x6b\xef\x6a\xeb\x7a\x51\x9d\x9b\xc5\x75\xeb\x52\xdb\x9d\xa8\xcb\xcd\xeb\x6e\xcb\x83\x5e\x76\xe7\x7b\x86\xf0\xbd\x7a\x03\xf0\xb1\xf7\xb1\xf5\xf9\xb5\x5f\x7f\xc2"
"\x5e\x68\x2b\xc4\xf9\x45\xd6\xc7\xd5\xc5\x45\x03\xea\x8a\xfb\xf6\xb3\xf5\xc5\x83\x87\x0d\xc5\xba\xe1\x91\x96\x92\x11\xc3\x87\x13\x24\xce\xbf\xd3\x2a\xdc\x7d\xcf"
"\x86\x3b\x71\xc9\xbd\xf7\x60\x74\x5f\xaf\xea\x92\x8d\xf7\xde\x57\x5e\x72\xff\x88\x8d\x45\xd4\xa6\x61\xf7\x17\xa0\x07\x36\x11\xfc\xc0\x07\x04\xd5\x66\x51\x1b\x5b"
"\x36\x57\xa0\x07\xb7\x6c\xc5\x68\xdb\xc0\x1a\xf5\x43\x0f\x6e\xdb\x81\x1e\x7e\x28\x44\x8b\xd1\xf6\x9e\xe5\x68\xc7\xf6\x87\xf9\x9e\x3b\xec\xfd\x6d\xfd\x71\xfe\x4e"
"\xeb\x92\xfc\xdd\x3b\x77\xf1\xd6\xdd\x20\x4a\x3f\xb0\xdd\x3d\xab\x10\xd8\xee\x4b\x8f\x8a\x76\xb4\xad\x87\xd3\x49\xeb\x72\x15\x92\x3c\x85\xe8\xd2\x29\x44\x97\x4e"
"\x21\xba\x74\x0a\xd1\xa5\x0b\x96\xe4\x05\x83\x13\x19\x2c\x3a\x91\xc1\x60\x2c\x73\x75\x0d\x4d\x5f\xa8\x3f\x98\x3c\x7f\x39\xa9\x0f\x40\x72\xa3\x82\x42\x01\x01\x8c"
"\x52\xaa\xe0\x94\xb4\x34\x34\x10\x90\x6d\xa0\x8a\x08\x0f\xa2\x64\xc6\xe0\x20\x26\x44\x1e\xcc\x85\xa8\xe5\x8c\x46\x1e\xa4\xd5\x82\x45\x0c\x27\xb5\x5c\xb8\x0e\x56"
"\x5d\x24\xc1\x98\xa4\x14\xeb\x1f\x23\x61\x5a\x49\xc1\x6e\xc6\x01\xc3\x04\xe7\x0a\x14\x34\xa1\x6a\x15\x0e\x0a\x0b\xc5\xda\x08\x5d\x01\xd2\x47\xc0\xf4\x34\x44\xc0"
"\xf4\x34\x46\xc0\xf4\x04\x48\x63\x8c\x5c\x47\x31\x61\x51\x82\x29\x9a\x56\xf1\xd1\x8c\xd0\xaa\x35\xad\x8a\x11\x31\x73\x25\x8a\x8f\x8d\x13\x5a\x25\xd2\xaa\x04\x8c"
"\xd8\xc4\x4a\x80\xcb\x71\xe5\xa8\x4d\xbc\x59\x68\x95\x04\x45\xf8\xa4\x36\x7f\x88\xca\x78\xd4\x43\x19\xa1\xd1\xd6\xbe\xcb\xaa\x56\xaf\x5d\xbf\x71\xd3\xe6\x5d\xf5"
"\x7a\xf1\x03\x02\x05\x28\xc4\xdd\x42\x01\x5e\xb4\x42\xf4\xa2\x15\xa2\x17\xad\x00\x2f\x5a\x21\x7a\xd1\x0a\xd1\x8b\x56\x80\x17\xad\x10\xbd\x68\x85\xe8\x45\x2b\xc4"
"\x8d\x44\x21\x6e\x24\x0a\xd1\x8b\x56\x88\x5e\x34\x0d\x5e\x34\x0d\xbb\x49\x28\xec\x4d\x8c\x24\x8f\x11\x55\xce\x88\x2a\x67\x44\x95\x33\xa2\xca\xad\xf0\xaa\x2f\x6c"
"\x59\x85\xb0\x45\x15\x01\x0d\x00\x2a\x06\x1a\x24\xc9\x1b\x24\x0a\x30\x48\xdc\xae\x06\x89\xdb\xd5\x20\x71\xbb\x1a\x24\xb6\x32\x48\x6c\x65\x90\xb8\x5d\x0d\x81\xed"
"\x6a\x08\x34\xb0\x0c\x36\xbc\x2a\xd8\xfc\x56\xc3\xb6\xb5\x16\xb8\xad\x87\xad\x6b\x23\x6c\x5d\x9b\xe0\xf9\x66\x78\xbe\xcb\x48\xb9\x6f\x61\xe1\x01\x30\x52\x8a\x00"
"\x48\x14\x24\x46\x29\x11\xc7\x51\x72\x79\x1c\x49\x26\x48\x12\xa0\x55\xc3\xc0\x69\xc0\x16\x6b\x42\x49\x71\x6b\x0b\x35\xc0\xe8\x1b\x8c\xa4\x3e\x0a\x19\x8d\x0c\x85"
"\xa2\xa2\x60\x28\x19\xce\x14\x2d\x65\x03\x63\x08\xc6\x0c\xc3\x1a\x98\xe4\xc7\x1a\xda\x4a\x99\x64\x09\x62\x03\x53\x10\x6b\x48\x25\xb4\xe9\xe0\x0f\x67\xc2\x50\x1b"
"\x72\xc0\xf3\xce\x23\x18\x8b\x54\x0d\x86\x44\xca\x65\xf6\x05\xdf\xb8\x1f\x70\xec\x1f\xa4\x2f\xa4\xfa\xeb\x8b\x80\x06\x00\x15\x53\xfd\xb5\x03\x03\x65\xc6\x41\x85"
"\x45\x03\x8a\x29\x6a\x20\x33\x58\x32\x88\x1b\x3c\x04\x2a\x97\x4c\xd1\x0c\x67\x46\x8b\xbb\xed\x24\x22\xbc\x4c\x2d\x65\xe6\x51\x65\x6c\xe0\x02\x29\x67\x59\x26\xe5"
"\x34\x55\x24\x67\x59\x2d\x65\xaa\xc1\xe6\x57\xaf\x85\xfb\xf5\x52\x66\x03\x89\x98\xfb\xa4\xe9\xdc\x7d\x1b\xa5\xcc\xfd\x64\x3a\x77\xff\x26\x92\x33\x6f\x06\x71\xb7"
"\x48\xb8\xcc\x5d\x70\xdd\x2d\x73\xda\x7e\x7f\x39\xc5\x8d\x1e\x03\x34\x9e\xe2\xe6\xcd\xa7\x84\xa0\x60\x5a\x85\x03\x42\x82\xb1\x3a\x2c\xb4\x00\x85\x87\xc1\x4c\xd4"
"\x85\xc1\x4c\x8c\x08\x83\x99\x18\x19\xae\x8b\xd0\xae\xa3\xf4\x21\x91\xbc\x5c\x2f\x04\xb5\xa2\x63\x70\x40\xeb\x56\x58\x1d\x0b\x45\xe3\x62\xa1\x68\x7c\x2c\x14\x4d"
"\x88\x85\xa2\x89\x71\xf1\x09\x50\x94\x6d\x9d\x28\x98\xdb\x44\xc7\xf0\x6d\x58\x21\xb9\x1d\x9d\xd4\x16\xa3\xf6\xed\x84\xe4\x0e\x74\x4a\xaa\x68\xb4\xac\xe9\x19\xed"
"\xd3\x84\xcc\x8e\xd1\x31\xf0\x82\xeb\x58\x9d\xde\x29\x83\x13\xcc\x38\x3a\x06\x53\x3c\xb6\xa4\x77\xe6\x79\x70\x91\xd3\xb3\x3a\x77\x12\x32\x85\xe8\x98\xd4\x02\xd4"
"\x25\x8b\xe0\x85\x2e\x42\x66\xd7\xe8\x18\x63\x01\xca\xce\x22\xf9\xae\xd9\x42\x66\xb7\xe8\x98\x1c\x8c\x72\xbb\x55\xa2\xee\xb9\x79\x42\x66\x0f\x07\xbb\x9e\x3d\x2a"
"\x51\xaf\x9e\x79\xe5\xa8\x77\xf7\x5e\x42\x66\x3e\x14\xe1\xf3\x7b\x83\xcf\x26\xb7\xa8\xeb\xe1\x38\x6f\xed\xc3\xf7\xed\x27\x24\x0f\x1d\x22\x8a\x35\x6c\x68\xcd\xf0"
"\x11\xc3\x4a\x6a\x47\x11\x75\x23\x46\x8e\xb2\x8d\xc4\xd4\x58\x28\x59\x3a\x76\xac\xc3\x0d\x1e\x47\x76\x29\xe5\xc7\x8c\x13\x92\x27\x38\x0a\x4f\x9c\x50\x89\x26\x4f"
"\x9c\xc4\x8f\x9f\x8c\xd1\x14\x00\xe0\x53\xc7\x57\xa0\x69\x53\xa6\x0a\xc9\xd3\x1d\xaf\x67\x4c\xc7\x68\x26\x3c\x9a\x35\x63\xa6\x12\xcd\x26\xe3\x67\x95\x54\xa1\x39"
"\xd3\x00\x09\xcf\x25\xd9\x39\xfd\xb4\x7c\xdf\xb9\xf6\x51\xb6\x51\x42\xd0\x42\x7a\x01\x0e\x58\xb4\xb0\x80\x5a\xbc\x88\xb0\xa8\x97\x2c\x5e\x2c\xba\x9e\x54\xf9\x22"
"\xd2\xa2\x5e\x5a\x5e\xee\xc8\x54\x2c\x92\x58\xd4\xb7\x54\x54\x88\x7e\x68\x59\xe5\x92\xa5\xb7\xf0\xf3\x2b\x71\xd9\xf2\xf9\x58\xbd\xa2\x6a\x9d\x7a\xe5\xf2\x15\x4a"
"\xf5\x2a\x32\x67\x25\xbf\x6c\x15\x2e\x5b\x33\x1f\x53\xb7\xae\xb5\xa8\xd7\xdd\x7a\x2b\x14\x5f\xa7\xbe\x6d\xcd\x3a\xa5\xfa\x76\x32\xe7\x36\x7e\xf5\xed\x58\x7d\xc7"
"\x32\xac\xbe\x73\xb5\x52\x7d\x17\x99\x7d\xc7\x9d\xc2\x86\xbb\xd7\x82\xa0\xf7\xdc\x5d\xad\xbe\xf7\xae\x7b\xf8\xf5\xf7\xe2\xf4\x07\x36\xf1\x1b\x1f\x10\x82\xb6\xd2"
"\x5b\x70\xc0\x83\x5b\xb1\x7a\x1b\x0c\xea\x43\xdb\x60\x50\x1f\xde\x06\x83\xba\x7d\x1b\x0c\xea\x8e\x87\x1e\xde\x0e\x83\xba\xf3\xc1\x1d\xfc\xe6\x9d\x42\xf2\x23\xf4"
"\x6e\xe0\xb1\xe7\x11\x7e\xd7\x1e\x87\x95\x59\xe4\x01\x5d\xc7\x9c\xed\x3e\x7f\xe7\x9e\xfd\x47\x8e\x9f\x3d\x4f\xf3\xbd\x87\x4d\x9b\x3d\x57\x44\xb0\x63\x00\xc1\x8e"
"\x81\xd5\xfb\x40\x78\xee\x7b\xb0\xe4\x4f\x03\xbd\x0f\xf4\x01\xd0\x19\x49\xde\x19\x71\xf9\x9f\x11\x97\xff\x19\x71\xf9\x9f\x11\x97\xff\x19\x71\xf9\x9f\x11\x97\xff"
"\x19\x71\xf9\x9f\x85\xe5\x7f\x16\x18\x74\x87\x25\x5e\x26\xc9\x2b\x03\xd3\x55\x26\x9a\xae\x32\xb1\x6a\x19\x98\xae\x32\xd1\x74\x95\x89\xa6\xab\x0c\x4c\x57\x99\x68"
"\xba\xca\x44\xd3\x55\x06\xa6\xab\x4c\x34\x5d\x65\xa2\xe9\x2a\x13\xb9\x96\x89\x5c\xcb\x44\xd3\x55\x26\x9a\xae\xf9\x60\xba\xe6\x03\xeb\x9d\x60\x45\xf6\x00\xbc\xde"
"\x0f\x96\xe4\x08\x58\x97\xe3\x60\x59\xce\x9a\xa4\x8e\x66\xcf\x9b\x64\xb9\xe7\xe1\x4a\x9b\x28\x87\x99\xe3\x4d\x7e\xb9\x3c\x5c\x7b\x9b\x50\x6e\x6f\xb8\x0e\x03\xdb"
"\x36\x0d\xac\xd1\x6c\xb8\xce\xf5\xb4\x40\x22\x88\xd6\xd2\x60\x63\x02\x29\x5a\x04\xd1\xa6\x20\x3a\x30\x30\x50\xa5\x0a\x0d\x07\x3b\x13\xae\x03\xeb\xa0\x27\x34\x06"
"\x16\x99\x00\x8f\x26\xc2\x93\x24\x09\x1b\xde\x01\xae\xd9\x60\x8d\xfa\x13\xa1\x85\x00\xa6\x8b\xa8\x42\xb6\xb0\x98\x60\x0b\x4b\x82\x74\x23\x9d\x60\x7a\xd4\x48\x66"
"\x34\x31\x0a\x56\x33\xa1\x2d\x05\x30\x1d\x3e\x21\x48\x3f\xb1\x74\x02\x18\x0d\x00\xd3\x93\x3f\x31\x97\x4e\x99\x34\x99\x45\x53\x01\x4c\x4f\x8b\x34\x97\x4e\x9f\x3a"
"\x8d\x45\x33\x00\xa3\xce\x9c\x60\x2e\x85\x79\xca\xa2\xd9\x00\x74\xe7\x30\xe6\xd2\xb9\xb3\xe7\xb0\xa8\x0c\xf0\xf0\xbc\x2a\x73\xe9\xfc\xb2\x79\x2c\x5a\x00\x20\x77"
"\xa1\xd5\x5c\xba\x68\xc1\x42\x16\x2d\x06\x30\xbd\x64\x83\xb9\xb4\x7c\xf1\x12\x16\x2d\x05\x00\x5b\xf1\x9d\xb9\xf4\x96\xa5\x15\x2c\xaa\x04\x30\xbd\xcc\x6c\x2e\x5d"
"\x5e\xb9\x8c\x45\x55\x20\xfc\x8a\x1e\xe6\xd2\x95\x55\x2b\x58\xb4\x0a\xc0\xf4\x6a\x64\x2e\x5d\xb3\x6a\x35\x8b\xaa\x01\x4c\xaf\x5d\x61\x2e\xbd\xb5\x7a\x2d\x8b\xd6"
"\x81\x04\xb7\x6d\x37\x97\xde\xbe\xee\x36\x16\xad\x87\x76\xee\x58\x69\x2e\x05\x4f\x8f\x45\x77\x01\x98\xde\x50\x6a\x2e\xbd\xfb\x2e\x28\x72\x0f\xd0\xbd\x25\xe6\x52"
"\xf0\xf2\xcc\x13\x37\x4e\x99\x3e\x6b\xee\xfc\x45\xe5\xb7\x2c\x5f\xb9\xe6\xd6\xdb\xef\xbc\xfb\x3e\x66\x13\x35\x91\xd9\x4c\x95\xb2\x68\x57\x1f\xb6\x70\x1f\xa8\xe9"
"\x49\xc2\x4c\x3f\xa3\xd7\xeb\x59\xf4\x3c\xc8\xf1\x02\xd0\xd1\x00\x33\xfd\xea\xd1\xa3\x47\xf5\xef\x51\x13\xf4\xa7\x81\xde\x07\xfa\x80\x9a\xa0\xfd\xb0\x50\x66\x3c"
"\xf3\xde\xe9\xf7\x3f\xa0\xa8\x0f\x99\x8f\x24\x67\xb8\x8f\xce\x4a\xd8\xc2\x8f\x11\xf3\x89\xa4\x90\xf9\x16\x62\xa9\x17\x24\x10\x0f\x95\xec\x36\xd3\x48\xaf\x37\xb1"
"\x85\x4a\x18\x9f\x08\xd0\xbc\x49\xc2\xe4\x81\xc9\xcd\xeb\x4e\x86\xcf\x85\x1d\xa5\x6c\x2e\x24\xc7\x8e\x32\x4f\x52\x06\x96\x14\x78\x2c\xa0\xcc\xf4\xad\x10\x59\x65"
"\xd1\x03\xdf\x31\x3b\xc8\x52\x6e\xc7\x4e\x92\x79\x84\x34\x70\x8f\xec\x21\x01\xeb\xd3\x22\xd6\xe7\xf6\x1d\x01\x3a\x4e\x72\xa3\xcf\x02\xcf\x2f\x64\xdc\xe8\xf3\x20"
"\xec\x2f\x04\x37\x9a\x86\x6b\x12\x5c\x79\x78\x9e\xeb\xcf\x8d\xee\x0d\xf9\xbe\x27\xd8\xc2\x42\x3f\x6e\xdf\x30\x92\x99\x0a\x90\x7e\xea\x34\x92\x99\x05\x50\x7e\xd6"
"\x6c\x29\x37\x75\x2e\xe9\x34\xe9\x9b\xb7\x53\x5c\xe0\x6e\xa0\xc7\x80\x9e\x01\x7a\x16\xe8\x15\x20\x88\xaa\x16\xbd\x0d\xd7\xf7\x80\x3e\x04\xfa\x01\x50\xad\x92\xe2"
"\x36\x43\xc4\x65\x33\x44\x5c\x54\xe1\x40\x26\xa0\x74\x20\x0e\x28\x07\xa8\x07\x94\x83\xe8\x4b\x20\xa0\xdf\xc0\x12\xa0\x11\x40\xa3\xe0\x39\x6c\x19\xaa\x52\x0a\x97"
"\xee\xda\xc9\x6f\xdf\x85\x0d\x8f\xee\xa9\xdd\x0b\x1e\xf3\xe3\x4f\x82\x9f\x4b\x3f\xb1\x9f\x7f\xec\x09\xfb\x5e\xdb\x93\x98\x7e\x6a\xff\x62\xfa\xc0\x53\xfc\x63\x07"
"\x20\xbb\x17\xd3\x4f\x3f\xa6\xa4\x6b\xc8\x9c\xa7\xf9\xdd\x35\xd8\x70\x70\x4f\xed\x61\xa2\xee\xe0\xa1\xe7\x6d\x87\x30\xfd\xdc\x11\xfe\xd9\xe7\xec\x87\x6d\xcf\x63"
"\xfa\x85\x23\x8b\xe9\x17\x5f\xe0\x9f\x7d\x11\xb2\x87\x31\xfd\xd2\xb3\x4a\xfa\x65\x32\xe7\x25\xfe\x99\x97\xb1\xe1\xe8\x9e\xda\xd7\x88\xba\xa3\xc7\x4e\xda\x8e\x61"
"\xfa\xc4\x71\xfe\xd5\x13\xf6\xd7\x6c\x27\x31\xfd\xfa\xf1\xc5\xf4\x1b\xaf\xf3\xaf\xbe\x01\xd9\xd7\x30\xfd\xe6\xab\x4a\xfa\x2d\x32\xe7\x4d\xfe\x95\xb7\x84\x6f\xdf"
"\x99\xff\x71\x12\x46\xef\xbe\x33\xb9\xf0\xd4\xbb\xfc\xdb\xa7\x70\xe1\xe9\xb7\x5f\x29\x7c\xff\xf4\xbe\x5d\x86\x0f\xde\x2f\xae\xfd\x88\xa8\xfb\xe0\xcc\xe7\xb6\x33"
"\x78\xd4\xc7\x67\x71\xe9\x27\xdb\x8b\xa8\x4f\x3f\xfe\xc4\x42\x7f\xf6\xe9\xa7\xe2\x16\xf1\xe1\x67\xf6\x8f\x6c\x9f\x0b\x17\xbe\x9c\xff\x05\xa6\xbe\xfa\xd2\x42\x7f"
"\xfd\xd5\x57\x8e\xe7\x5f\xc3\xf3\x8f\x30\xfd\xcd\x87\xfc\x7b\xdf\xe0\xc2\x6f\x81\xe3\xb9\x6f\x4d\xbb\x0c\xff\x39\x57\x5c\xfb\x3d\x51\xf7\x9f\xef\xbe\xb7\x7d\x87"
"\x47\xfd\x78\x1e\x97\x5e\x00\x8e\x3f\xfd\x78\xc1\x42\xff\xfc\xd3\x4f\x50\xb3\x9a\xbe\xf8\xf3\x2f\x56\xfa\x57\x98\x17\x15\x74\xed\xc5\x5f\xf9\x1f\x6a\x31\x6d\x7b"
"\x46\x50\xd9\x7f\xe8\x20\x86\x75\xab\xe9\xdf\x6c\x75\x98\xfe\xfd\x15\x41\xf5\xc7\x0f\xd9\x18\x5d\xfa\xa3\x9a\xfe\xf3\xf7\x4b\xe5\x34\xf1\xdb\x9f\x98\x26\x77\x0b"
"\x2a\xc9\x0f\xd0\x1b\x29\x30\x92\x91\xd2\x72\xda\x8f\x90\x29\x69\x44\xe6\xf8\xf1\xbb\x91\xfd\x7b\xdb\xf7\xb8\x90\x02\x59\xfc\xa9\x92\x5d\x06\xb9\x7f\x71\xad\x82"
"\xa8\x93\x07\x28\x6c\x01\x78\x54\x20\x8d\x4b\x55\x20\x4b\x50\xa0\xaa\x00\xbc\x17\x82\x57\x06\xe3\x52\xf5\xf6\xd7\x4b\x35\x6a\x88\xa5\xe1\xd2\xb0\xed\x6f\x94\x6a"
"\xc3\xf8\x50\x2d\x1e\xa5\x83\xa2\x11\xdb\x71\x69\x64\x48\x79\xa9\x3e\x22\xb2\x88\x32\xe8\xf4\x80\x45\x0d\x04\xf8\x33\xca\x0a\xc4\x18\xa3\xf8\x70\x06\x8f\x8a\x86"
"\x62\x31\x50\xac\x55\x68\x79\x29\x78\x33\x45\x54\x6c\x74\x6b\x07\x0e\x00\x7f\x06\x8a\x25\xc4\xc5\xf3\xa6\x04\x4c\x27\xee\xc6\x34\xfb\x0c\x04\x72\xc3\xab\xe9\x36"
"\xac\xb9\x9a\x6e\xdb\x26\xa9\x82\x4e\x4e\x6c\x8b\xe9\x76\xaf\x00\x1c\x30\x55\xd3\x29\xed\xda\x57\xd3\xa9\x29\xf0\xb8\x43\x72\xaa\x92\x4e\x23\x73\x3a\xf0\xbb\xd3"
"\xec\x0a\x9b\x02\x17\x66\x40\x77\x32\x33\xbe\xd8\x65\xe8\x98\x59\x5c\x8b\x89\xba\x8e\x9d\xba\xda\x3a\xe1\x51\x9d\x79\x5c\x9a\x05\xdd\x11\x3a\x67\x01\x2a\x10\x08"
"\x9e\xeb\x62\xc7\xb6\xae\x3c\xa7\x87\x0b\xc6\x28\x9b\xe3\xd3\xb3\x71\x61\x37\xa8\x9d\xd7\x2d\x77\x97\xa1\x7b\x5e\x71\x6d\x2f\xa2\xae\x7b\xcf\x3e\xb6\x9e\x78\x14"
"\x00\x81\x52\x0b\xd4\xb6\xe6\x5b\x0a\x50\x81\x95\xe0\x7b\x14\xd8\x7b\xd9\xfa\xf0\x3d\xfa\xc2\xa5\x17\x46\xfd\x7a\xf0\x39\xfd\x84\x6f\x8b\xe6\x17\xc2\xa0\x0c\x28"
"\x3a\x60\x28\x1e\x90\x28\xc6\xe6\x8a\xc5\xd8\x1c\xa6\x87\xbc\xc7\x0f\xac\x8f\xcf\xd1\xc3\x77\xf3\x25\xc3\x31\x3d\x72\x18\x3f\x62\x24\xa6\x47\xbf\xc7\x3b\x62\x72"
"\xe9\xfc\x98\xb1\x18\x8d\xcb\xe1\x4b\xc7\x65\xd2\xe3\xc3\x4a\x46\x8c\x1a\x53\xca\x0f\x1c\x2f\x86\xe2\x30\x3d\x61\x20\xdf\x7f\x02\xa6\x27\xf6\x17\x2e\x4c\x3a\x5b"
"\x88\xa9\xc9\x93\x2c\xf4\x94\xc9\x93\x1d\x8e\xee\xf4\x69\x56\x7a\xc6\xf4\xe9\xd3\x95\xf4\x4c\x32\x69\xe2\x94\x19\x7c\xff\x99\x98\x9e\xd3\x1f\xa3\xb2\xb9\x05\x68"
"\xde\x1c\xd8\x8d\xe7\xcf\x81\xdd\x78\xc1\x1c\xd8\x8d\x17\xce\x9b\xbf\xa0\x8c\x9f\xbd\xb0\x81\xdb\x1a\x18\x03\x0b\xb3\x2f\x50\x3f\x58\xec\x03\xe0\x0a\x0b\x76\xd3"
"\x03\xd4\x46\x41\x15\x11\xa0\x13\x03\xf4\x07\xc1\x99\xd0\xd7\x32\x44\x9d\x31\x8a\xb1\x45\x5d\x32\xd9\x18\x4c\xb5\x0a\xb0\xd0\xad\x5b\xb5\x72\xb8\xad\xb1\x62\xb1"
"\xb8\x58\x2b\x1d\x1f\x17\x17\x57\x45\x27\xb4\x8e\xaf\xa6\xd9\x84\x44\x2b\x6d\x4e\x4c\x4c\x2c\x17\x47\x90\x8f\x69\x23\xa8\xda\xc6\xc0\x2c\x4c\x6e\x5b\x63\x68\x97"
"\x6c\xda\x6e\x68\xe0\x90\xa6\xc5\x80\xd2\xd2\xd3\x0e\x18\x32\xd2\xf5\x0e\x87\xd4\x90\xd9\x8e\xc9\x48\xd9\x0e\x23\x58\xdb\x09\x06\x90\xeb\x64\xe3\xc0\x21\x15\x4b"
"\xf1\xb8\xc6\xd0\x99\x37\xd9\x3b\xd9\x3a\x5d\x34\x64\x65\xa6\x76\xe6\xb6\x1b\x1a\x38\xa4\x39\x31\xb0\x06\xba\xe5\x1c\x30\xe4\x76\xd3\x3b\x1c\x52\x43\x5e\x56\xa7"
"\xdc\x2e\xdb\x61\x3c\x6b\x7b\xc2\x70\xf6\xe8\x69\xeb\x21\xa8\x7a\x89\xa5\x7a\xf7\xaa\x31\xe4\xf7\x36\xd9\x7b\xda\x7a\x5e\x34\x58\xf2\xba\xe6\xf7\xa8\x2d\x20\xea"
"\x2c\xd6\x02\x9b\x15\x7a\x59\xc0\xf7\xd5\xf3\xfd\xfa\xf3\x03\x8a\xed\x03\x6d\x03\x7f\x1d\x3c\x84\xb0\x0f\x85\xf1\x2b\x1c\x36\xe0\x71\xc3\xf0\x61\x25\x75\xc3\x07"
"\x0d\x86\x91\x1d\x05\xd1\x54\x7a\x5c\x8c\xa5\x74\xfc\xb8\x71\xe0\xb6\x16\xde\x3f\x40\xd8\xbc\xe5\x81\xfb\x71\xe9\xd6\x2d\x18\x3d\xd8\xaf\xba\x74\xdb\xd6\x07\xcb"
"\x4b\x1f\x1a\xbf\xad\x88\x7a\x78\xec\x43\x80\x8a\x1e\x26\xf8\x11\xdb\x05\xd5\x0e\x51\x1b\x3b\x77\x54\x20\x70\x47\xc1\x0f\x1d\x51\x63\xd8\xb3\xfb\x91\x1d\xe8\xd1"
"\x3d\x7a\x13\x46\x8f\xf5\x2d\x47\x8f\x3f\xf6\x28\xdf\xf7\x71\xfb\x10\xdb\x10\x5c\xb8\x77\xc0\x92\xc2\xfd\x7b\xf7\xf1\x03\xf6\x83\x28\x83\x31\x7a\xa2\x6f\x15\x7a"
"\xea\x89\x27\x2f\x3d\x95\x91\x46\x87\x11\x41\xe9\x81\xc1\xe9\x81\x21\xe9\x81\xea\x74\x95\x26\x5d\x15\x6a\xd3\x72\x81\x35\x60\x74\x0f\xd6\x1b\x72\xd5\x73\xf5\xc6"
"\xbc\xe8\x24\xdc\x7f\x0d\xf7\xe7\x81\x6a\xe1\xfe\x12\x50\x18\x90\x11\x28\xad\xde\x80\xfb\x83\xf1\x56\xf5\x86\xf7\x43\x80\xe6\x00\x2d\x07\xba\x83\xe2\x6b\x9e\xe1"
"\x0f\x3e\x83\xe9\x43\x21\x8b\xe9\xc3\x87\x94\xf4\x11\x32\xe7\x30\xff\xec\x11\x8c\x5e\x0c\x55\x22\x08\x45\xbe\xa8\x37\x29\xd1\xcb\x64\xd2\xf3\x2f\xbc\xc4\x3f\xf7"
"\x32\xa6\x8f\xa9\x31\x7a\x4d\x63\xa5\x8f\xbf\xf6\xda\x6b\x4a\xfa\x04\x99\xf4\xea\xb1\xe3\xfc\x2b\x27\xf8\x93\xc5\xf6\xd7\x6d\xaf\xff\xfa\xe6\x5b\x84\xfd\x6d\xdb"
"\xdb\xb8\xf0\x9d\x93\x8f\x1b\xde\x7d\xa7\xa4\xee\x5d\x47\x18\xa8\xf0\xd4\x49\xe1\x93\x4f\xcf\x7e\xdc\x01\x17\x7e\xf6\xe9\xa3\x86\xcf\x4f\x7d\x56\xfb\x25\x51\xf7"
"\xf9\x17\x5f\xda\xbe\x80\xe0\xcf\x97\xfc\xd7\x26\x5c\xf8\xcd\x49\xe1\xdb\x73\x67\x8b\xbf\xd1\x89\xc1\x9c\xfd\x86\xef\xfe\xa3\xaf\xfd\x81\xa8\xfb\xee\xfb\x5f\x45"
"\x3b\xf6\xe3\x49\xe1\xc2\x4f\x67\x8b\x7f\xc4\xd4\xcf\x3f\x59\xe8\x5f\x7e\xfe\x59\x04\xe2\xf4\x45\x32\xe7\x17\xfe\xfc\x45\xfb\x0f\xb6\x5f\x71\xa1\x0d\x8a\xd8\xcf"
"\x16\xdb\x30\x55\x67\xb7\xd0\xbf\xd5\xd5\x89\x6b\x88\xfe\x3d\xa4\x82\xfe\xe3\xb7\xdf\xf9\xda\x3f\x30\xfd\x67\xad\x12\x11\x64\x97\x3f\xf9\x4b\x60\x95\xc8\x4b\x07"
"\x0c\x52\x52\x52\xeb\x47\xd4\x49\x65\x94\x4d\xc6\x9f\x47\x76\x3f\x1b\x85\x69\xff\x5a\x38\x06\xbb\x64\xa5\x03\xe4\x72\x79\x15\xad\xf0\x0f\xe0\xcf\x83\xe5\xa1\x41"
"\xba\xc0\xb3\x4a\x1a\x86\x56\x05\xa1\x9c\xa0\x4b\x95\x28\x58\x15\x84\x51\x08\xdc\xa8\x83\x43\xca\x91\xc6\xa4\xae\x42\xa1\x26\x0d\xff\x75\x28\xf0\xf1\x03\x99\x7e"
"\xe0\xc3\xa0\x5b\x5a\xa8\x18\x7e\x56\xa9\x85\x09\xae\x0b\x3f\x60\x88\xd4\x45\xd4\x1a\x88\xba\x48\xbd\xc1\xa6\xc7\x74\xd4\xf9\xc5\x34\x13\x85\x0b\xa3\x41\xf6\x98"
"\xb3\xa6\x68\x58\x7d\x31\xce\xd5\xa7\xa4\x63\xc9\x9c\xd6\xeb\x51\x1c\x13\xcb\x1b\xe3\xc0\x8c\x1a\x71\x61\x02\x30\x4b\x3c\xab\x4c\xe8\x20\xfa\xf7\x07\x0c\xe6\x78"
"\xb6\x36\x89\xa8\x33\xb7\x69\x6b\x6b\xc3\x87\xe9\xed\x49\x70\x22\x86\x92\xe1\x84\xad\x1d\x19\x9f\xac\xc7\x85\xed\xa1\x78\xca\x59\x65\x7b\x58\x10\xa9\x29\x4a\xd4"
"\x81\x8c\x68\x97\xca\x87\x75\x80\x72\x49\x76\x83\xcd\x80\xe9\xf4\x60\x4c\x67\x9c\x5f\x8f\x32\xd3\x33\x94\xa8\x23\x19\x9f\xa9\xe7\xd3\x3a\xf2\x9c\x49\xf8\xa4\xd3"
"\x59\xd8\x27\x0b\x71\xa7\x47\x0c\x3c\x2e\xae\xcd\x22\xea\xf8\xce\x59\xb6\xce\xb8\x50\x80\x81\xec\x72\xf6\x63\xb0\xa6\x5d\xbb\xec\x34\x64\x0b\x5d\xed\x59\xb6\xac"
"\x8b\x86\x1c\xde\x90\xdd\xb9\x36\x97\xa8\xcb\xe9\x96\x6b\xeb\x86\xa9\x9e\xdd\xf9\x1e\x3d\x33\x51\x2f\x45\x0f\x9e\xeb\x65\xcf\xb5\xe5\x62\x94\x9f\x86\x91\xe5\xeb"
"\x4a\x04\x66\x16\xa3\x82\xb0\x4a\xd4\x07\x8e\xcb\x50\x5f\xae\x12\x89\x87\x20\xbd\xfb\xe1\xc2\xfe\xa0\x85\xc2\xb3\xfb\xfa\x63\x0a\x0e\xcc\xe8\xfa\x03\x33\x8c\x8a"
"\x7b\x57\xd3\x70\x1a\x82\xe9\x41\x35\xe5\xf4\xe0\x41\x03\xf9\x1a\x30\x9b\x43\xcf\x63\x7a\xd8\xb3\xe5\x74\xc9\xd0\x61\x4a\x7a\x38\x99\x53\xc2\x0f\x19\x8e\x0b\x47"
"\x00\x83\x91\x67\xf7\x8d\xc0\x70\x62\x66\xa1\x47\x8f\x1a\xe5\x98\x02\x63\xa0\x93\x63\x87\xac\x47\xa5\x63\xc6\x8a\xbe\x5b\x7c\xa9\x1e\xce\x46\x9e\x53\xa2\x09\x64"
"\xc4\xb8\xf1\xd5\xf4\xc4\xd1\x13\x30\x9a\xf4\x75\x35\x0d\x3e\x1c\x38\x70\x61\xd5\x34\x44\x90\xe1\x44\x84\xab\xa6\x01\x17\x63\x7a\xc6\xc1\x72\x7a\xe6\x8c\xe9\xfc"
"\xc1\x99\xf6\xb7\x6c\x6f\xe1\xc2\x59\x27\x97\x14\xce\x9e\xb5\x8f\x3f\x39\x1b\x66\xfc\x9b\xc2\x85\x85\xf3\x17\x60\x6a\xd1\x42\x0b\xbd\x78\xd1\x22\x47\x7b\x4b\x40"
"\x4e\x00\xc4\xfc\x9c\x72\xe1\xdb\xa5\xf3\x17\xe8\xc4\x03\x91\x1a\xc3\x2d\x15\x89\xf5\x07\x22\xcb\x6c\x95\x98\xae\xaa\xb1\xd2\x2b\x00\x93\x5a\xe9\x95\xb0\xf9\x2b"
"\x69\x38\x84\xaf\x5a\xb1\x92\x5f\xbe\x0a\xd3\xab\x97\x0b\x17\xd6\x88\x3c\xab\xd7\x58\xe8\xb5\xd5\xd5\x0e\x9e\xeb\x96\x57\xd0\xb7\xdd\xba\xae\x92\xbe\x7d\xed\x6d"
"\xe5\xf4\xfa\xd5\xb7\xf3\x73\xd6\x8b\x67\x25\x98\xbe\x13\x16\xe4\x5d\x9a\x0a\xb4\xc1\x74\x57\x35\x7d\xf7\x9d\x1b\xf8\x3b\xee\xc6\xf4\x3d\x73\x30\x7d\xef\x1d\x95"
"\xb4\x78\x10\x42\x6f\x3c\x88\xe9\xfb\x5f\xa9\xa4\x37\x6d\xbc\xbf\x9a\xde\xbc\xe9\x81\x72\x7a\xcb\x7d\x9b\xad\xf4\x56\x47\xf3\x0f\x3a\x9a\xdf\x46\xb2\x5b\xb6\x3e"
"\x78\x69\x9b\xe8\xb8\xa5\x79\x09\x0f\xa5\x7a\x0b\x0a\xc5\x64\x35\x15\xd3\x49\xf5\x3c\x30\xb8\x1c\x6d\x81\x38\x8b\x5c\x8c\xb3\xc8\x9d\xf1\x32\x26\x48\x7e\x0d\x71"
"\x16\xa6\x35\xc4\x58\x0c\x71\x12\x26\x5e\x0c\x9d\xb1\x50\xaa\x8d\x94\x49\x81\x78\x4a\x4a\xaa\xd4\x25\x90\x71\x0d\xc1\x0b\x4c\xc5\xc8\x2d\xb0\xf2\x62\x08\x52\x2a"
"\x15\x5a\xc7\x46\xab\xf8\xd8\x56\x42\x7c\x42\xb4\x0a\x96\x5d\x62\x02\xc4\xd1\x12\x59\x21\x3e\x29\x5a\xe5\x88\xc0\x56\xc2\x4e\xc7\x96\xa3\x76\xe6\x64\x21\xbe\x3d"
"\x14\xe1\xdb\xb7\xc3\xea\x0e\xa1\x7c\x6a\x07\x87\xdf\xfb\x9e\x87\xdf\xab\x78\x53\xf4\x73\x15\xa0\x2a\x0c\xae\x2a\x06\x9f\xb0\x00\xfc\xca\x02\xb8\x4e\x05\xdf\x77"
"\x2a\x5c\x97\x82\x2f\xb9\x14\xae\x77\x83\x2f\x79\x37\x5c\x77\x83\x0f\xb9\x1b\xae\x87\x25\x79\x87\x41\xe9\x87\x45\xa5\x1f\x16\x95\x7e\x58\x54\xfa\x9b\x0d\x34\xeb"
"\xf8\xa6\x84\x09\x00\xef\x23\x40\x41\x6a\x82\x43\x43\x40\x6f\x21\x6a\x82\xd1\x40\x88\x28\x44\x4b\xb2\x21\x51\x10\x34\x8a\x27\xb4\x66\x88\x40\xb5\xa1\xcc\xba\x8e"
"\x4e\x0f\x91\xeb\xc8\x74\x22\xe0\x64\x15\xbe\x14\x41\x59\x84\x36\x1b\xc9\xb8\x4e\x05\x04\xd7\x69\x2a\xd0\x52\xa0\xbb\x81\x76\xc3\xbb\x27\x80\x9e\x49\x09\x3f\x94"
"\x2d\x33\x1e\x3e\x94\xcd\x1c\x91\x1f\xe6\x8e\x3c\x27\x0f\x7d\x9e\x11\xcf\xa9\x48\xe6\x45\x79\x36\x1b\xf2\xaa\x94\x39\x26\x47\xcc\x1b\xd2\x6c\xee\x8d\x37\x5d\x47"
"\x81\xf3\x87\x2f\x37\xda\x88\x5f\x6c\x64\x52\x9c\x66\x3f\x85\x29\xa5\x42\x49\xd1\x64\x8e\x92\x97\xd3\x98\x52\xc9\x95\x54\x10\x29\x53\xf1\x81\x41\x10\xf6\x0c\x84"
"\xa9\x1c\x16\x2a\x68\xc2\x03\xb5\xa2\xb1\x3c\x14\x1c\x11\xa6\xab\xd5\x13\x75\x11\x91\x7a\x5b\xa4\xa0\x31\x88\xef\x8d\x06\x41\xc3\x04\x46\x61\x64\x62\x0e\x05\x47"
"\x1b\x4d\x76\xbd\x4d\x7f\x31\x38\x26\xc2\x2f\x3a\xb2\xb6\x35\x51\x17\xd3\xaa\xd8\xd6\x4a\xd0\xc4\xca\xd5\x22\x6e\xa9\x09\x06\xdc\x59\xcb\x12\x75\x09\x89\xb9\xb6"
"\x44\x41\xd3\x56\x1e\x25\xe2\x94\xc5\xa8\x5d\xb2\xa0\x69\x2f\x87\x56\x52\xda\x5b\xcd\xa9\xed\x52\x04\x4d\x07\x91\x39\xc4\xa0\xcc\xe9\x69\x69\x55\xe6\x8c\xd4\x74"
"\x3e\x29\x03\x73\x3c\xc6\xe6\xce\x49\xd5\x66\x00\x96\x56\x73\x97\xac\xac\x72\x73\x57\xa1\x4b\x51\x76\x0e\xdf\xd5\x42\x75\xcb\xc9\x11\xc1\x7f\x66\x37\x3b\x6b\xcb"
"\x15\x34\x79\x22\xef\xee\x79\x82\xa6\x87\xc8\xb7\x67\x0f\xab\xb9\x57\xf7\x9e\x82\xa6\xb7\xc8\x37\xbf\xb7\xd5\x6c\xc9\xcf\xaf\x32\x5b\x7b\x59\xf8\x24\x2b\xe6\xfa"
"\x14\x60\x73\x5f\xe0\xdb\xaf\x2f\xf0\xed\x2f\xf2\x2d\xec\xd7\xbf\x28\xbb\xa8\xcf\xd5\x0f\x05\x32\x07\x00\x5f\xd6\xde\xda\x56\x2c\x68\x06\x8a\x0a\x19\x34\x50\xd0"
"\x0c\x16\x7b\x3e\x64\xf0\xa1\xe0\xa1\x83\x86\xd4\x96\x10\x75\x43\x87\xad\xb4\x0d\x13\x34\xc3\xc5\x26\x47\x0c\xaf\x09\x1e\x39\x22\xbe\x76\x34\x51\x37\x72\xd4\x5c"
"\x88\x28\x69\xc6\x88\x5a\x18\x3b\x46\xd0\x94\x8a\xb2\x8d\x2b\x5d\x8c\xc6\x8f\xb3\x9a\x27\x8c\x1d\x2f\x68\x26\x8a\x1c\x27\x4d\xb4\x9a\x27\x4f\x9a\x54\x65\x9e\x32"
"\x61\x32\x9f\x34\x05\x73\xf0\x81\x80\x79\x3a\x88\x35\x63\x3a\x88\x35\x53\x14\x0b\x82\x01\x45\xd9\xb3\xa7\xcd\xb2\x50\x73\x66\xcf\x76\x88\x35\xc7\x3e\xda\x36\x57"
"\xd0\x94\x89\xbc\xe7\x95\x09\x9a\xf9\x22\xef\x05\xf3\xad\xe6\x85\xf3\x16\x08\x9a\x45\x22\xdf\xc5\x8b\xac\x66\x88\x5f\x55\x99\xcb\x17\x2e\xe1\x93\xca\x31\x57\xb1"
"\x14\x9b\x6f\x01\xbe\x95\xb7\x00\xdf\x65\x22\x5f\x08\x0b\x14\x65\x57\x55\x2c\xb7\x50\x2b\xaa\xaa\x1c\x7c\x57\x00\xdf\xd1\xf6\x12\xdb\x4a\x41\xb3\x4a\x64\xb9\x7a"
"\x55\x4d\xf0\x9a\xd5\xf1\xb5\x6b\x89\xba\x35\xd5\x9b\x6d\xd5\x82\xe6\x56\xb1\xc9\x75\xb7\x0a\x9a\xdb\xc4\xee\xde\x7e\x9b\xd5\xbc\x7e\xdd\xed\x82\xe6\x0e\x51\x27"
"\x77\xde\x61\x35\xdf\x75\xe7\x9d\x55\xe6\x0d\xeb\xef\xe2\x93\x36\x60\xee\x9e\xbb\xb1\xf9\x5e\x68\xf2\xbe\x7b\xa1\xc9\x8d\x62\x93\xf7\xdf\xb7\xb1\x28\x7b\xd3\x3d"
"\xf7\x5b\xa8\x07\x36\x6d\x72\x34\xf9\x80\x7d\xad\x6d\xb3\xa0\xd9\x22\xf2\xdd\xba\x65\x31\x7a\x70\xab\xa0\xd9\x26\xf2\x7e\x68\x9b\xd5\xfc\xf0\x83\x0f\x09\x9a\xed"
"\x22\xef\x1d\xdb\xad\xe6\x9d\x3b\x76\x54\x99\x77\x3d\xbc\x93\x4f\xda\x85\xb9\x47\x76\x63\xf3\x1e\xe0\xfd\xe8\x1e\xe0\xfd\x98\xc8\xfb\xf1\x47\x1f\x2b\xca\xde\xfb"
"\xc8\xe3\x16\x6a\xdf\xde\xbd\x0e\xde\xfb\x80\xf7\x5a\xe8\x4e\x09\x8c\x60\x6b\x41\xf3\xa4\xc8\xf6\xa9\x27\x95\xe8\x00\x29\x7b\xaa\x02\x3d\xfd\xc4\x01\x25\xaa\x21"
"\xd9\xa7\xe3\x9f\x50\xa2\x83\x64\x04\x80\xb0\xfd\x70\x50\xf6\x6c\xa6\xf0\xe2\x4b\xcf\xbd\x80\xb3\x5f\x7e\xc9\x42\xbd\xf2\xf2\xcb\x8e\xfd\xef\xe8\x7e\xf8\x54\xe9"
"\xb9\x17\x5e\xc5\xe8\xf8\x6b\x95\xe8\xc4\xd1\xe3\x56\xea\xe4\x89\x13\x27\x94\xd4\xeb\x64\xd2\xb3\xaf\x9c\xe4\x33\x5f\xc7\xd4\x5b\x99\x05\xe8\xed\xb7\xc0\xc2\xbd"
"\xf3\x16\x58\xb8\x77\xdf\x92\x58\xb3\x4f\xbd\xfd\xce\xbb\x4f\xf0\x6f\x9e\x72\x18\xa5\x61\x5e\x6c\x7a\x46\xc7\x4e\x7c\x56\xf3\xc1\xfe\x98\x25\x10\xda\x22\x9a\x8b"
"\xe0\x67\xc0\xab\x8e\x10\x80\xeb\x04\xc4\x43\xb4\x2b\xcb\x28\x69\x2c\xd6\xee\x1e\x66\x27\xae\x39\xcc\x4e\x82\xe9\x87\x2d\x20\x51\xc2\xb0\xe2\x16\x90\x04\x25\x93"
"\xa5\xda\x34\x30\x70\xe9\xd2\x34\x2e\x3d\x43\xca\x64\x92\x69\x5c\x66\x47\x92\x8b\xef\x24\x65\x30\x98\x46\xcc\x93\x1c\x9b\x05\xc1\x1a\x81\x60\x03\xbb\x48\x98\xae"
"\x10\x9a\x47\xdd\xa6\x68\x72\x99\x9e\x62\x24\xbd\x2f\xe1\x6a\xb2\x7a\xc2\x21\x5f\x4f\xeb\xf5\x45\xbf\xa9\x58\xf9\x3a\x2a\xae\x75\xac\x63\xf7\xe0\x13\xe2\x04\xd6"
"\x1c\xad\x4a\xc4\xa8\x8d\xb9\x12\x81\xab\x2b\xb0\xed\xa2\x55\xc9\x62\xd8\xbb\x12\x4c\x4f\x52\x39\x4a\x6d\x9b\x22\xb0\x1d\xa0\x08\xdf\x21\x15\xa7\x71\x1d\xf9\x0c"
"\xf8\xdc\x06\x22\xdd\x9d\x3a\xf3\x59\x82\xd0\x35\x9b\xee\x92\xec\x38\x83\xcb\xcd\xcb\xe9\x56\xdb\x83\xa8\xcb\xeb\xde\xc3\x06\x5f\x1f\xf5\x86\x68\x74\x7e\xef\xde"
"\x8e\x68\xb4\x85\xec\x92\xcf\xf7\xb2\x08\x5d\x0b\x1c\x85\xfb\x14\xd4\x03\x2a\x2b\x9c\x81\xf5\x07\xef\xb5\xd0\x5a\x81\x8a\xfa\x17\x0a\x5d\x07\x38\x5e\x17\x0f\xc0"
"\x68\x20\x3c\x1a\x54\x3c\x50\x89\x06\x93\xf1\x83\xba\x55\xa1\x21\x45\x83\x95\x68\x28\xc9\x0e\x11\xb4\x7c\xd6\x50\x7b\x0f\x5b\x0f\xc7\xa4\xe9\xe4\xb1\x93\x05\x47"
"\x24\x24\x89\x7b\x99\xf3\x48\x27\x18\x46\x3d\x02\x46\x3c\x4a\x92\x17\x05\x93\x28\x4a\x9c\x44\x51\xe2\x24\x8a\x82\x49\x14\x25\x4e\xa2\x28\x71\x12\x45\x89\x93\xc8"
"\x04\x93\xc8\x04\x75\x12\x60\x76\x24\x79\x62\x03\x5d\x80\x73\x5f\x52\xc0\x79\x1a\x21\x4e\x10\x42\xab\x82\x21\x0d\x22\x55\x5c\x50\x30\xa9\xd5\xc0\x04\x09\xa5\x34"
"\x8c\x0e\x86\x53\x17\x41\x86\x1b\x61\x36\x45\x19\x8d\x14\xc3\x48\xa2\x38\xc6\x24\x81\xef\x25\x49\xf8\x5e\x12\x4e\xd4\xe0\x8c\x3b\x1e\x02\x62\xf1\x09\x24\xd3\x46"
"\x2a\xee\x3e\xd2\xd0\x64\x98\x4d\xc9\xed\xa4\x4c\x7b\x0a\xb9\x8e\x74\x68\x18\x85\x15\x10\x0b\x51\x85\x04\x17\x51\xea\xc0\x10\x1e\x2c\x01\xa5\x95\x5b\x34\xe1\x5a"
"\xad\xe3\xf3\x8d\xc8\x08\x8b\x46\x1f\x19\x09\xf7\x95\x1a\x43\xb8\x1e\x8e\xda\x4c\x70\x1a\x11\xdb\xda\xa2\x89\x8b\x8d\x75\xac\xce\xc4\x04\xab\x86\x05\x67\x58\xa9"
"\x31\x93\x49\x86\x38\x96\x0f\x33\x63\x4d\xdb\x30\xa1\x7d\x8a\xbc\x9d\x88\xba\x85\xf8\x0e\x11\xed\xc4\x9d\xa5\x12\xa5\xa7\xa6\x15\xa0\x8c\xb6\x30\x8d\x32\xdb\xc2"
"\x34\xea\xd8\x16\xa6\x11\x7c\x00\xd9\x11\xb6\x1a\xce\xa1\xf0\x28\xcf\x53\xca\x10\xbd\xa1\xb1\x73\xca\x10\x50\xbe\x1e\xc8\xd0\xe0\x13\xd3\x16\x1c\x4f\x6a\x83\x40"
"\xad\xc1\x64\x10\x17\x1c\x02\x6b\x4d\x43\x88\x1f\x66\x32\x3a\x29\xc5\x44\x4a\x83\xb8\x48\xbd\x94\x0b\x36\x38\xc3\x87\x38\x48\x1d\x52\x80\xc2\xd4\x20\xb3\x56\x4d"
"\x5a\xa9\xf0\x30\xad\x26\x54\xd0\x45\xc0\xc1\x63\x44\x38\x0e\x32\x1a\x78\xbd\xd1\x21\xba\xc6\x63\xae\xc8\x83\xc5\x99\x22\x37\x12\x57\xcc\x47\xb0\xfb\xa8\xc3\xe0"
"\xf8\xcb\xa5\xcd\x8d\xbc\x53\x14\xb7\x81\xf2\x32\x45\x1d\x76\x2d\x29\x25\x35\xa3\xa3\x87\x59\x73\xd8\x2e\xb3\x24\xcf\x2c\x2a\xd2\x2c\x2a\xd2\x2c\x2a\xd2\x2c\x2a"
"\x32\x05\x5e\xa5\xc2\x2c\xce\x00\xea\xd8\xa8\xdd\xba\x06\xd0\x2a\x61\xa2\x44\xab\x02\xdf\xbe\x18\x5a\x49\xc1\x4e\x91\x10\x7a\x4e\x34\x9a\xe1\x19\x0b\x33\xd2\x2c"
"\xce\x48\x26\x19\xf4\xdd\x1e\x40\x6b\xfb\x14\x38\xb5\x4b\x25\xbd\x59\xaf\xeb\x03\xb3\x42\x14\x23\x37\x8a\xf0\xa8\x12\xc1\x07\x06\x42\x54\x6b\x79\xab\xfa\xb3\xe1"
"\xb8\xd8\xe8\x72\x14\x1f\x13\x27\x44\x25\xc8\x8d\x7c\x42\x3c\xa6\xda\xca\xe1\x18\x2e\x49\xc5\xb7\x6b\x0b\x00\x36\x95\x4f\xe9\x70\xd9\x02\x39\x54\x9b\xe9\xb9\xfa"
"\xcd\xed\xea\x17\xbf\x73\x38\xc9\xdc\x50\x49\x5e\x28\x01\x90\xd4\x0c\x2a\x6c\x77\xed\x8b\x5a\xa3\x31\x86\x22\x26\x4c\x1e\xca\x85\x69\xe5\x70\xe2\x41\xc2\x89\x07"
"\xc1\x44\xc8\x41\x79\x06\x22\xd4\x04\xca\x14\xf1\x7e\x0c\x85\xc0\xd0\x53\x1c\x6b\x96\x6a\xdb\x80\xc2\x93\xc9\x36\x5c\x72\x3b\x50\x78\xaa\xab\xd9\x76\x9f\x1f\x42"
"\x44\xa4\x16\x5c\x2f\x7d\x64\x8d\xc6\xa8\x37\x5c\x89\x71\x09\x31\xad\xe4\xd1\x18\xb5\x6e\x75\x40\x13\xdb\xda\x60\x67\x6c\xcc\x45\x4d\x9c\xd1\x2f\x36\xaa\xfe\x5b"
"\xd3\x04\x5b\xfc\xef\xb6\x04\x4c\x25\xc9\x2d\x6d\xda\x42\xdf\x61\x89\xb7\x01\x63\xdd\x26\xa5\x6d\xfb\x02\xd4\x21\x05\xd4\x9e\x96\x02\x6a\x4f\x4f\x01\xb5\x67\x74"
"\x48\x4b\x4f\xe5\xcd\x19\xa2\xba\x08\x03\x11\x0c\x0a\x73\xcc\xc2\xd0\x49\x44\x50\x70\x88\x3a\x37\xc8\x58\xaf\x24\x35\x5c\x43\x14\x44\x2e\x25\xc9\xa3\x60\x4e\x52"
"\xe2\x9c\xa4\x60\x4e\xaa\x49\x93\x84\x0c\x93\x32\x5a\x99\x54\xaa\xf5\x93\xca\xc2\x91\x9f\xcc\x08\x7d\xf5\x27\x65\x8c\x5c\xea\xc7\x04\x48\x65\x8c\x42\x4e\x85\x2a"
"\x41\x0f\xa0\x38\x56\x1a\x48\x30\x2a\x39\xe2\xfc\x83\x48\xce\x3f\x98\xe4\xe4\x21\x52\x2e\x40\x2d\xe5\x14\x1a\x79\x1a\x19\x4a\x48\xe0\x9b\x13\x99\x36\xc8\xea\x17"
"\xae\x0d\x14\x54\x3a\x0d\x8d\x51\x84\x6e\x9d\x5f\x64\x44\x38\x1f\x12\x89\x65\xfa\x60\x5e\xad\x77\x88\xaa\xba\x2c\xa8\x4c\x01\xab\x34\x20\x48\x51\xbf\x4a\x03\x8c"
"\x1e\x02\xc1\xee\x4a\x30\x14\x29\xae\x54\x3f\x8e\x92\x43\xab\x01\xd2\x34\x52\x01\x2d\x29\xb1\x8c\x96\x5b\xfd\x02\x69\xc4\x07\x04\x3a\x98\x6a\x9c\xbd\x57\x81\xc3"
"\xa3\xa4\x03\x1d\xfe\x8e\x12\x7a\x1f\x28\x5a\x00\x85\x2b\x6f\x89\x93\x37\x0c\x2a\xe1\xda\x5b\xce\x5f\x01\x6d\x28\xa1\x67\x34\xf4\x2c\x10\xda\x52\x41\x5b\x41\x58"
"\x16\xac\xc0\xb2\x10\xa5\xd5\x4f\x1d\x8c\x28\x9e\x56\xf3\x81\x21\x8e\x36\xd5\xce\x8e\x04\x8a\x6d\x06\x05\x52\x97\x1b\xad\x9f\x9f\x30\x4f\xdd\x1b\x85\xcf\xd2\x2e"
"\xcf\x47\x51\xcd\x04\xc5\xc8\xa1\xf1\x00\xe8\x9c\x1c\x1a\x0e\x50\x4a\x39\x7f\x9a\x48\x03\x96\x12\x9b\x0a\xcb\x82\x14\x98\x0a\xa6\x8b\xfc\x42\x82\x83\x78\x25\x34"
"\x28\x11\x2b\x8a\xa9\xd7\x8c\x54\xa2\x3e\x0d\x17\x7f\xc8\xcb\x89\x68\x22\x39\x5e\x4a\xc4\x1f\xa9\xf8\x23\x6b\xf8\x73\x13\x12\xe9\xe5\xa7\xa5\xf5\x9a\xa2\x7f\x47"
"\x7a\xe9\x9f\x6b\xda\xa1\x88\x10\x0f\xc5\x38\x2e\xa4\x1b\x5d\x29\xff\xe7\x35\xe9\xf8\x7a\xe8\x7f\x22\x5d\xe9\xb0\xfb\x8f\xf3\x22\xbe\xbe\xaa\x91\xfa\x71\x70\x3c"
"\x71\x5e\x9c\xc5\x9c\x83\x76\x65\x98\xae\x16\x6e\x30\x88\x6e\xaf\xaf\x3e\x6a\x70\x5f\xff\xfe\x72\x0b\xce\x9c\x67\xd5\x06\x62\x78\x7d\x4e\x36\xe4\x73\xa5\x11\x17"
"\xfe\x57\x24\x24\x5d\x2a\x5c\xad\xe1\xca\xf8\x6a\x57\x1b\xcb\x5c\xb6\x53\x0d\x3b\xe3\xca\xdf\xb5\xa8\x77\xa1\x2e\x57\x69\xa8\xb9\x96\xd5\xbd\xaa\x07\x17\x2e\x57"
"\x07\xa1\x71\x25\xdd\xa8\x78\x37\x49\x44\xcf\x01\x71\xfe\xf3\x25\x5f\xf2\x25\xd7\x24\xf1\xbb\x7c\xe3\x06\x96\x6e\x22\xda\xf9\x2b\xe1\xd4\xcd\x4b\x71\x40\x05\xce"
"\x4c\x30\xd0\x7b\xce\xcc\xe7\x40\xdf\x39\x33\xf7\x03\xcd\x71\x66\xc4\x9b\x62\x67\xe6\x08\x50\xa0\x33\xf3\x1b\xd0\x7e\x67\x66\x2d\xd0\x76\x67\xe6\x3c\xd0\x66\x67"
"\xe6\x0e\xa0\x35\xce\xcc\x4e\xa0\x45\xce\xcc\x21\x42\xc4\xff\x97\xd3\xe3\x40\x9d\x9d\x99\x52\x42\x04\xf1\x97\xd3\x56\xa0\x3e\xce\x4c\x1e\xd0\xa8\x16\x74\xb5\xbb"
"\x6f\xda\xfb\x92\x2f\xdd\x5c\x3b\x4a\xfc\x95\x76\xd4\xa7\x8c\x9b\xe3\x45\xff\xff\xd9\xae\xfe\xe2\x14\xe2\x5b\xd1\xbe\xf4\xef\x87\xae\x12\x2f\x56\xc6\x19\xe6\x23"
"\x25\xf5\xe9\xa6\x2e\x7f\xd7\xa8\x52\x03\xf3\xd3\xe2\x1c\xe9\xe5\xa7\xc5\x24\x69\x21\xf9\xc0\xf4\xbf\x00\x4c\xbf\x77\x63\x7b\xd1\x4b\x57\x82\x1c\x5e\xa2\x99\x57"
"\x63\x44\x44\xfd\x1c\x22\x1b\x06\x1d\xc9\x02\xb7\xb2\x9e\x91\xd0\x96\x4d\xae\xe3\x8e\x2b\x29\xb9\x9c\x75\xbd\x4a\xae\x2f\x72\x2a\xb9\x41\x72\x09\xec\x78\x0d\xd0"
"\x79\xc9\xba\x46\xcf\xfe\xa6\x3a\x04\xe1\x16\x0f\xbc\x1a\x17\xf5\x08\x99\x11\xa4\xd7\x80\x62\x73\x81\x46\x97\x22\x4d\x72\x6c\x18\x2e\xf5\x26\x90\xf7\x27\x92\xfa"
"\x9f\x7a\xba\x3c\xe4\x57\x7e\xc4\xff\x30\x2b\x1c\x8d\x3a\xca\xb8\x4c\x0f\xb1\xc4\xe5\x09\x52\xff\x4a\x72\x75\xc6\x5c\x2e\xeb\x7e\x7b\x35\xeb\xda\x88\xc4\xa5\xdd"
"\x6b\x29\x46\x7a\x2f\x26\x69\xfc\xf1\xe5\xee\xd4\xcb\xed\x28\x43\xb6\xa4\x3b\xce\x1a\x97\xdf\x7b\x16\x69\xbe\xb6\xc4\xad\xb6\xeb\xf2\x72\xa9\x4e\x5e\x2d\x49\x5e"
"\xa9\xef\x99\xbb\x7c\x21\xea\x77\x3e\x77\xe6\x2e\xa2\x12\xce\x9d\xf1\xf2\xbd\x4b\x6b\xae\xa3\xe5\x94\xc6\xf9\xe3\x2c\xe0\x4b\xbe\x44\xf8\x8e\xc1\x9a\x02\x92\xae"
"\xe7\x5a\xae\x47\x4c\xa4\x2b\x80\xbc\x7a\x6e\x71\xf5\x29\xe9\xb6\xcb\x37\xb8\x71\xfd\x71\x87\x07\x0d\xcc\xb7\xfb\xe1\x9a\x6b\xa1\x06\x9b\xc3\xd5\xed\x83\x74\x2f"
"\xe7\xc9\xac\xc1\x69\x8c\xf7\xb6\x3d\xce\x5c\x5c\xfa\x7a\xb5\x39\x8f\xe3\x40\xf7\x73\x38\xf7\xa3\x42\xd2\x89\x78\xdc\x77\x63\x27\xfc\x71\x95\xcb\x6d\xf7\x74\x3f"
"\xbe\x72\x3b\x2a\x24\xdc\x4e\x15\x49\xf7\xde\x35\xdc\xdc\xbd\x33\x20\xbc\x9c\x3d\x12\x1e\x47\x92\x8d\x08\xd6\x70\xa8\x1b\x9c\xa6\x12\x2e\x5b\xb8\x47\x7b\xa4\xcb"
"\xb3\x26\xd5\x78\x65\xa8\xbd\xc2\x07\x0f\x25\x78\x1c\xa6\x5d\x1d\xe7\x26\x4e\x63\x5d\xfa\x47\x36\x44\x1d\x0d\x9a\xf7\xc5\xca\x7c\xa1\x46\xaf\x29\xc5\x37\xb6\xbe"
"\xe4\x4b\x3e\xa3\xe4\x8b\xa4\xfb\x26\x8d\x2f\xf9\x14\xfe\x2f\x10\xdd\x37\x45\x7d\xaa\xfd\x97\xa9\xeb\xdf\xaf\x2a\xc9\x5f\x21\xa8\xe4\x1f\xe8\xfd\xdf\xde\x66\x0c"
"\xe9\x2f\xb6\x8a\x88\xbe\x33\x3b\x88\xa7\x90\x41\x04\x31\xce\xf5\xbd\xcc\x8d\xa0\x88\xe3\x03\xbe\xa6\x7f\x5c\x4a\xcb\x3c\x5e\xb4\x2c\xd7\x14\xdb\x26\x04\x91\xb5"
"\x80\x24\x62\x65\xa9\xe7\x73\xea\x6a\x97\x51\x83\xce\x4b\x9a\x65\x29\x6f\x4c\x5f\x2e\xb4\xc4\x2d\x1f\x87\xdc\x6b\x1c\x71\xaf\x15\xd0\xe8\x38\xa0\x46\x5b\x43\x44"
"\x0b\xf5\x50\xe1\x45\xa6\xa6\xe8\x4f\x3f\xa2\x45\xba\xb8\x5e\x6a\x30\x7e\x6e\xc3\x7a\x79\x68\xfc\x08\x97\x91\x6f\x41\x51\x67\x79\xe4\x65\x60\x1d\xef\xdc\xb2\x57"
"\xf8\xba\x32\x41\x84\x27\x2b\x4f\xc6\x8d\xb2\xab\x7f\x26\xbd\xa6\xfe\x79\x72\x96\xba\xce\x44\xff\xcb\x93\xe2\x0a\x6b\xef\x85\xbd\x88\x25\xf3\xd6\xa0\x93\x9d\xb4"
"\xfe\xb4\xe6\x6a\xe9\x26\xc4\x68\x4a\xbc\xcb\x7c\xae\xf4\xd7\x8d\xbf\x8b\x61\xb8\x7a\x91\x7a\x56\x6b\x20\x5b\xe3\x19\xaf\x43\xd0\x90\xbf\x8b\x00\xde\x84\x92\x36"
"\xc8\xb9\xd9\xae\x66\xea\x36\x50\x56\x03\xf9\x9a\x19\xc3\x1b\x15\xef\x26\x89\xe8\x39\x20\xce\x43\x46\x92\x74\x99\x8d\xce\xe1\x69\xe9\x5a\x96\x36\x60\xee\xbc\x5e"
"\x91\x4e\xe2\x6d\x2d\x5e\x51\x8f\xc4\xa5\x21\xcf\x37\x52\x0f\x63\xed\xa9\xdc\xab\x9c\x3d\x96\x8a\xa4\xd1\x22\xee\xac\x1a\xcc\xa4\x86\x6d\xb8\xf0\x91\x79\x58\x08"
"\x37\xe9\x64\x4d\xec\x6f\x2e\xea\x6e\x49\xb7\x3c\x4d\x7c\xc3\xee\xb9\x2f\xfe\x46\x99\xb9\xc8\xef\x52\xc6\xf3\xd6\x45\xbe\xc6\x75\xe6\xd5\xa6\xc8\x1a\xbf\x91\xba"
"\x4d\x88\xc6\xcc\x85\xac\x85\x2b\x09\xb9\x68\xc0\x6d\xa1\x34\xce\xd3\xcf\xdb\x22\x70\x33\x22\x0d\x59\xc2\x8a\x68\x0e\x2c\x79\x5a\x8f\x16\xac\x93\x6b\x80\x53\x2d"
"\x05\x32\x32\x77\xec\xd2\xd4\x36\x9f\xd5\x28\x96\x69\x9c\xf3\x34\xa0\xe3\x4e\xd1\xa6\x03\x65\x3b\x33\x22\x84\x99\xe4\xcc\x3c\x06\x74\xa7\x33\x33\x1e\xe8\x9c\x33"
"\x63\x05\x1a\xee\xcc\xbc\x00\xb4\xc6\x99\x29\x03\x12\x9c\x99\xcf\xc4\x07\xce\x8c\x1c\xe8\x27\x67\x66\x39\xd0\x29\x67\xa6\x07\x50\x57\x67\x66\x0e\xd0\x62\xd7\x76"
"\xc6\x3b\x33\xbd\x80\xba\x3b\x33\x6a\xa0\x20\x47\x46\xed\x89\x8a\x5d\xba\xba\xb1\x11\xf4\x76\x53\x20\xa1\x37\x3a\xd2\x60\x26\x36\x6e\x72\x91\xb7\x31\x6a\x06\x11"
"\x48\x9a\x98\x78\xde\xd6\xa7\xb7\x75\xe0\x75\x4d\x35\xbe\xfa\x1a\xb4\xec\x1d\xf5\x48\x1d\xdf\x7b\x35\x8d\x8c\xc4\x32\xe4\x55\xf1\x1a\xc5\x8f\x7e\x4d\x6f\x29\x1e"
"\x9d\x40\x6e\x60\x11\x79\xc7\x99\x7e\x44\xc3\x75\xed\x0e\x53\x44\xfe\x2e\xe6\xf3\xef\x2d\xe5\xbe\x3d\xb6\x68\xf3\xf5\xb6\x3f\xc8\x9a\xf0\xc2\xa4\x8d\xda\x73\xcf"
"\xa9\xe6\xcd\xbc\xbb\x08\xee\x61\x47\xa9\x7a\x3b\x2a\x6d\xb4\x65\xcf\xe1\x6d\xcc\xf4\x35\xee\xfd\x48\xaf\xc9\x39\x73\x95\xf7\xfa\xbd\x8d\xe6\x9d\x02\xd2\x43\x19"
"\x21\x6e\x9b\xca\xbf\xc9\x8b\xbe\xb1\xed\xaa\xa5\x5b\x58\xe3\x5e\x6f\x73\x03\x87\xae\xc7\xe0\xde\xb0\x55\xaf\x90\x10\xd7\xbb\x49\x34\xe9\x0b\x4b\xfe\xe7\x5c\x64"
"\x4f\xcf\xe7\x9f\x73\x83\xaf\xcf\x0f\xf5\xde\xa6\xcb\xd3\x06\x75\x3d\xde\x7a\x85\xd0\xd2\x86\x8b\xaf\x65\xe3\xdf\xc0\xeb\xf3\xb4\x32\x5a\x0f\x2b\xd3\x02\x04\x7b"
"\xed\xcb\xff\x2f\x34\x5d\x37\x8b\x9a\x03\xd1\xfe\x5e\x6c\x51\xa3\xa0\xda\x07\xa6\x5b\x06\xa6\xaf\xcc\xce\x23\x97\x29\xa6\x71\xd3\xdd\x12\x20\xdd\x30\x96\xda\xc2"
"\x38\xea\x95\x3a\x19\xd7\x39\x77\xdc\x67\x80\xf2\x3a\x4c\xfa\x5f\x4d\x52\x37\x14\x29\xf3\x74\x79\x3d\x00\xff\xdf\x58\xa7\xe1\xb0\x7b\x98\x0b\x0f\xbf\xa5\x49\x4b"
"\xdd\x78\xa0\xb1\xa5\x1c\xfd\xbc\x07\x53\x9a\x0b\x69\x34\xf5\xd8\x23\xf2\xd4\x9c\x2c\x7e\x8d\x1b\x62\x69\x63\x1e\x8f\x5f\x63\x1b\xdf\x8d\x15\xf3\xf2\xf8\xba\xbb"
"\x23\x69\x18\x77\x6a\xe1\xc0\x34\x5e\xbb\x05\x11\x68\xaf\x91\xe6\x86\x17\x57\xfb\xec\x4d\xb4\x16\x39\xba\x3e\xb7\xf5\x7f\xc2\x6d\xf5\x88\x87\x4b\xff\x0a\x83\xde"
"\x10\x8d\x78\x85\xc6\x5e\xa6\x9f\xc7\xbc\xbc\xb1\xf0\xbb\x2f\xc8\xfe\x8f\x05\xd9\x65\x9e\x26\xa4\x45\x01\xf0\x26\x97\x98\x2b\x7a\xf7\x16\xf0\x96\x35\x66\x83\x9a"
"\x18\xc1\x66\xfc\xac\x26\x19\xb8\x77\x52\xd6\xf2\xe3\x83\xff\xa6\x73\x04\xaf\xea\x69\x72\x63\x6d\xf6\xcc\xc1\xff\xb2\xe3\x26\x6d\x74\xd3\x6e\x22\xa6\xd3\x92\x03"
"\x83\x9b\x1b\x67\x71\x41\xbd\xd7\xe0\xbc\x36\x80\xfd\xd7\x1b\x48\xf0\xea\xf7\x5e\x0d\x35\xde\x00\x98\xbe\x06\xcd\x5e\xc3\xa7\x23\xd2\x6b\x1a\x8b\x7d\x37\x21\x9c"
"\xe4\x75\x93\xf1\xfc\x5e\xa1\x11\x70\xd2\xc8\x7b\x6f\xe6\xa7\x39\x0e\xb2\x96\x04\x04\xa4\x4d\xef\xf4\x4d\x86\x6f\x6e\x1e\xdf\x26\x78\xb7\xb8\xbb\x8d\xac\x84\xff"
"\x8e\xee\x36\x37\xfe\x37\xaf\x2b\x7f\x51\x77\x1a\xb4\x81\x3c\x03\x79\x54\xf3\xa6\xdf\xab\x29\xf2\x6f\x2c\x5c\xef\xe1\x10\xb8\x1b\x25\xa5\x4b\x30\x4e\xda\x82\xa8"
"\xd8\x75\x46\xf0\xbd\xd9\x28\xbf\x7f\x55\x98\xdd\x25\x92\xee\xc5\x64\xdd\x78\xf4\xfb\xfa\x76\x11\xd9\xbf\x20\x1a\xed\x39\x69\xe4\xce\x49\x23\x6d\xc1\x3e\x25\x6b"
"\x7e\x86\xb8\xcf\x8e\xeb\xd8\x97\xae\x2f\x48\xdb\xd4\x59\x87\xd4\x73\xed\x35\xf0\x85\xaf\xf1\x30\x5a\xd6\x5c\xa8\xb9\xa9\x55\xea\xaa\xf0\xe6\x96\xdc\xb5\x1f\x4f"
"\x36\x77\x1e\xe9\x0e\xa4\x1a\x83\x4d\xd7\x88\x7a\xbc\x09\xda\xdc\xc8\xbb\x0d\x98\x5f\xa3\x1a\x53\x5c\xb5\x6b\x5e\x67\x64\x0b\x6d\x97\x37\x19\xa5\xd7\xa7\xc5\x1b"
"\xb3\x62\xd7\x07\x66\x5b\x76\xd2\x21\xf5\x7a\xe0\xd3\xe8\x88\xba\xb9\x01\xee\xbb\xc1\xf5\x2e\x6a\xc9\xf5\x22\xe3\xe6\xcc\xb6\xb4\x85\x60\xc9\x1b\x7a\x24\x25\xcd"
"\xf9\x5e\x5e\xd6\x2e\x71\x25\x66\x78\x65\xdd\x36\x6b\x25\xaf\x7d\xa2\x2d\x69\xe9\x54\x69\xec\x44\xef\x8a\xa8\x5e\x57\xa9\x17\x81\x9a\x35\x0c\x6e\x4c\x1b\xed\x7d"
"\x23\x26\x6a\xc9\x35\x98\x6a\xe4\x12\xa0\x68\xba\x23\x37\xb2\xc9\x34\x6a\x6e\x28\x67\xe5\x5e\xa3\x92\x2f\xdf\x4d\x26\x7c\xe9\xaf\x48\x9d\xdd\xf2\xe9\xcd\x96\x68"
"\x39\xaf\xeb\x49\x1d\x81\xf8\x6b\x28\xff\xa7\x6f\x08\x7d\xc9\x97\x7c\xc9\x97\x7c\xe9\x7f\x32\xfd\xd7\x82\xa5\xce\x4d\x40\x87\x4e\xd7\xc1\xef\x30\x51\xff\xf7\x59"
"\xaf\x30\x3c\xec\xcc\x3c\xeb\x9a\xe1\x5d\x11\xc6\x61\xf7\xcc\x61\xd7\xbf\xf4\xca\xbb\x72\xeb\xec\xca\xe0\xb0\xeb\x9b\x43\xae\x75\x1a\x48\xc0\xbb\xbe\xe9\xec\xda"
"\x4e\x03\x6e\x0d\x32\x87\x5a\x88\x92\xc4\x24\xfc\x53\x30\xd1\x97\x7c\xc9\x67\x47\xff\xee\x55\xd7\xd9\xb7\xa9\xfc\x77\x7a\xd5\xff\x00\x97\x8e\x84\xeb\xa6\xe2\x4b"
"\xbe\xe4\x33\xb9\xff\x23\x96\xc7\x07\xa6\x6f\x10\x4c\x77\x01\xca\x02\xc2\x37\x36\x16\xe9\x37\x64\xcd\xaf\xd5\x6c\x67\xb9\x5d\x7d\xd0\xdd\x97\x7c\xc9\x97\x7c\xc9"
"\x97\x7c\xc9\x97\x7c\xc9\x97\xfe\xbf\x39\x6e\x7f\xef\xb7\x09\x3e\xbf\xb7\xf3\x35\x7a\x9e\xbe\xe4\x4b\xbe\xe4\xb3\xe9\xff\x8a\x30\x7b\x67\x5f\x24\xdd\x37\x69\x7c"
"\x0a\xff\xc7\x8e\xb5\xfe\x21\xd1\xff\x75\x53\xf4\x9f\x3b\xd5\xfd\x7f\xa8\xda\xce\xff\x45\xd8\x9f\x6a\x98\x9d\xfc\x2f\x3e\x7d\xa6\xae\x4d\xd0\xce\x7f\x05\xd3\x9b"
"\xd2\x23\xea\x6f\xd1\xb8\x4b\xfa\x3f";

enum {
    kSpvMeshVs, kSpvMeshFs, kSpvDepthVs, kSpvMatVs, kSpvMatFs, kSpvSkyVs,
    kSpvSkyFs, kSpvPartVs, kSpvPartFs, kSpvBlitVs, kSpvBlitFs, kSpvTexVs,
    kSpvTexFs, kVs, kFs, kOvVs, kOvFs,
    kSpvCount
};

// Where each shader starts, in words; the last is the end of the last one.
static const uint32_t kSpvAt[kSpvCount + 1] = {
    0, 2349, 3701, 3970, 5096, 8609, 8928, 10101, 10666, 11035,
    11222, 11341, 11688, 11991, 12162, 12247, 12363, 12476
};

// All seventeen, inflated on first use and kept: the scene is made again when
// the device is, and each time it needs the same words. Null if they cannot
// be had, and then no shader module is made and the renderer stays closed.
inline const uint32_t* spv_all() {
    static uint32_t* words = nullptr;
    if (words) return words;
    const size_t n = kSpvAt[kSpvCount];
    unsigned char* planes = (unsigned char*)malloc(n * 4);
    uint32_t* w = (uint32_t*)malloc(n * 4);
    if (planes && w && __nexa_inflate(kSpvZ, sizeof(kSpvZ) - 1, planes, n * 4)) {
        for (size_t i = 0; i < n; ++i)
            w[i] = (uint32_t)planes[i] | ((uint32_t)planes[n + i] << 8) |
                   ((uint32_t)planes[2 * n + i] << 16) | ((uint32_t)planes[3 * n + i] << 24);
        words = w;
        w = nullptr;
    }
    free(planes);
    free(w);
    return words;
}

inline const uint32_t* spv(int which) {
    const uint32_t* all = spv_all();
    return all ? all + kSpvAt[which] : nullptr;
}

inline size_t spv_words(int which) { return kSpvAt[which + 1] - kSpvAt[which]; }


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
// [nexa:vkshader-begin]
    // gfx3d.shader on the textured path. Textured geometry is drawn by
    // draw_tex, which is a pipeline of its own -- so without these a custom
    // shader would be skipped for anything carrying a texture while shading
    // everything else, which is not a rule anybody could guess.
    uint64_t pipeCustomTex[2] = {};
// [nexa:vkshader-end]
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
    vp.w = 4096; vp.h = 4096; vp.maxd = 1;
    d.CmdSetViewport(d.cmd, 0, 1, &vp);
    struct Sc { int32_t x, y; uint32_t w, h; } sc{ 0, 0, 4096, 4096 };
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
    if (g.color.image && g.pipeMesh[0] && g.rw == w && g.rh == h && g.reqSamples == want) return 1;
    /* The frame that just finished can still be reading the old picture.
       Destroying it first loses the device, and the view stays black. */
    if ((g.color.image || g.depth.image || g.resolve.image) && d.DeviceWaitIdle)
        d.DeviceWaitIdle(d.device);
    destroy_target(d);
    uint32_t tried = want;
    for (;;) {
        uint32_t usage = tried > 1 ? (0x10u | 0x1u) : (0x10u | 0x4u);
        int ok = make_image(d, g.color, 37, (uint32_t)w, (uint32_t)h, tried, usage, 1);
        if (ok) {
            g.samples = tried;
            ok = tried <= 1 || make_image(d, g.resolve, 37, (uint32_t)w, (uint32_t)h, 1, 0x2 | 0x4, 1);
        }
        if (ok) ok = make_image(d, g.depth, g.depthFmt, (uint32_t)w, (uint32_t)h, g.samples, 0x20, g.depthAspect) ? 1 : 0;
        int stencil = g.depthAspect == 6;
        if (ok) ok = make_pass(d, 37, g.depthFmt, g.samples, 1, stencil, &g.passC);
        if (ok) ok = make_pass(d, 37, g.depthFmt, g.samples, 0, stencil, &g.passL);
        if (ok) ok = make_fb(d, g.passC, g.color.view, g.depth.view, (uint32_t)w, (uint32_t)h, &g.fbC);
        if (ok) ok = make_fb(d, g.passL, g.color.view, g.depth.view, (uint32_t)w, (uint32_t)h, &g.fbL);
        if (ok) ok = make_scene_pipes(d);
        if (ok) {
            g.rw = w; g.rh = h; g.reqSamples = want;
            g.colorLayout = 0; g.depthLayout = 0; g.resolveLayout = 0;
            return 1;
        }
        destroy_target(d);
        if (tried == 8) tried = 4;
        else if (tried == 4) tried = 2;
        else if (tried == 2) tried = 1;
        else { fail("color image"); return 0; }
    }
}

inline int ensure_shadow(Dev& d) {
    Gpu& g = gpu();
    if (g.shadow.image && g.pipeShadow) return 1;
    if (!make_image(d, g.shadow, 126, 4096, 4096, 1, 0x20 | 0x4, 2)) { fail("shadow image"); return 0; }
    struct Att { uint32_t flags, format, samples, load, store, sload, sstore, initial, final; } at{};
    at.format = 126; at.samples = 1; at.load = 1; at.store = 0; at.sload = 2; at.sstore = 1; at.initial = 3; at.final = 5;
    struct Ref { uint32_t attachment, layout; } depth{ 0, 3 };
    struct Sub { uint32_t flags, bind, inCount; const void* in; uint32_t colorCount; const void* colors; const void* resolve; const Ref* depth; uint32_t preserve; const void* p; } sub{};
    sub.depth = &depth;
    struct RP { uint32_t sType; const void* pNext; uint32_t flags; uint32_t ac; const Att* a; uint32_t sc; const Sub* s; uint32_t dc; const void* dep; } rp{};
    rp.sType = 38; rp.ac = 1; rp.a = &at; rp.sc = 1; rp.s = &sub;
    if (d.CreateRenderPass(d.device, &rp, nullptr, &g.passSh) != 0) { fail("shadow pass"); return 0; }
    if (!make_fb(d, g.passSh, g.shadow.view, 0, 4096, 4096, &g.fbSh)) { fail("shadow fb"); return 0; }
    const At atv[] = { {0, 0, 106, 0} };
    if (!pipe_ex(d, g.sh[2], 0, 3, 0, 1, 1, 1, 0, 1, 1, 2.0f, 1.5f, 0, 0, 36, atv, 1, 0, g.layoutDepth, g.passSh, &g.pipeShadow)) { fail("shadow pipe"); return 0; }
    g.shadowLayout = 0;
    return 1;
}

// [nexa:vkshader-begin]
// gfx3d.shader on the textured path. The modules are the same two the
// untextured pipelines were built from; only the vertex layout and the layout
// object differ, so a shader written to the documented contract -- position and
// colour, and the push-constant block -- is valid on both. It cannot read the
// texture: declaring a sampler would make it invalid on the untextured
// pipelines, which have no descriptor set.
inline void custom_tex_down(Dev& d) {
    Gpu& g = gpu();
    for (int i = 0; i < 2; ++i) {
        if (g.pipeCustomTex[i] && d.DestroyPipeline) d.DestroyPipeline(d.device, g.pipeCustomTex[i], nullptr);
        g.pipeCustomTex[i] = 0;
    }
}

inline int custom_tex_pipes(Dev& d) {
    Gpu& g = gpu();
    custom_tex_down(d);
    if (!d.shCustomTex[0] || !d.shCustomTex[1] || !g.layoutTex) return 1;
    const At texAt[] = { {0, 0, 106, 0}, {1, 0, 106, 12}, {2, 0, 103, 24} };
    if (!pipe_ex(d, d.shCustomTex[0], d.shCustomTex[1], 3, 2, 0, 1, 1, 0, 1, 0, 0, 0, 0, 0xF, 32, texAt, 3, 1, g.layoutTex, d.pass, &g.pipeCustomTex[0]) ||
        !pipe_ex(d, d.shCustomTex[0], d.shCustomTex[1], 3, 0, 0, 1, 1, 0, 1, 0, 0, 0, 0, 0xF, 32, texAt, 3, 1, g.layoutTex, d.pass, &g.pipeCustomTex[1])) {
        custom_tex_down(d);
        return 0;
    }
    return 1;
}
// [nexa:vkshader-end]
inline void destroy_pipes_swap(Dev& d) {
    Gpu& g = gpu();
    if (g.pipeBlit && d.DestroyPipeline) { d.DestroyPipeline(d.device, g.pipeBlit, nullptr); g.pipeBlit = 0; }
    for (int i = 0; i < 2; ++i) if (g.pipeTex[i] && d.DestroyPipeline) { d.DestroyPipeline(d.device, g.pipeTex[i], nullptr); g.pipeTex[i] = 0; }
// [nexa:vkshader-begin]
    custom_tex_down(d);
// [nexa:vkshader-end]
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
// [nexa:vkshader-begin]
    custom_tex_pipes(d);
// [nexa:vkshader-end]
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
    // The first thirteen of nexa_vk14_spv.inc, kSpvMeshVs to kSpvTexFs, in g.sh's order.
    for (int i = 0; i < 13; ++i) if (!shader_mod(d, spv(kSpvMeshVs + i), spv_words(kSpvMeshVs + i), &g.sh[i])) { fail("scene shader"); scene_down(d); return 0; }
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
// [nexa:vkshader-begin]
    // One wide range rather than two narrow ones: a custom shader declares the
    // same mat4-plus-sixteen-floats block it declares on the untextured path,
    // and a block has to fit inside a range that names its stage. The built-in
    // textured shader still reads uMVP at 0 and its cutout flag at 64, which
    // this range covers as well -- so both fit, and every push into it names
    // both stages.
    if (!layout_of(d, oneB, 1, 1 | 16, 0, 128, 0, 0, 0, 0, &g.slTex, &g.layoutTex)) { fail("tex layout"); scene_down(d); return 0; }
// [nexa:vkshader-end]
// [nexa:novkshader-begin]
    if (!layout_of(d, oneB, 1, 1, 0, 64, 1, 0x10, 64, 4, &g.slTex, &g.layoutTex)) { fail("tex layout"); scene_down(d); return 0; }
// [nexa:novkshader-end]
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
    if (!buffer_alloc(d, 2048ull * 512ull, 0x10, &g.drawBuf, &g.drawMem, &g.drawMap)) { fail("draw ubo"); scene_down(d); return 0; }
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
    if (!ensure_target(d, rw, rh, samples) || !g.pipeMesh[0]) return 0;
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
    begin_pass(d, g.passSh, g.fbSh, 4096, 4096, 1, 1, 1, 1);
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
    if (!draw || g.drawN >= 2048 || !g.drawMap) return -1;
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
    begin_pass(d, g.passSh, g.fbSh, 4096, 4096, 1, 1, 1, 1);
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
// [nexa:vkshader-begin]
    // The custom pipeline stands in for the built-in one of the same kind, and
    // what goes up after the transform is whichever block that shader reads:
    // the author's sixteen floats, or the cutout flag.
    const int useCustom = d.customOn && g.pipeCustomTex[pipe];
    d.CmdBindPipeline(d.cmd, 0, useCustom ? g.pipeCustomTex[pipe] : g.pipeTex[pipe]);
    d.CmdBindDescriptorSets(d.cmd, 0, g.layoutTex, 0, 1, &set, 0, nullptr);
    d.CmdPushConstants(d.cmd, g.layoutTex, 1 | 16, 0, 64, mvp);
    if (useCustom) {
        // The sixteenth float is uTexOn -- the renderer telling the shader a
        // texture is bound. The other fifteen are the author's.
        float block[16];
        std::memcpy(block, d.userPC, sizeof block);
        block[15] = 1.0f;
        d.CmdPushConstants(d.cmd, g.layoutTex, 1 | 16, 64, 64, block);
    }
    else d.CmdPushConstants(d.cmd, g.layoutTex, 1 | 16, 64, 4, &cut);
// [nexa:vkshader-end]
// [nexa:novkshader-begin]
    d.CmdBindPipeline(d.cmd, 0, g.pipeTex[pipe]);
    d.CmdBindDescriptorSets(d.cmd, 0, g.layoutTex, 0, 1, &set, 0, nullptr);
    d.CmdPushConstants(d.cmd, g.layoutTex, 1, 0, 64, mvp);
    d.CmdPushConstants(d.cmd, g.layoutTex, 0x10, 64, 4, &cut);
// [nexa:novkshader-end]
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


inline int& keep_epoch() {
    static int n = 1;
    return n;
}

// [nexa:vkshader-begin]
inline void custom_down(Dev& d);   // defined with the rest of gfx3d.shader, below
// [nexa:vkshader-end]
inline void shutdown() {
    keep_epoch()++;
    Dev& d = dev();
    if (d.device && d.DeviceWaitIdle) d.DeviceWaitIdle(d.device);
    scene_down(d);
    d.framing = 0;
    destroy_swapchain(d);
    if (d.ovView && d.DestroyImageView) d.DestroyImageView(d.device, d.ovView, nullptr);
    if (d.ovImg && d.DestroyImage) d.DestroyImage(d.device, d.ovImg, nullptr);
    if (d.ovMem && d.FreeMemory) d.FreeMemory(d.device, d.ovMem, nullptr);
    if (d.readMap && d.UnmapMemory) d.UnmapMemory(d.device, d.readMem);
    if (d.readBuf && d.DestroyBuffer) d.DestroyBuffer(d.device, d.readBuf, nullptr);
    if (d.readMem && d.FreeMemory) d.FreeMemory(d.device, d.readMem, nullptr);
    d.readMap = nullptr; d.readBuf = d.readMem = 0; d.readCap = 0;
    if (d.ovMap && d.UnmapMemory) d.UnmapMemory(d.device, d.ovStageMem);
    if (d.ovStage && d.DestroyBuffer) d.DestroyBuffer(d.device, d.ovStage, nullptr);
    if (d.ovStageMem && d.FreeMemory) d.FreeMemory(d.device, d.ovStageMem, nullptr);
    if (d.smap && d.UnmapMemory) d.UnmapMemory(d.device, d.smem);
    if (d.sbuf && d.DestroyBuffer) d.DestroyBuffer(d.device, d.sbuf, nullptr);
    if (d.smem && d.FreeMemory) d.FreeMemory(d.device, d.smem, nullptr);
    d.smap = nullptr; d.sbuf = d.smem = 0; d.scap = d.sused = 0;
    if (d.vmap && d.UnmapMemory) d.UnmapMemory(d.device, d.vmem);
    if (d.vbuf && d.DestroyBuffer) d.DestroyBuffer(d.device, d.vbuf, nullptr);
    if (d.vmem && d.FreeMemory) d.FreeMemory(d.device, d.vmem, nullptr);
    if (d.quad && d.DestroyBuffer) d.DestroyBuffer(d.device, d.quad, nullptr);
    if (d.quadMem && d.FreeMemory) d.FreeMemory(d.device, d.quadMem, nullptr);
// [nexa:vkshader-begin]
    custom_down(d);
// [nexa:vkshader-end]
    if (d.pipeTri && d.DestroyPipeline) d.DestroyPipeline(d.device, d.pipeTri, nullptr);
    if (d.pipeTwo && d.DestroyPipeline) d.DestroyPipeline(d.device, d.pipeTwo, nullptr);
    if (d.pipeLine && d.DestroyPipeline) d.DestroyPipeline(d.device, d.pipeLine, nullptr);
    if (d.pipeOv && d.DestroyPipeline) d.DestroyPipeline(d.device, d.pipeOv, nullptr);
    if (d.layout && d.DestroyPipelineLayout) d.DestroyPipelineLayout(d.device, d.layout, nullptr);
    if (d.layoutOv && d.DestroyPipelineLayout) d.DestroyPipelineLayout(d.device, d.layoutOv, nullptr);
    if (d.pass && d.DestroyRenderPass) d.DestroyRenderPass(d.device, d.pass, nullptr);
    if (d.passOv && d.DestroyRenderPass) d.DestroyRenderPass(d.device, d.passOv, nullptr);
    if (d.shVs && d.DestroyShaderModule) d.DestroyShaderModule(d.device, d.shVs, nullptr);
    if (d.shFs && d.DestroyShaderModule) d.DestroyShaderModule(d.device, d.shFs, nullptr);
    if (d.shOvVs && d.DestroyShaderModule) d.DestroyShaderModule(d.device, d.shOvVs, nullptr);
    if (d.shOvFs && d.DestroyShaderModule) d.DestroyShaderModule(d.device, d.shOvFs, nullptr);
    if (d.descPool && d.DestroyDescriptorPool) d.DestroyDescriptorPool(d.device, d.descPool, nullptr);
    if (d.setLayout && d.DestroyDescriptorSetLayout) d.DestroyDescriptorSetLayout(d.device, d.setLayout, nullptr);
    if (d.sampler && d.DestroySampler) d.DestroySampler(d.device, d.sampler, nullptr);
    if (d.pool && d.DestroyCommandPool) d.DestroyCommandPool(d.device, d.pool, nullptr);
    if (d.fence && d.DestroyFence) d.DestroyFence(d.device, d.fence, nullptr);
    if (d.semImg && d.DestroySemaphore) d.DestroySemaphore(d.device, d.semImg, nullptr);
    if (d.semDraw && d.DestroySemaphore) d.DestroySemaphore(d.device, d.semDraw, nullptr);
    if (d.device && d.DestroyDevice) d.DestroyDevice(d.device, nullptr);
    if (d.surface && d.DestroySurfaceKHR && d.instance) d.DestroySurfaceKHR(d.instance, d.surface, nullptr);
    if (d.instance && d.DestroyInstance) d.DestroyInstance(d.instance, nullptr);
    if (d.dll) FreeLibrary(d.dll);
    d = Dev{};
}

inline int open(void* hwnd, int vsyncOn) {
    shutdown();
    last_error() = "";
    Dev& d = dev();
    d.vsync = vsyncOn ? 1 : 0;
    if (!hwnd) { fail("hwnd"); return 0; }
    d.dll = LoadLibraryA("vulkan-1.dll");
    if (!d.dll) { fail("vulkan-1.dll"); return 0; }
    d.GetInstanceProcAddr = (Dev::gp_t)GetProcAddress(d.dll, "vkGetInstanceProcAddr");
    if (!d.GetInstanceProcAddr) { fail("vkGetInstanceProcAddr"); shutdown(); return 0; }
    d.EnumerateInstanceVersion = (decltype(d.EnumerateInstanceVersion))GetProcAddress(d.dll, "vkEnumerateInstanceVersion");
    uint32_t ver = 0;
    if (!d.EnumerateInstanceVersion || d.EnumerateInstanceVersion(&ver) != 0 || ver < kApi14) {
        fail("vulkan 1.4"); shutdown(); return 0;
    }
    d.CreateInstance = (decltype(d.CreateInstance))d.GetInstanceProcAddr(nullptr, "vkCreateInstance");
    if (!d.CreateInstance) { fail("vkCreateInstance"); shutdown(); return 0; }

    struct App { uint32_t sType; const void* pNext; const char* name; uint32_t appVer; const char* engine; uint32_t engVer; uint32_t api; } app{};
    app.sType = 0; app.name = "nexa"; app.engine = "nexa"; app.api = kApi14;
    const char* exts[] = { "VK_KHR_surface", "VK_KHR_win32_surface" };
    struct CI { uint32_t sType; const void* pNext; uint32_t flags; const App* app; uint32_t layerCount; const void* layers; uint32_t extCount; const char* const* exts; } ci{};
    ci.sType = 1; ci.app = &app; ci.extCount = 2; ci.exts = exts;
    if (d.CreateInstance(&ci, nullptr, &d.instance) != 0 || !d.instance) { fail("instance"); shutdown(); return 0; }

    if (!load_inst(d, d.DestroyInstance, "vkDestroyInstance")) { shutdown(); return 0; }
    if (!load_inst(d, d.EnumeratePhysicalDevices, "vkEnumeratePhysicalDevices")) { shutdown(); return 0; }
    if (!load_inst(d, d.GetPhysicalDeviceProperties, "vkGetPhysicalDeviceProperties")) { shutdown(); return 0; }
    if (!load_inst(d, d.GetPhysicalDeviceQueueFamilyProperties, "vkGetPhysicalDeviceQueueFamilyProperties")) { shutdown(); return 0; }
    if (!load_inst(d, d.GetPhysicalDeviceSurfaceSupportKHR, "vkGetPhysicalDeviceSurfaceSupportKHR")) { shutdown(); return 0; }
    if (!load_inst(d, d.GetPhysicalDeviceSurfaceFormatsKHR, "vkGetPhysicalDeviceSurfaceFormatsKHR")) { shutdown(); return 0; }
    if (!load_inst(d, d.GetPhysicalDeviceSurfaceCapabilitiesKHR, "vkGetPhysicalDeviceSurfaceCapabilitiesKHR")) { shutdown(); return 0; }
    if (!load_inst(d, d.GetPhysicalDeviceSurfacePresentModesKHR, "vkGetPhysicalDeviceSurfacePresentModesKHR")) { shutdown(); return 0; }
    if (!load_inst(d, d.GetPhysicalDeviceMemoryProperties, "vkGetPhysicalDeviceMemoryProperties")) { shutdown(); return 0; }
    if (!load_inst(d, d.GetPhysicalDeviceFormatProperties, "vkGetPhysicalDeviceFormatProperties")) { shutdown(); return 0; }
    if (!load_inst(d, d.EnumerateDeviceExtensionProperties, "vkEnumerateDeviceExtensionProperties")) { shutdown(); return 0; }
    if (!load_inst(d, d.CreateWin32SurfaceKHR, "vkCreateWin32SurfaceKHR")) { shutdown(); return 0; }
    if (!load_inst(d, d.DestroySurfaceKHR, "vkDestroySurfaceKHR")) { shutdown(); return 0; }
    if (!load_inst(d, d.CreateDevice, "vkCreateDevice")) { shutdown(); return 0; }
    if (!load_inst(d, d.GetDeviceProcAddr, "vkGetDeviceProcAddr")) { shutdown(); return 0; }

    struct Surf { uint32_t sType; const void* pNext; uint32_t flags; HINSTANCE inst; HWND hwnd; } surf{};
    surf.sType = 1000009000; surf.inst = GetModuleHandleA(nullptr); surf.hwnd = (HWND)hwnd;
    if (d.CreateWin32SurfaceKHR(d.instance, &surf, nullptr, &d.surface) != 0) { fail("surface"); shutdown(); return 0; }

    uint32_t dc = 0;
    d.EnumeratePhysicalDevices(d.instance, &dc, nullptr);
    if (!dc) { fail("device"); shutdown(); return 0; }
    std::vector<void*> phys(dc);
    d.EnumeratePhysicalDevices(d.instance, &dc, phys.data());
    d.phys = nullptr;
    for (void* p : phys) {
        uint32_t props[256] = {};
        d.GetPhysicalDeviceProperties(p, props);
        if (props[0] < kApi14) continue;
        uint32_t qc = 0;
        d.GetPhysicalDeviceQueueFamilyProperties(p, &qc, nullptr);
        std::vector<uint32_t> qf((size_t)qc * 6);
        d.GetPhysicalDeviceQueueFamilyProperties(p, &qc, qf.data());
        for (uint32_t i = 0; i < qc; ++i) {
            if ((qf[i * 6] & 1u) == 0) continue;
            uint32_t support = 0;
            d.GetPhysicalDeviceSurfaceSupportKHR(p, i, d.surface, &support);
            if (!support) continue;
            d.phys = p;
            d.qfam = i;
            break;
        }
        if (d.phys) break;
    }
    if (!d.phys) { fail("vulkan 1.4 device"); shutdown(); return 0; }

    uint32_t ec = 0;
    d.EnumerateDeviceExtensionProperties(d.phys, nullptr, &ec, nullptr);
    std::vector<char> names((size_t)ec * 260);
    int hasSwap = 0;
    if (ec) {
        d.EnumerateDeviceExtensionProperties(d.phys, nullptr, &ec, names.data());
        for (uint32_t i = 0; i < ec; ++i) {
            if (std::strcmp(names.data() + (size_t)i * 260, "VK_KHR_swapchain") == 0) hasSwap = 1;
        }
    }
    if (!hasSwap) { fail("swapchain"); shutdown(); return 0; }

    float prio = 1.0f;
    struct Q { uint32_t sType; const void* pNext; uint32_t flags; uint32_t family; uint32_t count; const float* prio; } q{};
    q.sType = 2; q.family = d.qfam; q.count = 1; q.prio = &prio;
    const char* dext[] = { "VK_KHR_swapchain" };
    struct DI { uint32_t sType; const void* pNext; uint32_t flags; uint32_t qc; const Q* q; uint32_t lc; const void* layers; uint32_t ec; const char* const* exts; const void* features; } di{};
    di.sType = 3; di.qc = 1; di.q = &q; di.ec = 1; di.exts = dext;
    if (d.CreateDevice(d.phys, &di, nullptr, &d.device) != 0 || !d.device) { fail("CreateDevice"); shutdown(); return 0; }

#define NEXA_LOAD(fn, name) if (!load_dev(d, d.fn, name)) { shutdown(); return 0; }
    NEXA_LOAD(DestroyDevice, "vkDestroyDevice")
    NEXA_LOAD(GetDeviceQueue, "vkGetDeviceQueue")
    NEXA_LOAD(DeviceWaitIdle, "vkDeviceWaitIdle")
    NEXA_LOAD(CreateSwapchainKHR, "vkCreateSwapchainKHR")
    NEXA_LOAD(DestroySwapchainKHR, "vkDestroySwapchainKHR")
    NEXA_LOAD(GetSwapchainImagesKHR, "vkGetSwapchainImagesKHR")
    NEXA_LOAD(CreateImageView, "vkCreateImageView")
    NEXA_LOAD(DestroyImageView, "vkDestroyImageView")
    NEXA_LOAD(CreateImage, "vkCreateImage")
    NEXA_LOAD(DestroyImage, "vkDestroyImage")
    NEXA_LOAD(GetImageMemoryRequirements, "vkGetImageMemoryRequirements")
    NEXA_LOAD(AllocateMemory, "vkAllocateMemory")
    NEXA_LOAD(FreeMemory, "vkFreeMemory")
    NEXA_LOAD(BindImageMemory, "vkBindImageMemory")
    NEXA_LOAD(BindBufferMemory, "vkBindBufferMemory")
    NEXA_LOAD(CreateRenderPass, "vkCreateRenderPass")
    NEXA_LOAD(DestroyRenderPass, "vkDestroyRenderPass")
    NEXA_LOAD(CreateFramebuffer, "vkCreateFramebuffer")
    NEXA_LOAD(DestroyFramebuffer, "vkDestroyFramebuffer")
    NEXA_LOAD(CreateShaderModule, "vkCreateShaderModule")
    NEXA_LOAD(DestroyShaderModule, "vkDestroyShaderModule")
    NEXA_LOAD(CreatePipelineLayout, "vkCreatePipelineLayout")
    NEXA_LOAD(DestroyPipelineLayout, "vkDestroyPipelineLayout")
    NEXA_LOAD(CreateGraphicsPipelines, "vkCreateGraphicsPipelines")
    NEXA_LOAD(DestroyPipeline, "vkDestroyPipeline")
    NEXA_LOAD(CreateCommandPool, "vkCreateCommandPool")
    NEXA_LOAD(DestroyCommandPool, "vkDestroyCommandPool")
    NEXA_LOAD(ResetCommandPool, "vkResetCommandPool")
    NEXA_LOAD(AllocateCommandBuffers, "vkAllocateCommandBuffers")
    NEXA_LOAD(BeginCommandBuffer, "vkBeginCommandBuffer")
    NEXA_LOAD(EndCommandBuffer, "vkEndCommandBuffer")
    NEXA_LOAD(CmdBeginRenderPass, "vkCmdBeginRenderPass")
    NEXA_LOAD(CmdEndRenderPass, "vkCmdEndRenderPass")
    NEXA_LOAD(CmdBindPipeline, "vkCmdBindPipeline")
    NEXA_LOAD(CmdBindVertexBuffers, "vkCmdBindVertexBuffers")
    NEXA_LOAD(CmdPushConstants, "vkCmdPushConstants")
    NEXA_LOAD(CmdDraw, "vkCmdDraw")
    NEXA_LOAD(CmdSetViewport, "vkCmdSetViewport")
    NEXA_LOAD(CmdSetScissor, "vkCmdSetScissor")
    NEXA_LOAD(CmdBindDescriptorSets, "vkCmdBindDescriptorSets")
    NEXA_LOAD(CmdPipelineBarrier, "vkCmdPipelineBarrier")
    NEXA_LOAD(CmdCopyBufferToImage, "vkCmdCopyBufferToImage")
    NEXA_LOAD(CmdCopyImageToBuffer, "vkCmdCopyImageToBuffer")
    NEXA_LOAD(CmdResolveImage, "vkCmdResolveImage")
    NEXA_LOAD(CreateBuffer, "vkCreateBuffer")
    NEXA_LOAD(DestroyBuffer, "vkDestroyBuffer")
    NEXA_LOAD(GetBufferMemoryRequirements, "vkGetBufferMemoryRequirements")
    NEXA_LOAD(MapMemory, "vkMapMemory")
    NEXA_LOAD(FlushMappedMemoryRanges, "vkFlushMappedMemoryRanges")
    NEXA_LOAD(UnmapMemory, "vkUnmapMemory")
    NEXA_LOAD(CreateFence, "vkCreateFence")
    NEXA_LOAD(DestroyFence, "vkDestroyFence")
    NEXA_LOAD(CreateSemaphore, "vkCreateSemaphore")
    NEXA_LOAD(DestroySemaphore, "vkDestroySemaphore")
    NEXA_LOAD(WaitForFences, "vkWaitForFences")
    NEXA_LOAD(ResetFences, "vkResetFences")
    NEXA_LOAD(QueueSubmit, "vkQueueSubmit")
    NEXA_LOAD(QueuePresentKHR, "vkQueuePresentKHR")
    NEXA_LOAD(AcquireNextImageKHR, "vkAcquireNextImageKHR")
    NEXA_LOAD(CreateSampler, "vkCreateSampler")
    NEXA_LOAD(DestroySampler, "vkDestroySampler")
    NEXA_LOAD(CreateDescriptorSetLayout, "vkCreateDescriptorSetLayout")
    NEXA_LOAD(DestroyDescriptorSetLayout, "vkDestroyDescriptorSetLayout")
    NEXA_LOAD(CreateDescriptorPool, "vkCreateDescriptorPool")
    NEXA_LOAD(DestroyDescriptorPool, "vkDestroyDescriptorPool")
    NEXA_LOAD(AllocateDescriptorSets, "vkAllocateDescriptorSets")
    NEXA_LOAD(UpdateDescriptorSets, "vkUpdateDescriptorSets")
#undef NEXA_LOAD

    d.GetDeviceQueue(d.device, d.qfam, 0, &d.queue);

    struct MP { uint32_t count; struct { uint32_t flags, heap; } types[32]; uint32_t heapCount; struct { uint64_t size; uint32_t flags; uint32_t padh; } heaps[16]; } mp{};
    d.GetPhysicalDeviceMemoryProperties(d.phys, &mp);
    d.memCount = mp.count;
    if (d.memCount > 32) d.memCount = 32;
    for (uint32_t i = 0; i < d.memCount; ++i) d.memFlags[i] = mp.types[i].flags;

// [nexa:vkshader-begin]
    // 64 bytes of transform, then sixteen floats for gfx3d.shader_set. Vulkan
    // promises a layout may hold 128, so this asks for no more than every
    // implementation has. Fragment as well as vertex: a value may be wanted in
    // either, and a push must name every stage of the ranges it touches.
    struct RangePC { uint32_t stage, offset, size; } pc{ 1 | 16, 0, 128 };
// [nexa:vkshader-end]
// [nexa:novkshader-begin]
    struct RangePC { uint32_t stage, offset, size; } pc{ 1, 0, 64 };
// [nexa:novkshader-end]
    struct PL { uint32_t sType; const void* pNext; uint32_t flags; uint32_t setCount; const void* sets; uint32_t pcCount; const RangePC* pc; } pl{};
    pl.sType = 30; pl.pcCount = 1; pl.pc = &pc;
    if (d.CreatePipelineLayout(d.device, &pl, nullptr, &d.layout) != 0) { fail("layout"); shutdown(); return 0; }

    struct Bind { uint32_t binding, type, count, stages; const void* samplers; } bind{};
    bind.type = 1; bind.count = 1; bind.stages = 0x10;
    struct SL { uint32_t sType; const void* pNext; uint32_t flags; uint32_t n; const Bind* b; } sl{};
    sl.sType = 32; sl.n = 1; sl.b = &bind;
    if (d.CreateDescriptorSetLayout(d.device, &sl, nullptr, &d.setLayout) != 0) { fail("set layout"); shutdown(); return 0; }
    struct PL2 { uint32_t sType; const void* pNext; uint32_t flags; uint32_t setCount; const uint64_t* sets; uint32_t pcCount; const void* pc; } pl2{};
    pl2.sType = 30; pl2.setCount = 1; pl2.sets = &d.setLayout;
    if (d.CreatePipelineLayout(d.device, &pl2, nullptr, &d.layoutOv) != 0) { fail("overlay layout"); shutdown(); return 0; }

    if (!shader_mod(d, spv(kVs), spv_words(kVs), &d.shVs) || !shader_mod(d, spv(kFs), spv_words(kFs), &d.shFs) ||
        !shader_mod(d, spv(kOvVs), spv_words(kOvVs), &d.shOvVs) || !shader_mod(d, spv(kOvFs), spv_words(kOvFs), &d.shOvFs)) {
        fail("shader"); shutdown(); return 0;
    }

    struct Samp { uint32_t sType; const void* pNext; uint32_t flags; uint32_t mag, minf, mip, u, v, w; float bias; uint32_t anisoEn; float maxAniso; uint32_t cmpEn, cmp; float minLod, maxLod; uint32_t border, unnorm; } samp{};
    samp.sType = 31; samp.u = samp.v = samp.w = 2;
    if (d.CreateSampler(d.device, &samp, nullptr, &d.sampler) != 0) { fail("sampler"); shutdown(); return 0; }
    struct PS { uint32_t type, count; } ps{ 1, 1 };
    struct DP { uint32_t sType; const void* pNext; uint32_t flags; uint32_t maxSets; uint32_t n; const PS* p; } dp{};
    dp.sType = 33; dp.maxSets = 1; dp.n = 1; dp.p = &ps;
    if (d.CreateDescriptorPool(d.device, &dp, nullptr, &d.descPool) != 0) { fail("desc pool"); shutdown(); return 0; }
    struct DA { uint32_t sType; const void* pNext; uint64_t pool; uint32_t count; const uint64_t* layouts; } da{};
    da.sType = 34; da.pool = d.descPool; da.count = 1; da.layouts = &d.setLayout;
    if (d.AllocateDescriptorSets(d.device, &da, &d.descSet) != 0) { fail("desc set"); shutdown(); return 0; }

    struct CP { uint32_t sType; const void* pNext; uint32_t flags; uint32_t family; } cp{};
    cp.sType = 39; cp.flags = 2; cp.family = d.qfam;
    if (d.CreateCommandPool(d.device, &cp, nullptr, &d.pool) != 0) { fail("cmd pool"); shutdown(); return 0; }
    struct CA { uint32_t sType; const void* pNext; uint64_t pool; uint32_t level; uint32_t count; } ca{};
    ca.sType = 40; ca.pool = d.pool; ca.count = 1;
    if (d.AllocateCommandBuffers(d.device, &ca, &d.cmd) != 0 || !d.cmd) { fail("cmd"); shutdown(); return 0; }

    struct FC { uint32_t sType; const void* pNext; uint32_t flags; } fc{};
    fc.sType = 8; fc.flags = 1;
    if (d.CreateFence(d.device, &fc, nullptr, &d.fence) != 0) { fail("fence"); shutdown(); return 0; }
    fc.flags = 0; fc.sType = 9;
    if (d.CreateSemaphore(d.device, &fc, nullptr, &d.semImg) != 0 || d.CreateSemaphore(d.device, &fc, nullptr, &d.semDraw) != 0) {
        fail("semaphore"); shutdown(); return 0;
    }

    if (!buffer_alloc(d, 16ull * 1024ull * 1024ull, 0x80, &d.vbuf, &d.vmem, &d.vmap)) { fail("vertex buffer"); shutdown(); return 0; }
    d.vcap = 16ull * 1024ull * 1024ull;
    d.scap = 32ull << 20;
    if (!buffer_alloc(d, d.scap, 0x80, &d.sbuf, &d.smem, &d.smap)) {
        d.sbuf = 0; d.smem = 0; d.smap = nullptr; d.scap = 0;
    }
    d.sused = 0;
    float quad[24] = {
        -1, 1, 0, 0,  1, 1, 1, 0,  1, -1, 1, 1,
        -1, 1, 0, 0,  1, -1, 1, 1,  -1, -1, 0, 1
    };
    void* qm = nullptr;
    if (!buffer_alloc(d, sizeof(quad), 0x80, &d.quad, &d.quadMem, &qm)) { fail("quad"); shutdown(); return 0; }
    std::memcpy(qm, quad, sizeof(quad));

    RECT rc{};
    GetClientRect((HWND)hwnd, &rc);
    int ww = rc.right > 0 ? rc.right : 16;
    int hh = rc.bottom > 0 ? rc.bottom : 16;
    scene_boot(d);
    if (!rebuild_swapchain(d, ww, hh)) { fail("swapchain build"); shutdown(); return 0; }
    return 1;
}

inline int alive() { return dev().device != nullptr && dev().swapchain != 0; }

inline int begin(float r, float g, float b, int w, int h) {
    Dev& d = dev();
    if (!d.device) return 0;
    if (d.framing) return 1;
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
    struct BI { uint32_t sType; const void* pNext; uint32_t flags; const void* inherit; } bi{};
    bi.sType = 42; bi.flags = 1;
    if (d.BeginCommandBuffer(d.cmd, &bi) != 0) return 0;
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
    d.framing = 1;
    d.vused = 0;
    scene_note_simple();
    return 1;
}

// [nexa:vkshader-begin]
// --- gfx3d.shader on Vulkan -------------------------------------------------
//
// Vulkan takes SPIR-V, so nothing is compiled here: the words become two
// shader modules and then the same three pipelines the built-in shader has.
// What a custom shader has to agree with is what it replaces -- the vertex is
// position and colour, six floats, and the transform arrives as this
// renderer's push constant.
inline void custom_down(Dev& d) {
    custom_tex_down(d);
    if (d.pipeCustomTri && d.DestroyPipeline) d.DestroyPipeline(d.device, d.pipeCustomTri, nullptr);
    if (d.pipeCustomTwo && d.DestroyPipeline) d.DestroyPipeline(d.device, d.pipeCustomTwo, nullptr);
    if (d.pipeCustomLine && d.DestroyPipeline) d.DestroyPipeline(d.device, d.pipeCustomLine, nullptr);
    d.pipeCustomTri = d.pipeCustomTwo = d.pipeCustomLine = 0;
    for (int i = 0; i < 2; ++i) {
        if (d.shCustom[i] && d.DestroyShaderModule) d.DestroyShaderModule(d.device, d.shCustom[i], nullptr);
        d.shCustom[i] = 0;
        // Only when it is a module of its own; otherwise it was shCustom.
        if (d.shCustomTex[i] && d.shCustomTex[i] != d.shCustom[i] && d.DestroyShaderModule) {
            d.DestroyShaderModule(d.device, d.shCustomTex[i], nullptr);
        }
        d.shCustomTex[i] = 0;
    }
    d.customOn = 0;
}

inline int custom_up(const unsigned int* vw, size_t vbytes, const unsigned int* fw, size_t fbytes,
                     const unsigned int* tvw, size_t tvbytes, const unsigned int* tfw, size_t tfbytes,
                     int haveTex) {
    Dev& d = dev();
    if (!d.device || !d.pass) return 0;
    // The frame in flight may still be reading what is about to go.
    if (d.DeviceWaitIdle) d.DeviceWaitIdle(d.device);
    custom_down(d);
    if (!shader_mod(d, vw, vbytes / 4, &d.shCustom[0])) { custom_down(d); return 0; }
    if (!shader_mod(d, fw, fbytes / 4, &d.shCustom[1])) { custom_down(d); return 0; }
    if (haveTex) {
        if (!shader_mod(d, tvw, tvbytes / 4, &d.shCustomTex[0])) { custom_down(d); return 0; }
        if (!shader_mod(d, tfw, tfbytes / 4, &d.shCustomTex[1])) { custom_down(d); return 0; }
    } else {
        d.shCustomTex[0] = d.shCustom[0];
        d.shCustomTex[1] = d.shCustom[1];
    }
    if (!make_pipe(d, d.shCustom[0], d.shCustom[1], 3, 2, 1, 0, d.pass, 24, 0, d.layout, &d.pipeCustomTri) ||
        !make_pipe(d, d.shCustom[0], d.shCustom[1], 3, 0, 1, 0, d.pass, 24, 0, d.layout, &d.pipeCustomTwo) ||
        !make_pipe(d, d.shCustom[0], d.shCustom[1], 1, 0, 1, 0, d.pass, 24, 0, d.layout, &d.pipeCustomLine)) {
        custom_down(d);
        return 0;
    }
    // Textured geometry is drawn by another pipeline again, so the same two
    // modules are built into that as well. All of it or none: a shader that
    // takes on half a scene is worse than one that is refused.
    if (!custom_tex_pipes(d)) {
        custom_down(d);
        return 0;
    }
    return 1;
}

inline int custom_use(int on) {
    Dev& d = dev();
    if (on && !d.pipeCustomTri) return 0;
    d.customOn = on ? 1 : 0;
    return 1;
}

inline int custom_ready() { return dev().pipeCustomTri != 0; }

// One of the sixteen. Takes effect on the next draw, since that is when the
// block goes up.
inline int custom_set(int slot, float v) {
    if (slot < 0 || slot >= 16) return 0;
    dev().userPC[slot] = v;
    return 1;
}
// [nexa:vkshader-end]
inline void draw(const float* xyzrgb, int nvert, int lines, int twoSided, const float* mvp) {
    Dev& d = dev();
    if (!d.framing || !xyzrgb || nvert <= 0 || !mvp) return;
    uint64_t bytes = (uint64_t)nvert * 6ull * 4ull;
    if (d.vused + bytes > d.vcap) return;
    std::memcpy((char*)d.vmap + d.vused, xyzrgb, (size_t)bytes);
    float clip[16];
    for (int c = 0; c < 4; ++c) {
        float x = mvp[c * 4 + 0], y = mvp[c * 4 + 1], z = mvp[c * 4 + 2], w = mvp[c * 4 + 3];
        clip[c * 4 + 0] = x;
        clip[c * 4 + 1] = y;
        clip[c * 4 + 2] = 0.5f * z + 0.5f * w;
        clip[c * 4 + 3] = w;
    }
// [nexa:vkshader-begin]
    uint64_t pipe = d.customOn
        ? (lines ? d.pipeCustomLine : (twoSided ? d.pipeCustomTwo : d.pipeCustomTri))
        : (lines ? d.pipeLine : (twoSided ? d.pipeTwo : d.pipeTri));
// [nexa:vkshader-end]
// [nexa:novkshader-begin]
    uint64_t pipe = lines ? d.pipeLine : (twoSided ? d.pipeTwo : d.pipeTri);
// [nexa:novkshader-end]
    d.CmdBindPipeline(d.cmd, 0, pipe);
// [nexa:vkshader-begin]
    // Both stages, because the range covers both; the user block only when
    // there is a custom shader to read it.
    d.CmdPushConstants(d.cmd, d.layout, 1 | 16, 0, 64, clip);
    if (d.customOn) d.CmdPushConstants(d.cmd, d.layout, 1 | 16, 64, 64, d.userPC);
// [nexa:vkshader-end]
// [nexa:novkshader-begin]
    d.CmdPushConstants(d.cmd, d.layout, 1, 0, 64, clip);
// [nexa:novkshader-end]
    uint64_t off = d.vused;
    d.CmdBindVertexBuffers(d.cmd, 0, 1, &d.vbuf, &off);
    d.CmdDraw(d.cmd, (uint32_t)nvert, 1, 0, 0);
    d.vused += bytes;
}

inline void set_overlay(const void* bgra, int w, int h) {
    Dev& d = dev();
    d.pendOv = bgra;
    d.pendW = w;
    d.pendH = h;
}

inline void record_read(Dev& d) {
    if (!d.wantRead || d.imageIndex >= d.images.size() || d.swapW < 1 || d.swapH < 1) return;
    uint64_t bytes = (uint64_t)d.swapW * (uint64_t)d.swapH * 4ull;
    if (d.readCap < bytes || !d.readBuf) {
        if (d.readMap && d.UnmapMemory) d.UnmapMemory(d.device, d.readMem);
        if (d.readBuf && d.DestroyBuffer) d.DestroyBuffer(d.device, d.readBuf, nullptr);
        if (d.readMem && d.FreeMemory) d.FreeMemory(d.device, d.readMem, nullptr);
        d.readMap = nullptr; d.readBuf = d.readMem = 0; d.readCap = 0;
        if (!buffer_alloc(d, bytes, 0x2, &d.readBuf, &d.readMem, &d.readMap)) return;
        d.readCap = bytes;
    }
    struct Range { uint32_t aspect, mip, levels, layer, layers; };
    struct Barr { uint32_t sType; const void* pNext; uint32_t srcA, dstA, oldL, newL, srcQ, dstQ; uint64_t image; Range range; } b{};
    b.sType = 45; b.srcA = 0x100; b.dstA = 0x800; b.oldL = 1000001002; b.newL = 6; b.srcQ = ~0u; b.dstQ = ~0u;
    b.image = d.images[d.imageIndex]; b.range = { 1, 0, 1, 0, 1 };
    d.CmdPipelineBarrier(d.cmd, 0x400, 0x1000, 0, 0, nullptr, 0, nullptr, 1, &b);
    struct Subr { uint32_t aspect, mip, layer, count; };
    struct Copy { uint64_t off; uint32_t row, height; Subr sub; int32_t x, y, z; uint32_t w, h, dep; } copy{};
    copy.sub = { 1, 0, 0, 1 }; copy.w = (uint32_t)d.swapW; copy.h = (uint32_t)d.swapH; copy.dep = 1;
    d.CmdCopyImageToBuffer(d.cmd, d.images[d.imageIndex], 6, d.readBuf, 1, &copy);
    b.srcA = 0x800; b.dstA = 0; b.oldL = 6; b.newL = 1000001002;
    d.CmdPipelineBarrier(d.cmd, 0x1000, 0x2000, 0, 0, nullptr, 0, nullptr, 1, &b);
    d.readW = d.swapW; d.readH = d.swapH;
}

inline void present(const void* bgra, int bw, int bh) {
    Dev& d = dev();
    if (!d.framing) return;
    if (!bgra && d.pendOv) { bgra = d.pendOv; bw = d.pendW; bh = d.pendH; }
    d.pendOv = nullptr;
    if (scene_on()) scene_finish(d);
    else if (scene_simple_open()) d.CmdEndRenderPass(d.cmd);
    scene_note_done();
    if (bgra && bw > 0 && bh > 0) draw_overlay(d, bgra, bw, bh);
    if (d.wantRead) record_read(d);
    d.EndCommandBuffer(d.cmd);
    if (d.ResetFences(d.device, 1, &d.fence) != 0) { d.framing = 0; return; }
    uint32_t stage = 0x400;
    struct Sub { uint32_t sType; const void* pNext; uint32_t waitCount; const uint64_t* waits; const uint32_t* stages; uint32_t cmdCount; void* const* cmds; uint32_t sigCount; const uint64_t* sigs; } sub{};
    sub.sType = 4; sub.waitCount = 1; sub.waits = &d.semImg; sub.stages = &stage; sub.cmdCount = 1; sub.cmds = &d.cmd; sub.sigCount = 1; sub.sigs = &d.semDraw;
    int32_t qs = d.QueueSubmit(d.queue, 1, &sub, d.fence);
    d.framing = 0;
    if (qs != 0) return;
    struct Pr { uint32_t sType; const void* pNext; uint32_t waitCount; const uint64_t* waits; uint32_t n; const uint64_t* swap; const uint32_t* idx; int32_t* results; } pr{};
    pr.sType = 1000001001; pr.waitCount = 1; pr.waits = &d.semDraw; pr.n = 1; pr.swap = &d.swapchain; pr.idx = &d.imageIndex;
    int32_t pres = d.QueuePresentKHR(d.queue, &pr);
    if (pres == -1000001004 || pres == 1000001003) d.dirty = 1;
}

inline void set_vsync(int on) {
    Dev& d = dev();
    int want = on ? 1 : 0;
    if (d.vsync == want) return;
    d.vsync = want;
    d.dirty = 1;
}

} // namespace detail

using Pix = detail::Pix;

inline int open(void* hwnd, int vsyncOn) { return detail::open(hwnd, vsyncOn); }
inline void shutdown() { detail::shutdown(); }
inline int alive() { return detail::alive(); }
inline int begin(float r, float g, float b, int w, int h) { return detail::begin(r, g, b, w, h); }
inline void draw(const float* xyzrgb, int nvert, int lines, int twoSided, const float* mvp) {
    detail::draw(xyzrgb, nvert, lines, twoSided, mvp);
}
inline void set_overlay(const void* bgra, int w, int h) { detail::set_overlay(bgra, w, h); }
// [nexa:vkshader-begin]
// gfx3d.shader. The words are SPIR-V already, so these only build the
// pipelines and choose which of them a mesh is drawn with.
inline int custom_up(const unsigned int* vw, size_t vbytes, const unsigned int* fw, size_t fbytes,
                     const unsigned int* tvw, size_t tvbytes, const unsigned int* tfw, size_t tfbytes,
                     int haveTex) {
    return detail::custom_up(vw, vbytes, fw, fbytes, tvw, tvbytes, tfw, tfbytes, haveTex);
}
inline int custom_use(int on) { return detail::custom_use(on); }
inline int custom_ready() { return detail::custom_ready(); }
inline int custom_set(int slot, float v) { return detail::custom_set(slot, v); }
// [nexa:vkshader-end]
inline void present(const void* bgra, int bw, int bh) { detail::present(bgra, bw, bh); }
inline void set_vsync(int on) { detail::set_vsync(on); }
inline int scene_ok() { return detail::scene_ok(); }
inline int scene_begin(int ww, int wh, int rw, int rh, int samples, float r, float g, float b) {
    return detail::scene_begin(ww, wh, rw, rh, samples, r, g, b);
}
inline void scene_uniforms(const void* bytes, int n) { detail::scene_uniforms(bytes, n); }
inline int shadow_begin() { return detail::shadow_begin(); }
inline void shadow_draw(const float* xyz, int nvert, const float* mvp) { detail::shadow_draw(xyz, nvert, mvp); }
inline void shadow_draw_kept(uint64_t off, int nvert, const float* mvp) { detail::shadow_draw_kept(off, nvert, mvp); }
inline void shadow_end() { detail::shadow_end(); }
inline void mesh_draw(const float* xyz, int nvert, const void* draw, int kind) { detail::mesh_draw(xyz, nvert, draw, kind); }
inline void mesh_draw_kept(uint64_t off, int nvert, const void* draw, int kind) { detail::mesh_draw_kept(off, nvert, draw, kind); }
inline void mat_draw(const float* xyz, int nvert, const void* draw, int blend, const detail::Pix maps[5]) {
    detail::mat_draw(xyz, nvert, draw, blend, maps);
}
inline void mat_draw_kept(uint64_t off, int nvert, const void* draw, int blend, const detail::Pix maps[5]) {
    detail::mat_draw_kept(off, nvert, draw, blend, maps);
}
inline void sky_draw(const float* xyz, int nvert, const void* pc, const detail::Pix faces[6]) {
    detail::sky_draw(xyz, nvert, pc, faces);
}
inline void sky_draw_kept(uint64_t off, int nvert, const void* pc, const detail::Pix faces[6]) {
    detail::sky_draw_kept(off, nvert, pc, faces);
}
inline uint64_t vert_keep(const void* data, uint64_t bytes) { return detail::vert_keep(data, bytes); }
inline int epoch() { return detail::keep_epoch(); }
inline void part_draw(const float* xyz, int nvert, const unsigned char* rgba, int w, int h, const void* cam) {
    detail::part_draw(xyz, nvert, rgba, w, h, cam);
}
inline void depth_range(float a, float b) { detail::depth_range(a, b); }
inline void draw_tex(const float* xyz, int nvert, const unsigned char* rgba, int w, int h, int cutout, int twoSided, const float* mvp) {
    detail::draw_tex(xyz, nvert, rgba, w, h, cutout, twoSided, mvp);
}
inline void arm_read(int on) { detail::dev().wantRead = on ? 1 : 0; }
inline int take_read(std::vector<unsigned char>& rgba, int& w, int& h) {
    detail::Dev& d = detail::dev();
    w = 0; h = 0;
    if (!d.readMap || d.readW < 1 || d.readH < 1) return 0;
    if (d.WaitForFences(d.device, 1, &d.fence, 1, ~0ull) != 0) return 0;
    w = d.readW; h = d.readH;
    rgba.resize((size_t)w * (size_t)h * 4);
    const unsigned char* src = (const unsigned char*)d.readMap;
    int bgra = d.colorFmt == 44;
    for (int i = 0; i < w * h; ++i) {
        unsigned char b0 = src[i * 4], b1 = src[i * 4 + 1], b2 = src[i * 4 + 2], b3 = src[i * 4 + 3];
        if (bgra) { rgba[(size_t)i * 4] = b2; rgba[(size_t)i * 4 + 1] = b1; rgba[(size_t)i * 4 + 2] = b0; }
        else { rgba[(size_t)i * 4] = b0; rgba[(size_t)i * 4 + 1] = b1; rgba[(size_t)i * 4 + 2] = b2; }
        rgba[(size_t)i * 4 + 3] = b3;
    }
    d.wantRead = 0;
    return 1;
}

} // namespace nexa_vk14
)NEXA_VK14";
}
