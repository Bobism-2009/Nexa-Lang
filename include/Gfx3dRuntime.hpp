#pragma once

// std/gfx3d -- a 3D window, shaped like std/gfx.
//
// The loop is gfx's loop: open, then poll / clear / draw / present until
// closed(). What changes is what a draw means. gfx owns a framebuffer and
// writes pixels into it; gfx3d hands triangles to the GPU and lets the depth
// buffer decide what is in front, so there is no gfx.get here and no
// gfx.save -- there is no array of pixels to read back.
//
// WRITTEN FROM SCRATCH, which here means what it means everywhere else in
// this compiler:
//
//   No GL headers. <GL/gl.h> is not installed on every machine that can run
//   a GL program, and requiring it would put a -dev package between a user
//   and a spinning cube. The entry points this file calls are declared below
//   and fetched by name at run time, so the build needs nothing but a C++
//   compiler and the program needs nothing but a driver.
//
//   No GLU, no GLEW, no GLFW, no glm. gluPerspective and gluLookAt live in
//   libGLU, which is a separate library and frequently absent; both are
//   twenty lines of arithmetic and are written out in __nexa_g3_perspective
//   and __nexa_g3_look_at instead. The cube is six faces of two triangles,
//   spelled out.
//
//   No extension loading and no shader compiler. Everything here is OpenGL
//   1.1 -- glBegin, glEnd, glLoadMatrixf -- which every driver exports
//   directly from the system library. A core-profile pipeline would need
//   wglGetProcAddress on Windows, glXGetProcAddress on Linux, a GLSL
//   compiler at run time and a pair of shaders to feed it, none of which a
//   first version needs in order to put a cube on screen.
//
// The renderer is a choice the API makes room for and only one answer fills
// today. gfx3d.renderer("vulkan") is accepted and quietly gives you OpenGL;
// gfx3d.backend() reports "opengl", which is the truth rather than the
// request. When a Vulkan backend exists, the switch is already where
// programs expect it.
//
// Platforms: Windows (Win32 + WGL) and Linux (X11 + GLX). macOS and wasm
// have no backend yet, so gfx3d.open returns 0 there and every other call
// does nothing -- the same "no window open is not an error" rule gfx has.

#include <string>

namespace nexa {

// Defined below; the runtime is assembled from these three pieces.
inline std::string gfx3dPlatformCpp();
inline std::string gfx3dApiCpp();

inline std::string gfx3dRuntimeCpp() {
    return R"NEXA_GFX3D(
// ---------------------------------------------------------------------------
// std/gfx3d runtime
// ---------------------------------------------------------------------------

#include <cmath>
#include <cstring>
#include <string>

#if defined(_WIN32)
  #include <windows.h>
#elif defined(__linux__) && !defined(NEXA_WASM)
  #include <X11/Xlib.h>
  #include <X11/Xutil.h>
  #include <dlfcn.h>
#endif

// --- the slice of OpenGL 1.1 this runtime uses ------------------------------
//
// Declared here rather than included, so no GL development package is needed
// to build a Nexa program. The names and signatures are the ABI every driver
// has exported since 1995; the constants are their standard values.

typedef unsigned int  __nexa_GLenum;
typedef unsigned int  __nexa_GLbitfield;
typedef int           __nexa_GLint;
typedef int           __nexa_GLsizei;
typedef float         __nexa_GLfloat;
typedef unsigned char __nexa_GLubyte;
typedef void          __nexa_GLvoid;

#define NEXA_GL_DEPTH_BUFFER_BIT 0x00000100u
#define NEXA_GL_COLOR_BUFFER_BIT 0x00004000u
#define NEXA_GL_TRIANGLES        0x0004u
#define NEXA_GL_DEPTH_TEST       0x0B71u
#define NEXA_GL_CULL_FACE        0x0B44u
#define NEXA_GL_BACK             0x0405u
#define NEXA_GL_CCW              0x0901u
#define NEXA_GL_MODELVIEW        0x1700u
#define NEXA_GL_PROJECTION       0x1701u

#if defined(_WIN32)
  #define NEXA_GLAPI __stdcall
#else
  #define NEXA_GLAPI
#endif

typedef void (NEXA_GLAPI *__nexa_pfn_glClearColor)(__nexa_GLfloat, __nexa_GLfloat, __nexa_GLfloat, __nexa_GLfloat);
typedef void (NEXA_GLAPI *__nexa_pfn_glClear)(__nexa_GLbitfield);
typedef void (NEXA_GLAPI *__nexa_pfn_glEnable)(__nexa_GLenum);
typedef void (NEXA_GLAPI *__nexa_pfn_glDisable)(__nexa_GLenum);
typedef void (NEXA_GLAPI *__nexa_pfn_glCullFace)(__nexa_GLenum);
typedef void (NEXA_GLAPI *__nexa_pfn_glFrontFace)(__nexa_GLenum);
typedef void (NEXA_GLAPI *__nexa_pfn_glViewport)(__nexa_GLint, __nexa_GLint, __nexa_GLsizei, __nexa_GLsizei);
typedef void (NEXA_GLAPI *__nexa_pfn_glMatrixMode)(__nexa_GLenum);
typedef void (NEXA_GLAPI *__nexa_pfn_glLoadMatrixf)(const __nexa_GLfloat*);
typedef void (NEXA_GLAPI *__nexa_pfn_glBegin)(__nexa_GLenum);
typedef void (NEXA_GLAPI *__nexa_pfn_glEnd)(void);
typedef void (NEXA_GLAPI *__nexa_pfn_glVertex3f)(__nexa_GLfloat, __nexa_GLfloat, __nexa_GLfloat);
typedef void (NEXA_GLAPI *__nexa_pfn_glColor3ub)(__nexa_GLubyte, __nexa_GLubyte, __nexa_GLubyte);

struct __nexa_GL {
    __nexa_pfn_glClearColor  ClearColor  = nullptr;
    __nexa_pfn_glClear       Clear       = nullptr;
    __nexa_pfn_glEnable      Enable      = nullptr;
    __nexa_pfn_glDisable     Disable     = nullptr;
    __nexa_pfn_glCullFace    CullFace    = nullptr;
    __nexa_pfn_glFrontFace   FrontFace   = nullptr;
    __nexa_pfn_glViewport    Viewport    = nullptr;
    __nexa_pfn_glMatrixMode  MatrixMode  = nullptr;
    __nexa_pfn_glLoadMatrixf LoadMatrixf = nullptr;
    __nexa_pfn_glBegin       Begin       = nullptr;
    __nexa_pfn_glEnd         End         = nullptr;
    __nexa_pfn_glVertex3f    Vertex3f    = nullptr;
    __nexa_pfn_glColor3ub    Color3ub    = nullptr;
    int loaded = 0;
};
static __nexa_GL __nexa_gl;

// --- state ------------------------------------------------------------------

struct __nexa_G3State {
    int ready = 0;
    int closed = 0;
    int w = 0, h = 0;

    // Camera. Defaults put the eye back along +Z looking at the origin, so a
    // program that never calls gfx3d.camera still sees what it draws there.
    float eye[3]    = {0.0f, 0.0f, 5.0f};
    float target[3] = {0.0f, 0.0f, 0.0f};
    float fov = 60.0f, znear = 0.1f, zfar = 500.0f;

    // What the program asked for, and what it got. They differ only while
    // there is a renderer that is not built yet.
    std::string requested = "opengl";

    // Frame cap: 0 is uncapped, which is where every program starts.
    double frame_ms = 0.0;
    double next_deadline = 0.0;

#if defined(_WIN32)
    HWND hwnd = nullptr;
    HDC hdc = nullptr;
    HGLRC glrc = nullptr;
#elif defined(__linux__) && !defined(NEXA_WASM)
    Display* dpy = nullptr;
    Window win = 0;
    void* ctx = nullptr;
    void* libgl = nullptr;
    Atom wm_delete = 0;
#endif
};
static __nexa_G3State __nexa_g3;

// --- matrix maths, written out ----------------------------------------------
//
// OpenGL matrices are column-major: the element at row r, column c is m[c*4+r].
// Both of these build the matrix the fixed-function pipeline expects, and are
// the reason libGLU is not needed.

static void __nexa_g3_perspective(float* m, float fovDeg, float aspect, float zn, float zf) {
    for (int i = 0; i < 16; i++) m[i] = 0.0f;
    if (aspect <= 0.0f) aspect = 1.0f;
    const float pi = 3.14159265358979323846f;
    float f = 1.0f / std::tan(fovDeg * 0.5f * pi / 180.0f);
    m[0]  = f / aspect;
    m[5]  = f;
    m[10] = (zf + zn) / (zn - zf);
    m[11] = -1.0f;
    m[14] = (2.0f * zf * zn) / (zn - zf);
}

static void __nexa_g3_look_at(float* m, const float* eye, const float* at) {
    float fx = at[0] - eye[0], fy = at[1] - eye[1], fz = at[2] - eye[2];
    float fl = std::sqrt(fx * fx + fy * fy + fz * fz);
    if (fl < 1e-6f) { fx = 0.0f; fy = 0.0f; fz = -1.0f; fl = 1.0f; }
    fx /= fl; fy /= fl; fz /= fl;

    // Up is +Y. When the view is straight up or down that is parallel to the
    // forward vector and the cross product collapses, so lean the up vector
    // aside rather than handing back a matrix of zeroes.
    float ux = 0.0f, uy = 1.0f, uz = 0.0f;
    if (std::fabs(fx) < 1e-5f && std::fabs(fz) < 1e-5f) { ux = 0.0f; uy = 0.0f; uz = (fy > 0.0f) ? 1.0f : -1.0f; }

    float sx = fy * uz - fz * uy;
    float sy = fz * ux - fx * uz;
    float sz = fx * uy - fy * ux;
    float sl = std::sqrt(sx * sx + sy * sy + sz * sz);
    if (sl < 1e-6f) sl = 1.0f;
    sx /= sl; sy /= sl; sz /= sl;

    float vx = sy * fz - sz * fy;
    float vy = sz * fx - sx * fz;
    float vz = sx * fy - sy * fx;

    m[0] = sx;  m[4] = sy;  m[8]  = sz;  m[12] = -(sx * eye[0] + sy * eye[1] + sz * eye[2]);
    m[1] = vx;  m[5] = vy;  m[9]  = vz;  m[13] = -(vx * eye[0] + vy * eye[1] + vz * eye[2]);
    m[2] = -fx; m[6] = -fy; m[10] = -fz; m[14] =  (fx * eye[0] + fy * eye[1] + fz * eye[2]);
    m[3] = 0.0f; m[7] = 0.0f; m[11] = 0.0f; m[15] = 1.0f;
}

// Push the camera into the fixed-function pipeline. Called once per frame from
// gfx3d.clear, which is the point at which both matrices are known and the
// window size is settled.
static void __nexa_g3_apply_camera(void) {
    if (!__nexa_gl.loaded || !__nexa_g3.ready) return;
    float proj[16], view[16];
    float aspect = (__nexa_g3.h > 0) ? (float)__nexa_g3.w / (float)__nexa_g3.h : 1.0f;
    __nexa_g3_perspective(proj, __nexa_g3.fov, aspect, __nexa_g3.znear, __nexa_g3.zfar);
    __nexa_g3_look_at(view, __nexa_g3.eye, __nexa_g3.target);
    __nexa_gl.Viewport(0, 0, __nexa_g3.w, __nexa_g3.h);
    __nexa_gl.MatrixMode(NEXA_GL_PROJECTION);
    __nexa_gl.LoadMatrixf(proj);
    __nexa_gl.MatrixMode(NEXA_GL_MODELVIEW);
    __nexa_gl.LoadMatrixf(view);
}

// --- monotonic clock, for the frame cap -------------------------------------

static double __nexa_g3_now_ms(void) {
#if defined(_WIN32)
    LARGE_INTEGER f, c;
    QueryPerformanceFrequency(&f);
    QueryPerformanceCounter(&c);
    return (double)c.QuadPart * 1000.0 / (double)f.QuadPart;
#else
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (double)ts.tv_sec * 1000.0 + (double)ts.tv_nsec / 1000000.0;
#endif
}
)NEXA_GFX3D" + gfx3dPlatformCpp() + gfx3dApiCpp();
}


// The window, the context and the swap. Everything above this line is the
// same on every platform; everything below it is what differs, and it is kept
// to four operations -- make a window with a GL context on it, pump its
// events, swap its buffers, tear it down -- so that adding a third platform
// is filling in those four and nothing else.
inline std::string gfx3dPlatformCpp() {
    return R"NEXA_GFX3D(
// --- symbol loading ---------------------------------------------------------

#if defined(_WIN32)
static HMODULE __nexa_g3_glmod = nullptr;
#endif

// One name out of the system GL library. The library itself is opened by
// __nexa_g3_load_gl below; this is only the lookup, so that the binding macro
// reads the same on both platforms.
static void* __nexa_g3_sym(const char* name) {
#if defined(_WIN32)
    if (!__nexa_g3_glmod) return nullptr;
    return (void*)GetProcAddress(__nexa_g3_glmod, name);
#elif defined(__linux__) && !defined(NEXA_WASM)
    if (!__nexa_g3.libgl) return nullptr;
    return dlsym(__nexa_g3.libgl, name);
#else
    (void)name;
    return nullptr;
#endif
}

#define NEXA_GL_BIND(field, name)                                               \
    __nexa_gl.field = (__nexa_pfn_gl##field)__nexa_g3_sym(name);                \
    if (!__nexa_gl.field) return 0;

// Open the system GL library and bind the thirteen entry points this runtime
// uses. Every one of them is OpenGL 1.1, so a driver that cannot supply them
// cannot draw at all, and failing here is the honest answer rather than
// crashing on the first null pointer in the draw loop.
static int __nexa_g3_load_gl(void) {
    if (__nexa_gl.loaded) return 1;
#if defined(_WIN32)
    if (!__nexa_g3_glmod) __nexa_g3_glmod = LoadLibraryA("opengl32.dll");
    if (!__nexa_g3_glmod) return 0;
#elif defined(__linux__) && !defined(NEXA_WASM)
    if (!__nexa_g3.libgl) __nexa_g3.libgl = dlopen("libGL.so.1", RTLD_LAZY | RTLD_LOCAL);
    if (!__nexa_g3.libgl) __nexa_g3.libgl = dlopen("libGL.so", RTLD_LAZY | RTLD_LOCAL);
    if (!__nexa_g3.libgl) return 0;
#else
    return 0;
#endif
    NEXA_GL_BIND(ClearColor,  "glClearColor")
    NEXA_GL_BIND(Clear,       "glClear")
    NEXA_GL_BIND(Enable,      "glEnable")
    NEXA_GL_BIND(Disable,     "glDisable")
    NEXA_GL_BIND(CullFace,    "glCullFace")
    NEXA_GL_BIND(FrontFace,   "glFrontFace")
    NEXA_GL_BIND(Viewport,    "glViewport")
    NEXA_GL_BIND(MatrixMode,  "glMatrixMode")
    NEXA_GL_BIND(LoadMatrixf, "glLoadMatrixf")
    NEXA_GL_BIND(Begin,       "glBegin")
    NEXA_GL_BIND(End,         "glEnd")
    NEXA_GL_BIND(Vertex3f,    "glVertex3f")
    NEXA_GL_BIND(Color3ub,    "glColor3ub")
    __nexa_gl.loaded = 1;
    return 1;
}

// --- Windows: Win32 + WGL ---------------------------------------------------

#if defined(_WIN32)

typedef HGLRC (WINAPI *__nexa_pfn_wglCreateContext)(HDC);
typedef BOOL  (WINAPI *__nexa_pfn_wglMakeCurrent)(HDC, HGLRC);
typedef BOOL  (WINAPI *__nexa_pfn_wglDeleteContext)(HGLRC);

static LRESULT CALLBACK __nexa_g3_wndproc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    switch (msg) {
        case WM_CLOSE:
        case WM_DESTROY:
            __nexa_g3.closed = 1;
            return 0;
        case WM_SIZE:
            // The drawable size is the client area, which is what the viewport
            // and the aspect ratio are both computed from.
            __nexa_g3.w = (int)LOWORD(lp);
            __nexa_g3.h = (int)HIWORD(lp);
            return 0;
        default:
            break;
    }
    return DefWindowProcA(hwnd, msg, wp, lp);
}

static int __nexa_g3_platform_open(const std::string& title, int w, int h) {
    if (!__nexa_g3_load_gl()) return 0;

    __nexa_pfn_wglCreateContext wglCreate =
        (__nexa_pfn_wglCreateContext)__nexa_g3_sym("wglCreateContext");
    __nexa_pfn_wglMakeCurrent wglCurrent =
        (__nexa_pfn_wglMakeCurrent)__nexa_g3_sym("wglMakeCurrent");
    if (!wglCreate || !wglCurrent) return 0;

    WNDCLASSA wc;
    std::memset(&wc, 0, sizeof(wc));
    wc.lpfnWndProc = __nexa_g3_wndproc;
    wc.hInstance = GetModuleHandleA(nullptr);
    wc.lpszClassName = "NexaGfx3d";
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    // A GL window paints every pixel of its client area itself, so giving the
    // class a background brush only buys a flash of it before the first swap.
    wc.style = CS_OWNDC;
    RegisterClassA(&wc);

    RECT wr;
    wr.left = 0; wr.top = 0; wr.right = w; wr.bottom = h;
    AdjustWindowRect(&wr, WS_OVERLAPPEDWINDOW, FALSE);
    __nexa_g3.hwnd = CreateWindowExA(WS_EX_APPWINDOW, "NexaGfx3d", title.c_str(),
        WS_OVERLAPPEDWINDOW | WS_VISIBLE, CW_USEDEFAULT, CW_USEDEFAULT,
        wr.right - wr.left, wr.bottom - wr.top,
        nullptr, nullptr, wc.hInstance, nullptr);
    if (!__nexa_g3.hwnd) return 0;

    __nexa_g3.hdc = GetDC(__nexa_g3.hwnd);
    if (!__nexa_g3.hdc) return 0;

    PIXELFORMATDESCRIPTOR pfd;
    std::memset(&pfd, 0, sizeof(pfd));
    pfd.nSize = sizeof(pfd);
    pfd.nVersion = 1;
    pfd.dwFlags = PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER;
    pfd.iPixelType = PFD_TYPE_RGBA;
    pfd.cColorBits = 32;
    pfd.cDepthBits = 24;
    pfd.iLayerType = PFD_MAIN_PLANE;
    int pf = ChoosePixelFormat(__nexa_g3.hdc, &pfd);
    if (!pf || !SetPixelFormat(__nexa_g3.hdc, pf, &pfd)) return 0;

    __nexa_g3.glrc = wglCreate(__nexa_g3.hdc);
    if (!__nexa_g3.glrc) return 0;
    if (!wglCurrent(__nexa_g3.hdc, __nexa_g3.glrc)) return 0;
    return 1;
}

static void __nexa_g3_platform_poll(void) {
    MSG msg;
    while (PeekMessageA(&msg, nullptr, 0, 0, PM_REMOVE)) {
        TranslateMessage(&msg);
        DispatchMessageA(&msg);
    }
}

static void __nexa_g3_platform_swap(void) {
    if (__nexa_g3.hdc) SwapBuffers(__nexa_g3.hdc);
}

static void __nexa_g3_platform_close(void) {
    __nexa_pfn_wglMakeCurrent wglCurrent =
        (__nexa_pfn_wglMakeCurrent)__nexa_g3_sym("wglMakeCurrent");
    __nexa_pfn_wglDeleteContext wglDelete =
        (__nexa_pfn_wglDeleteContext)__nexa_g3_sym("wglDeleteContext");
    if (wglCurrent) wglCurrent(nullptr, nullptr);
    if (wglDelete && __nexa_g3.glrc) wglDelete(__nexa_g3.glrc);
    __nexa_g3.glrc = nullptr;
    if (__nexa_g3.hdc && __nexa_g3.hwnd) ReleaseDC(__nexa_g3.hwnd, __nexa_g3.hdc);
    __nexa_g3.hdc = nullptr;
    if (__nexa_g3.hwnd) DestroyWindow(__nexa_g3.hwnd);
    __nexa_g3.hwnd = nullptr;
}

// --- Linux: X11 + GLX -------------------------------------------------------

#elif defined(__linux__) && !defined(NEXA_WASM)

typedef XVisualInfo* (*__nexa_pfn_glXChooseVisual)(Display*, int, int*);
typedef void*        (*__nexa_pfn_glXCreateContext)(Display*, XVisualInfo*, void*, int);
typedef int          (*__nexa_pfn_glXMakeCurrent)(Display*, Window, void*);
typedef void         (*__nexa_pfn_glXSwapBuffers)(Display*, Window);
typedef void         (*__nexa_pfn_glXDestroyContext)(Display*, void*);

static int __nexa_g3_platform_open(const std::string& title, int w, int h) {
    if (!__nexa_g3_load_gl()) return 0;

    __nexa_pfn_glXChooseVisual   glXChooseVisual   = (__nexa_pfn_glXChooseVisual)__nexa_g3_sym("glXChooseVisual");
    __nexa_pfn_glXCreateContext  glXCreateContext  = (__nexa_pfn_glXCreateContext)__nexa_g3_sym("glXCreateContext");
    __nexa_pfn_glXMakeCurrent    glXMakeCurrent    = (__nexa_pfn_glXMakeCurrent)__nexa_g3_sym("glXMakeCurrent");
    if (!glXChooseVisual || !glXCreateContext || !glXMakeCurrent) return 0;

    __nexa_g3.dpy = XOpenDisplay(nullptr);
    if (!__nexa_g3.dpy) return 0;

    // GLX_RGBA is 4, GLX_DOUBLEBUFFER is 5, GLX_DEPTH_SIZE is 12, None is 0.
    // Spelled as their numbers because <GL/glx.h> is exactly the kind of
    // development package this runtime is written to do without.
    int attribs[] = { 4, 5, 12, 24, 0 };
    XVisualInfo* vi = glXChooseVisual(__nexa_g3.dpy, DefaultScreen(__nexa_g3.dpy), attribs);
    if (!vi) return 0;

    Window root = RootWindow(__nexa_g3.dpy, vi->screen);
    XSetWindowAttributes swa;
    std::memset(&swa, 0, sizeof(swa));
    swa.colormap = XCreateColormap(__nexa_g3.dpy, root, vi->visual, AllocNone);
    swa.event_mask = StructureNotifyMask | ExposureMask;
    __nexa_g3.win = XCreateWindow(__nexa_g3.dpy, root, 0, 0, (unsigned)w, (unsigned)h, 0,
        vi->depth, InputOutput, vi->visual, CWColormap | CWEventMask, &swa);
    if (!__nexa_g3.win) return 0;

    XStoreName(__nexa_g3.dpy, __nexa_g3.win, title.c_str());
    // Without this the window manager's close button kills the connection
    // instead of telling the program, and the process dies mid-frame.
    __nexa_g3.wm_delete = XInternAtom(__nexa_g3.dpy, "WM_DELETE_WINDOW", False);
    XSetWMProtocols(__nexa_g3.dpy, __nexa_g3.win, &__nexa_g3.wm_delete, 1);
    XMapWindow(__nexa_g3.dpy, __nexa_g3.win);

    __nexa_g3.ctx = glXCreateContext(__nexa_g3.dpy, vi, nullptr, 1);
    if (!__nexa_g3.ctx) return 0;
    if (!glXMakeCurrent(__nexa_g3.dpy, __nexa_g3.win, __nexa_g3.ctx)) return 0;
    return 1;
}

static void __nexa_g3_platform_poll(void) {
    if (!__nexa_g3.dpy) return;
    while (XPending(__nexa_g3.dpy)) {
        XEvent ev;
        XNextEvent(__nexa_g3.dpy, &ev);
        if (ev.type == ConfigureNotify) {
            __nexa_g3.w = ev.xconfigure.width;
            __nexa_g3.h = ev.xconfigure.height;
        } else if (ev.type == ClientMessage) {
            if ((Atom)ev.xclient.data.l[0] == __nexa_g3.wm_delete) __nexa_g3.closed = 1;
        } else if (ev.type == DestroyNotify) {
            __nexa_g3.closed = 1;
        }
    }
}

static void __nexa_g3_platform_swap(void) {
    __nexa_pfn_glXSwapBuffers glXSwapBuffers = (__nexa_pfn_glXSwapBuffers)__nexa_g3_sym("glXSwapBuffers");
    if (glXSwapBuffers && __nexa_g3.dpy && __nexa_g3.win) glXSwapBuffers(__nexa_g3.dpy, __nexa_g3.win);
}

static void __nexa_g3_platform_close(void) {
    __nexa_pfn_glXMakeCurrent    glXMakeCurrent    = (__nexa_pfn_glXMakeCurrent)__nexa_g3_sym("glXMakeCurrent");
    __nexa_pfn_glXDestroyContext glXDestroyContext = (__nexa_pfn_glXDestroyContext)__nexa_g3_sym("glXDestroyContext");
    if (__nexa_g3.dpy) {
        if (glXMakeCurrent) glXMakeCurrent(__nexa_g3.dpy, 0, nullptr);
        if (glXDestroyContext && __nexa_g3.ctx) glXDestroyContext(__nexa_g3.dpy, __nexa_g3.ctx);
        if (__nexa_g3.win) XDestroyWindow(__nexa_g3.dpy, __nexa_g3.win);
        XCloseDisplay(__nexa_g3.dpy);
    }
    __nexa_g3.ctx = nullptr;
    __nexa_g3.win = 0;
    __nexa_g3.dpy = nullptr;
}

// --- everywhere else --------------------------------------------------------
//
// macOS wants NSOpenGLView and an Objective-C++ slice; wasm wants WebGL, which
// is a different API and not OpenGL 1.1 at all. Neither is written yet, so
// gfx3d.open answers 0 and the rest of the module does nothing -- exactly what
// every gfx call does before the first gfx.open.

#else

static int  __nexa_g3_platform_open(const std::string& title, int w, int h) {
    (void)title; (void)w; (void)h;
    return 0;
}
static void __nexa_g3_platform_poll(void) {}
static void __nexa_g3_platform_swap(void) {}
static void __nexa_g3_platform_close(void) {}

#endif
)NEXA_GFX3D";
}

// The fifteen calls std/gfx3d offers, in the shape std/gfx offers its own.
inline std::string gfx3dApiCpp() {
    return R"NEXA_GFX3D(
// --- the module -------------------------------------------------------------

static int __nexa_gfx3d_open(const std::string& title, int w, int h) {
    if (__nexa_g3.ready) return 1;
    // Clamped rather than refused, the way gfx.open clamps its framebuffer.
    if (w < 1) w = 1;
    if (h < 1) h = 1;
    if (w > 8192) w = 8192;
    if (h > 8192) h = 8192;
    __nexa_g3.w = w;
    __nexa_g3.h = h;
    __nexa_g3.closed = 0;
    if (!__nexa_g3_platform_open(title, w, h)) {
        __nexa_g3.closed = 1;
        __nexa_g3.ready = 0;
        return 0;
    }
    // Depth testing is the whole reason this module exists rather than being
    // gfx with three coordinates, so it is on from the start rather than being
    // something a program has to remember to ask for. Back faces are dropped
    // for the same reason: a solid cube has no use for the inside of itself.
    __nexa_gl.Enable(NEXA_GL_DEPTH_TEST);
    __nexa_gl.Enable(NEXA_GL_CULL_FACE);
    __nexa_gl.CullFace(NEXA_GL_BACK);
    __nexa_gl.FrontFace(NEXA_GL_CCW);
    __nexa_g3.ready = 1;
    return 1;
}

static void __nexa_gfx3d_close(void) {
    if (!__nexa_g3.ready) return;
    __nexa_g3_platform_close();
    __nexa_g3.ready = 0;
    __nexa_g3.closed = 1;
}

static void __nexa_gfx3d_poll(void) {
    if (!__nexa_g3.ready) return;
    __nexa_g3_platform_poll();
}

static int __nexa_gfx3d_closed(void) {
    return __nexa_g3.closed;
}

static int __nexa_gfx3d_width(void) {
    return __nexa_g3.ready ? __nexa_g3.w : 0;
}

static int __nexa_gfx3d_height(void) {
    return __nexa_g3.ready ? __nexa_g3.h : 0;
}

// Colours arrive 0..255 like every colour in gfx; GL wants them 0..1.
static void __nexa_gfx3d_clear(int r, int g, int b) {
    if (!__nexa_g3.ready) return;
    if (r < 0) r = 0; if (r > 255) r = 255;
    if (g < 0) g = 0; if (g > 255) g = 255;
    if (b < 0) b = 0; if (b > 255) b = 255;
    __nexa_gl.ClearColor((float)r / 255.0f, (float)g / 255.0f, (float)b / 255.0f, 1.0f);
    __nexa_gl.Clear(NEXA_GL_COLOR_BUFFER_BIT | NEXA_GL_DEPTH_BUFFER_BIT);
    // The camera is pushed here rather than in every draw call: clear is the
    // one call a frame is guaranteed to start with, and by this point the
    // window size that the aspect ratio depends on has settled for the frame.
    __nexa_g3_apply_camera();
}

static void __nexa_gfx3d_camera(double ex, double ey, double ez,
                                double tx, double ty, double tz) {
    __nexa_g3.eye[0] = (float)ex; __nexa_g3.eye[1] = (float)ey; __nexa_g3.eye[2] = (float)ez;
    __nexa_g3.target[0] = (float)tx; __nexa_g3.target[1] = (float)ty; __nexa_g3.target[2] = (float)tz;
}

static void __nexa_gfx3d_perspective(double fov, double zn, double zf) {
    // A field of view at or past a half turn has no picture in it, and a near
    // plane at zero puts a division by zero in the projection.
    if (fov < 1.0) fov = 1.0;
    if (fov > 179.0) fov = 179.0;
    if (zn < 1e-4) zn = 1e-4;
    if (zf <= zn) zf = zn + 1.0;
    __nexa_g3.fov = (float)fov;
    __nexa_g3.znear = (float)zn;
    __nexa_g3.zfar = (float)zf;
}

static void __nexa_gfx3d_tri(double x1, double y1, double z1,
                             double x2, double y2, double z2,
                             double x3, double y3, double z3,
                             int r, int g, int b) {
    if (!__nexa_g3.ready) return;
    if (r < 0) r = 0; if (r > 255) r = 255;
    if (g < 0) g = 0; if (g > 255) g = 255;
    if (b < 0) b = 0; if (b > 255) b = 255;
    // A lone triangle is drawn from both sides. Culling is what a closed
    // shape wants -- gfx3d.cube keeps it, and never shows a viewer its own
    // inside -- but a single triangle has no inside, and one that vanished
    // when the camera passed behind it would read as a bug in the program
    // rather than as a winding rule the program never asked about.
    __nexa_gl.Disable(NEXA_GL_CULL_FACE);
    __nexa_gl.Begin(NEXA_GL_TRIANGLES);
    __nexa_gl.Color3ub((__nexa_GLubyte)r, (__nexa_GLubyte)g, (__nexa_GLubyte)b);
    __nexa_gl.Vertex3f((float)x1, (float)y1, (float)z1);
    __nexa_gl.Vertex3f((float)x2, (float)y2, (float)z2);
    __nexa_gl.Vertex3f((float)x3, (float)y3, (float)z3);
    __nexa_gl.End();
    __nexa_gl.Enable(NEXA_GL_CULL_FACE);
}

// A cube of six faces, each two triangles, wound counter-clockwise seen from
// outside so that back-face culling keeps the outside and drops the inside.
//
// There is no light in this module yet, and a cube drawn in one flat colour
// reads as a hexagon rather than a box -- every face the same, no edge
// anywhere. So each face is drawn at a fixed fraction of the colour asked
// for. It is not lighting and does not move with the camera; it is a constant
// per face, chosen so that the three faces a viewer can see at once are three
// different shades and the shape reads as solid.
static void __nexa_gfx3d_cube(double cx, double cy, double cz, double size,
                              int r, int g, int b) {
    if (!__nexa_g3.ready) return;
    if (r < 0) r = 0; if (r > 255) r = 255;
    if (g < 0) g = 0; if (g > 255) g = 255;
    if (b < 0) b = 0; if (b > 255) b = 255;
    if (size < 0.0) size = -size;
    float h = (float)size * 0.5f;
    float x = (float)cx, y = (float)cy, z = (float)cz;

    // The eight corners, as (x, y, z) offsets of +/- h.
    const float sx[8] = {-1, 1, 1, -1, -1, 1, 1, -1};
    const float sy[8] = {-1, -1, 1, 1, -1, -1, 1, 1};
    const float sz[8] = { 1, 1, 1,  1, -1, -1, -1, -1};

    // Six faces as two triangles each, corners listed counter-clockwise from
    // outside: front, right, back, left, top, bottom.
    static const int face[6][6] = {
        {0, 1, 2, 0, 2, 3},
        {1, 5, 6, 1, 6, 2},
        {5, 4, 7, 5, 7, 6},
        {4, 0, 3, 4, 3, 7},
        {3, 2, 6, 3, 6, 7},
        {4, 5, 1, 4, 1, 0},
    };
    static const float shade[6] = {1.00f, 0.72f, 0.55f, 0.72f, 0.88f, 0.45f};

    __nexa_gl.Begin(NEXA_GL_TRIANGLES);
    for (int f = 0; f < 6; f++) {
        float k = shade[f];
        __nexa_gl.Color3ub((__nexa_GLubyte)((float)r * k),
                           (__nexa_GLubyte)((float)g * k),
                           (__nexa_GLubyte)((float)b * k));
        for (int i = 0; i < 6; i++) {
            int c = face[f][i];
            __nexa_gl.Vertex3f(x + sx[c] * h, y + sy[c] * h, z + sz[c] * h);
        }
    }
    __nexa_gl.End();
}

static void __nexa_gfx3d_maxfps(int fps) {
    __nexa_g3.frame_ms = (fps > 0) ? (1000.0 / (double)fps) : 0.0;
    __nexa_g3.next_deadline = 0.0;
}

static void __nexa_gfx3d_present(void) {
    if (!__nexa_g3.ready) return;
    __nexa_g3_platform_swap();
    if (__nexa_g3.frame_ms <= 0.0) return;

    // The wait is on a deadline, not a duration, so pacing does not drift by
    // however long the frame's drawing took. A frame that overruns by more
    // than a whole period starts the count again from itself rather than
    // firing off the frames it owes. Same rule as gfx.maxfps.
    double now = __nexa_g3_now_ms();
    if (__nexa_g3.next_deadline == 0.0) {
        __nexa_g3.next_deadline = now + __nexa_g3.frame_ms;
        return;
    }
    double wait = __nexa_g3.next_deadline - now;
    if (wait > 0.0) {
#if defined(_WIN32)
        Sleep((DWORD)wait);
#else
        struct timespec ts;
        ts.tv_sec = (time_t)(wait / 1000.0);
        ts.tv_nsec = (long)((wait - (double)ts.tv_sec * 1000.0) * 1000000.0);
        nanosleep(&ts, nullptr);
#endif
        __nexa_g3.next_deadline += __nexa_g3.frame_ms;
    } else if (-wait > __nexa_g3.frame_ms) {
        __nexa_g3.next_deadline = __nexa_g3_now_ms() + __nexa_g3.frame_ms;
    } else {
        __nexa_g3.next_deadline += __nexa_g3.frame_ms;
    }
}

// What the program asked for. A renderer that is not built yet is accepted
// and answered with OpenGL; gfx3d.backend() is where the difference shows.
static int __nexa_gfx3d_renderer(const std::string& name) {
    std::string n;
    for (size_t i = 0; i < name.size(); i++) {
        char c = name[i];
        n.push_back((c >= 'A' && c <= 'Z') ? (char)(c - 'A' + 'a') : c);
    }
    if (n != "opengl" && n != "vulkan") return 0;
    __nexa_g3.requested = n;
    return 1;
}

// What the window actually opened with, which is not always what was asked
// for. Empty before the first successful gfx3d.open.
static std::string __nexa_gfx3d_backend(void) {
    if (!__nexa_g3.ready) return std::string("");
    return std::string("opengl");
}
)NEXA_GFX3D";
}

}  // namespace nexa
