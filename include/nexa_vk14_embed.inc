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
static const uint32_t kSpvMeshFs[] = { 119734787, 65536, 524299, 178, 0, 131089, 1, 393227, 1, 1280527431, 1685353262, 808793134, 0, 196622, 0, 1, 917519, 4, 4, 1852399981, 0, 110, 113, 115, 122, 124, 147, 165, 170, 172, 196624, 4, 7, 196611, 2, 450, 262149, 4, 1852399981, 0, 393221, 11, 1684105331, 1885304687, 1982359139, 3880038, 262149, 10, 1885957219, 0, 196613, 27, 112, 196613, 45, 115, 196613, 48, 111, 196613, 53, 121, 196613, 63, 120, 196613, 71, 100, 262149, 75, 1634227061, 7827300, 196613, 108, 7106403, 393221, 110, 1180658791, 1953394546, 1768120646, 26478, 262149, 113, 1869760118, 29806, 262149, 115, 1667318390, 107, 196613, 120, 7239027, 262149, 122, 1853182838, 70, 262149, 124, 1853182838, 66, 262149, 134, 1852138323, 101, 327686, 134, 0, 1936683116, 0, 327686, 134, 1, 1718183020, 0, 327686, 134, 2, 1869640556, 116, 262150, 134, 3, 7369580, 327686, 134, 4, 1180855649, 7105641, 262150, 134, 5, 6778726, 327686, 134, 6, 1868983913, 0, 196613, 136, 117, 262149, 147, 1734954102, 29800, 262149, 148, 1634886000, 109, 262149, 165, 1735345782, 0, 262149, 170, 1734439526, 0, 262149, 172, 1886142838, 24936, 262215, 75, 33, 2, 262215, 75, 34, 0, 262215, 110, 11, 17, 262215, 113, 30, 0, 262215, 115, 30, 1, 262215, 122, 30, 2, 262215, 124, 30, 3, 262215, 129, 6, 16, 262215, 130, 6, 16, 262215, 131, 6, 16, 262215, 132, 6, 16, 196679, 134, 2, 327752, 134, 0, 35, 0, 327752, 134, 1, 35, 256, 327752, 134, 2, 35, 512, 327752, 134, 3, 35, 768, 327752, 134, 4, 35, 1024, 327752, 134, 5, 35, 1040, 327752, 134, 6, 35, 1056, 262215, 136, 33, 0, 262215, 136, 34, 0, 262215, 147, 30, 4, 262215, 165, 30, 5, 262215, 170, 30, 0, 262215, 172, 30, 6, 131091, 2, 196641, 3, 2, 196630, 6, 32, 262167, 7, 6, 4, 262176, 8, 7, 7, 262177, 9, 6, 8, 262165, 13, 32, 0, 262187, 13, 14, 3, 262176, 15, 7, 6, 262187, 6, 18, 0, 131092, 19, 262187, 6, 23, 1065353216, 262167, 25, 6, 3, 262176, 26, 7, 25, 262187, 6, 34, 1056964608, 262187, 13, 38, 2, 262167, 46, 6, 2, 262176, 47, 7, 46, 262187, 6, 49, 977272832, 327724, 46, 50, 49, 49, 262165, 51, 32, 1, 262176, 52, 7, 51, 262187, 51, 54, 4294967295, 262187, 51, 61, 1, 589849, 72, 6, 1, 0, 0, 0, 1, 0, 196635, 73, 72, 262176, 74, 0, 73, 262203, 74, 75, 0, 262187, 13, 88, 0, 262187, 6, 92, 990057071, 262187, 6, 104, 1091567616, 262176, 109, 1, 19, 262203, 109, 110, 1, 262176, 112, 1, 25, 262203, 112, 113, 1, 262203, 112, 115, 1, 262167, 117, 19, 3, 262203, 112, 122, 1, 262203, 112, 124, 1, 262187, 13, 128, 16, 262172, 129, 7, 128, 262172, 130, 7, 128, 262172, 131, 7, 128, 262172, 132, 7, 128, 262167, 133, 51, 4, 589854, 134, 129, 130, 131, 132, 7, 7, 133, 262176, 135, 2, 134, 262203, 135, 136, 2, 262187, 51, 137, 6, 262176, 138, 2, 51, 262187, 51, 141, 0, 262176, 146, 1, 7, 262203, 146, 147, 1, 262187, 51, 159, 5, 262176, 160, 2, 7, 262176, 164, 1, 6, 262203, 164, 165, 1, 262176, 169, 3, 7, 262203, 169, 170, 3, 262203, 164, 172, 1, 327734, 2, 4, 0, 3, 131320, 5, 262203, 26, 108, 7, 262203, 26, 120, 7, 262203, 8, 148, 7, 262205, 19, 111, 110, 262205, 25, 114, 113, 262205, 25, 116, 115, 393296, 117, 118, 111, 111, 111, 393385, 25, 119, 118, 114, 116, 196670, 108, 119, 262205, 19, 121, 110, 262205, 25, 123, 122, 262205, 25, 125, 124, 393296, 117, 126, 121, 121, 121, 393385, 25, 127, 126, 123, 125, 196670, 120, 127, 393281, 138, 139, 136, 137, 38, 262205, 51, 140, 139, 327853, 19, 142, 140, 141, 196855, 144, 0, 262394, 142, 143, 144, 131320, 143, 262205, 25, 145, 120, 262205, 7, 149, 147, 196670, 148, 149, 327737, 6, 150, 11, 148, 327811, 6, 151, 23, 150, 327822, 25, 152, 145, 151, 262205, 25, 153, 108, 327811, 25, 154, 153, 152, 196670, 108, 154, 131321, 144, 131320, 144, 262205, 25, 155, 108, 393296, 25, 156, 18, 18, 18, 393296, 25, 157, 23, 23, 23, 524300, 25, 158, 1, 43, 155, 156, 157, 327745, 160, 161, 136, 159, 262205, 7, 162, 161, 524367, 25, 163, 162, 162, 0, 1, 2, 262205, 6, 166, 165, 393296, 25, 167, 166, 166, 166, 524300, 25, 168, 1, 46, 158, 163, 167, 196670, 108, 168, 262205, 25, 171, 108, 262205, 6, 173, 172, 327761, 6, 174, 171, 0, 327761, 6, 175, 171, 1, 327761, 6, 176, 171, 2, 458832, 7, 177, 174, 175, 176, 173, 196670, 170, 177, 65789, 65592, 327734, 6, 11, 0, 9, 196663, 8, 10, 131320, 12, 262203, 26, 27, 7, 262203, 15, 45, 7, 262203, 47, 48, 7, 262203, 52, 53, 7, 262203, 52, 63, 7, 262203, 15, 71, 7, 327745, 15, 16, 10, 14, 262205, 6, 17, 16, 327868, 19, 20, 17, 18, 196855, 22, 0, 262394, 20, 21, 22, 131320, 21, 131326, 23, 131320, 22, 262205, 7, 28, 10, 524367, 25, 29, 28, 28, 0, 1, 2, 327745, 15, 30, 10, 14, 262205, 6, 31, 30, 393296, 25, 32, 31, 31, 31, 327816, 25, 33, 29, 32, 327822, 25, 35, 33, 34, 393296, 25, 36, 34, 34, 34, 327809, 25, 37, 35, 36, 196670, 27, 37, 327745, 15, 39, 27, 38, 262205, 6, 40, 39, 327866, 19, 41, 40, 23, 196855, 43, 0, 262394, 41, 42, 43, 131320, 42, 131326, 23, 131320, 43, 196670, 45, 18, 196670, 48, 50, 196670, 53, 54, 131321, 55, 131320, 55, 262390, 57, 58, 0, 131321, 59, 131320, 59, 262205, 51, 60, 53, 327859, 19, 62, 60, 61, 262394, 62, 56, 57, 131320, 56, 196670, 63, 54, 131321, 64, 131320, 64, 262390, 66, 67, 0, 131321, 68, 131320, 68, 262205, 51, 69, 63, 327859, 19, 70, 69, 61, 262394, 70, 65, 66, 131320, 65, 262205, 73, 76, 75, 262205, 25, 77, 27, 458831, 46, 78, 77, 77, 0, 1, 262205, 51, 79, 63, 262255, 6, 80, 79, 262205, 51, 81, 53, 262255, 6, 82, 81, 327760, 46, 83, 80, 82, 262205, 46, 84, 48, 327813, 46, 85, 83, 84, 327809, 46, 86, 78, 85, 327767, 7, 87, 76, 86, 327761, 6, 89, 87, 0, 196670, 71, 89, 327745, 15, 90, 27, 38, 262205, 6, 91, 90, 327811, 6, 93, 91, 92, 262205, 6, 94, 71, 327866, 19, 95, 93, 94, 393385, 6, 96, 95, 18, 23, 262205, 6, 97, 45, 327809, 6, 98, 97, 96, 196670, 45, 98, 131321, 67, 131320, 67, 262205, 51, 99, 63, 327808, 51, 100, 99, 61, 196670, 63, 100, 131321, 64, 131320, 66, 131321, 58, 131320, 58, 262205, 51, 101, 53, 327808, 51, 102, 101, 61, 196670, 53, 102, 131321, 55, 131320, 57, 262205, 6, 103, 45, 327816, 6, 105, 103, 104, 131326, 105, 65592 };
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
static const uint32_t kSpvMatFs[] = {
    119734787, 65536, 524299, 572, 0, 131089, 1, 393227, 1, 1280527431, 1685353262, 808793134,
    0, 196622, 0, 1, 983055, 4, 4, 1852399981, 0, 263, 360, 364,
    370, 382, 394, 541, 559, 564, 566, 196624, 4, 7, 196611, 2,
    450, 262149, 4, 1852399981, 0, 393221, 11, 1684105331, 1885304687, 1982359139, 3880038, 262149,
    10, 1885957219, 0, 589829, 22, 1684105331, 1719019621, 1719024435, 1719024435, 828783411, 993093179, 0,
    196613, 17, 110, 196613, 18, 25968, 262149, 19, 1700949089, 28516, 262149, 20,
    1635018093, 108, 262149, 21, 1735749490, 104, 196613, 35, 112, 196613, 53, 115,
    196613, 56, 111, 196613, 61, 121, 196613, 71, 120, 196613, 79, 7365988,
    262149, 83, 1634227061, 7827300, 262149, 116, 1701996900, 29795, 262149, 118, 1667592307, 0,
    196613, 119, 86, 262149, 123, 1852401779, 0, 196613, 129, 12358, 196613, 136,
    105, 262149, 153, 1852138323, 101, 327686, 153, 0, 1936683116, 0, 327686, 153,
    1, 1718183020, 0, 327686, 153, 2, 1869640556, 116, 262150, 153, 3, 7369580,
    327686, 153, 4, 1180855649, 7105641, 262150, 153, 5, 6778726, 327686, 153, 6,
    1868983913, 0, 196613, 155, 117, 196613, 164, 7631969, 196613, 172, 5009268, 196613,
    180, 76, 262149, 187, 1953720676, 0, 262149, 211, 1953460339, 0, 196613, 219,
    115, 196613, 244, 7103598, 196613, 249, 26739, 262149, 263, 1734954102, 29800, 262149,
    264, 1634886000, 109, 262149, 267, 1886216556, 0, 196613, 283, 72, 196613, 309,
    7629164, 262149, 311, 2002874948, 0, 262150, 311, 0, 7370349, 262150, 311, 1,
    30317, 393222, 311, 2, 1751607660, 1886801268, 0, 327686, 311, 3, 1952542062, 0,
    327686, 311, 4, 1869377379, 114, 327686, 311, 5, 1734437990, 115, 327686, 311,
    6, 1920235621, 97, 327686, 311, 7, 1180855649, 7105641, 196613, 313, 100, 196613,
    325, 99, 262149, 339, 1717987684, 0, 196613, 358, 30325, 196613, 360, 7755126,
    196613, 362, 7172718, 393221, 364, 1180658791, 1953394546, 1768120646, 26478, 196613, 370, 20086,
    196613, 377, 7233908, 196613, 382, 21622, 196613, 389, 7629154, 196613, 394, 17014,
    196613, 401, 7364973, 262149, 405, 1700949089, 28516, 262149, 412, 1935753845, 101, 196613,
    428, 28276, 262149, 429, 1836207733, 0, 196613, 458, 104, 262149, 459, 1768245365,
    7628903, 196613, 464, 30820, 196613, 467, 31076, 196613, 470, 30824, 196613, 479,
    31080, 262149, 501, 1635018093, 108, 262149, 508, 1952796021, 27745, 262149, 515, 1735749490,
    104, 262149, 523, 1970229877, 26727, 196613, 531, 7106403, 262149, 541, 1702446454, 0,
    262149, 542, 1634886000, 109, 262149, 544, 1634886000, 109, 262149, 546, 1634886000, 109,
    262149, 548, 1634886000, 109, 262149, 550, 1634886000, 109, 262149, 559, 1735345782, 0,
    262149, 564, 1734439526, 0, 262149, 566, 1886142838, 24936, 262215, 83, 33, 2,
    262215, 83, 34, 0, 262215, 148, 6, 16, 262215, 149, 6, 16,
    262215, 150, 6, 16, 262215, 151, 6, 16, 196679, 153, 2, 327752,
    153, 0, 35, 0, 327752, 153, 1, 35, 256, 327752, 153, 2,
    35, 512, 327752, 153, 3, 35, 768, 327752, 153, 4, 35, 1024,
    327752, 153, 5, 35, 1040, 327752, 153, 6, 35, 1056, 262215, 155,
    33, 0, 262215, 155, 34, 0, 262215, 263, 30, 1, 196679, 311,
    2, 262216, 311, 0, 5, 327752, 311, 0, 7, 16, 327752, 311,
    0, 35, 0, 262216, 311, 1, 5, 327752, 311, 1, 7, 16,
    327752, 311, 1, 35, 64, 262216, 311, 2, 5, 327752, 311, 2,
    7, 16, 327752, 311, 2, 35, 128, 262216, 311, 3, 5, 327752,
    311, 3, 7, 16, 327752, 311, 3, 35, 192, 327752, 311, 4,
    35, 256, 327752, 311, 5, 35, 272, 327752, 311, 6, 35, 288,
    327752, 311, 7, 35, 304, 262215, 313, 33, 1, 262215, 313, 34,
    0, 262215, 360, 30, 6, 262215, 364, 11, 17, 262215, 370, 30,
    3, 262215, 382, 30, 4, 262215, 394, 30, 5, 262215, 412, 33,
    3, 262215, 412, 34, 0, 262215, 429, 33, 4, 262215, 429, 34,
    0, 262215, 459, 33, 7, 262215, 459, 34, 0, 262215, 508, 33,
    5, 262215, 508, 34, 0, 262215, 523, 33, 6, 262215, 523, 34,
    0, 262215, 541, 30, 0, 262215, 559, 30, 2, 262215, 564, 30,
    0, 262215, 566, 30, 7, 131091, 2, 196641, 3, 2, 196630, 6,
    32, 262167, 7, 6, 4, 262176, 8, 7, 7, 262177, 9, 6,
    8, 262167, 13, 6, 3, 262176, 14, 7, 13, 262176, 15, 7,
    6, 524321, 16, 13, 14, 14, 14, 15, 15, 262165, 24, 32,
    0, 262187, 24, 25, 3, 262187, 6, 28, 0, 131092, 29, 262187,
    6, 33, 1065353216, 262187, 6, 42, 1056964608, 262187, 24, 46, 2, 262167,
    54, 6, 2, 262176, 55, 7, 54, 262187, 6, 57, 977272832, 327724,
    54, 58, 57, 57, 262165, 59, 32, 1, 262176, 60, 7, 59,
    262187, 59, 62, 4294967295, 262187, 59, 69, 1, 589849, 80, 6, 1,
    0, 0, 0, 1, 0, 196635, 81, 80, 262176, 82, 0, 81,
    262203, 82, 83, 0, 262187, 24, 96, 0, 262187, 6, 100, 990057071,
    262187, 6, 112, 1091567616, 393260, 13, 117, 28, 28, 28, 262187, 6,
    124, 1119879168, 262187, 6, 125, 1086324736, 262187, 6, 130, 1025758986, 393260, 13,
    131, 130, 130, 130, 262187, 59, 137, 0, 262187, 59, 144, 16,
    262187, 24, 147, 16, 262172, 148, 7, 147, 262172, 149, 7, 147,
    262172, 150, 7, 147, 262172, 151, 7, 147, 262167, 152, 59, 4,
    589854, 153, 148, 149, 150, 151, 7, 7, 152, 262176, 154, 2,
    153, 262203, 154, 155, 2, 262187, 59, 156, 6, 262176, 157, 2,
    59, 262176, 166, 2, 6, 262176, 174, 2, 7, 262187, 6, 191,
    925353388, 393260, 13, 195, 28, 28, 33, 262187, 59, 201, 3, 262187,
    6, 215, 1069547520, 262187, 59, 222, 2, 262187, 24, 240, 1, 262176,
    262, 1, 7, 262203, 262, 263, 1, 262168, 310, 7, 4, 655390,
    311, 310, 310, 310, 310, 7, 7, 7, 7, 262176, 312, 2,
    311, 262203, 312, 313, 2, 262187, 59, 314, 7, 393260, 13, 334,
    33, 33, 33, 262187, 6, 350, 1043878380, 262176, 359, 1, 54, 262203,
    359, 360, 1, 262176, 363, 1, 29, 262203, 363, 364, 1, 262176,
    369, 1, 13, 262203, 369, 370, 1, 262203, 369, 382, 1, 262203,
    369, 394, 1, 262203, 82, 412, 0, 262187, 59, 418, 4, 262203,
    82, 429, 0, 262187, 6, 434, 1073741824, 262203, 82, 459, 0, 262187,
    6, 492, 1082130432, 262203, 82, 508, 0, 262187, 59, 517, 8, 262203,
    82, 523, 0, 262187, 6, 529, 1057803469, 262187, 59, 532, 5, 262203,
    369, 541, 1, 262176, 558, 1, 6, 262203, 558, 559, 1, 262176,
    563, 3, 7, 262203, 563, 564, 3, 262203, 558, 566, 1, 327734,
    2, 4, 0, 3, 131320, 5, 262203, 55, 358, 7, 262203, 14,
    362, 7, 262203, 14, 366, 7, 262203, 14, 377, 7, 262203, 14,
    379, 7, 262203, 14, 389, 7, 262203, 14, 391, 7, 262203, 60,
    401, 7, 262203, 14, 405, 7, 262203, 14, 409, 7, 262203, 14,
    428, 7, 262203, 15, 458, 7, 262203, 55, 464, 7, 262203, 55,
    467, 7, 262203, 15, 470, 7, 262203, 15, 479, 7, 262203, 15,
    501, 7, 262203, 15, 505, 7, 262203, 15, 515, 7, 262203, 15,
    520, 7, 262203, 14, 531, 7, 262203, 14, 536, 7, 262203, 14,
    542, 7, 262203, 14, 544, 7, 262203, 14, 546, 7, 262203, 15,
    548, 7, 262203, 15, 550, 7, 262205, 54, 361, 360, 196670, 358,
    361, 262205, 29, 365, 364, 196855, 368, 0, 262394, 365, 367, 372,
    131320, 367, 262205, 13, 371, 370, 196670, 366, 371, 131321, 368, 131320,
    372, 262205, 13, 373, 370, 262271, 13, 374, 373, 196670, 366, 374,
    131321, 368, 131320, 368, 262205, 13, 375, 366, 393228, 13, 376, 1,
    69, 375, 196670, 362, 376, 262205, 29, 378, 364, 196855, 381, 0,
    262394, 378, 380, 384, 131320, 380, 262205, 13, 383, 382, 196670, 379,
    383, 131321, 381, 131320, 384, 262205, 13, 385, 382, 262271, 13, 386,
    385, 196670, 379, 386, 131321, 381, 131320, 381, 262205, 13, 387, 379,
    393228, 13, 388, 1, 69, 387, 196670, 377, 388, 262205, 29, 390,
    364, 196855, 393, 0, 262394, 390, 392, 396, 131320, 392, 262205, 13,
    395, 394, 196670, 391, 395, 131321, 393, 131320, 396, 262205, 13, 397,
    394, 262271, 13, 398, 397, 196670, 391, 398, 131321, 393, 131320, 393,
    262205, 13, 399, 391, 393228, 13, 400, 1, 69, 399, 196670, 389,
    400, 393281, 166, 402, 313, 156, 46, 262205, 6, 403, 402, 262254,
    59, 404, 403, 196670, 401, 404, 262205, 59, 406, 401, 327879, 59,
    407, 406, 69, 327851, 29, 408, 407, 137, 196855, 411, 0, 262394,
    408, 410, 417, 131320, 410, 262205, 81, 413, 412, 262205, 54, 414,
    358, 327767, 7, 415, 413, 414, 524367, 13, 416, 415, 415, 0,
    1, 2, 196670, 409, 416, 131321, 411, 131320, 417, 327745, 174, 419,
    313, 418, 262205, 7, 420, 419, 524367, 13, 421, 420, 420, 0,
    1, 2, 196670, 409, 421, 131321, 411, 131320, 411, 262205, 13, 422,
    409, 196670, 405, 422, 262205, 59, 423, 401, 327879, 59, 424, 423,
    222, 327851, 29, 425, 424, 137, 196855, 427, 0, 262394, 425, 426,
    427, 131320, 426, 262205, 81, 430, 429, 262205, 54, 431, 358, 327767,
    7, 432, 430, 431, 524367, 13, 433, 432, 432, 0, 1, 2,
    327822, 13, 435, 433, 434, 393296, 13, 436, 33, 33, 33, 327811,
    13, 437, 435, 436, 196670, 428, 437, 262205, 13, 438, 377, 327745,
    15, 439, 428, 96, 262205, 6, 440, 439, 327822, 13, 441, 438,
    440, 262205, 13, 442, 389, 327745, 15, 443, 428, 240, 262205, 6,
    444, 443, 327822, 13, 445, 442, 444, 327809, 13, 446, 441, 445,
    262205, 13, 447, 362, 327745, 15, 448, 428, 46, 262205, 6, 449,
    448, 327822, 13, 450, 447, 449, 327809, 13, 451, 446, 450, 393228,
    13, 452, 1, 69, 451, 196670, 362, 452, 131321, 427, 131320, 427,
    262205, 59, 453, 401, 327879, 59, 454, 453, 144, 327851, 29, 455,
    454, 137, 196855, 457, 0, 262394, 455, 456, 457, 131320, 456, 262205,
    81, 460, 459, 262205, 54, 461, 358, 327767, 7, 462, 460, 461,
    327761, 6, 463, 462, 0, 196670, 458, 463, 262205, 54, 465, 358,
    262351, 54, 466, 465, 196670, 464, 466, 262205, 54, 468, 358, 262352,
    54, 469, 468, 196670, 467, 469, 262205, 81, 471, 459, 262205, 54,
    472, 358, 262205, 54, 473, 464, 327809, 54, 474, 472, 473, 327767,
    7, 475, 471, 474, 327761, 6, 476, 475, 0, 262205, 6, 477,
    458, 327811, 6, 478, 476, 477, 196670, 470, 478, 262205, 81, 480,
    459, 262205, 54, 481, 358, 262205, 54, 482, 467, 327809, 54, 483,
    481, 482, 327767, 7, 484, 480, 483, 327761, 6, 485, 484, 0,
    262205, 6, 486, 458, 327811, 6, 487, 485, 486, 196670, 479, 487,
    262205, 13, 488, 362, 262205, 13, 489, 377, 262205, 6, 490, 470,
    327822, 13, 491, 489, 490, 327822, 13, 493, 491, 492, 327811, 13,
    494, 488, 493, 262205, 13, 495, 389, 262205, 6, 496, 479, 327822,
    13, 497, 495, 496, 327822, 13, 498, 497, 492, 327811, 13, 499,
    494, 498, 393228, 13, 500, 1, 69, 499, 196670, 362, 500, 131321,
    457, 131320, 457, 262205, 59, 502, 401, 327879, 59, 503, 502, 418,
    327851, 29, 504, 503, 137, 196855, 507, 0, 262394, 504, 506, 513,
    131320, 506, 262205, 81, 509, 508, 262205, 54, 510, 358, 327767, 7,
    511, 509, 510, 327761, 6, 512, 511, 0, 196670, 505, 512, 131321,
    507, 131320, 513, 196670, 505, 28, 131321, 507, 131320, 507, 262205, 6,
    514, 505, 196670, 501, 514, 262205, 59, 516, 401, 327879, 59, 518,
    516, 517, 327851, 29, 519, 518, 137, 196855, 522, 0, 262394, 519,
    521, 528, 131320, 521, 262205, 81, 524, 523, 262205, 54, 525, 358,
    327767, 7, 526, 524, 525, 327761, 6, 527, 526, 0, 196670, 520,
    527, 131321, 522, 131320, 528, 196670, 520, 529, 131321, 522, 131320, 522,
    262205, 6, 530, 520, 196670, 515, 530, 393281, 166, 533, 313, 532,
    96, 262205, 6, 534, 533, 327864, 29, 535, 534, 42, 196855, 538,
    0, 262394, 535, 537, 540, 131320, 537, 262205, 13, 539, 405, 196670,
    536, 539, 131321, 538, 131320, 540, 262205, 13, 543, 362, 196670, 542,
    543, 262205, 13, 545, 541, 196670, 544, 545, 262205, 13, 547, 405,
    196670, 546, 547, 262205, 6, 549, 501, 196670, 548, 549, 262205, 6,
    551, 515, 196670, 550, 551, 589881, 13, 552, 22, 542, 544, 546,
    548, 550, 196670, 536, 552, 131321, 538, 131320, 538, 262205, 13, 553,
    536, 196670, 531, 553, 262205, 13, 554, 531, 327745, 174, 555, 155,
    532, 262205, 7, 556, 555, 524367, 13, 557, 556, 556, 0, 1,
    2, 262205, 6, 560, 559, 393296, 13, 561, 560, 560, 560, 524300,
    13, 562, 1, 46, 554, 557, 561, 196670, 531, 562, 262205, 13,
    565, 531, 262205, 6, 567, 566, 327761, 6, 568, 565, 0, 327761,
    6, 569, 565, 1, 327761, 6, 570, 565, 2, 458832, 7, 571,
    568, 569, 570, 567, 196670, 564, 571, 65789, 65592, 327734, 6, 11,
    0, 9, 196663, 8, 10, 131320, 12, 262203, 14, 35, 7, 262203,
    15, 53, 7, 262203, 55, 56, 7, 262203, 60, 61, 7, 262203,
    60, 71, 7, 262203, 15, 79, 7, 327745, 15, 26, 10, 25,
    262205, 6, 27, 26, 327868, 29, 30, 27, 28, 196855, 32, 0,
    262394, 30, 31, 32, 131320, 31, 131326, 33, 131320, 32, 262205, 7,
    36, 10, 524367, 13, 37, 36, 36, 0, 1, 2, 327745, 15,
    38, 10, 25, 262205, 6, 39, 38, 393296, 13, 40, 39, 39,
    39, 327816, 13, 41, 37, 40, 327822, 13, 43, 41, 42, 393296,
    13, 44, 42, 42, 42, 327809, 13, 45, 43, 44, 196670, 35,
    45, 327745, 15, 47, 35, 46, 262205, 6, 48, 47, 327866, 29,
    49, 48, 33, 196855, 51, 0, 262394, 49, 50, 51, 131320, 50,
    131326, 33, 131320, 51, 196670, 53, 28, 196670, 56, 58, 196670, 61,
    62, 131321, 63, 131320, 63, 262390, 65, 66, 0, 131321, 67, 131320,
    67, 262205, 59, 68, 61, 327859, 29, 70, 68, 69, 262394, 70,
    64, 65, 131320, 64, 196670, 71, 62, 131321, 72, 131320, 72, 262390,
    74, 75, 0, 131321, 76, 131320, 76, 262205, 59, 77, 71, 327859,
    29, 78, 77, 69, 262394, 78, 73, 74, 131320, 73, 262205, 81,
    84, 83, 262205, 13, 85, 35, 458831, 54, 86, 85, 85, 0,
    1, 262205, 59, 87, 71, 262255, 6, 88, 87, 262205, 59, 89,
    61, 262255, 6, 90, 89, 327760, 54, 91, 88, 90, 262205, 54,
    92, 56, 327813, 54, 93, 91, 92, 327809, 54, 94, 86, 93,
    327767, 7, 95, 84, 94, 327761, 6, 97, 95, 0, 196670, 79,
    97, 327745, 15, 98, 35, 46, 262205, 6, 99, 98, 327811, 6,
    101, 99, 100, 262205, 6, 102, 79, 327866, 29, 103, 101, 102,
    393385, 6, 104, 103, 28, 33, 262205, 6, 105, 53, 327809, 6,
    106, 105, 104, 196670, 53, 106, 131321, 75, 131320, 75, 262205, 59,
    107, 71, 327808, 59, 108, 107, 69, 196670, 71, 108, 131321, 72,
    131320, 74, 131321, 66, 131320, 66, 262205, 59, 109, 61, 327808, 59,
    110, 109, 69, 196670, 61, 110, 131321, 63, 131320, 65, 262205, 6,
    111, 53, 327816, 6, 113, 111, 112, 131326, 113, 65592, 327734, 13,
    22, 0, 16, 196663, 14, 17, 196663, 14, 18, 196663, 14, 19,
    196663, 15, 20, 196663, 15, 21, 131320, 23, 262203, 14, 116, 7,
    262203, 14, 118, 7, 262203, 14, 119, 7, 262203, 15, 123, 7,
    262203, 14, 129, 7, 262203, 60, 136, 7, 262203, 15, 164, 7,
    262203, 14, 172, 7, 262203, 14, 180, 7, 262203, 15, 187, 7,
    262203, 15, 211, 7, 262203, 15, 219, 7, 262203, 15, 244, 7,
    262203, 15, 249, 7, 262203, 8, 264, 7, 262203, 15, 267, 7,
    262203, 14, 283, 7, 262203, 14, 309, 7, 262203, 14, 325, 7,
    262203, 14, 339, 7, 196670, 116, 117, 196670, 118, 117, 262205, 13,
    120, 18, 262271, 13, 121, 120, 393228, 13, 122, 1, 69, 121,
    196670, 119, 122, 262205, 6, 126, 21, 524300, 6, 127, 1, 43,
    126, 28, 33, 524300, 6, 128, 1, 46, 124, 125, 127, 196670,
    123, 128, 262205, 13, 132, 19, 262205, 6, 133, 20, 393296, 13,
    134, 133, 133, 133, 524300, 13, 135, 1, 46, 131, 132, 134,
    196670, 129, 135, 196670, 136, 137, 131321, 138, 131320, 138, 262390, 140,
    141, 0, 131321, 142, 131320, 142, 262205, 59, 143, 136, 327857, 29,
    145, 143, 144, 262394, 145, 139, 140, 131320, 139, 262205, 59, 146,
    136, 393281, 157, 158, 155, 156, 96, 262205, 59, 159, 158, 327855,
    29, 160, 146, 159, 196855, 162, 0, 262394, 160, 161, 162, 131320,
    161, 131321, 140, 131320, 162, 196670, 164, 33, 262205, 59, 165, 136,
    458817, 166, 167, 155, 137, 165, 25, 262205, 6, 168, 167, 327860,
    29, 169, 168, 28, 196855, 171, 0, 262394, 169, 170, 179, 131320,
    170, 262205, 59, 173, 136, 393281, 174, 175, 155, 137, 173, 262205,
    7, 176, 175, 524367, 13, 177, 176, 176, 0, 1, 2, 393228,
    13, 178, 1, 69, 177, 196670, 172, 178, 131321, 171, 131320, 179,
    262205, 59, 181, 136, 393281, 174, 182, 155, 137, 181, 262205, 7,
    183, 182, 524367, 13, 184, 183, 183, 0, 1, 2, 262205, 13,
    185, 18, 327811, 13, 186, 184, 185, 196670, 180, 186, 262205, 13,
    188, 180, 393228, 6, 189, 1, 66, 188, 196670, 187, 189, 262205,
    6, 190, 187, 327864, 29, 192, 190, 191, 196855, 194, 0, 262394,
    192, 193, 196, 131320, 193, 196670, 172, 195, 131321, 194, 131320, 196,
    262205, 13, 197, 180, 262205, 6, 198, 187, 393296, 13, 199, 198,
    198, 198, 327816, 13, 200, 197, 199, 196670, 172, 200, 262205, 59,
    202, 136, 458817, 166, 203, 155, 201, 202, 46, 262205, 6, 204,
    203, 262205, 6, 205, 187, 327813, 6, 206, 204, 205, 262205, 6,
    207, 187, 327813, 6, 208, 206, 207, 327809, 6, 209, 33, 208,
    327816, 6, 210, 33, 209, 196670, 164, 210, 131321, 194, 131320, 194,
    131321, 171, 131320, 171, 196670, 211, 33, 262205, 59, 212, 136, 458817,
    166, 213, 155, 201, 212, 96, 262205, 6, 214, 213, 327864, 29,
    216, 214, 215, 196855, 218, 0, 262394, 216, 217, 218, 131320, 217,
    262205, 13, 220, 172, 262271, 13, 221, 220, 262205, 59, 223, 136,
    393281, 174, 224, 155, 222, 223, 262205, 7, 225, 224, 524367, 13,
    226, 225, 225, 0, 1, 2, 393228, 13, 227, 1, 69, 226,
    327828, 6, 228, 221, 227, 196670, 219, 228, 262205, 6, 229, 219,
    262205, 59, 230, 136, 458817, 166, 231, 155, 201, 230, 96, 262205,
    6, 232, 231, 327864, 29, 233, 229, 232, 196855, 235, 0, 262394,
    233, 234, 236, 131320, 234, 196670, 211, 28, 131321, 235, 131320, 236,
    262205, 6, 237, 219, 458764, 6, 238, 1, 40, 237, 28, 262205,
    59, 239, 136, 458817, 166, 241, 155, 201, 239, 240, 262205, 6,
    242, 241, 458764, 6, 243, 1, 26, 238, 242, 196670, 211, 243,
    131321, 235, 131320, 235, 131321, 218, 131320, 218, 262205, 13, 245, 17,
    262205, 13, 246, 172, 327828, 6, 247, 245, 246, 458764, 6, 248,
    1, 40, 247, 28, 196670, 244, 248, 196670, 249, 33, 393281, 157,
    250, 155, 156, 46, 262205, 59, 251, 250, 327853, 29, 252, 251,
    137, 196855, 254, 0, 262394, 252, 253, 254, 131320, 253, 262205, 59,
    255, 136, 393281, 157, 256, 155, 156, 240, 262205, 59, 257, 256,
    327850, 29, 258, 255, 257, 131321, 254, 131320, 254, 458997, 29, 259,
    252, 218, 258, 253, 196855, 261, 0, 262394, 259, 260, 261, 131320,
    260, 262205, 7, 265, 263, 196670, 264, 265, 327737, 6, 266, 11,
    264, 196670, 249, 266, 131321, 261, 131320, 261, 262205, 6, 268, 244,
    262205, 6, 269, 164, 327813, 6, 270, 268, 269, 262205, 6, 271,
    211, 327813, 6, 272, 270, 271, 262205, 6, 273, 249, 327813, 6,
    274, 272, 273, 196670, 267, 274, 262205, 59, 275, 136, 393281, 174,
    276, 155, 69, 275, 262205, 7, 277, 276, 524367, 13, 278, 277,
    277, 0, 1, 2, 262205, 6, 279, 267, 327822, 13, 280, 278,
    279, 262205, 13, 281, 116, 327809, 13, 282, 281, 280, 196670, 116,
    282, 262205, 13, 284, 172, 262205, 13, 285, 119, 327809, 13, 286,
    284, 285, 393228, 13, 287, 1, 69, 286, 196670, 283, 287, 262205,
    59, 288, 136, 393281, 174, 289, 155, 69, 288, 262205, 7, 290,
    289, 524367, 13, 291, 290, 290, 0, 1, 2, 262205, 13, 292,
    17, 262205, 13, 293, 283, 327828, 6, 294, 292, 293, 458764, 6,
    295, 1, 40, 294, 28, 262205, 6, 296, 123, 458764, 6, 297,
    1, 26, 295, 296, 327822, 13, 298, 291, 297, 262205, 6, 299,
    164, 327822, 13, 300, 298, 299, 262205, 6, 301, 211, 327822, 13,
    302, 300, 301, 262205, 6, 303, 249, 327822, 13, 304, 302, 303,
    262205, 13, 305, 118, 327809, 13, 306, 305, 304, 196670, 118, 306,
    131321, 141, 131320, 141, 262205, 59, 307, 136, 327808, 59, 308, 307,
    69, 196670, 136, 308, 131321, 138, 131320, 140, 327745, 174, 315, 313,
    314, 262205, 7, 316, 315, 524367, 13, 317, 316, 316, 0, 1,
    2, 262205, 13, 318, 116, 327809, 13, 319, 317, 318, 196670, 309,
    319, 393281, 166, 320, 313, 314, 25, 262205, 6, 321, 320, 327866,
    29, 322, 321, 42, 196855, 324, 0, 262394, 322, 323, 324, 131320,
    323, 262205, 13, 326, 116, 393296, 13, 327, 28, 28, 28, 393296,
    13, 328, 33, 33, 33, 524300, 13, 329, 1, 43, 326, 327,
    328, 196670, 325, 329, 262205, 13, 330, 325, 327745, 174, 331, 313,
    314, 262205, 7, 332, 331, 524367, 13, 333, 332, 332, 0, 1,
    2, 262205, 13, 335, 325, 327811, 13, 336, 334, 335, 327813, 13,
    337, 333, 336, 327809, 13, 338, 330, 337, 196670, 309, 338, 131321,
    324, 131320, 324, 262205, 13, 340, 19, 262205, 6, 341, 20, 327811,
    6, 342, 33, 341, 327822, 13, 343, 340, 342, 196670, 339, 343,
    262205, 13, 344, 309, 262205, 13, 345, 339, 327813, 13, 346, 344,
    345, 262205, 13, 347, 118, 262205, 13, 348, 129, 327813, 13, 349,
    347, 348, 327822, 13, 351, 349, 350, 327809, 13, 352, 346, 351,
    393296, 13, 353, 28, 28, 28, 393296, 13, 354, 33, 33, 33,
    524300, 13, 355, 1, 43, 352, 353, 354, 131326, 355, 65592,
};
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
