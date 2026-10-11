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
#include <cstring>
#include <vector>

namespace nexa_vk14 {

inline const char*& last_error() {
    static const char* e = "";
    return e;
}

namespace detail {

static const uint32_t kApi14 = 0x00404000u;

static const uint32_t kVs[] = {
    119734787, 65536, 0, 29, 0, 131089, 0, 131089, 1, 196622, 0, 1, 589839, 0, 21, 1852399981, 0, 16, 17, 18, 19, 262215, 16, 30, 0, 262215, 17, 30, 1, 262215, 19, 30, 0, 262215, 18, 11, 0, 196679, 7, 2, 262216, 7, 0, 5, 327752, 7, 0, 7, 16, 327752, 7, 0, 35, 0, 131091, 1, 196641, 2, 1, 196630, 3, 32, 262167, 4, 3, 3, 262167, 5, 3, 4, 262168, 6, 5, 4, 196638, 7, 6, 262176, 8, 1, 4, 262176, 9, 3, 5, 262176, 10, 3, 4, 262176, 11, 9, 7, 262165, 12, 32, 0, 262187, 12, 13, 0, 262187, 3, 14, 1065353216, 262176, 15, 9, 6, 262203, 8, 16, 1, 262203, 8, 17, 1, 262203, 9, 18, 3, 262203, 10, 19, 3, 262203, 11, 20, 9, 327734, 1, 21, 0, 2, 131320, 22, 262205, 4, 23, 16, 327760, 5, 24, 23, 14, 327745, 15, 25, 20, 13, 262205, 6, 26, 25, 327825, 5, 27, 26, 24, 196670, 18, 27, 262205, 4, 28, 17, 196670, 19, 28, 65789, 65592
};
static const uint32_t kFs[] = {
    119734787, 65536, 0, 15, 0, 131089, 1, 196622, 0, 1, 458767, 4, 11, 1852399981, 0, 9, 10, 196624, 11, 7, 262215, 9, 30, 0, 262215, 10, 30, 0, 131091, 1, 196641, 2, 1, 196630, 3, 32, 262167, 4, 3, 3, 262167, 5, 3, 4, 262187, 3, 6, 1065353216, 262176, 7, 1, 4, 262176, 8, 3, 5, 262203, 7, 9, 1, 262203, 8, 10, 3, 327734, 1, 11, 0, 2, 131320, 12, 262205, 4, 13, 9, 327760, 5, 14, 13, 6, 196670, 10, 14, 65789, 65592
};
static const uint32_t kOvVs[] = {
    119734787, 65536, 0, 20, 0, 131089, 1, 196622, 0, 1, 589839, 0, 15, 1852399981, 0, 11, 12, 13, 14, 262215, 11, 30, 0, 262215, 12, 30, 1, 262215, 14, 30, 0, 262215, 13, 11, 0, 131091, 1, 196641, 2, 1, 196630, 3, 32, 262167, 4, 3, 2, 262167, 5, 3, 4, 262187, 3, 6, 0, 262187, 3, 7, 1065353216, 262176, 8, 1, 4, 262176, 9, 3, 5, 262176, 10, 3, 4, 262203, 8, 11, 1, 262203, 8, 12, 1, 262203, 9, 13, 3, 262203, 10, 14, 3, 327734, 1, 15, 0, 2, 131320, 16, 262205, 4, 17, 11, 262205, 4, 18, 12, 393296, 5, 19, 17, 6, 7, 196670, 13, 19, 196670, 14, 18, 65789, 65592
};
static const uint32_t kOvFs[] = {
    119734787, 65536, 0, 19, 0, 131089, 1, 196622, 0, 1, 458767, 4, 14, 1852399981, 0, 11, 12, 196624, 14, 7, 262215, 11, 30, 0, 262215, 12, 30, 0, 262215, 13, 34, 0, 262215, 13, 33, 0, 131091, 1, 196641, 2, 1, 196630, 3, 32, 262167, 4, 3, 2, 262167, 5, 3, 4, 589849, 6, 3, 1, 0, 0, 0, 1, 0, 196635, 7, 6, 262176, 8, 0, 7, 262176, 9, 1, 4, 262176, 10, 3, 5, 262203, 9, 11, 1, 262203, 10, 12, 3, 262203, 8, 13, 0, 327734, 1, 14, 0, 2, 131320, 15, 262205, 4, 16, 11, 262205, 7, 17, 13, 327767, 5, 18, 17, 16, 196670, 12, 18, 65789, 65592
};

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
    uint64_t shVs = 0, shFs = 0, shOvVs = 0, shOvFs = 0;
    uint64_t pool = 0;
    uint64_t vbuf = 0, vmem = 0;
    void* vmap = nullptr;
    uint64_t vcap = 0, vused = 0;
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
    NEXA_VK_FN(CmdResolveImage, void(__stdcall*)(void*, uint64_t, uint32_t, uint64_t, uint32_t, uint32_t, const void*))
    NEXA_VK_FN(CreateBuffer, int32_t(__stdcall*)(void*, const void*, const void*, uint64_t*))
    NEXA_VK_FN(DestroyBuffer, void(__stdcall*)(void*, uint64_t, const void*))
    NEXA_VK_FN(GetBufferMemoryRequirements, void(__stdcall*)(void*, uint64_t, void*))
    NEXA_VK_FN(MapMemory, int32_t(__stdcall*)(void*, uint64_t, uint64_t, uint64_t, uint32_t, void**))
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
    int mt = mem_type(d, req.bits, 2u | 4u);
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
    sci.layers = 1; sci.usage = 0x10; sci.pre = caps.currentTransform ? caps.currentTransform : 1; sci.composite = alpha; sci.present = mode; sci.clipped = 1;
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

#include "nexa_vk14_scene.hpp"

inline void shutdown() {
    Dev& d = dev();
    if (d.device && d.DeviceWaitIdle) d.DeviceWaitIdle(d.device);
    scene_down(d);
    d.framing = 0;
    destroy_swapchain(d);
    if (d.ovView && d.DestroyImageView) d.DestroyImageView(d.device, d.ovView, nullptr);
    if (d.ovImg && d.DestroyImage) d.DestroyImage(d.device, d.ovImg, nullptr);
    if (d.ovMem && d.FreeMemory) d.FreeMemory(d.device, d.ovMem, nullptr);
    if (d.ovMap && d.UnmapMemory) d.UnmapMemory(d.device, d.ovStageMem);
    if (d.ovStage && d.DestroyBuffer) d.DestroyBuffer(d.device, d.ovStage, nullptr);
    if (d.ovStageMem && d.FreeMemory) d.FreeMemory(d.device, d.ovStageMem, nullptr);
    if (d.vmap && d.UnmapMemory) d.UnmapMemory(d.device, d.vmem);
    if (d.vbuf && d.DestroyBuffer) d.DestroyBuffer(d.device, d.vbuf, nullptr);
    if (d.vmem && d.FreeMemory) d.FreeMemory(d.device, d.vmem, nullptr);
    if (d.quad && d.DestroyBuffer) d.DestroyBuffer(d.device, d.quad, nullptr);
    if (d.quadMem && d.FreeMemory) d.FreeMemory(d.device, d.quadMem, nullptr);
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
    NEXA_LOAD(CmdResolveImage, "vkCmdResolveImage")
    NEXA_LOAD(CreateBuffer, "vkCreateBuffer")
    NEXA_LOAD(DestroyBuffer, "vkDestroyBuffer")
    NEXA_LOAD(GetBufferMemoryRequirements, "vkGetBufferMemoryRequirements")
    NEXA_LOAD(MapMemory, "vkMapMemory")
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

    struct RangePC { uint32_t stage, offset, size; } pc{ 1, 0, 64 };
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

    if (!shader_mod(d, kVs, sizeof(kVs) / 4, &d.shVs) || !shader_mod(d, kFs, sizeof(kFs) / 4, &d.shFs) ||
        !shader_mod(d, kOvVs, sizeof(kOvVs) / 4, &d.shOvVs) || !shader_mod(d, kOvFs, sizeof(kOvFs) / 4, &d.shOvFs)) {
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
    uint64_t pipe = lines ? d.pipeLine : (twoSided ? d.pipeTwo : d.pipeTri);
    d.CmdBindPipeline(d.cmd, 0, pipe);
    d.CmdPushConstants(d.cmd, d.layout, 1, 0, 64, clip);
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

inline void present(const void* bgra, int bw, int bh) {
    Dev& d = dev();
    if (!d.framing) return;
    if (!bgra && d.pendOv) { bgra = d.pendOv; bw = d.pendW; bh = d.pendH; }
    d.pendOv = nullptr;
    if (scene_on()) scene_finish(d);
    else if (scene_simple_open()) d.CmdEndRenderPass(d.cmd);
    scene_note_done();
    if (bgra && bw > 0 && bh > 0) draw_overlay(d, bgra, bw, bh);
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
inline void present(const void* bgra, int bw, int bh) { detail::present(bgra, bw, bh); }
inline void set_vsync(int on) { detail::set_vsync(on); }
inline int scene_ok() { return detail::scene_ok(); }
inline int scene_begin(int ww, int wh, int rw, int rh, int samples, float r, float g, float b) {
    return detail::scene_begin(ww, wh, rw, rh, samples, r, g, b);
}
inline void scene_uniforms(const void* bytes, int n) { detail::scene_uniforms(bytes, n); }
inline int shadow_begin() { return detail::shadow_begin(); }
inline void shadow_draw(const float* xyz, int nvert, const float* mvp) { detail::shadow_draw(xyz, nvert, mvp); }
inline void shadow_end() { detail::shadow_end(); }
inline void mesh_draw(const float* xyz, int nvert, const void* draw, int kind) { detail::mesh_draw(xyz, nvert, draw, kind); }
inline void mat_draw(const float* xyz, int nvert, const void* draw, int blend, const detail::Pix maps[5]) {
    detail::mat_draw(xyz, nvert, draw, blend, maps);
}
inline void sky_draw(const float* xyz, int nvert, const void* pc, const detail::Pix faces[6]) {
    detail::sky_draw(xyz, nvert, pc, faces);
}
inline void part_draw(const float* xyz, int nvert, const unsigned char* rgba, int w, int h, const void* cam) {
    detail::part_draw(xyz, nvert, rgba, w, h, cam);
}
inline void depth_range(float a, float b) { detail::depth_range(a, b); }
inline void draw_tex(const float* xyz, int nvert, const unsigned char* rgba, int w, int h, int cutout, int twoSided, const float* mvp) {
    detail::draw_tex(xyz, nvert, rgba, w, h, cutout, twoSided, mvp);
}

} // namespace nexa_vk14
