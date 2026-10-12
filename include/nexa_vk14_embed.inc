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

static const uint32_t kSpvMeshVs[] = { 119734787, 65536, 524299, 398, 0, 131089, 1, 393227, 1, 1280527431, 1685353262, 808793134, 0, 196622, 0, 1, 1048591, 0, 4, 1852399981, 0, 206, 216, 254, 263, 289, 305, 313, 315, 317, 318, 331, 196611, 2, 450, 262149, 4, 1852399981, 0, 524293, 16, 1684105331, 1719019621, 1719024435, 1719024435, 828980019, 59, 196613, 12, 110, 196613, 13, 25968, 196613, 14, 7627117, 262149, 15, 1885956979, 0, 262149, 18, 1701996900, 29795, 196613, 21, 105, 262149, 41, 1852138323, 101, 327686, 41, 0, 1936683116, 0, 327686, 41, 1, 1718183020, 0, 327686, 41, 2, 1869640556, 116, 262150, 41, 3, 7369580, 327686, 41, 4, 1180855649, 7105641, 262150, 41, 5, 6778726, 327686, 41, 6, 1868983913, 0, 196613, 43, 117, 196613, 60, 7631969, 196613, 70, 5009268, 196613, 78, 76, 262149, 85, 1953720676, 0, 262149, 110, 1953460339, 0, 196613, 118, 115, 196613, 161, 7629164, 262149, 163, 2002874948, 0, 262150, 163, 0, 7370349, 262150, 163, 1, 30317, 393222, 163, 2, 1751607660, 1886801268, 0, 327686, 163, 3, 1952542062, 0, 327686, 163, 4, 1869377379, 114, 327686, 163, 5, 1734437990, 115, 327686, 163, 6, 1920235621, 97, 327686, 163, 7, 1180855649, 7105641, 196613, 165, 100, 196613, 178, 99, 196613, 201, 6650213, 262149, 206, 1936674913, 0, 393221, 214, 1348430951, 1700164197, 2019914866, 0, 393222, 214, 0, 1348430951, 1953067887, 7237481, 458758, 214, 1, 1348430951, 1953393007, 1702521171, 0, 458758, 214, 2, 1130327143, 1148217708, 1635021673, 6644590, 458758, 214, 3, 1130327143, 1147956341, 1635021673, 6644590, 196613, 216, 0, 262149, 254, 1734954102, 29800, 262149, 263, 1735345782, 0, 262149, 270, 1953720676, 0, 262149, 274, 1918989427, 116, 262149, 289, 1886142838, 24936, 196613, 298, 7106403, 262149, 305, 1819231073, 0, 262149, 313, 1869760118, 29806, 262149, 315, 1667318390, 107, 262149, 317, 1853182838, 70, 262149, 318, 1853182838, 66, 196613, 320, 110, 262149, 331, 1836207713, 0, 262149, 336, 1634886000, 109, 262149, 338, 1634886000, 109, 262149, 341, 1634886000, 109, 262149, 345, 1634886000, 109, 262149, 348, 1634886000, 109, 262149, 350, 1634886000, 109, 262149, 353, 1634886000, 109, 262149, 357, 1634886000, 109, 262149, 369, 1634886000, 109, 262149, 370, 1634886000, 109, 262149, 373, 1634886000, 109, 262149, 377, 1634886000, 109, 262149, 382, 1634886000, 109, 262149, 383, 1634886000, 109, 262149, 386, 1634886000, 109, 262149, 390, 1634886000, 109, 262215, 36, 6, 16, 262215, 37, 6, 16, 262215, 38, 6, 16, 262215, 39, 6, 16, 196679, 41, 2, 327752, 41, 0, 35, 0, 327752, 41, 1, 35, 256, 327752, 41, 2, 35, 512, 327752, 41, 3, 35, 768, 327752, 41, 4, 35, 1024, 327752, 41, 5, 35, 1040, 327752, 41, 6, 35, 1056, 262215, 43, 33, 0, 262215, 43, 34, 0, 196679, 163, 2, 262216, 163, 0, 5, 327752, 163, 0, 7, 16, 327752, 163, 0, 35, 0, 262216, 163, 1, 5, 327752, 163, 1, 7, 16, 327752, 163, 1, 35, 64, 262216, 163, 2, 5, 327752, 163, 2, 7, 16, 327752, 163, 2, 35, 128, 262216, 163, 3, 5, 327752, 163, 3, 7, 16, 327752, 163, 3, 35, 192, 327752, 163, 4, 35, 256, 327752, 163, 5, 35, 272, 327752, 163, 6, 35, 288, 327752, 163, 7, 35, 304, 262215, 165, 33, 1, 262215, 165, 34, 0, 262215, 206, 30, 0, 196679, 214, 2, 327752, 214, 0, 11, 0, 327752, 214, 1, 11, 1, 327752, 214, 2, 11, 3, 327752, 214, 3, 11, 4, 262215, 254, 30, 4, 262215, 263, 30, 5, 262215, 289, 30, 6, 262215, 305, 30, 2, 262215, 313, 30, 0, 262215, 315, 30, 1, 262215, 317, 30, 2, 262215, 318, 30, 3, 262215, 331, 30, 1, 131091, 2, 196641, 3, 2, 196630, 6, 32, 262167, 7, 6, 3, 262176, 8, 7, 7, 262165, 9, 32, 1, 262176, 10, 7, 9, 458785, 11, 7, 8, 8, 8, 10, 262187, 6, 19, 0, 393260, 7, 20, 19, 19, 19, 262187, 9, 22, 0, 262187, 9, 29, 16, 131092, 30, 262167, 33, 6, 4, 262165, 34, 32, 0, 262187, 34, 35, 16, 262172, 36, 33, 35, 262172, 37, 33, 35, 262172, 38, 33, 35, 262172, 39, 33, 35, 262167, 40, 9, 4, 589854, 41, 36, 37, 38, 39, 33, 33, 40, 262176, 42, 2, 41, 262203, 42, 43, 2, 262187, 9, 44, 6, 262187, 34, 45, 0, 262176, 46, 2, 9, 262176, 59, 7, 6, 262187, 6, 61, 1065353216, 262187, 34, 63, 3, 262176, 64, 2, 6, 262176, 72, 2, 33, 262187, 6, 89, 925353388, 393260, 7, 93, 19, 19, 61, 262187, 9, 99, 3, 262187, 34, 101, 2, 262187, 6, 114, 1069547520, 262187, 9, 121, 2, 262187, 34, 139, 1, 262187, 9, 143, 1, 262168, 162, 33, 4, 655390, 163, 162, 162, 162, 162, 33, 33, 33, 33, 262176, 164, 2, 163, 262203, 164, 165, 2, 262187, 9, 166, 7, 262187, 6, 174, 1056964608, 393260, 7, 187, 61, 61, 61, 262176, 200, 7, 33, 262176, 202, 2, 162, 262176, 205, 1, 7, 262203, 205, 206, 1, 262172, 213, 6, 139, 393246, 214, 33, 6, 213, 213, 262176, 215, 3, 214, 262203, 215, 216, 3, 262176, 225, 3, 33, 262187, 9, 227, 5, 262167, 232, 6, 2, 262176, 234, 3, 6, 262203, 225, 254, 3, 262203, 234, 263, 3, 262187, 6, 266, 981668463, 262187, 6, 277, 1048576000, 262203, 234, 289, 3, 262187, 9, 290, 4, 262203, 205, 305, 1, 262176, 312, 3, 7, 262203, 312, 313, 3, 262203, 312, 315, 3, 262203, 312, 317, 3, 262203, 312, 318, 3, 262168, 323, 7, 3, 262203, 205, 331, 1, 262187, 9, 335, 4294967295, 327734, 2, 4, 0, 3, 131320, 5, 262203, 200, 201, 7, 262203, 59, 270, 7, 262203, 59, 274, 7, 262203, 8, 298, 7, 262203, 8, 302, 7, 262203, 8, 320, 7, 262203, 8, 336, 7, 262203, 8, 338, 7, 262203, 8, 341, 7, 262203, 10, 345, 7, 262203, 8, 348, 7, 262203, 8, 350, 7, 262203, 8, 353, 7, 262203, 10, 357, 7, 262203, 8, 369, 7, 262203, 8, 370, 7, 262203, 8, 373, 7, 262203, 10, 377, 7, 262203, 8, 382, 7, 262203, 8, 383, 7, 262203, 8, 386, 7, 262203, 10, 390, 7, 327745, 202, 203, 165, 143, 262205, 162, 204, 203, 262205, 7, 207, 206, 327761, 6, 208, 207, 0, 327761, 6, 209, 207, 1, 327761, 6, 210, 207, 2, 458832, 33, 211, 208, 209, 210, 61, 327825, 33, 212, 204, 211, 196670, 201, 212, 327745, 202, 217, 165, 22, 262205, 162, 218, 217, 262205, 7, 219, 206, 327761, 6, 220, 219, 0, 327761, 6, 221, 219, 1, 327761, 6, 222, 219, 2, 458832, 33, 223, 220, 221, 222, 61, 327825, 33, 224, 218, 223, 327745, 225, 226, 216, 22, 196670, 226, 224, 393281, 64, 228, 165, 227, 63, 262205, 6, 229, 228, 393281, 64, 230, 165, 44, 45, 262205, 6, 231, 230, 327760, 232, 233, 229, 231, 393281, 234, 235, 216, 22, 63, 262205, 6, 236, 235, 327822, 232, 237, 233, 236, 327745, 225, 238, 216, 22, 262205, 33, 239, 238, 458831, 232, 240, 239, 239, 0, 1, 327809, 232, 241, 240, 237, 393281, 234, 242, 216, 22, 45, 327761, 6, 243, 241, 0, 196670, 242, 243, 393281, 234, 244, 216, 22, 139, 327761, 6, 245, 241, 1, 196670, 244, 245, 393281, 234, 246, 216, 22, 101, 262205, 6, 247, 246, 327813, 6, 248, 247, 174, 393281, 234, 249, 216, 22, 63, 262205, 6, 250, 249, 327813, 6, 251, 250, 174, 327809, 6, 252, 248, 251, 393281, 234, 253, 216, 22, 101, 196670, 253, 252, 327745, 202, 255, 165, 121, 262205, 162, 256, 255, 262205, 7, 257, 206, 327761, 6, 258, 257, 0, 327761, 6, 259, 257, 1, 327761, 6, 260, 257, 2, 458832, 33, 261, 258, 259, 260, 61, 327825, 33, 262, 256, 261, 196670, 254, 262, 196670, 263, 19, 393281, 64, 264, 43, 227, 63, 262205, 6, 265, 264, 327866, 30, 267, 265, 266, 196855, 269, 0, 262394, 267, 268, 269, 131320, 268, 262205, 33, 271, 201, 524367, 7, 272, 271, 271, 0, 1, 2, 393228, 6, 273, 1, 66, 272, 196670, 270, 273, 393281, 64, 275, 43, 227, 63, 262205, 6, 276, 275, 327813, 6, 278, 276, 277, 196670, 274, 278, 262205, 6, 279, 270, 262205, 6, 280, 274, 327811, 6, 281, 279, 280, 393281, 64, 282, 43, 227, 63, 262205, 6, 283, 282, 262205, 6, 284, 274, 327811, 6, 285, 283, 284, 458764, 6, 286, 1, 40, 285, 266, 327816, 6, 287, 281, 286, 524300, 6, 288, 1, 43, 287, 19, 61, 196670, 263, 288, 131321, 269, 131320, 269, 393281, 64, 291, 165, 290, 63, 262205, 6, 292, 291, 196670, 289, 292, 393281, 64, 293, 165, 227, 45, 262205, 6, 294, 293, 327864, 30, 295, 294, 174, 196855, 297, 0, 262394, 295, 296, 319, 131320, 296, 393281, 64, 299, 165, 227, 139, 262205, 6, 300, 299, 327866, 30, 301, 300, 174, 196855, 304, 0, 262394, 301, 303, 307, 131320, 303, 262205, 7, 306, 305, 196670, 302, 306, 131321, 304, 131320, 307, 327745, 72, 308, 165, 290, 262205, 33, 309, 308, 524367, 7, 310, 309, 309, 0, 1, 2, 196670, 302, 310, 131321, 304, 131320, 304, 262205, 7, 311, 302, 196670, 298, 311, 262205, 7, 314, 298, 196670, 313, 314, 262205, 7, 316, 298, 196670, 315, 316, 196670, 317, 20, 196670, 318, 20, 131321, 297, 131320, 319, 327745, 202, 321, 165, 99, 262205, 162, 322, 321, 327761, 33, 324, 322, 0, 524367, 7, 325, 324, 324, 0, 1, 2, 327761, 33, 326, 322, 1, 524367, 7, 327, 326, 326, 0, 1, 2, 327761, 33, 328, 322, 2, 524367, 7, 329, 328, 328, 0, 1, 2, 393296, 323, 330, 325, 327, 329, 262205, 7, 332, 331, 327825, 7, 333, 330, 332, 393228, 7, 334, 1, 69, 333, 196670, 320, 334, 262205, 7, 337, 320, 196670, 336, 337, 262205, 33, 339, 201, 524367, 7, 340, 339, 339, 0, 1, 2, 196670, 338, 340, 327745, 72, 342, 165, 290, 262205, 33, 343, 342, 524367, 7, 344, 343, 343, 0, 1, 2, 196670, 341, 344, 196670, 345, 335, 524345, 7, 346, 16, 336, 338, 341, 345, 196670, 313, 346, 262205, 7, 347, 313, 262205, 7, 349, 320, 196670, 348, 349, 262205, 33, 351, 201, 524367, 7, 352, 351, 351, 0, 1, 2, 196670, 350, 352, 327745, 72, 354, 165, 290, 262205, 33, 355, 354, 524367, 7, 356, 355, 355, 0, 1, 2, 196670, 353, 356, 393281, 46, 358, 43, 44, 139, 262205, 9, 359, 358, 196670, 357, 359, 524345, 7, 360, 16, 348, 350, 353, 357, 327811, 7, 361, 347, 360, 196670, 317, 361, 393281, 64, 362, 165, 227, 101, 262205, 6, 363, 362, 327866, 30, 364, 363, 174, 196855, 366, 0, 262394, 364, 365, 395, 131320, 365, 262205, 7, 367, 320, 262271, 7, 368, 367, 196670, 369, 368, 262205, 33, 371, 201, 524367, 7, 372, 371, 371, 0, 1, 2, 196670, 370, 372, 327745, 72, 374, 165, 290, 262205, 33, 375, 374, 524367, 7, 376, 375, 375, 0, 1, 2, 196670, 373, 376, 196670, 377, 335, 524345, 7, 378, 16, 369, 370, 373, 377, 196670, 315, 378, 262205, 7, 379, 315, 262205, 7, 380, 320, 262271, 7, 381, 380, 196670, 382, 381, 262205, 33, 384, 201, 524367, 7, 385, 384, 384, 0, 1, 2, 196670, 383, 385, 327745, 72, 387, 165, 290, 262205, 33, 388, 387, 524367, 7, 389, 388, 388, 0, 1, 2, 196670, 386, 389, 393281, 46, 391, 43, 44, 139, 262205, 9, 392, 391, 196670, 390, 392, 524345, 7, 393, 16, 382, 383, 386, 390, 327811, 7, 394, 379, 393, 196670, 318, 394, 131321, 366, 131320, 395, 262205, 7, 396, 313, 196670, 315, 396, 262205, 7, 397, 317, 196670, 318, 397, 131321, 366, 131320, 366, 131321, 297, 131320, 297, 65789, 65592, 327734, 7, 16, 0, 11, 196663, 8, 12, 196663, 8, 13, 196663, 8, 14, 196663, 10, 15, 131320, 17, 262203, 8, 18, 7, 262203, 10, 21, 7, 262203, 59, 60, 7, 262203, 8, 70, 7, 262203, 8, 78, 7, 262203, 59, 85, 7, 262203, 59, 110, 7, 262203, 59, 118, 7, 262203, 8, 161, 7, 262203, 8, 178, 7, 196670, 18, 20, 196670, 21, 22, 131321, 23, 131320, 23, 262390, 25, 26, 0, 131321, 27, 131320, 27, 262205, 9, 28, 21, 327857, 30, 31, 28, 29, 262394, 31, 24, 25, 131320, 24, 262205, 9, 32, 21, 393281, 46, 47, 43, 44, 45, 262205, 9, 48, 47, 327855, 30, 49, 32, 48, 196855, 51, 0, 262394, 49, 50, 51, 131320, 50, 131321, 25, 131320, 51, 262205, 9, 53, 21, 262205, 9, 54, 15, 327850, 30, 55, 53, 54, 196855, 57, 0, 262394, 55, 56, 57, 131320, 56, 131321, 26, 131320, 57, 196670, 60, 61, 262205, 9, 62, 21, 458817, 64, 65, 43, 22, 62, 63, 262205, 6, 66, 65, 327860, 30, 67, 66, 19, 196855, 69, 0, 262394, 67, 68, 77, 131320, 68, 262205, 9, 71, 21, 393281, 72, 73, 43, 22, 71, 262205, 33, 74, 73, 524367, 7, 75, 74, 74, 0, 1, 2, 393228, 7, 76, 1, 69, 75, 196670, 70, 76, 131321, 69, 131320, 77, 262205, 9, 79, 21, 393281, 72, 80, 43, 22, 79, 262205, 33, 81, 80, 524367, 7, 82, 81, 81, 0, 1, 2, 262205, 7, 83, 13, 327811, 7, 84, 82, 83, 196670, 78, 84, 262205, 7, 86, 78, 393228, 6, 87, 1, 66, 86, 196670, 85, 87, 262205, 6, 88, 85, 327864, 30, 90, 88, 89, 196855, 92, 0, 262394, 90, 91, 94, 131320, 91, 196670, 70, 93, 131321, 92, 131320, 94, 262205, 7, 95, 78, 262205, 6, 96, 85, 393296, 7, 97, 96, 96, 96, 327816, 7, 98, 95, 97, 196670, 70, 98, 262205, 9, 100, 21, 458817, 64, 102, 43, 99, 100, 101, 262205, 6, 103, 102, 262205, 6, 104, 85, 327813, 6, 105, 103, 104, 262205, 6, 106, 85, 327813, 6, 107, 105, 106, 327809, 6, 108, 61, 107, 327816, 6, 109, 61, 108, 196670, 60, 109, 131321, 92, 131320, 92, 131321, 69, 131320, 69, 196670, 110, 61, 262205, 9, 111, 21, 458817, 64, 112, 43, 99, 111, 45, 262205, 6, 113, 112, 327864, 30, 115, 113, 114, 196855, 117, 0, 262394, 115, 116, 117, 131320, 116, 262205, 7, 119, 70, 262271, 7, 120, 119, 262205, 9, 122, 21, 393281, 72, 123, 43, 121, 122, 262205, 33, 124, 123, 524367, 7, 125, 124, 124, 0, 1, 2, 393228, 7, 126, 1, 69, 125, 327828, 6, 127, 120, 126, 196670, 118, 127, 262205, 6, 128, 118, 262205, 9, 129, 21, 458817, 64, 130, 43, 99, 129, 45, 262205, 6, 131, 130, 327864, 30, 132, 128, 131, 196855, 134, 0, 262394, 132, 133, 135, 131320, 133, 196670, 110, 19, 131321, 134, 131320, 135, 262205, 6, 136, 118, 458764, 6, 137, 1, 40, 136, 19, 262205, 9, 138, 21, 458817, 64, 140, 43, 99, 138, 139, 262205, 6, 141, 140, 458764, 6, 142, 1, 26, 137, 141, 196670, 110, 142, 131321, 134, 131320, 134, 131321, 117, 131320, 117, 262205, 9, 144, 21, 393281, 72, 145, 43, 143, 144, 262205, 33, 146, 145, 524367, 7, 147, 146, 146, 0, 1, 2, 262205, 7, 148, 12, 262205, 7, 149, 70, 327828, 6, 150, 148, 149, 458764, 6, 151, 1, 40, 150, 19, 327822, 7, 152, 147, 151, 262205, 6, 153, 60, 327822, 7, 154, 152, 153, 262205, 6, 155, 110, 327822, 7, 156, 154, 155, 262205, 7, 157, 18, 327809, 7, 158, 157, 156, 196670, 18, 158, 131321, 26, 131320, 26, 262205, 9, 159, 21, 327808, 9, 160, 159, 143, 196670, 21, 160, 131321, 23, 131320, 25, 327745, 72, 167, 165, 166, 262205, 33, 168, 167, 524367, 7, 169, 168, 168, 0, 1, 2, 262205, 7, 170, 18, 327809, 7, 171, 169, 170, 196670, 161, 171, 393281, 64, 172, 165, 166, 63, 262205, 6, 173, 172, 327866, 30, 175, 173, 174, 196855, 177, 0, 262394, 175, 176, 177, 131320, 176, 262205, 7, 179, 18, 393296, 7, 180, 19, 19, 19, 393296, 7, 181, 61, 61, 61, 524300, 7, 182, 1, 43, 179, 180, 181, 196670, 178, 182, 262205, 7, 183, 178, 327745, 72, 184, 165, 166, 262205, 33, 185, 184, 524367, 7, 186, 185, 185, 0, 1, 2, 262205, 7, 188, 178, 327811, 7, 189, 187, 188, 327813, 7, 190, 186, 189, 327809, 7, 191, 183, 190, 196670, 161, 191, 131321, 177, 131320, 177, 262205, 7, 192, 161, 262205, 7, 193, 14, 327813, 7, 194, 192, 193, 393296, 7, 195, 19, 19, 19, 393296, 7, 196, 61, 61, 61, 524300, 7, 197, 1, 43, 194, 195, 196, 131326, 197, 65592 };
static const uint32_t kSpvMeshFs[] = { 119734787, 66816, 524299, 254, 0, 131089, 1, 393227, 1, 1280527431, 1685353262, 808793134, 0, 196622, 0, 1, 1048591, 4, 4, 1852399981, 0, 94, 180, 183, 185, 190, 192, 202, 212, 230, 234, 241, 196624, 4, 7, 196611, 2, 450, 262149, 4, 1852399981, 0, 393221, 11, 1684105331, 1885304687, 1982359139, 3880038, 262149, 10, 1885957219, 0, 196613, 27, 112, 196613, 75, 115, 262149, 76, 1818321779, 101, 196613, 80, 105, 196613, 90, 100, 262149, 94, 1634227061, 7827300, 327685, 152, 1701080681, 1818386808, 101, 196613, 178, 7106403, 393221, 180, 1180658791, 1953394546, 1768120646, 26478, 262149, 183, 1869760118, 29806, 262149, 185, 1667318390, 107, 196613, 188, 7239027, 262149, 190, 1853182838, 70, 262149, 192, 1853182838, 66, 262149, 200, 1852138323, 101, 327686, 200, 0, 1936683116, 0, 327686, 200, 1, 1718183020, 0, 327686, 200, 2, 1869640556, 116, 262150, 200, 3, 7369580, 327686, 200, 4, 1180855649, 7105641, 262150, 200, 5, 6778726, 327686, 200, 6, 1868983913, 0, 196613, 202, 117, 262149, 212, 1734954102, 29800, 262149, 213, 1634886000, 109, 262149, 230, 1735345782, 0, 262149, 234, 1886142838, 24936, 262149, 241, 1734439526, 0, 262215, 94, 33, 2, 262215, 94, 34, 0, 196679, 152, 24, 262215, 180, 11, 17, 262215, 183, 30, 0, 262215, 185, 30, 1, 262215, 190, 30, 2, 262215, 192, 30, 3, 262215, 195, 6, 16, 262215, 196, 6, 16, 262215, 197, 6, 16, 262215, 198, 6, 16, 196679, 200, 2, 327752, 200, 0, 35, 0, 327752, 200, 1, 35, 256, 327752, 200, 2, 35, 512, 327752, 200, 3, 35, 768, 327752, 200, 4, 35, 1024, 327752, 200, 5, 35, 1040, 327752, 200, 6, 35, 1056, 262215, 202, 33, 0, 262215, 202, 34, 0, 262215, 212, 30, 4, 262215, 230, 30, 5, 262215, 234, 30, 6, 262215, 241, 30, 0, 131091, 2, 196641, 3, 2, 196630, 6, 32, 262167, 7, 6, 4, 262176, 8, 7, 7, 262177, 9, 6, 8, 262165, 13, 32, 0, 262187, 13, 14, 3, 262176, 15, 7, 6, 262187, 6, 18, 0, 131092, 19, 262187, 6, 23, 1065353216, 262167, 25, 6, 3, 262176, 26, 7, 25, 262187, 6, 34, 1056964608, 262187, 13, 38, 2, 262187, 13, 45, 0, 262187, 13, 60, 1, 262187, 6, 77, 1010827264, 262165, 78, 32, 1, 262176, 79, 7, 78, 262187, 78, 81, 0, 262187, 78, 88, 16, 589849, 91, 6, 1, 0, 0, 0, 1, 0, 196635, 92, 91, 262176, 93, 0, 92, 262203, 93, 94, 0, 262167, 96, 6, 2, 262187, 13, 99, 16, 262172, 100, 96, 99, 262187, 6, 101, 3211864058, 262187, 6, 102, 3201061344, 327724, 96, 103, 101, 102, 262187, 6, 104, 1064440302, 262187, 6, 105, 3208959771, 327724, 96, 106, 104, 105, 262187, 6, 107, 3183535000, 262187, 6, 108, 3211652203, 327724, 96, 109, 107, 108, 262187, 6, 110, 1051762308, 262187, 6, 111, 1050048288, 327724, 96, 112, 110, 111, 262187, 6, 113, 3211425662, 262187, 6, 114, 1055545736, 327724, 96, 115, 113, 114, 262187, 6, 116, 3209740500, 262187, 6, 117, 3210808912, 327724, 96, 118, 116, 117, 262187, 6, 119, 3200514852, 262187, 6, 120, 1049474200, 327724, 96, 121, 119, 120, 262187, 6, 122, 1064931168, 262187, 6, 123, 1061267692, 327724, 96, 124, 122, 123, 262187, 6, 125, 1055059832, 262187, 6, 126, 3212419372, 327724, 96, 127, 125, 126, 262187, 6, 128, 1057592576, 262187, 6, 129, 3203566922, 327724, 96, 130, 128, 129, 262187, 6, 131, 3196561928, 262187, 6, 132, 3201728006, 327724, 96, 133, 131, 132, 262187, 6, 134, 1061863138, 262187, 6, 135, 1044582281, 327724, 96, 136, 134, 135, 262187, 6, 137, 3195515288, 262187, 6, 138, 1065303976, 327724, 96, 139, 137, 138, 262187, 6, 140, 3209717972, 262187, 6, 141, 1063916682, 327724, 96, 142, 140, 141, 262187, 6, 143, 1045209904, 262187, 6, 144, 1061769832, 327724, 96, 145, 143, 144, 262187, 6, 146, 1041451160, 262187, 6, 147, 3188745312, 327724, 96, 148, 146, 147, 1245228, 100, 149, 103, 106, 109, 112, 115, 118, 121, 124, 127, 130, 133, 136, 139, 142, 145, 148, 262176, 151, 7, 100, 262176, 153, 7, 96, 262187, 6, 163, 983386450, 262187, 78, 171, 1, 262187, 6, 174, 1098907648, 262176, 179, 1, 19, 262203, 179, 180, 1, 262176, 182, 1, 25, 262203, 182, 183, 1, 262203, 182, 185, 1, 262203, 182, 190, 1, 262203, 182, 192, 1, 262172, 195, 7, 99, 262172, 196, 7, 99, 262172, 197, 7, 99, 262172, 198, 7, 99, 262167, 199, 78, 4, 589854, 200, 195, 196, 197, 198, 7, 7, 199, 262176, 201, 2, 200, 262203, 201, 202, 2, 262187, 78, 203, 6, 262176, 204, 2, 78, 262176, 211, 1, 7, 262203, 211, 212, 1, 262187, 78, 224, 5, 262176, 225, 2, 7, 262176, 229, 1, 6, 262203, 229, 230, 1, 262203, 229, 234, 1, 262187, 6, 236, 1069547520, 262176, 240, 3, 7, 262203, 240, 241, 3, 327734, 2, 4, 0, 3, 131320, 5, 262203, 26, 178, 7, 262203, 26, 188, 7, 262203, 8, 213, 7, 262205, 19, 181, 180, 262205, 25, 184, 183, 262205, 25, 186, 185, 393385, 25, 187, 181, 184, 186, 196670, 178, 187, 262205, 19, 189, 180, 262205, 25, 191, 190, 262205, 25, 193, 192, 393385, 25, 194, 189, 191, 193, 196670, 188, 194, 393281, 204, 205, 202, 203, 38, 262205, 78, 206, 205, 327853, 19, 207, 206, 81, 196855, 209, 0, 262394, 207, 208, 209, 131320, 208, 262205, 25, 210, 188, 262205, 7, 214, 212, 196670, 213, 214, 327737, 6, 215, 11, 213, 327811, 6, 216, 23, 215, 327822, 25, 217, 210, 216, 262205, 25, 218, 178, 327811, 25, 219, 218, 217, 196670, 178, 219, 131321, 209, 131320, 209, 262205, 25, 220, 178, 393296, 25, 221, 18, 18, 18, 393296, 25, 222, 23, 23, 23, 524300, 25, 223, 1, 43, 220, 221, 222, 327745, 225, 226, 202, 224, 262205, 7, 227, 226, 524367, 25, 228, 227, 227, 0, 1, 2, 262205, 6, 231, 230, 393296, 25, 232, 231, 231, 231, 524300, 25, 233, 1, 46, 223, 228, 232, 196670, 178, 233, 262205, 6, 235, 234, 327866, 19, 237, 235, 236, 196855, 239, 0, 262394, 237, 238, 239, 131320, 238, 262205, 25, 242, 183, 327761, 6, 243, 242, 0, 327761, 6, 244, 242, 1, 327761, 6, 245, 242, 2, 458832, 7, 246, 243, 244, 245, 23, 196670, 241, 246, 65789, 131320, 239, 262205, 25, 248, 178, 262205, 6, 249, 234, 327761, 6, 250, 248, 0, 327761, 6, 251, 248, 1, 327761, 6, 252, 248, 2, 458832, 7, 253, 250, 251, 252, 249, 196670, 241, 253, 65789, 65592, 327734, 6, 11, 0, 9, 196663, 8, 10, 131320, 12, 262203, 26, 27, 7, 262203, 15, 75, 7, 262203, 15, 76, 7, 262203, 79, 80, 7, 262203, 15, 90, 7, 327739, 151, 152, 7, 149, 327745, 15, 16, 10, 14, 262205, 6, 17, 16, 327868, 19, 20, 17, 18, 196855, 22, 0, 262394, 20, 21, 22, 131320, 21, 131326, 23, 131320, 22, 262205, 7, 28, 10, 524367, 25, 29, 28, 28, 0, 1, 2, 327745, 15, 30, 10, 14, 262205, 6, 31, 30, 393296, 25, 32, 31, 31, 31, 327816, 25, 33, 29, 32, 327822, 25, 35, 33, 34, 393296, 25, 36, 34, 34, 34, 327809, 25, 37, 35, 36, 196670, 27, 37, 327745, 15, 39, 27, 38, 262205, 6, 40, 39, 327866, 19, 41, 40, 23, 262312, 19, 42, 41, 196855, 44, 0, 262394, 42, 43, 44, 131320, 43, 327745, 15, 46, 27, 45, 262205, 6, 47, 46, 327864, 19, 48, 47, 18, 131321, 44, 131320, 44, 458997, 19, 49, 41, 22, 48, 43, 262312, 19, 50, 49, 196855, 52, 0, 262394, 50, 51, 52, 131320, 51, 327745, 15, 53, 27, 45, 262205, 6, 54, 53, 327866, 19, 55, 54, 23, 131321, 52, 131320, 52, 458997, 19, 56, 49, 44, 55, 51, 262312, 19, 57, 56, 196855, 59, 0, 262394, 57, 58, 59, 131320, 58, 327745, 15, 61, 27, 60, 262205, 6, 62, 61, 327864, 19, 63, 62, 18, 131321, 59, 131320, 59, 458997, 19, 64, 56, 52, 63, 58, 262312, 19, 65, 64, 196855, 67, 0, 262394, 65, 66, 67, 131320, 66, 327745, 15, 68, 27, 60, 262205, 6, 69, 68, 327866, 19, 70, 69, 23, 131321, 67, 131320, 67, 458997, 19, 71, 64, 59, 70, 66, 196855, 73, 0, 262394, 71, 72, 73, 131320, 72, 131326, 23, 131320, 73, 196670, 75, 18, 196670, 76, 77, 196670, 80, 81, 131321, 82, 131320, 82, 262390, 84, 85, 0, 131321, 86, 131320, 86, 262205, 78, 87, 80, 327857, 19, 89, 87, 88, 262394, 89, 83, 84, 131320, 83, 262205, 92, 95, 94, 262205, 25, 97, 27, 458831, 96, 98, 97, 97, 0, 1, 262205, 78, 150, 80, 327745, 153, 154, 152, 150, 262205, 96, 155, 154, 262205, 6, 156, 76, 327822, 96, 157, 155, 156, 327809, 96, 158, 98, 157, 327767, 7, 159, 95, 158, 327761, 6, 160, 159, 0, 196670, 90, 160, 327745, 15, 161, 27, 38, 262205, 6, 162, 161, 327811, 6, 164, 162, 163, 262205, 6, 165, 90, 327866, 19, 166, 164, 165, 393385, 6, 167, 166, 18, 23, 262205, 6, 168, 75, 327809, 6, 169, 168, 167, 196670, 75, 169, 131321, 85, 131320, 85, 262205, 78, 170, 80, 327808, 78, 172, 170, 171, 196670, 80, 172, 131321, 82, 131320, 84, 262205, 6, 173, 75, 327816, 6, 175, 173, 174, 131326, 175, 65592 };
static const uint32_t kSpvDepthVs[] = {
    119734787, 65536, 524299, 47, 0, 131089, 1, 393227, 1, 1280527431, 1685353262, 808793134,
    0, 196622, 0, 1, 458767, 0, 4, 1852399981, 0, 13, 25, 196611,
    2, 450, 262149, 4, 1852399981, 0, 393221, 11, 1348430951, 1700164197, 2019914866, 0,
    393222, 11, 0, 1348430951, 1953067887, 7237481, 458758, 11, 1, 1348430951, 1953393007, 1702521171,
    0, 458758, 11, 2, 1130327143, 1148217708, 1635021673, 6644590, 458758, 11, 3, 1130327143,
    1147956341, 1635021673, 6644590, 196613, 13, 0, 196613, 17, 17232, 262150, 17, 0,
    7370349, 196613, 19, 25456, 262149, 25, 1936674913, 0, 196679, 11, 2, 327752,
    11, 0, 11, 0, 327752, 11, 1, 11, 1, 327752, 11, 2,
    11, 3, 327752, 11, 3, 11, 4, 196679, 17, 2, 262216, 17,
    0, 5, 327752, 17, 0, 7, 16, 327752, 17, 0, 35, 0,
    262215, 25, 30, 0, 131091, 2, 196641, 3, 2, 196630, 6, 32,
    262167, 7, 6, 4, 262165, 8, 32, 0, 262187, 8, 9, 1,
    262172, 10, 6, 9, 393246, 11, 7, 6, 10, 10, 262176, 12,
    3, 11, 262203, 12, 13, 3, 262165, 14, 32, 1, 262187, 14,
    15, 0, 262168, 16, 7, 4, 196638, 17, 16, 262176, 18, 9,
    17, 262203, 18, 19, 9, 262176, 20, 9, 16, 262167, 23, 6,
    3, 262176, 24, 1, 23, 262203, 24, 25, 1, 262187, 6, 27,
    1065353216, 262176, 33, 3, 7, 262187, 8, 35, 2, 262176, 36, 3,
    6, 262187, 6, 39, 1056964608, 262187, 8, 41, 3, 327734, 2, 4,
    0, 3, 131320, 5, 327745, 20, 21, 19, 15, 262205, 16, 22,
    21, 262205, 23, 26, 25, 327761, 6, 28, 26, 0, 327761, 6,
    29, 26, 1, 327761, 6, 30, 26, 2, 458832, 7, 31, 28,
    29, 30, 27, 327825, 7, 32, 22, 31, 327745, 33, 34, 13,
    15, 196670, 34, 32, 393281, 36, 37, 13, 15, 35, 262205, 6,
    38, 37, 327813, 6, 40, 38, 39, 393281, 36, 42, 13, 15,
    41, 262205, 6, 43, 42, 327813, 6, 44, 43, 39, 327809, 6,
    45, 40, 44, 393281, 36, 46, 13, 15, 35, 196670, 46, 45,
    65789, 65592,
};
static const uint32_t kSpvMatVs[] = {
    119734787, 65536, 524299, 175, 0, 131089, 1, 393227, 1, 1280527431, 1685353262, 808793134,
    0, 196622, 0, 1, 1179663, 0, 4, 1852399981, 0, 21, 34, 80,
    83, 134, 136, 140, 143, 148, 157, 159, 161, 171, 196611, 2,
    450, 262149, 4, 1852399981, 0, 196613, 9, 6650213, 262149, 11, 2002874948, 0,
    262150, 11, 0, 7370349, 262150, 11, 1, 30317, 393222, 11, 2, 1751607660,
    1886801268, 0, 327686, 11, 3, 1952542062, 0, 327686, 11, 4, 1869377379, 114,
    327686, 11, 5, 1734437990, 115, 327686, 11, 6, 1920235621, 97, 327686, 11,
    7, 1180855649, 7105641, 196613, 13, 100, 262149, 21, 1936674913, 0, 393221, 32,
    1348430951, 1700164197, 2019914866, 0, 393222, 32, 0, 1348430951, 1953067887, 7237481, 458758, 32,
    1, 1348430951, 1953393007, 1702521171, 0, 458758, 32, 2, 1130327143, 1148217708, 1635021673, 6644590,
    458758, 32, 3, 1130327143, 1147956341, 1635021673, 6644590, 196613, 34, 0, 262149, 80,
    1702446454, 0, 262149, 83, 1735345782, 0, 262149, 91, 1852138323, 101, 327686, 91,
    0, 1936683116, 0, 327686, 91, 1, 1718183020, 0, 327686, 91, 2, 1869640556,
    116, 262150, 91, 3, 7369580, 327686, 91, 4, 1180855649, 7105641, 262150, 91,
    5, 6778726, 327686, 91, 6, 1868983913, 0, 196613, 93, 117, 262149, 102,
    1953720676, 0, 262149, 106, 1918989427, 116, 196613, 123, 78, 196613, 134, 20086,
    262149, 136, 1836207713, 0, 196613, 140, 21622, 262149, 143, 1851872353, 0, 196613,
    148, 17014, 196613, 157, 7755126, 196613, 159, 7755105, 262149, 161, 1734954102, 29800,
    262149, 171, 1886142838, 24936, 196679, 11, 2, 262216, 11, 0, 5, 327752,
    11, 0, 7, 16, 327752, 11, 0, 35, 0, 262216, 11, 1,
    5, 327752, 11, 1, 7, 16, 327752, 11, 1, 35, 64, 262216,
    11, 2, 5, 327752, 11, 2, 7, 16, 327752, 11, 2, 35,
    128, 262216, 11, 3, 5, 327752, 11, 3, 7, 16, 327752, 11,
    3, 35, 192, 327752, 11, 4, 35, 256, 327752, 11, 5, 35,
    272, 327752, 11, 6, 35, 288, 327752, 11, 7, 35, 304, 262215,
    13, 33, 1, 262215, 13, 34, 0, 262215, 21, 30, 0, 196679,
    32, 2, 327752, 32, 0, 11, 0, 327752, 32, 1, 11, 1,
    327752, 32, 2, 11, 3, 327752, 32, 3, 11, 4, 262215, 80,
    30, 0, 262215, 83, 30, 2, 262215, 86, 6, 16, 262215, 87,
    6, 16, 262215, 88, 6, 16, 262215, 89, 6, 16, 196679, 91,
    2, 327752, 91, 0, 35, 0, 327752, 91, 1, 35, 256, 327752,
    91, 2, 35, 512, 327752, 91, 3, 35, 768, 327752, 91, 4,
    35, 1024, 327752, 91, 5, 35, 1040, 327752, 91, 6, 35, 1056,
    262215, 93, 33, 0, 262215, 93, 34, 0, 262215, 134, 30, 3,
    262215, 136, 30, 1, 262215, 140, 30, 4, 262215, 143, 30, 2,
    262215, 148, 30, 5, 262215, 157, 30, 6, 262215, 159, 30, 3,
    262215, 161, 30, 1, 262215, 171, 30, 7, 131091, 2, 196641, 3,
    2, 196630, 6, 32, 262167, 7, 6, 4, 262176, 8, 7, 7,
    262168, 10, 7, 4, 655390, 11, 10, 10, 10, 10, 7, 7,
    7, 7, 262176, 12, 2, 11, 262203, 12, 13, 2, 262165, 14,
    32, 1, 262187, 14, 15, 1, 262176, 16, 2, 10, 262167, 19,
    6, 3, 262176, 20, 1, 19, 262203, 20, 21, 1, 262187, 6,
    23, 1065353216, 262165, 29, 32, 0, 262187, 29, 30, 1, 262172, 31,
    6, 30, 393246, 32, 7, 6, 31, 31, 262176, 33, 3, 32,
    262203, 33, 34, 3, 262187, 14, 35, 0, 262176, 44, 3, 7,
    262187, 14, 46, 5, 262187, 29, 47, 3, 262176, 48, 2, 6,
    262187, 14, 51, 6, 262187, 29, 52, 0, 262167, 55, 6, 2,
    262176, 57, 3, 6, 262187, 29, 69, 2, 262187, 6, 72, 1056964608,
    262176, 79, 3, 19, 262203, 79, 80, 3, 262203, 57, 83, 3,
    262187, 6, 84, 0, 262187, 29, 85, 16, 262172, 86, 7, 85,
    262172, 87, 7, 85, 262172, 88, 7, 85, 262172, 89, 7, 85,
    262167, 90, 14, 4, 589854, 91, 86, 87, 88, 89, 7, 7,
    90, 262176, 92, 2, 91, 262203, 92, 93, 2, 262187, 6, 96,
    981668463, 131092, 97, 262176, 101, 7, 6, 262187, 6, 109, 1048576000, 262168,
    121, 19, 3, 262176, 122, 7, 121, 262187, 14, 124, 3, 262203,
    79, 134, 3, 262203, 20, 136, 1, 262203, 79, 140, 3, 262176,
    142, 1, 7, 262203, 142, 143, 1, 262203, 79, 148, 3, 262176,
    152, 1, 6, 262176, 156, 3, 55, 262203, 156, 157, 3, 262176,
    158, 1, 55, 262203, 158, 159, 1, 262203, 44, 161, 3, 262187,
    14, 162, 2, 262203, 57, 171, 3, 262187, 14, 172, 4, 327734,
    2, 4, 0, 3, 131320, 5, 262203, 8, 9, 7, 262203, 101,
    102, 7, 262203, 101, 106, 7, 262203, 122, 123, 7, 327745, 16,
    17, 13, 15, 262205, 10, 18, 17, 262205, 19, 22, 21, 327761,
    6, 24, 22, 0, 327761, 6, 25, 22, 1, 327761, 6, 26,
    22, 2, 458832, 7, 27, 24, 25, 26, 23, 327825, 7, 28,
    18, 27, 196670, 9, 28, 327745, 16, 36, 13, 35, 262205, 10,
    37, 36, 262205, 19, 38, 21, 327761, 6, 39, 38, 0, 327761,
    6, 40, 38, 1, 327761, 6, 41, 38, 2, 458832, 7, 42,
    39, 40, 41, 23, 327825, 7, 43, 37, 42, 327745, 44, 45,
    34, 35, 196670, 45, 43, 393281, 48, 49, 13, 46, 47, 262205,
    6, 50, 49, 393281, 48, 53, 13, 51, 52, 262205, 6, 54,
    53, 327760, 55, 56, 50, 54, 393281, 57, 58, 34, 35, 47,
    262205, 6, 59, 58, 327822, 55, 60, 56, 59, 327745, 44, 61,
    34, 35, 262205, 7, 62, 61, 458831, 55, 63, 62, 62, 0,
    1, 327809, 55, 64, 63, 60, 393281, 57, 65, 34, 35, 52,
    327761, 6, 66, 64, 0, 196670, 65, 66, 393281, 57, 67, 34,
    35, 30, 327761, 6, 68, 64, 1, 196670, 67, 68, 393281, 57,
    70, 34, 35, 69, 262205, 6, 71, 70, 327813, 6, 73, 71,
    72, 393281, 57, 74, 34, 35, 47, 262205, 6, 75, 74, 327813,
    6, 76, 75, 72, 327809, 6, 77, 73, 76, 393281, 57, 78,
    34, 35, 69, 196670, 78, 77, 262205, 7, 81, 9, 524367, 19,
    82, 81, 81, 0, 1, 2, 196670, 80, 82, 196670, 83, 84,
    393281, 48, 94, 93, 46, 47, 262205, 6, 95, 94, 327866, 97,
    98, 95, 96, 196855, 100, 0, 262394, 98, 99, 100, 131320, 99,
    262205, 7, 103, 9, 524367, 19, 104, 103, 103, 0, 1, 2,
    393228, 6, 105, 1, 66, 104, 196670, 102, 105, 393281, 48, 107,
    93, 46, 47, 262205, 6, 108, 107, 327813, 6, 110, 108, 109,
    196670, 106, 110, 262205, 6, 111, 102, 262205, 6, 112, 106, 327811,
    6, 113, 111, 112, 393281, 48, 114, 93, 46, 47, 262205, 6,
    115, 114, 262205, 6, 116, 106, 327811, 6, 117, 115, 116, 458764,
    6, 118, 1, 40, 117, 96, 327816, 6, 119, 113, 118, 524300,
    6, 120, 1, 43, 119, 84, 23, 196670, 83, 120, 131321, 100,
    131320, 100, 327745, 16, 125, 13, 124, 262205, 10, 126, 125, 327761,
    7, 127, 126, 0, 524367, 19, 128, 127, 127, 0, 1, 2,
    327761, 7, 129, 126, 1, 524367, 19, 130, 129, 129, 0, 1,
    2, 327761, 7, 131, 126, 2, 524367, 19, 132, 131, 131, 0,
    1, 2, 393296, 121, 133, 128, 130, 132, 196670, 123, 133, 262205,
    121, 135, 123, 262205, 19, 137, 136, 327825, 19, 138, 135, 137,
    393228, 19, 139, 1, 69, 138, 196670, 134, 139, 262205, 121, 141,
    123, 262205, 7, 144, 143, 524367, 19, 145, 144, 144, 0, 1,
    2, 327825, 19, 146, 141, 145, 393228, 19, 147, 1, 69, 146,
    196670, 140, 147, 262205, 19, 149, 134, 262205, 19, 150, 140, 458764,
    19, 151, 1, 68, 149, 150, 327745, 152, 153, 143, 47, 262205,
    6, 154, 153, 327822, 19, 155, 151, 154, 196670, 148, 155, 262205,
    55, 160, 159, 196670, 157, 160, 327745, 16, 163, 13, 162, 262205,
    10, 164, 163, 262205, 19, 165, 21, 327761, 6, 166, 165, 0,
    327761, 6, 167, 165, 1, 327761, 6, 168, 165, 2, 458832, 7,
    169, 166, 167, 168, 23, 327825, 7, 170, 164, 169, 196670, 161,
    170, 393281, 48, 173, 13, 172, 47, 262205, 6, 174, 173, 196670,
    171, 174, 65789, 65592,
};
static const uint32_t kSpvMatFs[] = { 119734787, 66816, 524299, 638, 0, 131089, 1, 393227, 1, 1280527431, 1685353262, 808793134, 0, 196622, 0, 1, 1507343, 4, 4, 1852399981, 0, 102, 222, 329, 379, 426, 430, 436, 448, 460, 478, 495, 525, 574, 589, 607, 625, 630, 632, 196624, 4, 7, 196611, 2, 450, 262149, 4, 1852399981, 0, 393221, 11, 1684105331, 1885304687, 1982359139, 3880038, 262149, 10, 1885957219, 0, 589829, 22, 1684105331, 1719019621, 1719024435, 1719024435, 828783411, 993093179, 0, 196613, 17, 110, 196613, 18, 25968, 262149, 19, 1700949089, 28516, 262149, 20, 1635018093, 108, 262149, 21, 1735749490, 104, 196613, 35, 112, 196613, 83, 115, 262149, 84, 1818321779, 101, 196613, 88, 105, 196613, 98, 7365988, 262149, 102, 1634227061, 7827300, 327685, 160, 1701080681, 1818386808, 101, 262149, 186, 1701996900, 29795, 262149, 188, 1667592307, 0, 196613, 189, 86, 262149, 193, 1852401779, 0, 196613, 199, 12358, 196613, 206, 105, 262149, 220, 1852138323, 101, 327686, 220, 0, 1936683116, 0, 327686, 220, 1, 1718183020, 0, 327686, 220, 2, 1869640556, 116, 262150, 220, 3, 7369580, 327686, 220, 4, 1180855649, 7105641, 262150, 220, 5, 6778726, 327686, 220, 6, 1868983913, 0, 196613, 222, 117, 196613, 231, 7631969, 196613, 239, 5009268, 196613, 247, 76, 262149, 254, 1953720676, 0, 262149, 278, 1953460339, 0, 196613, 286, 115, 196613, 310, 7103598, 196613, 315, 26739, 262149, 329, 1734954102, 29800, 262149, 330, 1634886000, 109, 262149, 333, 1886216556, 0, 196613, 349, 72, 196613, 375, 7629164, 262149, 377, 2002874948, 0, 262150, 377, 0, 7370349, 262150, 377, 1, 30317, 393222, 377, 2, 1751607660, 1886801268, 0, 327686, 377, 3, 1952542062, 0, 327686, 377, 4, 1869377379, 114, 327686, 377, 5, 1734437990, 115, 327686, 377, 6, 1920235621, 97, 327686, 377, 7, 1180855649, 7105641, 196613, 379, 100, 196613, 391, 99, 262149, 405, 1717987684, 0, 196613, 424, 30325, 196613, 426, 7755126, 196613, 428, 7172718, 393221, 430, 1180658791, 1953394546, 1768120646, 26478, 196613, 436, 20086, 196613, 443, 7233908, 196613, 448, 21622, 196613, 455, 7629154, 196613, 460, 17014, 196613, 467, 7364973, 262149, 471, 1700949089, 28516, 262149, 478, 1935753845, 101, 196613, 494, 28276, 262149, 495, 1836207733, 0, 196613, 524, 104, 262149, 525, 1768245365, 7628903, 196613, 530, 30820, 196613, 533, 31076, 196613, 536, 30824, 196613, 545, 31080, 262149, 567, 1635018093, 108, 262149, 574, 1952796021, 27745, 262149, 581, 1735749490, 104, 262149, 589, 1970229877, 26727, 196613, 597, 7106403, 262149, 607, 1702446454, 0, 262149, 608, 1634886000, 109, 262149, 610, 1634886000, 109, 262149, 612, 1634886000, 109, 262149, 614, 1634886000, 109, 262149, 616, 1634886000, 109, 262149, 625, 1735345782, 0, 262149, 630, 1734439526, 0, 262149, 632, 1886142838, 24936, 262215, 102, 33, 2, 262215, 102, 34, 0, 196679, 160, 24, 262215, 215, 6, 16, 262215, 216, 6, 16, 262215, 217, 6, 16, 262215, 218, 6, 16, 196679, 220, 2, 327752, 220, 0, 35, 0, 327752, 220, 1, 35, 256, 327752, 220, 2, 35, 512, 327752, 220, 3, 35, 768, 327752, 220, 4, 35, 1024, 327752, 220, 5, 35, 1040, 327752, 220, 6, 35, 1056, 262215, 222, 33, 0, 262215, 222, 34, 0, 262215, 329, 30, 1, 196679, 377, 2, 262216, 377, 0, 5, 327752, 377, 0, 7, 16, 327752, 377, 0, 35, 0, 262216, 377, 1, 5, 327752, 377, 1, 7, 16, 327752, 377, 1, 35, 64, 262216, 377, 2, 5, 327752, 377, 2, 7, 16, 327752, 377, 2, 35, 128, 262216, 377, 3, 5, 327752, 377, 3, 7, 16, 327752, 377, 3, 35, 192, 327752, 377, 4, 35, 256, 327752, 377, 5, 35, 272, 327752, 377, 6, 35, 288, 327752, 377, 7, 35, 304, 262215, 379, 33, 1, 262215, 379, 34, 0, 262215, 426, 30, 6, 262215, 430, 11, 17, 262215, 436, 30, 3, 262215, 448, 30, 4, 262215, 460, 30, 5, 262215, 478, 33, 3, 262215, 478, 34, 0, 262215, 495, 33, 4, 262215, 495, 34, 0, 262215, 525, 33, 7, 262215, 525, 34, 0, 262215, 574, 33, 5, 262215, 574, 34, 0, 262215, 589, 33, 6, 262215, 589, 34, 0, 262215, 607, 30, 0, 262215, 625, 30, 2, 262215, 630, 30, 0, 262215, 632, 30, 7, 131091, 2, 196641, 3, 2, 196630, 6, 32, 262167, 7, 6, 4, 262176, 8, 7, 7, 262177, 9, 6, 8, 262167, 13, 6, 3, 262176, 14, 7, 13, 262176, 15, 7, 6, 524321, 16, 13, 14, 14, 14, 15, 15, 262165, 24, 32, 0, 262187, 24, 25, 3, 262187, 6, 28, 0, 131092, 29, 262187, 6, 33, 1065353216, 262187, 6, 42, 1056964608, 262187, 24, 46, 2, 262187, 24, 53, 0, 262187, 24, 68, 1, 262187, 6, 85, 1010827264, 262165, 86, 32, 1, 262176, 87, 7, 86, 262187, 86, 89, 0, 262187, 86, 96, 16, 589849, 99, 6, 1, 0, 0, 0, 1, 0, 196635, 100, 99, 262176, 101, 0, 100, 262203, 101, 102, 0, 262167, 104, 6, 2, 262187, 24, 107, 16, 262172, 108, 104, 107, 262187, 6, 109, 3211864058, 262187, 6, 110, 3201061344, 327724, 104, 111, 109, 110, 262187, 6, 112, 1064440302, 262187, 6, 113, 3208959771, 327724, 104, 114, 112, 113, 262187, 6, 115, 3183535000, 262187, 6, 116, 3211652203, 327724, 104, 117, 115, 116, 262187, 6, 118, 1051762308, 262187, 6, 119, 1050048288, 327724, 104, 120, 118, 119, 262187, 6, 121, 3211425662, 262187, 6, 122, 1055545736, 327724, 104, 123, 121, 122, 262187, 6, 124, 3209740500, 262187, 6, 125, 3210808912, 327724, 104, 126, 124, 125, 262187, 6, 127, 3200514852, 262187, 6, 128, 1049474200, 327724, 104, 129, 127, 128, 262187, 6, 130, 1064931168, 262187, 6, 131, 1061267692, 327724, 104, 132, 130, 131, 262187, 6, 133, 1055059832, 262187, 6, 134, 3212419372, 327724, 104, 135, 133, 134, 262187, 6, 136, 1057592576, 262187, 6, 137, 3203566922, 327724, 104, 138, 136, 137, 262187, 6, 139, 3196561928, 262187, 6, 140, 3201728006, 327724, 104, 141, 139, 140, 262187, 6, 142, 1061863138, 262187, 6, 143, 1044582281, 327724, 104, 144, 142, 143, 262187, 6, 145, 3195515288, 262187, 6, 146, 1065303976, 327724, 104, 147, 145, 146, 262187, 6, 148, 3209717972, 262187, 6, 149, 1063916682, 327724, 104, 150, 148, 149, 262187, 6, 151, 1045209904, 262187, 6, 152, 1061769832, 327724, 104, 153, 151, 152, 262187, 6, 154, 1041451160, 262187, 6, 155, 3188745312, 327724, 104, 156, 154, 155, 1245228, 108, 157, 111, 114, 117, 120, 123, 126, 129, 132, 135, 138, 141, 144, 147, 150, 153, 156, 262176, 159, 7, 108, 262176, 161, 7, 104, 262187, 6, 171, 983386450, 262187, 86, 179, 1, 262187, 6, 182, 1098907648, 393260, 13, 187, 28, 28, 28, 262187, 6, 194, 1119879168, 262187, 6, 195, 1086324736, 262187, 6, 200, 1025758986, 393260, 13, 201, 200, 200, 200, 262172, 215, 7, 107, 262172, 216, 7, 107, 262172, 217, 7, 107, 262172, 218, 7, 107, 262167, 219, 86, 4, 589854, 220, 215, 216, 217, 218, 7, 7, 219, 262176, 221, 2, 220, 262203, 221, 222, 2, 262187, 86, 223, 6, 262176, 224, 2, 86, 262176, 233, 2, 6, 262176, 241, 2, 7, 262187, 6, 258, 925353388, 393260, 13, 262, 28, 28, 33, 262187, 86, 268, 3, 262187, 6, 282, 1069547520, 262187, 86, 289, 2, 262176, 328, 1, 7, 262203, 328, 329, 1, 262168, 376, 7, 4, 655390, 377, 376, 376, 376, 376, 7, 7, 7, 7, 262176, 378, 2, 377, 262203, 378, 379, 2, 262187, 86, 380, 7, 393260, 13, 400, 33, 33, 33, 262187, 6, 416, 1043878380, 262176, 425, 1, 104, 262203, 425, 426, 1, 262176, 429, 1, 29, 262203, 429, 430, 1, 262176, 435, 1, 13, 262203, 435, 436, 1, 262203, 435, 448, 1, 262203, 435, 460, 1, 262203, 101, 478, 0, 262187, 86, 484, 4, 262203, 101, 495, 0, 262187, 6, 500, 1073741824, 262203, 101, 525, 0, 262187, 6, 558, 1082130432, 262203, 101, 574, 0, 262187, 86, 583, 8, 262203, 101, 589, 0, 262187, 6, 595, 1057803469, 262187, 86, 598, 5, 262203, 435, 607, 1, 262176, 624, 1, 6, 262203, 624, 625, 1, 262176, 629, 3, 7, 262203, 629, 630, 3, 262203, 624, 632, 1, 327734, 2, 4, 0, 3, 131320, 5, 262203, 161, 424, 7, 262203, 14, 428, 7, 262203, 14, 432, 7, 262203, 14, 443, 7, 262203, 14, 445, 7, 262203, 14, 455, 7, 262203, 14, 457, 7, 262203, 87, 467, 7, 262203, 14, 471, 7, 262203, 14, 475, 7, 262203, 14, 494, 7, 262203, 15, 524, 7, 262203, 161, 530, 7, 262203, 161, 533, 7, 262203, 15, 536, 7, 262203, 15, 545, 7, 262203, 15, 567, 7, 262203, 15, 571, 7, 262203, 15, 581, 7, 262203, 15, 586, 7, 262203, 14, 597, 7, 262203, 14, 602, 7, 262203, 14, 608, 7, 262203, 14, 610, 7, 262203, 14, 612, 7, 262203, 15, 614, 7, 262203, 15, 616, 7, 262205, 104, 427, 426, 196670, 424, 427, 262205, 29, 431, 430, 196855, 434, 0, 262394, 431, 433, 438, 131320, 433, 262205, 13, 437, 436, 196670, 432, 437, 131321, 434, 131320, 438, 262205, 13, 439, 436, 262271, 13, 440, 439, 196670, 432, 440, 131321, 434, 131320, 434, 262205, 13, 441, 432, 393228, 13, 442, 1, 69, 441, 196670, 428, 442, 262205, 29, 444, 430, 196855, 447, 0, 262394, 444, 446, 450, 131320, 446, 262205, 13, 449, 448, 196670, 445, 449, 131321, 447, 131320, 450, 262205, 13, 451, 448, 262271, 13, 452, 451, 196670, 445, 452, 131321, 447, 131320, 447, 262205, 13, 453, 445, 393228, 13, 454, 1, 69, 453, 196670, 443, 454, 262205, 29, 456, 430, 196855, 459, 0, 262394, 456, 458, 462, 131320, 458, 262205, 13, 461, 460, 196670, 457, 461, 131321, 459, 131320, 462, 262205, 13, 463, 460, 262271, 13, 464, 463, 196670, 457, 464, 131321, 459, 131320, 459, 262205, 13, 465, 457, 393228, 13, 466, 1, 69, 465, 196670, 455, 466, 393281, 233, 468, 379, 223, 46, 262205, 6, 469, 468, 262254, 86, 470, 469, 196670, 467, 470, 262205, 86, 472, 467, 327879, 86, 473, 472, 179, 327851, 29, 474, 473, 89, 196855, 477, 0, 262394, 474, 476, 483, 131320, 476, 262205, 100, 479, 478, 262205, 104, 480, 424, 327767, 7, 481, 479, 480, 524367, 13, 482, 481, 481, 0, 1, 2, 196670, 475, 482, 131321, 477, 131320, 483, 327745, 241, 485, 379, 484, 262205, 7, 486, 485, 524367, 13, 487, 486, 486, 0, 1, 2, 196670, 475, 487, 131321, 477, 131320, 477, 262205, 13, 488, 475, 196670, 471, 488, 262205, 86, 489, 467, 327879, 86, 490, 489, 289, 327851, 29, 491, 490, 89, 196855, 493, 0, 262394, 491, 492, 493, 131320, 492, 262205, 100, 496, 495, 262205, 104, 497, 424, 327767, 7, 498, 496, 497, 524367, 13, 499, 498, 498, 0, 1, 2, 327822, 13, 501, 499, 500, 393296, 13, 502, 33, 33, 33, 327811, 13, 503, 501, 502, 196670, 494, 503, 262205, 13, 504, 443, 327745, 15, 505, 494, 53, 262205, 6, 506, 505, 327822, 13, 507, 504, 506, 262205, 13, 508, 455, 327745, 15, 509, 494, 68, 262205, 6, 510, 509, 327822, 13, 511, 508, 510, 327809, 13, 512, 507, 511, 262205, 13, 513, 428, 327745, 15, 514, 494, 46, 262205, 6, 515, 514, 327822, 13, 516, 513, 515, 327809, 13, 517, 512, 516, 393228, 13, 518, 1, 69, 517, 196670, 428, 518, 131321, 493, 131320, 493, 262205, 86, 519, 467, 327879, 86, 520, 519, 96, 327851, 29, 521, 520, 89, 196855, 523, 0, 262394, 521, 522, 523, 131320, 522, 262205, 100, 526, 525, 262205, 104, 527, 424, 327767, 7, 528, 526, 527, 327761, 6, 529, 528, 0, 196670, 524, 529, 262205, 104, 531, 424, 262351, 104, 532, 531, 196670, 530, 532, 262205, 104, 534, 424, 262352, 104, 535, 534, 196670, 533, 535, 262205, 100, 537, 525, 262205, 104, 538, 424, 262205, 104, 539, 530, 327809, 104, 540, 538, 539, 327767, 7, 541, 537, 540, 327761, 6, 542, 541, 0, 262205, 6, 543, 524, 327811, 6, 544, 542, 543, 196670, 536, 544, 262205, 100, 546, 525, 262205, 104, 547, 424, 262205, 104, 548, 533, 327809, 104, 549, 547, 548, 327767, 7, 550, 546, 549, 327761, 6, 551, 550, 0, 262205, 6, 552, 524, 327811, 6, 553, 551, 552, 196670, 545, 553, 262205, 13, 554, 428, 262205, 13, 555, 443, 262205, 6, 556, 536, 327822, 13, 557, 555, 556, 327822, 13, 559, 557, 558, 327811, 13, 560, 554, 559, 262205, 13, 561, 455, 262205, 6, 562, 545, 327822, 13, 563, 561, 562, 327822, 13, 564, 563, 558, 327811, 13, 565, 560, 564, 393228, 13, 566, 1, 69, 565, 196670, 428, 566, 131321, 523, 131320, 523, 262205, 86, 568, 467, 327879, 86, 569, 568, 484, 327851, 29, 570, 569, 89, 196855, 573, 0, 262394, 570, 572, 579, 131320, 572, 262205, 100, 575, 574, 262205, 104, 576, 424, 327767, 7, 577, 575, 576, 327761, 6, 578, 577, 0, 196670, 571, 578, 131321, 573, 131320, 579, 196670, 571, 28, 131321, 573, 131320, 573, 262205, 6, 580, 571, 196670, 567, 580, 262205, 86, 582, 467, 327879, 86, 584, 582, 583, 327851, 29, 585, 584, 89, 196855, 588, 0, 262394, 585, 587, 594, 131320, 587, 262205, 100, 590, 589, 262205, 104, 591, 424, 327767, 7, 592, 590, 591, 327761, 6, 593, 592, 0, 196670, 586, 593, 131321, 588, 131320, 594, 196670, 586, 595, 131321, 588, 131320, 588, 262205, 6, 596, 586, 196670, 581, 596, 393281, 233, 599, 379, 598, 53, 262205, 6, 600, 599, 327864, 29, 601, 600, 42, 196855, 604, 0, 262394, 601, 603, 606, 131320, 603, 262205, 13, 605, 471, 196670, 602, 605, 131321, 604, 131320, 606, 262205, 13, 609, 428, 196670, 608, 609, 262205, 13, 611, 607, 196670, 610, 611, 262205, 13, 613, 471, 196670, 612, 613, 262205, 6, 615, 567, 196670, 614, 615, 262205, 6, 617, 581, 196670, 616, 617, 589881, 13, 618, 22, 608, 610, 612, 614, 616, 196670, 602, 618, 131321, 604, 131320, 604, 262205, 13, 619, 602, 196670, 597, 619, 262205, 13, 620, 597, 327745, 241, 621, 222, 598, 262205, 7, 622, 621, 524367, 13, 623, 622, 622, 0, 1, 2, 262205, 6, 626, 625, 393296, 13, 627, 626, 626, 626, 524300, 13, 628, 1, 46, 620, 623, 627, 196670, 597, 628, 262205, 13, 631, 597, 262205, 6, 633, 632, 327761, 6, 634, 631, 0, 327761, 6, 635, 631, 1, 327761, 6, 636, 631, 2, 458832, 7, 637, 634, 635, 636, 633, 196670, 630, 637, 65789, 65592, 327734, 6, 11, 0, 9, 196663, 8, 10, 131320, 12, 262203, 14, 35, 7, 262203, 15, 83, 7, 262203, 15, 84, 7, 262203, 87, 88, 7, 262203, 15, 98, 7, 327739, 159, 160, 7, 157, 327745, 15, 26, 10, 25, 262205, 6, 27, 26, 327868, 29, 30, 27, 28, 196855, 32, 0, 262394, 30, 31, 32, 131320, 31, 131326, 33, 131320, 32, 262205, 7, 36, 10, 524367, 13, 37, 36, 36, 0, 1, 2, 327745, 15, 38, 10, 25, 262205, 6, 39, 38, 393296, 13, 40, 39, 39, 39, 327816, 13, 41, 37, 40, 327822, 13, 43, 41, 42, 393296, 13, 44, 42, 42, 42, 327809, 13, 45, 43, 44, 196670, 35, 45, 327745, 15, 47, 35, 46, 262205, 6, 48, 47, 327866, 29, 49, 48, 33, 262312, 29, 50, 49, 196855, 52, 0, 262394, 50, 51, 52, 131320, 51, 327745, 15, 54, 35, 53, 262205, 6, 55, 54, 327864, 29, 56, 55, 28, 131321, 52, 131320, 52, 458997, 29, 57, 49, 32, 56, 51, 262312, 29, 58, 57, 196855, 60, 0, 262394, 58, 59, 60, 131320, 59, 327745, 15, 61, 35, 53, 262205, 6, 62, 61, 327866, 29, 63, 62, 33, 131321, 60, 131320, 60, 458997, 29, 64, 57, 52, 63, 59, 262312, 29, 65, 64, 196855, 67, 0, 262394, 65, 66, 67, 131320, 66, 327745, 15, 69, 35, 68, 262205, 6, 70, 69, 327864, 29, 71, 70, 28, 131321, 67, 131320, 67, 458997, 29, 72, 64, 60, 71, 66, 262312, 29, 73, 72, 196855, 75, 0, 262394, 73, 74, 75, 131320, 74, 327745, 15, 76, 35, 68, 262205, 6, 77, 76, 327866, 29, 78, 77, 33, 131321, 75, 131320, 75, 458997, 29, 79, 72, 67, 78, 74, 196855, 81, 0, 262394, 79, 80, 81, 131320, 80, 131326, 33, 131320, 81, 196670, 83, 28, 196670, 84, 85, 196670, 88, 89, 131321, 90, 131320, 90, 262390, 92, 93, 0, 131321, 94, 131320, 94, 262205, 86, 95, 88, 327857, 29, 97, 95, 96, 262394, 97, 91, 92, 131320, 91, 262205, 100, 103, 102, 262205, 13, 105, 35, 458831, 104, 106, 105, 105, 0, 1, 262205, 86, 158, 88, 327745, 161, 162, 160, 158, 262205, 104, 163, 162, 262205, 6, 164, 84, 327822, 104, 165, 163, 164, 327809, 104, 166, 106, 165, 327767, 7, 167, 103, 166, 327761, 6, 168, 167, 0, 196670, 98, 168, 327745, 15, 169, 35, 46, 262205, 6, 170, 169, 327811, 6, 172, 170, 171, 262205, 6, 173, 98, 327866, 29, 174, 172, 173, 393385, 6, 175, 174, 28, 33, 262205, 6, 176, 83, 327809, 6, 177, 176, 175, 196670, 83, 177, 131321, 93, 131320, 93, 262205, 86, 178, 88, 327808, 86, 180, 178, 179, 196670, 88, 180, 131321, 90, 131320, 92, 262205, 6, 181, 83, 327816, 6, 183, 181, 182, 131326, 183, 65592, 327734, 13, 22, 0, 16, 196663, 14, 17, 196663, 14, 18, 196663, 14, 19, 196663, 15, 20, 196663, 15, 21, 131320, 23, 262203, 14, 186, 7, 262203, 14, 188, 7, 262203, 14, 189, 7, 262203, 15, 193, 7, 262203, 14, 199, 7, 262203, 87, 206, 7, 262203, 15, 231, 7, 262203, 14, 239, 7, 262203, 14, 247, 7, 262203, 15, 254, 7, 262203, 15, 278, 7, 262203, 15, 286, 7, 262203, 15, 310, 7, 262203, 15, 315, 7, 262203, 8, 330, 7, 262203, 15, 333, 7, 262203, 14, 349, 7, 262203, 14, 375, 7, 262203, 14, 391, 7, 262203, 14, 405, 7, 196670, 186, 187, 196670, 188, 187, 262205, 13, 190, 18, 262271, 13, 191, 190, 393228, 13, 192, 1, 69, 191, 196670, 189, 192, 262205, 6, 196, 21, 524300, 6, 197, 1, 43, 196, 28, 33, 524300, 6, 198, 1, 46, 194, 195, 197, 196670, 193, 198, 262205, 13, 202, 19, 262205, 6, 203, 20, 393296, 13, 204, 203, 203, 203, 524300, 13, 205, 1, 46, 201, 202, 204, 196670, 199, 205, 196670, 206, 89, 131321, 207, 131320, 207, 262390, 209, 210, 0, 131321, 211, 131320, 211, 262205, 86, 212, 206, 327857, 29, 213, 212, 96, 262394, 213, 208, 209, 131320, 208, 262205, 86, 214, 206, 393281, 224, 225, 222, 223, 53, 262205, 86, 226, 225, 327855, 29, 227, 214, 226, 196855, 229, 0, 262394, 227, 228, 229, 131320, 228, 131321, 209, 131320, 229, 196670, 231, 33, 262205, 86, 232, 206, 458817, 233, 234, 222, 89, 232, 25, 262205, 6, 235, 234, 327860, 29, 236, 235, 28, 196855, 238, 0, 262394, 236, 237, 246, 131320, 237, 262205, 86, 240, 206, 393281, 241, 242, 222, 89, 240, 262205, 7, 243, 242, 524367, 13, 244, 243, 243, 0, 1, 2, 393228, 13, 245, 1, 69, 244, 196670, 239, 245, 131321, 238, 131320, 246, 262205, 86, 248, 206, 393281, 241, 249, 222, 89, 248, 262205, 7, 250, 249, 524367, 13, 251, 250, 250, 0, 1, 2, 262205, 13, 252, 18, 327811, 13, 253, 251, 252, 196670, 247, 253, 262205, 13, 255, 247, 393228, 6, 256, 1, 66, 255, 196670, 254, 256, 262205, 6, 257, 254, 327864, 29, 259, 257, 258, 196855, 261, 0, 262394, 259, 260, 263, 131320, 260, 196670, 239, 262, 131321, 261, 131320, 263, 262205, 13, 264, 247, 262205, 6, 265, 254, 393296, 13, 266, 265, 265, 265, 327816, 13, 267, 264, 266, 196670, 239, 267, 262205, 86, 269, 206, 458817, 233, 270, 222, 268, 269, 46, 262205, 6, 271, 270, 262205, 6, 272, 254, 327813, 6, 273, 271, 272, 262205, 6, 274, 254, 327813, 6, 275, 273, 274, 327809, 6, 276, 33, 275, 327816, 6, 277, 33, 276, 196670, 231, 277, 131321, 261, 131320, 261, 131321, 238, 131320, 238, 196670, 278, 33, 262205, 86, 279, 206, 458817, 233, 280, 222, 268, 279, 53, 262205, 6, 281, 280, 327864, 29, 283, 281, 282, 196855, 285, 0, 262394, 283, 284, 285, 131320, 284, 262205, 13, 287, 239, 262271, 13, 288, 287, 262205, 86, 290, 206, 393281, 241, 291, 222, 289, 290, 262205, 7, 292, 291, 524367, 13, 293, 292, 292, 0, 1, 2, 393228, 13, 294, 1, 69, 293, 327828, 6, 295, 288, 294, 196670, 286, 295, 262205, 6, 296, 286, 262205, 86, 297, 206, 458817, 233, 298, 222, 268, 297, 53, 262205, 6, 299, 298, 327864, 29, 300, 296, 299, 196855, 302, 0, 262394, 300, 301, 303, 131320, 301, 196670, 278, 28, 131321, 302, 131320, 303, 262205, 6, 304, 286, 458764, 6, 305, 1, 40, 304, 28, 262205, 86, 306, 206, 458817, 233, 307, 222, 268, 306, 68, 262205, 6, 308, 307, 458764, 6, 309, 1, 26, 305, 308, 196670, 278, 309, 131321, 302, 131320, 302, 131321, 285, 131320, 285, 262205, 13, 311, 17, 262205, 13, 312, 239, 327828, 6, 313, 311, 312, 458764, 6, 314, 1, 40, 313, 28, 196670, 310, 314, 196670, 315, 33, 393281, 224, 316, 222, 223, 46, 262205, 86, 317, 316, 327853, 29, 318, 317, 89, 196855, 320, 0, 262394, 318, 319, 320, 131320, 319, 262205, 86, 321, 206, 393281, 224, 322, 222, 223, 68, 262205, 86, 323, 322, 327850, 29, 324, 321, 323, 131321, 320, 131320, 320, 458997, 29, 325, 318, 285, 324, 319, 196855, 327, 0, 262394, 325, 326, 327, 131320, 326, 262205, 7, 331, 329, 196670, 330, 331, 327737, 6, 332, 11, 330, 196670, 315, 332, 131321, 327, 131320, 327, 262205, 6, 334, 310, 262205, 6, 335, 231, 327813, 6, 336, 334, 335, 262205, 6, 337, 278, 327813, 6, 338, 336, 337, 262205, 6, 339, 315, 327813, 6, 340, 338, 339, 196670, 333, 340, 262205, 86, 341, 206, 393281, 241, 342, 222, 179, 341, 262205, 7, 343, 342, 524367, 13, 344, 343, 343, 0, 1, 2, 262205, 6, 345, 333, 327822, 13, 346, 344, 345, 262205, 13, 347, 186, 327809, 13, 348, 347, 346, 196670, 186, 348, 262205, 13, 350, 239, 262205, 13, 351, 189, 327809, 13, 352, 350, 351, 393228, 13, 353, 1, 69, 352, 196670, 349, 353, 262205, 86, 354, 206, 393281, 241, 355, 222, 179, 354, 262205, 7, 356, 355, 524367, 13, 357, 356, 356, 0, 1, 2, 262205, 13, 358, 17, 262205, 13, 359, 349, 327828, 6, 360, 358, 359, 458764, 6, 361, 1, 40, 360, 28, 262205, 6, 362, 193, 458764, 6, 363, 1, 26, 361, 362, 327822, 13, 364, 357, 363, 262205, 6, 365, 231, 327822, 13, 366, 364, 365, 262205, 6, 367, 278, 327822, 13, 368, 366, 367, 262205, 6, 369, 315, 327822, 13, 370, 368, 369, 262205, 13, 371, 188, 327809, 13, 372, 371, 370, 196670, 188, 372, 131321, 210, 131320, 210, 262205, 86, 373, 206, 327808, 86, 374, 373, 179, 196670, 206, 374, 131321, 207, 131320, 209, 327745, 241, 381, 379, 380, 262205, 7, 382, 381, 524367, 13, 383, 382, 382, 0, 1, 2, 262205, 13, 384, 186, 327809, 13, 385, 383, 384, 196670, 375, 385, 393281, 233, 386, 379, 380, 25, 262205, 6, 387, 386, 327866, 29, 388, 387, 42, 196855, 390, 0, 262394, 388, 389, 390, 131320, 389, 262205, 13, 392, 186, 393296, 13, 393, 28, 28, 28, 393296, 13, 394, 33, 33, 33, 524300, 13, 395, 1, 43, 392, 393, 394, 196670, 391, 395, 262205, 13, 396, 391, 327745, 241, 397, 379, 380, 262205, 7, 398, 397, 524367, 13, 399, 398, 398, 0, 1, 2, 262205, 13, 401, 391, 327811, 13, 402, 400, 401, 327813, 13, 403, 399, 402, 327809, 13, 404, 396, 403, 196670, 375, 404, 131321, 390, 131320, 390, 262205, 13, 406, 19, 262205, 6, 407, 20, 327811, 6, 408, 33, 407, 327822, 13, 409, 406, 408, 196670, 405, 409, 262205, 13, 410, 375, 262205, 13, 411, 405, 327813, 13, 412, 410, 411, 262205, 13, 413, 188, 262205, 13, 414, 199, 327813, 13, 415, 413, 414, 327822, 13, 417, 415, 416, 327809, 13, 418, 412, 417, 393296, 13, 419, 28, 28, 28, 393296, 13, 420, 33, 33, 33, 524300, 13, 421, 1, 43, 418, 419, 420, 131326, 421, 65592 };
static const uint32_t kSpvSkyVs[] = {
    119734787, 65536, 524299, 54, 0, 131089, 1, 393227, 1, 1280527431, 1685353262, 808793134,
    0, 196622, 0, 1, 524303, 0, 4, 1852399981, 0, 21, 34, 52,
    196611, 2, 450, 262149, 4, 1852399981, 0, 262149, 9, 1885957219, 0, 196613,
    11, 17232, 327686, 11, 0, 2003134838, 0, 262150, 11, 1, 6778726, 196613,
    13, 25456, 262149, 21, 1936674913, 0, 393221, 32, 1348430951, 1700164197, 2019914866, 0,
    393222, 32, 0, 1348430951, 1953067887, 7237481, 458758, 32, 1, 1348430951, 1953393007, 1702521171,
    0, 458758, 32, 2, 1130327143, 1148217708, 1635021673, 6644590, 458758, 32, 3, 1130327143,
    1147956341, 1635021673, 6644590, 196613, 34, 0, 262149, 52, 1919501430, 0, 196679, 11,
    2, 262216, 11, 0, 5, 327752, 11, 0, 7, 16, 327752, 11,
    0, 35, 0, 327752, 11, 1, 35, 64, 262215, 21, 30, 0,
    196679, 32, 2, 327752, 32, 0, 11, 0, 327752, 32, 1, 11,
    1, 327752, 32, 2, 11, 3, 327752, 32, 3, 11, 4, 262215,
    52, 30, 0, 131091, 2, 196641, 3, 2, 196630, 6, 32, 262167,
    7, 6, 4, 262176, 8, 7, 7, 262168, 10, 7, 4, 262174,
    11, 10, 7, 262176, 12, 9, 11, 262203, 12, 13, 9, 262165,
    14, 32, 1, 262187, 14, 15, 0, 262176, 16, 9, 10, 262167,
    19, 6, 3, 262176, 20, 1, 19, 262203, 20, 21, 1, 262187,
    6, 23, 1065353216, 262165, 29, 32, 0, 262187, 29, 30, 1, 262172,
    31, 6, 30, 393246, 32, 7, 6, 31, 31, 262176, 33, 3,
    32, 262203, 33, 34, 3, 262176, 37, 3, 7, 262187, 29, 39,
    2, 262176, 40, 3, 6, 262187, 6, 43, 1056964608, 262187, 29, 45,
    3, 262176, 51, 3, 19, 262203, 51, 52, 3, 327734, 2, 4,
    0, 3, 131320, 5, 262203, 8, 9, 7, 327745, 16, 17, 13,
    15, 262205, 10, 18, 17, 262205, 19, 22, 21, 327761, 6, 24,
    22, 0, 327761, 6, 25, 22, 1, 327761, 6, 26, 22, 2,
    458832, 7, 27, 24, 25, 26, 23, 327825, 7, 28, 18, 27,
    196670, 9, 28, 262205, 7, 35, 9, 589903, 7, 36, 35, 35,
    0, 1, 3, 3, 327745, 37, 38, 34, 15, 196670, 38, 36,
    393281, 40, 41, 34, 15, 39, 262205, 6, 42, 41, 327813, 6,
    44, 42, 43, 393281, 40, 46, 34, 15, 45, 262205, 6, 47,
    46, 327813, 6, 48, 47, 43, 327809, 6, 49, 44, 48, 393281,
    40, 50, 34, 15, 39, 196670, 50, 49, 262205, 19, 53, 21,
    196670, 52, 53, 65789, 65592,
};
static const uint32_t kSpvSkyFs[] = {
    119734787, 65536, 524299, 215, 0, 131089, 1, 393227, 1, 1280527431, 1685353262, 808793134,
    0, 196622, 0, 1, 458767, 4, 4, 1852399981, 0, 11, 209, 196624,
    4, 7, 196611, 2, 450, 262149, 4, 1852399981, 0, 196613, 9, 100,
    262149, 11, 1919501430, 0, 196613, 14, 97, 196613, 46, 30325, 196613, 57,
    99, 262149, 61, 1667319413, 12389, 262149, 81, 1667319413, 12645, 262149, 112, 1667319413,
    12901, 262149, 130, 1667319413, 13157, 262149, 153, 1667319413, 13413, 262149, 172, 1667319413,
    13669, 262149, 180, 1769107304, 7237498, 196613, 191, 17232, 327686, 191, 0, 2003134838,
    0, 262150, 191, 1, 6778726, 196613, 193, 25456, 262149, 209, 1734439526, 0,
    262215, 11, 30, 0, 262215, 61, 33, 0, 262215, 61, 34, 0,
    262215, 81, 33, 1, 262215, 81, 34, 0, 262215, 112, 33, 2,
    262215, 112, 34, 0, 262215, 130, 33, 3, 262215, 130, 34, 0,
    262215, 153, 33, 4, 262215, 153, 34, 0, 262215, 172, 33, 5,
    262215, 172, 34, 0, 196679, 191, 2, 262216, 191, 0, 5, 327752,
    191, 0, 7, 16, 327752, 191, 0, 35, 0, 327752, 191, 1,
    35, 64, 262215, 209, 30, 0, 131091, 2, 196641, 3, 2, 196630,
    6, 32, 262167, 7, 6, 3, 262176, 8, 7, 7, 262176, 10,
    1, 7, 262203, 10, 11, 1, 131092, 17, 262165, 18, 32, 0,
    262187, 18, 19, 0, 262176, 20, 7, 6, 262187, 18, 23, 1,
    262187, 18, 31, 2, 262187, 6, 40, 0, 262167, 44, 6, 2,
    262176, 45, 7, 44, 589849, 58, 6, 1, 0, 0, 0, 1,
    0, 196635, 59, 58, 262176, 60, 0, 59, 262203, 60, 61, 0,
    262187, 6, 64, 1056964608, 262167, 68, 6, 4, 262203, 60, 81, 0,
    262203, 60, 112, 0, 262203, 60, 130, 0, 262203, 60, 153, 0,
    262203, 60, 172, 0, 262187, 6, 181, 1065353216, 262187, 6, 187, 1068708659,
    262168, 190, 68, 4, 262174, 191, 190, 68, 262176, 192, 9, 191,
    262203, 192, 193, 9, 262165, 194, 32, 1, 262187, 194, 195, 1,
    262176, 196, 9, 68, 262187, 18, 201, 3, 262176, 202, 9, 6,
    262176, 208, 3, 68, 262203, 208, 209, 3, 327734, 2, 4, 0,
    3, 131320, 5, 262203, 8, 9, 7, 262203, 8, 14, 7, 262203,
    45, 46, 7, 262203, 8, 57, 7, 262203, 20, 180, 7, 262205,
    7, 12, 11, 393228, 7, 13, 1, 69, 12, 196670, 9, 13,
    262205, 7, 15, 9, 393228, 7, 16, 1, 4, 15, 196670, 14,
    16, 327745, 20, 21, 14, 19, 262205, 6, 22, 21, 327745, 20,
    24, 14, 23, 262205, 6, 25, 24, 327870, 17, 26, 22, 25,
    196855, 28, 0, 262394, 26, 27, 28, 131320, 27, 327745, 20, 29,
    14, 19, 262205, 6, 30, 29, 327745, 20, 32, 14, 31, 262205,
    6, 33, 32, 327870, 17, 34, 30, 33, 131321, 28, 131320, 28,
    458997, 17, 35, 26, 5, 34, 27, 196855, 37, 0, 262394, 35,
    36, 89, 131320, 36, 327745, 20, 38, 9, 19, 262205, 6, 39,
    38, 327866, 17, 41, 39, 40, 196855, 43, 0, 262394, 41, 42,
    71, 131320, 42, 327745, 20, 47, 9, 31, 262205, 6, 48, 47,
    262271, 6, 49, 48, 327745, 20, 50, 9, 23, 262205, 6, 51,
    50, 327760, 44, 52, 49, 51, 327745, 20, 53, 14, 19, 262205,
    6, 54, 53, 327760, 44, 55, 54, 54, 327816, 44, 56, 52,
    55, 196670, 46, 56, 262205, 59, 62, 61, 262205, 44, 63, 46,
    327822, 44, 65, 63, 64, 327760, 44, 66, 64, 64, 327809, 44,
    67, 65, 66, 327767, 68, 69, 62, 67, 524367, 7, 70, 69,
    69, 0, 1, 2, 196670, 57, 70, 131321, 43, 131320, 71, 327745,
    20, 72, 9, 31, 262205, 6, 73, 72, 327745, 20, 74, 9,
    23, 262205, 6, 75, 74, 327760, 44, 76, 73, 75, 327745, 20,
    77, 14, 19, 262205, 6, 78, 77, 327760, 44, 79, 78, 78,
    327816, 44, 80, 76, 79, 196670, 46, 80, 262205, 59, 82, 81,
    262205, 44, 83, 46, 327822, 44, 84, 83, 64, 327760, 44, 85,
    64, 64, 327809, 44, 86, 84, 85, 327767, 68, 87, 82, 86,
    524367, 7, 88, 87, 87, 0, 1, 2, 196670, 57, 88, 131321,
    43, 131320, 43, 131321, 37, 131320, 89, 327745, 20, 90, 14, 23,
    262205, 6, 91, 90, 327745, 20, 92, 14, 31, 262205, 6, 93,
    92, 327870, 17, 94, 91, 93, 196855, 96, 0, 262394, 94, 95,
    138, 131320, 95, 327745, 20, 97, 9, 23, 262205, 6, 98, 97,
    327866, 17, 99, 98, 40, 196855, 101, 0, 262394, 99, 100, 120,
    131320, 100, 327745, 20, 102, 9, 19, 262205, 6, 103, 102, 327745,
    20, 104, 9, 31, 262205, 6, 105, 104, 262271, 6, 106, 105,
    327760, 44, 107, 103, 106, 327745, 20, 108, 14, 23, 262205, 6,
    109, 108, 327760, 44, 110, 109, 109, 327816, 44, 111, 107, 110,
    196670, 46, 111, 262205, 59, 113, 112, 262205, 44, 114, 46, 327822,
    44, 115, 114, 64, 327760, 44, 116, 64, 64, 327809, 44, 117,
    115, 116, 327767, 68, 118, 113, 117, 524367, 7, 119, 118, 118,
    0, 1, 2, 196670, 57, 119, 131321, 101, 131320, 120, 327745, 20,
    121, 9, 19, 262205, 6, 122, 121, 327745, 20, 123, 9, 31,
    262205, 6, 124, 123, 327760, 44, 125, 122, 124, 327745, 20, 126,
    14, 23, 262205, 6, 127, 126, 327760, 44, 128, 127, 127, 327816,
    44, 129, 125, 128, 196670, 46, 129, 262205, 59, 131, 130, 262205,
    44, 132, 46, 327822, 44, 133, 132, 64, 327760, 44, 134, 64,
    64, 327809, 44, 135, 133, 134, 327767, 68, 136, 131, 135, 524367,
    7, 137, 136, 136, 0, 1, 2, 196670, 57, 137, 131321, 101,
    131320, 101, 131321, 96, 131320, 138, 327745, 20, 139, 9, 31, 262205,
    6, 140, 139, 327866, 17, 141, 140, 40, 196855, 143, 0, 262394,
    141, 142, 161, 131320, 142, 327745, 20, 144, 9, 19, 262205, 6,
    145, 144, 327745, 20, 146, 9, 23, 262205, 6, 147, 146, 327760,
    44, 148, 145, 147, 327745, 20, 149, 14, 31, 262205, 6, 150,
    149, 327760, 44, 151, 150, 150, 327816, 44, 152, 148, 151, 196670,
    46, 152, 262205, 59, 154, 153, 262205, 44, 155, 46, 327822, 44,
    156, 155, 64, 327760, 44, 157, 64, 64, 327809, 44, 158, 156,
    157, 327767, 68, 159, 154, 158, 524367, 7, 160, 159, 159, 0,
    1, 2, 196670, 57, 160, 131321, 143, 131320, 161, 327745, 20, 162,
    9, 19, 262205, 6, 163, 162, 262271, 6, 164, 163, 327745, 20,
    165, 9, 23, 262205, 6, 166, 165, 327760, 44, 167, 164, 166,
    327745, 20, 168, 14, 31, 262205, 6, 169, 168, 327760, 44, 170,
    169, 169, 327816, 44, 171, 167, 170, 196670, 46, 171, 262205, 59,
    173, 172, 262205, 44, 174, 46, 327822, 44, 175, 174, 64, 327760,
    44, 176, 64, 64, 327809, 44, 177, 175, 176, 327767, 68, 178,
    173, 177, 524367, 7, 179, 178, 178, 0, 1, 2, 196670, 57,
    179, 131321, 143, 131320, 143, 131321, 96, 131320, 96, 131321, 37, 131320,
    37, 327745, 20, 182, 9, 23, 262205, 6, 183, 182, 393228, 6,
    184, 1, 4, 183, 327811, 6, 185, 181, 184, 524300, 6, 186,
    1, 43, 185, 40, 181, 458764, 6, 188, 1, 26, 186, 187,
    196670, 180, 188, 262205, 7, 189, 57, 327745, 196, 197, 193, 195,
    262205, 68, 198, 197, 524367, 7, 199, 198, 198, 0, 1, 2,
    262205, 6, 200, 180, 393281, 202, 203, 193, 195, 201, 262205, 6,
    204, 203, 327813, 6, 205, 200, 204, 393296, 7, 206, 205, 205,
    205, 524300, 7, 207, 1, 46, 189, 199, 206, 196670, 57, 207,
    262205, 7, 210, 57, 327761, 6, 211, 210, 0, 327761, 6, 212,
    210, 1, 327761, 6, 213, 210, 2, 458832, 68, 214, 211, 212,
    213, 181, 196670, 209, 214, 65789, 65592,
};
static const uint32_t kSpvPartVs[] = {
    119734787, 65536, 524299, 95, 0, 131089, 1, 393227, 1, 1280527431, 1685353262, 808793134,
    0, 196622, 0, 1, 786447, 0, 4, 1852399981, 0, 21, 34, 56,
    58, 60, 62, 64, 196611, 2, 450, 262149, 4, 1852399981, 0, 196613,
    9, 6650213, 196613, 11, 7168323, 327686, 11, 0, 2003134838, 0, 327686, 11,
    1, 1785688688, 0, 262150, 11, 2, 6778726, 196613, 13, 117, 262149, 21,
    1936674913, 0, 393221, 32, 1348430951, 1700164197, 2019914866, 0, 393222, 32, 0, 1348430951,
    1953067887, 7237481, 458758, 32, 1, 1348430951, 1953393007, 1702521171, 0, 458758, 32, 2,
    1130327143, 1148217708, 1635021673, 6644590, 458758, 32, 3, 1130327143, 1147956341, 1635021673, 6644590, 196613,
    34, 0, 196613, 56, 7755126, 196613, 58, 7755105, 262149, 60, 1819231094, 29295,
    262149, 62, 1819231073, 29295, 262149, 64, 1735345782, 0, 262149, 76, 1953720676, 0,
    262149, 80, 1918989427, 116, 196679, 11, 2, 262216, 11, 0, 5, 327752,
    11, 0, 7, 16, 327752, 11, 0, 35, 0, 262216, 11, 1,
    5, 327752, 11, 1, 7, 16, 327752, 11, 1, 35, 64, 327752,
    11, 2, 35, 128, 262215, 13, 33, 0, 262215, 13, 34, 0,
    262215, 21, 30, 0, 196679, 32, 2, 327752, 32, 0, 11, 0,
    327752, 32, 1, 11, 1, 327752, 32, 2, 11, 3, 327752, 32,
    3, 11, 4, 262215, 56, 30, 0, 262215, 58, 30, 1, 262215,
    60, 30, 1, 262215, 62, 30, 2, 262215, 64, 30, 2, 131091,
    2, 196641, 3, 2, 196630, 6, 32, 262167, 7, 6, 4, 262176,
    8, 7, 7, 262168, 10, 7, 4, 327710, 11, 10, 10, 7,
    262176, 12, 2, 11, 262203, 12, 13, 2, 262165, 14, 32, 1,
    262187, 14, 15, 0, 262176, 16, 2, 10, 262167, 19, 6, 3,
    262176, 20, 1, 19, 262203, 20, 21, 1, 262187, 6, 23, 1065353216,
    262165, 29, 32, 0, 262187, 29, 30, 1, 262172, 31, 6, 30,
    393246, 32, 7, 6, 31, 31, 262176, 33, 3, 32, 262203, 33,
    34, 3, 262187, 14, 35, 1, 262176, 40, 3, 7, 262187, 29,
    42, 2, 262176, 43, 3, 6, 262187, 6, 46, 1056964608, 262187, 29,
    48, 3, 262167, 54, 6, 2, 262176, 55, 3, 54, 262203, 55,
    56, 3, 262176, 57, 1, 54, 262203, 57, 58, 1, 262203, 40,
    60, 3, 262176, 61, 1, 7, 262203, 61, 62, 1, 262203, 43,
    64, 3, 262187, 6, 65, 0, 262187, 14, 66, 2, 262176, 67,
    2, 6, 262187, 6, 70, 981668463, 131092, 71, 262176, 75, 7, 6,
    262187, 6, 83, 1048576000, 327734, 2, 4, 0, 3, 131320, 5, 262203,
    8, 9, 7, 262203, 75, 76, 7, 262203, 75, 80, 7, 327745,
    16, 17, 13, 15, 262205, 10, 18, 17, 262205, 19, 22, 21,
    327761, 6, 24, 22, 0, 327761, 6, 25, 22, 1, 327761, 6,
    26, 22, 2, 458832, 7, 27, 24, 25, 26, 23, 327825, 7,
    28, 18, 27, 196670, 9, 28, 327745, 16, 36, 13, 35, 262205,
    10, 37, 36, 262205, 7, 38, 9, 327825, 7, 39, 37, 38,
    327745, 40, 41, 34, 15, 196670, 41, 39, 393281, 43, 44, 34,
    15, 42, 262205, 6, 45, 44, 327813, 6, 47, 45, 46, 393281,
    43, 49, 34, 15, 48, 262205, 6, 50, 49, 327813, 6, 51,
    50, 46, 327809, 6, 52, 47, 51, 393281, 43, 53, 34, 15,
    42, 196670, 53, 52, 262205, 54, 59, 58, 196670, 56, 59, 262205,
    7, 63, 62, 196670, 60, 63, 196670, 64, 65, 393281, 67, 68,
    13, 66, 48, 262205, 6, 69, 68, 327866, 71, 72, 69, 70,
    196855, 74, 0, 262394, 72, 73, 74, 131320, 73, 262205, 7, 77,
    9, 524367, 19, 78, 77, 77, 0, 1, 2, 393228, 6, 79,
    1, 66, 78, 196670, 76, 79, 393281, 67, 81, 13, 66, 48,
    262205, 6, 82, 81, 327813, 6, 84, 82, 83, 196670, 80, 84,
    262205, 6, 85, 76, 262205, 6, 86, 80, 327811, 6, 87, 85,
    86, 393281, 67, 88, 13, 66, 48, 262205, 6, 89, 88, 262205,
    6, 90, 80, 327811, 6, 91, 89, 90, 458764, 6, 92, 1,
    40, 91, 70, 327816, 6, 93, 87, 92, 524300, 6, 94, 1,
    43, 93, 65, 23, 196670, 64, 94, 131321, 74, 131320, 74, 65789,
    65592,
};
static const uint32_t kSpvPartFs[] = {
    119734787, 65536, 524299, 60, 0, 131089, 1, 393227, 1, 1280527431, 1685353262, 808793134,
    0, 196622, 0, 1, 589839, 4, 4, 1852399981, 0, 17, 26, 41,
    46, 196624, 4, 7, 196611, 2, 450, 262149, 4, 1852399981, 0, 196613,
    9, 116, 262149, 13, 2019906677, 0, 196613, 17, 7755126, 196613, 22, 7106403,
    262149, 26, 1819231094, 29295, 196613, 31, 7168323, 327686, 31, 0, 2003134838, 0,
    327686, 31, 1, 1785688688, 0, 262150, 31, 2, 6778726, 196613, 33, 117,
    262149, 41, 1735345782, 0, 262149, 46, 1734439526, 0, 262215, 13, 33, 1,
    262215, 13, 34, 0, 262215, 17, 30, 0, 262215, 26, 30, 1,
    196679, 31, 2, 262216, 31, 0, 5, 327752, 31, 0, 7, 16,
    327752, 31, 0, 35, 0, 262216, 31, 1, 5, 327752, 31, 1,
    7, 16, 327752, 31, 1, 35, 64, 327752, 31, 2, 35, 128,
    262215, 33, 33, 0, 262215, 33, 34, 0, 262215, 41, 30, 2,
    262215, 46, 30, 0, 131091, 2, 196641, 3, 2, 196630, 6, 32,
    262167, 7, 6, 4, 262176, 8, 7, 7, 589849, 10, 6, 1,
    0, 0, 0, 1, 0, 196635, 11, 10, 262176, 12, 0, 11,
    262203, 12, 13, 0, 262167, 15, 6, 2, 262176, 16, 1, 15,
    262203, 16, 17, 1, 262167, 20, 6, 3, 262176, 21, 7, 20,
    262176, 25, 1, 7, 262203, 25, 26, 1, 262168, 30, 7, 4,
    327710, 31, 30, 30, 7, 262176, 32, 2, 31, 262203, 32, 33,
    2, 262165, 34, 32, 1, 262187, 34, 35, 2, 262176, 36, 2,
    7, 262176, 40, 1, 6, 262203, 40, 41, 1, 262176, 45, 3,
    7, 262203, 45, 46, 3, 262165, 48, 32, 0, 262187, 48, 49,
    3, 262176, 50, 7, 6, 327734, 2, 4, 0, 3, 131320, 5,
    262203, 8, 9, 7, 262203, 21, 22, 7, 262205, 11, 14, 13,
    262205, 15, 18, 17, 327767, 7, 19, 14, 18, 196670, 9, 19,
    262205, 7, 23, 9, 524367, 20, 24, 23, 23, 0, 1, 2,
    262205, 7, 27, 26, 524367, 20, 28, 27, 27, 0, 1, 2,
    327813, 20, 29, 24, 28, 327745, 36, 37, 33, 35, 262205, 7,
    38, 37, 524367, 20, 39, 38, 38, 0, 1, 2, 262205, 6,
    42, 41, 393296, 20, 43, 42, 42, 42, 524300, 20, 44, 1,
    46, 29, 39, 43, 196670, 22, 44, 262205, 20, 47, 22, 327745,
    50, 51, 9, 49, 262205, 6, 52, 51, 327745, 40, 53, 26,
    49, 262205, 6, 54, 53, 327813, 6, 55, 52, 54, 327761, 6,
    56, 47, 0, 327761, 6, 57, 47, 1, 327761, 6, 58, 47,
    2, 458832, 7, 59, 56, 57, 58, 55, 196670, 46, 59, 65789,
    65592,
};
static const uint32_t kSpvBlitVs[] = {
    119734787, 65536, 524299, 31, 0, 131089, 1, 393227, 1, 1280527431, 1685353262, 808793134,
    0, 196622, 0, 1, 589839, 0, 4, 1852399981, 0, 13, 18, 28,
    29, 196611, 2, 450, 262149, 4, 1852399981, 0, 393221, 11, 1348430951, 1700164197,
    2019914866, 0, 393222, 11, 0, 1348430951, 1953067887, 7237481, 458758, 11, 1, 1348430951,
    1953393007, 1702521171, 0, 458758, 11, 2, 1130327143, 1148217708, 1635021673, 6644590, 458758, 11,
    3, 1130327143, 1147956341, 1635021673, 6644590, 196613, 13, 0, 262149, 18, 1936674913, 0,
    196613, 28, 7755126, 196613, 29, 7755105, 196679, 11, 2, 327752, 11, 0,
    11, 0, 327752, 11, 1, 11, 1, 327752, 11, 2, 11, 3,
    327752, 11, 3, 11, 4, 262215, 18, 30, 0, 262215, 28, 30,
    0, 262215, 29, 30, 1, 131091, 2, 196641, 3, 2, 196630, 6,
    32, 262167, 7, 6, 4, 262165, 8, 32, 0, 262187, 8, 9,
    1, 262172, 10, 6, 9, 393246, 11, 7, 6, 10, 10, 262176,
    12, 3, 11, 262203, 12, 13, 3, 262165, 14, 32, 1, 262187,
    14, 15, 0, 262167, 16, 6, 2, 262176, 17, 1, 16, 262203,
    17, 18, 1, 262187, 6, 20, 0, 262187, 6, 21, 1065353216, 262176,
    25, 3, 7, 262176, 27, 3, 16, 262203, 27, 28, 3, 262203,
    17, 29, 1, 327734, 2, 4, 0, 3, 131320, 5, 262205, 16,
    19, 18, 327761, 6, 22, 19, 0, 327761, 6, 23, 19, 1,
    458832, 7, 24, 22, 23, 20, 21, 327745, 25, 26, 13, 15,
    196670, 26, 24, 262205, 16, 30, 29, 196670, 28, 30, 65789, 65592,
};
static const uint32_t kSpvBlitFs[] = {
    119734787, 65536, 524299, 20, 0, 131089, 1, 393227, 1, 1280527431, 1685353262, 808793134,
    0, 196622, 0, 1, 458767, 4, 4, 1852399981, 0, 9, 17, 196624,
    4, 7, 196611, 2, 450, 262149, 4, 1852399981, 0, 262149, 9, 1734439526,
    0, 262149, 13, 2019906677, 0, 196613, 17, 7755126, 262215, 9, 30, 0,
    262215, 13, 33, 0, 262215, 13, 34, 0, 262215, 17, 30, 0,
    131091, 2, 196641, 3, 2, 196630, 6, 32, 262167, 7, 6, 4,
    262176, 8, 3, 7, 262203, 8, 9, 3, 589849, 10, 6, 1,
    0, 0, 0, 1, 0, 196635, 11, 10, 262176, 12, 0, 11,
    262203, 12, 13, 0, 262167, 15, 6, 2, 262176, 16, 1, 15,
    262203, 16, 17, 1, 327734, 2, 4, 0, 3, 131320, 5, 262205,
    11, 14, 13, 262205, 15, 18, 17, 327767, 7, 19, 14, 18,
    196670, 9, 19, 65789, 65592,
};
static const uint32_t kSpvTexVs[] = {
    119734787, 65536, 524299, 60, 0, 131089, 1, 393227, 1, 1280527431, 1685353262, 808793134,
    0, 196622, 0, 1, 720911, 0, 4, 1852399981, 0, 21, 46, 51,
    52, 56, 58, 196611, 2, 450, 262149, 4, 1852399981, 0, 196613, 9,
    99, 196613, 11, 17232, 262150, 11, 0, 7370349, 196613, 13, 25456, 262149,
    21, 1936674913, 0, 393221, 44, 1348430951, 1700164197, 2019914866, 0, 393222, 44, 0,
    1348430951, 1953067887, 7237481, 458758, 44, 1, 1348430951, 1953393007, 1702521171, 0, 458758, 44,
    2, 1130327143, 1148217708, 1635021673, 6644590, 458758, 44, 3, 1130327143, 1147956341, 1635021673, 6644590,
    196613, 46, 0, 262149, 51, 1819231094, 0, 262149, 52, 1819231073, 0, 196613,
    56, 7755126, 196613, 58, 7755105, 196679, 11, 2, 262216, 11, 0, 5,
    327752, 11, 0, 7, 16, 327752, 11, 0, 35, 0, 262215, 21,
    30, 0, 196679, 44, 2, 327752, 44, 0, 11, 0, 327752, 44,
    1, 11, 1, 327752, 44, 2, 11, 3, 327752, 44, 3, 11,
    4, 262215, 51, 30, 0, 262215, 52, 30, 1, 262215, 56, 30,
    1, 262215, 58, 30, 2, 131091, 2, 196641, 3, 2, 196630, 6,
    32, 262167, 7, 6, 4, 262176, 8, 7, 7, 262168, 10, 7,
    4, 196638, 11, 10, 262176, 12, 9, 11, 262203, 12, 13, 9,
    262165, 14, 32, 1, 262187, 14, 15, 0, 262176, 16, 9, 10,
    262167, 19, 6, 3, 262176, 20, 1, 19, 262203, 20, 21, 1,
    262187, 6, 23, 1065353216, 262165, 29, 32, 0, 262187, 29, 30, 2,
    262176, 31, 7, 6, 262187, 6, 34, 1056964608, 262187, 29, 36, 3,
    262187, 29, 42, 1, 262172, 43, 6, 42, 393246, 44, 7, 6,
    43, 43, 262176, 45, 3, 44, 262203, 45, 46, 3, 262176, 48,
    3, 7, 262176, 50, 3, 19, 262203, 50, 51, 3, 262203, 20,
    52, 1, 262167, 54, 6, 2, 262176, 55, 3, 54, 262203, 55,
    56, 3, 262176, 57, 1, 54, 262203, 57, 58, 1, 327734, 2,
    4, 0, 3, 131320, 5, 262203, 8, 9, 7, 327745, 16, 17,
    13, 15, 262205, 10, 18, 17, 262205, 19, 22, 21, 327761, 6,
    24, 22, 0, 327761, 6, 25, 22, 1, 327761, 6, 26, 22,
    2, 458832, 7, 27, 24, 25, 26, 23, 327825, 7, 28, 18,
    27, 196670, 9, 28, 327745, 31, 32, 9, 30, 262205, 6, 33,
    32, 327813, 6, 35, 33, 34, 327745, 31, 37, 9, 36, 262205,
    6, 38, 37, 327813, 6, 39, 38, 34, 327809, 6, 40, 35,
    39, 327745, 31, 41, 9, 30, 196670, 41, 40, 262205, 7, 47,
    9, 327745, 48, 49, 46, 15, 196670, 49, 47, 262205, 19, 53,
    52, 196670, 51, 53, 262205, 54, 59, 58, 196670, 56, 59, 65789,
    65592,
};
static const uint32_t kSpvTexFs[] = {
    119734787, 65536, 524299, 57, 0, 131089, 1, 393227, 1, 1280527431, 1685353262, 808793134,
    0, 196622, 0, 1, 524303, 4, 4, 1852399981, 0, 17, 44, 49,
    196624, 4, 7, 196611, 2, 450, 262149, 4, 1852399981, 0, 196613, 9,
    116, 262149, 13, 2019906677, 0, 196613, 17, 7755126, 196613, 21, 17232, 262150,
    21, 0, 7632227, 196613, 23, 25456, 262149, 44, 1734439526, 0, 262149, 49,
    1819231094, 0, 262215, 13, 33, 0, 262215, 13, 34, 0, 262215, 17,
    30, 1, 196679, 21, 2, 327752, 21, 0, 35, 64, 262215, 44,
    30, 0, 262215, 49, 30, 0, 131091, 2, 196641, 3, 2, 196630,
    6, 32, 262167, 7, 6, 4, 262176, 8, 7, 7, 589849, 10,
    6, 1, 0, 0, 0, 1, 0, 196635, 11, 10, 262176, 12,
    0, 11, 262203, 12, 13, 0, 262167, 15, 6, 2, 262176, 16,
    1, 15, 262203, 16, 17, 1, 131092, 20, 196638, 21, 6, 262176,
    22, 9, 21, 262203, 22, 23, 9, 262165, 24, 32, 1, 262187,
    24, 25, 0, 262176, 26, 9, 6, 262187, 6, 29, 1056964608, 262165,
    33, 32, 0, 262187, 33, 34, 3, 262176, 35, 7, 6, 262176,
    43, 3, 7, 262203, 43, 44, 3, 262167, 45, 6, 3, 262176,
    48, 1, 45, 262203, 48, 49, 1, 262187, 6, 52, 1065353216, 327734,
    2, 4, 0, 3, 131320, 5, 262203, 8, 9, 7, 262205, 11,
    14, 13, 262205, 15, 18, 17, 327767, 7, 19, 14, 18, 196670,
    9, 19, 327745, 26, 27, 23, 25, 262205, 6, 28, 27, 327866,
    20, 30, 28, 29, 196855, 32, 0, 262394, 30, 31, 32, 131320,
    31, 327745, 35, 36, 9, 34, 262205, 6, 37, 36, 327864, 20,
    38, 37, 29, 131321, 32, 131320, 32, 458997, 20, 39, 30, 5,
    38, 31, 196855, 41, 0, 262394, 39, 40, 41, 131320, 40, 65788,
    131320, 41, 262205, 7, 46, 9, 524367, 45, 47, 46, 46, 0,
    1, 2, 262205, 45, 50, 49, 327813, 45, 51, 47, 50, 327761,
    6, 53, 51, 0, 327761, 6, 54, 51, 1, 327761, 6, 55,
    51, 2, 458832, 7, 56, 53, 54, 55, 52, 196670, 44, 56,
    65789, 65592,
};


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


inline int& keep_epoch() {
    static int n = 1;
    return n;
}

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
