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
// Platforms: Windows (Win32 + WGL), macOS (Cocoa + NSOpenGL), Linux (X11 +
// GLX) and the browser (WebGL). The first three are one renderer and three
// windows; the browser is a second renderer, because WebGL has none of the
// fixed-function pipeline the others draw with. The geometry is described
// once either way -- see "the vertex batch" below, which is the seam.
// Anywhere with no backend, gfx3d.open returns 0 and every other call does
// nothing -- the same "no window open is not an error" rule gfx has.

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
#elif defined(NEXA_WASM)
  // Emscripten's own headers, which ship with em++ itself -- unlike a GL
  // development package, having the compiler means having these. The WebGL
  // context attributes are a struct whose layout has to match exactly, which
  // is the one thing not worth hand-declaring.
  #include <emscripten/emscripten.h>
  #include <emscripten/html5.h>
#elif defined(__APPLE__)
  #import <Cocoa/Cocoa.h>
  #include <dlfcn.h>
#elif defined(__linux__)
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
typedef unsigned int  __nexa_GLuint;
typedef char          __nexa_GLchar;
typedef long          __nexa_GLsizeiptr;

#define NEXA_GL_DEPTH_BUFFER_BIT 0x00000100u
#define NEXA_GL_COLOR_BUFFER_BIT 0x00004000u
#define NEXA_GL_TRIANGLES        0x0004u
#define NEXA_GL_DEPTH_TEST       0x0B71u
#define NEXA_GL_CULL_FACE        0x0B44u
#define NEXA_GL_BACK             0x0405u
#define NEXA_GL_CCW              0x0901u
#define NEXA_GL_MODELVIEW        0x1700u
#define NEXA_GL_PROJECTION       0x1701u
#define NEXA_GL_FLOAT            0x1406u
#define NEXA_GL_ARRAY_BUFFER     0x8892u
#define NEXA_GL_DYNAMIC_DRAW     0x88E8u
#define NEXA_GL_FRAGMENT_SHADER  0x8B30u
#define NEXA_GL_VERTEX_SHADER    0x8B31u
#define NEXA_GL_COMPILE_STATUS   0x8B81u
#define NEXA_GL_LINK_STATUS      0x8B82u

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

// The GLES2 entry points the WebGL backend calls. Emscripten links these in
// itself, so unlike the desktop table they are ordinary symbols rather than
// pointers looked up at run time -- but they are still declared here rather
// than included, for the same reason: a runtime that declares what it calls
// cannot be broken by a header that is not there.
#if defined(NEXA_WASM)
extern "C" {
__nexa_GLuint glCreateShader(__nexa_GLenum);
void glShaderSource(__nexa_GLuint, __nexa_GLsizei, const __nexa_GLchar* const*, const __nexa_GLint*);
void glCompileShader(__nexa_GLuint);
void glGetShaderiv(__nexa_GLuint, __nexa_GLenum, __nexa_GLint*);
void glDeleteShader(__nexa_GLuint);
__nexa_GLuint glCreateProgram(void);
void glAttachShader(__nexa_GLuint, __nexa_GLuint);
void glLinkProgram(__nexa_GLuint);
void glGetProgramiv(__nexa_GLuint, __nexa_GLenum, __nexa_GLint*);
void glUseProgram(__nexa_GLuint);
__nexa_GLint glGetUniformLocation(__nexa_GLuint, const __nexa_GLchar*);
__nexa_GLint glGetAttribLocation(__nexa_GLuint, const __nexa_GLchar*);
void glUniformMatrix4fv(__nexa_GLint, __nexa_GLsizei, __nexa_GLubyte, const __nexa_GLfloat*);
void glGenBuffers(__nexa_GLsizei, __nexa_GLuint*);
void glBindBuffer(__nexa_GLenum, __nexa_GLuint);
void glBufferData(__nexa_GLenum, __nexa_GLsizeiptr, const void*, __nexa_GLenum);
void glEnableVertexAttribArray(__nexa_GLuint);
void glVertexAttribPointer(__nexa_GLuint, __nexa_GLint, __nexa_GLenum, __nexa_GLubyte, __nexa_GLsizei, const void*);
void glDrawArrays(__nexa_GLenum, __nexa_GLint, __nexa_GLsizei);
// The seven the two backends share, which on wasm are linked rather than
// looked up; __nexa_g3_load_gl points the table at them.
void glClearColor(__nexa_GLfloat, __nexa_GLfloat, __nexa_GLfloat, __nexa_GLfloat);
void glClear(__nexa_GLbitfield);
void glEnable(__nexa_GLenum);
void glDisable(__nexa_GLenum);
void glCullFace(__nexa_GLenum);
void glFrontFace(__nexa_GLenum);
void glViewport(__nexa_GLint, __nexa_GLint, __nexa_GLsizei, __nexa_GLsizei);
}
#endif

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

    // The camera, as matrices. The fixed-function pipeline takes these the
    // moment they are built; WebGL has no such pipeline and needs them again
    // at every draw, as a uniform, so they are kept rather than pushed.
    float proj[16] = {0};
    float view[16] = {0};

    // What the program asked for, and what it got. They differ only while
    // there is a renderer that is not built yet.
    std::string requested = "opengl";

    // Frame cap: 0 is uncapped, which is where every program starts.
    double frame_ms = 0.0;
    double next_deadline = 0.0;

    // --- input ---------------------------------------------------------
    // Two snapshots of every nameable key, taken once per gfx3d.poll. key()
    // asks the backend on the spot and keeps nothing; pressed() and
    // released() are the difference between these two rows.
    unsigned char k_now[64] = {0};
    unsigned char k_prev[64] = {0};
    int mx = 0, my = 0;            // cursor, in window pixels
    int wheel_y = 0, wheel_x = 0;  // whole notches since the last poll
    float wrem_y = 0.0f;           // a trackpad scrolls a fraction of a
    float wrem_x = 0.0f;           // notch; the remainder carries over
    std::string typed;             // text since the last poll, consumed on read
    int mb[3] = {0, 0, 0};         // left, right, middle

#if defined(_WIN32)
    HWND hwnd = nullptr;
    HDC hdc = nullptr;
    HGLRC glrc = nullptr;
#elif defined(__APPLE__) && !defined(NEXA_WASM)
    void* libgl = nullptr;
#elif defined(__linux__) && !defined(NEXA_WASM)
    Display* dpy = nullptr;
    Window win = 0;
    void* ctx = nullptr;
    void* libgl = nullptr;
    Atom wm_delete = 0;
#endif
};
static __nexa_G3State __nexa_g3;

// The two backends that cannot be asked whether a key is down keep a table
// their handlers write. Indexed by browser keyCode and by macOS virtual
// keycode respectively, which is why they are different sizes.
#if defined(NEXA_WASM)
static unsigned char __nexa_g3_wkeys[512] = {0};
#elif defined(__APPLE__)
static unsigned char __nexa_g3_mkeys[256] = {0};
#endif

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

// Multiply two column-major 4x4s: out = a * b. Only WebGL needs it -- the
// fixed-function pipeline multiplies projection by modelview itself -- but it
// is arithmetic, not a backend, so it lives up here with the other two.
static void __nexa_g3_mul4(float* out, const float* a, const float* b) {
    for (int c = 0; c < 4; c++) {
        for (int r = 0; r < 4; r++) {
            out[c * 4 + r] = a[0 * 4 + r] * b[c * 4 + 0]
                           + a[1 * 4 + r] * b[c * 4 + 1]
                           + a[2 * 4 + r] * b[c * 4 + 2]
                           + a[3 * 4 + r] * b[c * 4 + 3];
        }
    }
}

// --- the vertex batch -------------------------------------------------------
//
// Every shape in this module is triangles with a colour per vertex. They are
// written here once and handed to the platform to submit, because the two
// backends want them in completely different shapes: desktop OpenGL replays
// them one at a time through the fixed-function pipeline, and WebGL -- which
// has no fixed-function pipeline at all, no glBegin and no matrix stack --
// uploads them to a buffer for a shader to read. Keeping the geometry on this
// side of that split means gfx3d.cube is six faces of two triangles exactly
// once, whichever backend is underneath.
//
// The buffer is fixed and sized for the largest shape the module can draw, so
// a frame costs no allocation. gfx3d.cube is 36 vertices; nothing here comes
// close to the cap.

#define NEXA_G3_MAXVERTS 1024

static float __nexa_g3_vb[NEXA_G3_MAXVERTS * 6];  // x, y, z, r, g, b
static int   __nexa_g3_vn = 0;
static int   __nexa_g3_two_sided = 0;

// Declared here, defined by whichever backend is compiled in.
static void __nexa_g3_platform_camera(void);
static void __nexa_g3_batch_submit(void);
// Defined with the rest of the input below, but the event collectors up in
// the platform block need it: text is gated on focus like every other
// reading, and those collectors run before this file gets to input.
static int __nexa_g3_focused(void);

static void __nexa_g3_batch_begin(int twoSided) {
    __nexa_g3_vn = 0;
    __nexa_g3_two_sided = twoSided;
}

// Colours arrive 0..255 as they do everywhere in gfx; the batch keeps them
// 0..1, which is what both backends want in the end.
static void __nexa_g3_batch_vert(float x, float y, float z, float r, float g, float b) {
    if (__nexa_g3_vn >= NEXA_G3_MAXVERTS) return;
    float* v = &__nexa_g3_vb[__nexa_g3_vn * 6];
    v[0] = x; v[1] = y; v[2] = z;
    v[3] = r * (1.0f / 255.0f);
    v[4] = g * (1.0f / 255.0f);
    v[5] = b * (1.0f / 255.0f);
    __nexa_g3_vn++;
}

static void __nexa_g3_batch_end(void) {
    if (__nexa_g3_vn > 0) __nexa_g3_batch_submit();
    __nexa_g3_vn = 0;
}

// Rebuild the camera for this frame. Called once from gfx3d.clear, which is
// the point at which both matrices are known and the window size has settled.
// What is done with them afterwards is the backend's business.
static void __nexa_g3_apply_camera(void) {
    if (!__nexa_g3.ready) return;
    float aspect = (__nexa_g3.h > 0) ? (float)__nexa_g3.w / (float)__nexa_g3.h : 1.0f;
    __nexa_g3_perspective(__nexa_g3.proj, __nexa_g3.fov, aspect, __nexa_g3.znear, __nexa_g3.zfar);
    __nexa_g3_look_at(__nexa_g3.view, __nexa_g3.eye, __nexa_g3.target);
    __nexa_g3_platform_camera();
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
#elif !defined(NEXA_WASM) && (defined(__linux__) || defined(__APPLE__))
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
#if defined(NEXA_WASM)
    // Nothing to open: Emscripten links the GL half of WebGL into the
    // module, so the shared seven are taken by address rather than by name.
    // The six fixed-function entries stay null; nothing on this backend
    // calls them, because WebGL does not have them.
    __nexa_gl.ClearColor = glClearColor;
    __nexa_gl.Clear      = glClear;
    __nexa_gl.Enable     = glEnable;
    __nexa_gl.Disable    = glDisable;
    __nexa_gl.CullFace   = glCullFace;
    __nexa_gl.FrontFace  = glFrontFace;
    __nexa_gl.Viewport   = glViewport;
    __nexa_gl.loaded = 1;
    return 1;
#elif defined(_WIN32)
    if (!__nexa_g3_glmod) __nexa_g3_glmod = LoadLibraryA("opengl32.dll");
    if (!__nexa_g3_glmod) return 0;
#elif defined(__APPLE__)
    // The framework binary, opened by path. Apple ships it on every machine,
    // so there is nothing to install here either.
    if (!__nexa_g3.libgl)
        __nexa_g3.libgl = dlopen("/System/Library/Frameworks/OpenGL.framework/Versions/Current/OpenGL",
                                 RTLD_LAZY | RTLD_LOCAL);
    if (!__nexa_g3.libgl)
        __nexa_g3.libgl = dlopen("/System/Library/Frameworks/OpenGL.framework/OpenGL",
                                 RTLD_LAZY | RTLD_LOCAL);
    if (!__nexa_g3.libgl) return 0;
#elif defined(__linux__)
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
        case WM_MOUSEWHEEL:
        case WM_MOUSEHWHEEL: {
            // A wheel reports whole notches of WHEEL_DELTA; a trackpad sends
            // fractions of one, so the remainder is carried rather than
            // rounded away to nothing.
            float d = (float)GET_WHEEL_DELTA_WPARAM(wp) / (float)WHEEL_DELTA;
            if (msg == WM_MOUSEWHEEL) {
                __nexa_g3.wrem_y += d;
                int whole = (int)__nexa_g3.wrem_y;
                __nexa_g3.wheel_y += whole;
                __nexa_g3.wrem_y -= (float)whole;
            } else {
                __nexa_g3.wrem_x += d;
                int whole = (int)__nexa_g3.wrem_x;
                __nexa_g3.wheel_x += whole;
                __nexa_g3.wrem_x -= (float)whole;
            }
            return 0;
        }
        case WM_CHAR:
            // Printable text only: Enter, Tab, Escape and Backspace are keys,
            // and gfx3d.pressed is how a program reads those.
            // Gated like every other reading. Windows delivers WM_CHAR to the
            // focused window anyway, but "anyway" is the OS's promise rather
            // than this module's, and the other four backends collect text
            // from queues that are not so careful.
            if (!__nexa_g3_focused()) return 0;
            if (wp >= 32 && wp != 127 && __nexa_g3.typed.size() < 1024) {
                __nexa_g3.typed.push_back((char)wp);
            }
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

// --- macOS: Cocoa + NSOpenGL ------------------------------------------------
//
// The window is the one std/gfx already opens on this platform, with an
// NSOpenGLContext attached to its view instead of a bitmap blitted into it.
// The pixel format asks for NSOpenGLProfileVersionLegacy on purpose: that is
// the profile that still has the fixed-function pipeline this runtime draws
// with. A core profile would compile and then draw nothing, because glBegin
// does not exist in one.
//
// OpenGL is deprecated on macOS and has been since 10.14. Deprecated is not
// removed -- it still runs -- but a Metal backend is what this will eventually
// want, and that is a different renderer rather than a port of this one.

#elif defined(__APPLE__)

@interface __NexaG3Delegate : NSObject <NSWindowDelegate>
@end
@implementation __NexaG3Delegate
- (BOOL)windowShouldClose:(id)sender {
    (void)sender;
    __nexa_g3.closed = 1;
    return YES;
}
@end

@interface __NexaG3View : NSView
@end
@implementation __NexaG3View
- (BOOL)isOpaque { return YES; }
- (BOOL)acceptsFirstResponder { return YES; }
@end

static NSWindow* __nexa_g3_nswin = nil;
static __NexaG3View* __nexa_g3_nsview = nil;
static __NexaG3Delegate* __nexa_g3_nsdel = nil;
static NSOpenGLContext* __nexa_g3_nsctx = nil;

static int __nexa_g3_platform_open(const std::string& title, int w, int h) {
    if (!__nexa_g3_load_gl()) return 0;
    @autoreleasepool {
        [NSApplication sharedApplication];
        [NSApp setActivationPolicy:NSApplicationActivationPolicyRegular];
        static int launched = 0;
        if (!launched) {
            [NSApp finishLaunching];
            launched = 1;
        }
        NSRect content = NSMakeRect(0, 0, (CGFloat)w, (CGFloat)h);
        NSUInteger style = NSWindowStyleMaskTitled | NSWindowStyleMaskClosable |
                           NSWindowStyleMaskMiniaturizable | NSWindowStyleMaskResizable;
        __nexa_g3_nswin = [[NSWindow alloc] initWithContentRect:content
            styleMask:style backing:NSBackingStoreBuffered defer:NO];
        if (!__nexa_g3_nswin) return 0;
        [__nexa_g3_nswin setReleasedWhenClosed:NO];
        [__nexa_g3_nswin setTitle:[NSString stringWithUTF8String:title.c_str()]];

        __nexa_g3_nsview = [[__NexaG3View alloc] initWithFrame:content];
        [__nexa_g3_nswin setContentView:__nexa_g3_nsview];
        __nexa_g3_nsdel = [[__NexaG3Delegate alloc] init];
        [__nexa_g3_nswin setDelegate:__nexa_g3_nsdel];

        NSOpenGLPixelFormatAttribute attrs[] = {
            NSOpenGLPFAOpenGLProfile, NSOpenGLProfileVersionLegacy,
            NSOpenGLPFADoubleBuffer,
            NSOpenGLPFAColorSize, 24,
            NSOpenGLPFAAlphaSize, 8,
            NSOpenGLPFADepthSize, 24,
            0
        };
        NSOpenGLPixelFormat* pf = [[NSOpenGLPixelFormat alloc] initWithAttributes:attrs];
        if (!pf) return 0;
        __nexa_g3_nsctx = [[NSOpenGLContext alloc] initWithFormat:pf shareContext:nil];
        if (!__nexa_g3_nsctx) return 0;
        [__nexa_g3_nsctx setView:__nexa_g3_nsview];
        [__nexa_g3_nsctx makeCurrentContext];

        [__nexa_g3_nswin center];
        [__nexa_g3_nswin makeKeyAndOrderFront:nil];
        [NSApp activateIgnoringOtherApps:YES];
        return 1;
    }
}

static void __nexa_g3_platform_poll(void) {
    @autoreleasepool {
        // The drawable follows the view, so the size is read back rather than
        // tracked through a resize notification.
        if (__nexa_g3_nsview) {
            NSRect b = [__nexa_g3_nsview bounds];
            if ((int)b.size.width > 0) __nexa_g3.w = (int)b.size.width;
            if ((int)b.size.height > 0) __nexa_g3.h = (int)b.size.height;
        }
        for (;;) {
            NSEvent* ev = [NSApp nextEventMatchingMask:NSEventMaskAny
                                             untilDate:[NSDate distantPast]
                                                inMode:NSDefaultRunLoopMode
                                               dequeue:YES];
            NSEventType et = [ev type];
            if ((et == NSEventTypeKeyDown || et == NSEventTypeKeyUp) && __nexa_g3_focused()) {
                unsigned short kc = [ev keyCode];
                if (kc < 256) __nexa_g3_mkeys[kc] = (et == NSEventTypeKeyDown) ? 1 : 0;
                if (et == NSEventTypeKeyDown) {
                    NSString* chars = [ev characters];
                    const char* u = [chars UTF8String];
                    if (u) {
                        for (const char* q = u; *q; q++) {
                            unsigned char c = (unsigned char)*q;
                            if (c >= 32 && c != 127 && __nexa_g3.typed.size() < 1024) {
                                __nexa_g3.typed.push_back((char)c);
                            }
                        }
                    }
                }
            } else if (et == NSEventTypeFlagsChanged) {
                // Modifiers do not arrive as key events; the whole set is
                // re-read from the flags each time one of them changes.
                NSEventModifierFlags f = [ev modifierFlags];
                unsigned char sh = (f & NSEventModifierFlagShift) ? 1 : 0;
                unsigned char ct = (f & NSEventModifierFlagControl) ? 1 : 0;
                unsigned char al = (f & NSEventModifierFlagOption) ? 1 : 0;
                __nexa_g3_mkeys[56] = sh; __nexa_g3_mkeys[60] = sh;
                __nexa_g3_mkeys[59] = ct; __nexa_g3_mkeys[62] = ct;
                __nexa_g3_mkeys[58] = al; __nexa_g3_mkeys[61] = al;
            } else if (et == NSEventTypeScrollWheel) {
                // A trackpad reports pixels and a wheel reports lines; the
                // remainder carries over either way.
                float dy = (float)[ev scrollingDeltaY];
                float dx = (float)[ev scrollingDeltaX];
                if ([ev hasPreciseScrollingDeltas]) { dy /= 30.0f; dx /= 30.0f; }
                __nexa_g3.wrem_y += dy;
                __nexa_g3.wrem_x += dx;
                int wy = (int)__nexa_g3.wrem_y; __nexa_g3.wheel_y += wy; __nexa_g3.wrem_y -= (float)wy;
                int wx = (int)__nexa_g3.wrem_x; __nexa_g3.wheel_x += wx; __nexa_g3.wrem_x -= (float)wx;
            }
            [NSApp sendEvent:ev];
        }
        // A context whose view has been resized has to be told, or it keeps
        // drawing at the size it was made with.
        if (__nexa_g3_nsctx) [__nexa_g3_nsctx update];
    }
}

static void __nexa_g3_platform_swap(void) {
    if (__nexa_g3_nsctx) [__nexa_g3_nsctx flushBuffer];
}

static void __nexa_g3_platform_close(void) {
    @autoreleasepool {
        [NSOpenGLContext clearCurrentContext];
        if (__nexa_g3_nsctx) { [__nexa_g3_nsctx clearDrawable]; __nexa_g3_nsctx = nil; }
        if (__nexa_g3_nswin) {
            [__nexa_g3_nswin setDelegate:nil];
            [__nexa_g3_nswin close];
            __nexa_g3_nswin = nil;
        }
        __nexa_g3_nsview = nil;
        __nexa_g3_nsdel = nil;
    }
}

// --- wasm: Emscripten + WebGL -----------------------------------------------
//
// The one backend that is not OpenGL 1.1, because WebGL is not. There is no
// glBegin here, no matrix stack and no fixed-function anything: a triangle
// reaches the screen only through a shader reading a buffer. So this half
// carries what the others get from the driver -- a vertex shader that applies
// the camera, a fragment shader that paints the colour, and one buffer the
// batch is uploaded into.
//
// The GLSL is compiled by the browser at run time from the strings below, so
// there is still no shader compiler in the build and nothing to install. The
// two matrices are multiplied here rather than by a pipeline, which is what
// __nexa_g3_mul4 is for.

#elif defined(NEXA_WASM)

static __nexa_GLuint __nexa_g3_prog = 0;
static __nexa_GLuint __nexa_g3_vbo = 0;
static __nexa_GLint  __nexa_g3_u_mvp = -1;
static __nexa_GLint  __nexa_g3_a_pos = -1;
static __nexa_GLint  __nexa_g3_a_col = -1;

static const char* __nexa_g3_vs =
    "attribute vec3 aPos;\n"
    "attribute vec3 aCol;\n"
    "uniform mat4 uMVP;\n"
    "varying vec3 vCol;\n"
    "void main() {\n"
    "  vCol = aCol;\n"
    "  gl_Position = uMVP * vec4(aPos, 1.0);\n"
    "}\n";

static const char* __nexa_g3_fs =
    "precision mediump float;\n"
    "varying vec3 vCol;\n"
    "void main() {\n"
    "  gl_FragColor = vec4(vCol, 1.0);\n"
    "}\n";

static __nexa_GLuint __nexa_g3_compile(__nexa_GLenum kind, const char* src) {
    __nexa_GLuint s = glCreateShader(kind);
    if (!s) return 0;
    glShaderSource(s, 1, &src, nullptr);
    glCompileShader(s);
    __nexa_GLint ok = 0;
    glGetShaderiv(s, NEXA_GL_COMPILE_STATUS, &ok);
    if (!ok) { glDeleteShader(s); return 0; }
    return s;
}

static int __nexa_g3_build_program(void) {
    __nexa_GLuint vs = __nexa_g3_compile(NEXA_GL_VERTEX_SHADER, __nexa_g3_vs);
    __nexa_GLuint fs = __nexa_g3_compile(NEXA_GL_FRAGMENT_SHADER, __nexa_g3_fs);
    if (!vs || !fs) return 0;
    __nexa_g3_prog = glCreateProgram();
    if (!__nexa_g3_prog) return 0;
    glAttachShader(__nexa_g3_prog, vs);
    glAttachShader(__nexa_g3_prog, fs);
    glLinkProgram(__nexa_g3_prog);
    __nexa_GLint ok = 0;
    glGetProgramiv(__nexa_g3_prog, NEXA_GL_LINK_STATUS, &ok);
    if (!ok) { __nexa_g3_prog = 0; return 0; }
    glDeleteShader(vs);
    glDeleteShader(fs);
    __nexa_g3_u_mvp = glGetUniformLocation(__nexa_g3_prog, "uMVP");
    __nexa_g3_a_pos = glGetAttribLocation(__nexa_g3_prog, "aPos");
    __nexa_g3_a_col = glGetAttribLocation(__nexa_g3_prog, "aCol");
    glGenBuffers(1, &__nexa_g3_vbo);
    return __nexa_g3_vbo != 0;
}

// A browser pushes input rather than answering questions about it, so these
// five listeners are the whole of the wasm half: they write the same table
// and counters the other backends fill from their own event queues.
static EM_BOOL __nexa_g3_on_key(int type, const EmscriptenKeyboardEvent* e, void* user) {
    (void)user;
    if (e->keyCode < 512) {
        __nexa_g3_wkeys[e->keyCode] = (type == EMSCRIPTEN_EVENT_KEYDOWN) ? 1 : 0;
    }
    if (type == EMSCRIPTEN_EVENT_KEYPRESS && __nexa_g3_focused()
            && __nexa_g3.typed.size() < 1024) {
        // key is the character the layout produced, which is what typed()
        // is for; keyCode above is the position, which is what key() is for.
        for (const char* q = e->key; *q; q++) {
            unsigned char c = (unsigned char)*q;
            if (c >= 32 && c != 127) __nexa_g3.typed.push_back((char)c);
        }
    }
    return EM_TRUE;
}

static EM_BOOL __nexa_g3_on_mouse(int type, const EmscriptenMouseEvent* e, void* user) {
    (void)user;
    __nexa_g3.mx = e->targetX;
    __nexa_g3.my = e->targetY;
    if (type == EMSCRIPTEN_EVENT_MOUSEDOWN || type == EMSCRIPTEN_EVENT_MOUSEUP) {
        int v = (type == EMSCRIPTEN_EVENT_MOUSEDOWN) ? 1 : 0;
        if (e->button == 0) __nexa_g3.mb[0] = v;
        else if (e->button == 2) __nexa_g3.mb[1] = v;
        else if (e->button == 1) __nexa_g3.mb[2] = v;
    }
    return EM_TRUE;
}

static EM_BOOL __nexa_g3_on_wheel(int type, const EmscriptenWheelEvent* e, void* user) {
    (void)type; (void)user;
    // deltaMode 0 is pixels, 1 is lines, 2 is pages; only the first needs
    // scaling into the notch the other backends report.
    double sy = e->deltaY, sx = e->deltaX;
    if (e->deltaMode == 0) { sy /= 100.0; sx /= 100.0; }
    __nexa_g3.wrem_y += (float)(-sy);
    __nexa_g3.wrem_x += (float)(sx);
    int wy = (int)__nexa_g3.wrem_y; __nexa_g3.wheel_y += wy; __nexa_g3.wrem_y -= (float)wy;
    int wx = (int)__nexa_g3.wrem_x; __nexa_g3.wheel_x += wx; __nexa_g3.wrem_x -= (float)wx;
    return EM_TRUE;
}

static int __nexa_g3_platform_open(const std::string& title, int w, int h) {
    (void)title;  // a page has a <title>; the canvas has no name of its own
    EmscriptenWebGLContextAttributes attrs;
    emscripten_webgl_init_context_attributes(&attrs);
    attrs.alpha = 0;
    attrs.depth = 1;          // the whole point of this module
    attrs.stencil = 0;
    attrs.antialias = 1;
    attrs.majorVersion = 1;   // WebGL 1 / GLES2, which every browser has
    attrs.minorVersion = 0;
    EMSCRIPTEN_WEBGL_CONTEXT_HANDLE ctx = emscripten_webgl_create_context("#canvas", &attrs);
    if (ctx <= 0) return 0;
    if (emscripten_webgl_make_context_current(ctx) != EMSCRIPTEN_RESULT_SUCCESS) return 0;
    emscripten_set_canvas_element_size("#canvas", w, h);
    if (!__nexa_g3_load_gl()) return 0;
    if (!__nexa_g3_build_program()) return 0;
    // The window is where keys land once the canvas has focus; the canvas is
    // where the pointer lands, so its coordinates are already canvas-relative.
    emscripten_set_keydown_callback(EMSCRIPTEN_EVENT_TARGET_WINDOW, nullptr, EM_TRUE, __nexa_g3_on_key);
    emscripten_set_keyup_callback(EMSCRIPTEN_EVENT_TARGET_WINDOW, nullptr, EM_TRUE, __nexa_g3_on_key);
    emscripten_set_keypress_callback(EMSCRIPTEN_EVENT_TARGET_WINDOW, nullptr, EM_TRUE, __nexa_g3_on_key);
    emscripten_set_mousemove_callback("#canvas", nullptr, EM_TRUE, __nexa_g3_on_mouse);
    emscripten_set_mousedown_callback("#canvas", nullptr, EM_TRUE, __nexa_g3_on_mouse);
    emscripten_set_mouseup_callback("#canvas", nullptr, EM_TRUE, __nexa_g3_on_mouse);
    emscripten_set_wheel_callback("#canvas", nullptr, EM_TRUE, __nexa_g3_on_wheel);
    return 1;
}

static void __nexa_g3_platform_poll(void) {
    // Input arrives through browser callbacks rather than a queue to drain,
    // and this module reads none yet, so there is nothing here to pump. The
    // canvas is re-read in case the page resized it.
    int cw = 0, ch = 0;
    emscripten_get_canvas_element_size("#canvas", &cw, &ch);
    if (cw > 0) __nexa_g3.w = cw;
    if (ch > 0) __nexa_g3.h = ch;
}

static void __nexa_g3_platform_swap(void) {
    // A browser presents the canvas when the frame yields, so there is no
    // buffer to swap; what matters is giving the page a chance to composite.
    // NexaC links --wasm gfx3d builds with -sASYNCIFY so this can suspend.
    emscripten_sleep(0);
}

static void __nexa_g3_platform_close(void) {
    // The canvas belongs to the page, not to the program: there is nothing to
    // destroy, and a closed gfx3d window on wasm simply stops being drawn to.
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
    // Without asking, none of these ever arrive: the event mask is the
    // subscription, the way DragAcceptFiles is on Windows.
    swa.event_mask = StructureNotifyMask | ExposureMask |
                     KeyPressMask | KeyReleaseMask | ButtonPressMask | ButtonReleaseMask;
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
        } else if (ev.type == ButtonPress) {
            // X11 has no wheel: it reports one as a button press. 4 and 5
            // are up and down, 6 and 7 are left and right.
            unsigned int b = ev.xbutton.button;
            if (b == 4) __nexa_g3.wheel_y += 1;
            else if (b == 5) __nexa_g3.wheel_y -= 1;
            else if (b == 6) __nexa_g3.wheel_x -= 1;
            else if (b == 7) __nexa_g3.wheel_x += 1;
        } else if (ev.type == KeyPress && __nexa_g3_focused()) {
            char buf[32];
            KeySym ks = 0;
            int n = XLookupString(&ev.xkey, buf, (int)sizeof(buf), &ks, nullptr);
            for (int i = 0; i < n; i++) {
                unsigned char c = (unsigned char)buf[i];
                if (c >= 32 && c != 127 && __nexa_g3.typed.size() < 1024) {
                    __nexa_g3.typed.push_back((char)c);
                }
            }
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
// Windows, macOS, Linux and the browser all have a backend above. Anything
// else -- a BSD without X11, a WASI build, a platform nobody has tried --
// gets this, where gfx3d.open answers 0 and the rest of the module does
// nothing: exactly what every gfx call does before the first gfx.open.

#else

static int  __nexa_g3_platform_open(const std::string& title, int w, int h) {
    (void)title; (void)w; (void)h;
    return 0;
}
static void __nexa_g3_platform_poll(void) {}
static void __nexa_g3_platform_swap(void) {}
static void __nexa_g3_platform_close(void) {}


#endif


// --- input ------------------------------------------------------------------
//
// The same names std/gfx uses, answering the same questions, so that what a
// program knows about input in two dimensions is true in three. What differs
// is only what a coordinate means: gfx.mouse_x is a framebuffer pixel, because
// gfx has a framebuffer and scales it up; gfx3d has neither, so gfx3d.mouse_x
// is a window pixel. The top-left is 0,0 either way.
//
// This is gfx's design rather than gfx's code. The two modules keep separate
// window state -- gfx's reader reaches into __nexa_g for an X11 display and a
// browser key table -- so sharing the implementation would mean parameterising
// all of it over both. What IS shared is the part a program can see: every
// name below is a name gfx accepts, with the same aliases.
//
// Reading is gated on focus, exactly as gfx gates it: input that arrives while
// the window is not focused is dropped rather than saved up to arrive the
// moment it is.
//
// Two platforms answer "is this key down" on the spot -- GetAsyncKeyState on
// Windows, XQueryKeymap on X11 -- and two keep a table their event handlers
// write, because a browser has no such question to ask and macOS's own answer
// is session-wide rather than window-scoped.

static int __nexa_g3_focused(void) {
#if defined(_WIN32)
    return (__nexa_g3.hwnd && GetForegroundWindow() == __nexa_g3.hwnd) ? 1 : 0;
#elif defined(NEXA_WASM)
    // A browser has no "is my canvas focused" call; the page is what it
    // tracks, and a Nexa program is one canvas on one page.
    return EM_ASM_INT({ return document.hasFocus() ? 1 : 0; });
#elif defined(__APPLE__)
    return (__nexa_g3_nswin && [__nexa_g3_nswin isKeyWindow]) ? 1 : 0;
#elif defined(__linux__)
    if (!__nexa_g3.dpy) return 0;
    Window f = 0;
    int revert = 0;
    XGetInputFocus(__nexa_g3.dpy, &f, &revert);
    return (f == __nexa_g3.win) ? 1 : 0;
#else
    return 0;
#endif
}

static int __nexa_g3_key_down(const std::string& name) {
    if (name.empty() || !__nexa_g3.ready) return 0;
    if (!__nexa_g3_focused()) return 0;
    std::string s = name;
    for (char& c : s) if (c >= 'A' && c <= 'Z') c = (char)(c - 'A' + 'a');
    if (s == "return") s = "enter";
    if (s == "control") s = "ctrl";
    if (s == "bksp") s = "backspace";
    if (s == "del") s = "delete";

#if defined(_WIN32)
    auto k = [](int code) -> int { return (GetAsyncKeyState(code) & 0x8000) ? 1 : 0; };
    if (s.size() == 1) {
        char c = s[0];
        if (c >= '0' && c <= '9') return k((int)c);
        if (c >= 'a' && c <= 'z') return k((int)(c - 'a' + 'A'));
    }
    if (s == "escape") return k(VK_ESCAPE);
    if (s == "space") return k(VK_SPACE);
    if (s == "enter") return k(VK_RETURN);
    if (s == "up") return k(VK_UP);
    if (s == "down") return k(VK_DOWN);
    if (s == "left") return k(VK_LEFT);
    if (s == "right") return k(VK_RIGHT);
    if (s == "shift") return k(VK_SHIFT);
    if (s == "ctrl") return k(VK_CONTROL);
    if (s == "alt") return k(VK_MENU);
    if (s == "tab") return k(VK_TAB);
    if (s == "backspace") return k(VK_BACK);
    if (s == "delete") return k(VK_DELETE);
    if (s == "f11") return k(VK_F11);

#elif defined(NEXA_WASM)
    // Browser keyCode values, which is what the listeners record.
    auto k = [](int code) -> int {
        return (code >= 0 && code < 512 && __nexa_g3_wkeys[code]) ? 1 : 0;
    };
    if (s.size() == 1) {
        char c = s[0];
        if (c >= '0' && c <= '9') return k(48 + (c - '0'));
        if (c >= 'a' && c <= 'z') return k(65 + (c - 'a'));
    }
    if (s == "escape") return k(27);
    if (s == "space") return k(32);
    if (s == "enter") return k(13);
    if (s == "up") return k(38);
    if (s == "down") return k(40);
    if (s == "left") return k(37);
    if (s == "right") return k(39);
    if (s == "shift") return k(16);
    if (s == "ctrl") return k(17);
    if (s == "alt") return k(18);
    if (s == "tab") return k(9);
    if (s == "backspace") return k(8);
    if (s == "delete") return k(46);
    if (s == "f11") return k(122);

#elif defined(__APPLE__)
    // macOS virtual keycodes are positional; these are the ANSI layout's. The
    // table is written by the key events this window's own poll drains, which
    // is why CoreGraphics is not needed here: CGEventSourceKeyState would
    // answer for the whole session rather than for this window, and would pull
    // in a framework to do it.
    auto k = [](int code) -> int {
        return (code >= 0 && code < 256 && __nexa_g3_mkeys[code]) ? 1 : 0;
    };
    static const int letters[26] = {
        0, 11, 8, 2, 14, 3, 5, 4, 34, 38, 40, 37, 46,
        45, 31, 35, 12, 15, 1, 17, 32, 9, 13, 7, 16, 6
    };
    static const int digits[10] = {29, 18, 19, 20, 21, 23, 22, 26, 28, 25};
    if (s.size() == 1) {
        char c = s[0];
        if (c >= '0' && c <= '9') return k(digits[c - '0']);
        if (c >= 'a' && c <= 'z') return k(letters[c - 'a']);
    }
    if (s == "escape") return k(53);
    if (s == "space") return k(49);
    if (s == "enter") return k(36);
    if (s == "up") return k(126);
    if (s == "down") return k(125);
    if (s == "left") return k(123);
    if (s == "right") return k(124);
    if (s == "shift") return (k(56) || k(60)) ? 1 : 0;
    if (s == "ctrl") return (k(59) || k(62)) ? 1 : 0;
    if (s == "alt") return (k(58) || k(61)) ? 1 : 0;
    if (s == "tab") return k(48);
    if (s == "backspace") return k(51);
    if (s == "delete") return k(117);
    if (s == "f11") return k(103);

#elif defined(__linux__)
    if (!__nexa_g3.dpy) return 0;
    char km[32];
    XQueryKeymap(__nexa_g3.dpy, km);
    auto held = [&](KeySym sym) -> int {
        KeyCode kc = XKeysymToKeycode(__nexa_g3.dpy, sym);
        if (!kc) return 0;
        return (km[kc >> 3] & (1 << (kc & 7))) ? 1 : 0;
    };
    if (s.size() == 1) {
        char c = s[0];
        if (c >= '0' && c <= '9') return held((KeySym)c);
        if (c >= 'a' && c <= 'z') return (held((KeySym)c) || held((KeySym)(c - 'a' + 'A'))) ? 1 : 0;
    }
    if (s == "escape") return held(XK_Escape);
    if (s == "space") return held(XK_space);
    if (s == "enter") return held(XK_Return);
    if (s == "up") return held(XK_Up);
    if (s == "down") return held(XK_Down);
    if (s == "left") return held(XK_Left);
    if (s == "right") return held(XK_Right);
    if (s == "shift") return (held(XK_Shift_L) || held(XK_Shift_R)) ? 1 : 0;
    if (s == "ctrl") return (held(XK_Control_L) || held(XK_Control_R)) ? 1 : 0;
    if (s == "alt") return (held(XK_Alt_L) || held(XK_Alt_R)) ? 1 : 0;
    if (s == "tab") return held(XK_Tab);
    if (s == "backspace") return held(XK_BackSpace);
    if (s == "delete") return held(XK_Delete);
    if (s == "f11") return held(XK_F11);
#endif
    return 0;
}

// Every name an edge can be asked about. gfx3d.key() needs no table -- it
// forwards a name straight to the backend -- but pressed() and released() ask
// "did this change since the last poll", and a change needs a row to compare
// against, which needs a fixed set of names to be a row of.
static const char* const __nexa_g3_key_names[] = {
    "0", "1", "2", "3", "4", "5", "6", "7", "8", "9",
    "a", "b", "c", "d", "e", "f", "g", "h", "i", "j", "k", "l", "m",
    "n", "o", "p", "q", "r", "s", "t", "u", "v", "w", "x", "y", "z",
    "escape", "space", "enter", "up", "down", "left", "right",
    "shift", "ctrl", "alt", "tab", "backspace", "delete", "f11"
};
#define NEXA_G3_NKEYS ((int)(sizeof(__nexa_g3_key_names) / sizeof(__nexa_g3_key_names[0])))

static int __nexa_g3_key_slot(const std::string& name) {
    std::string s = name;
    for (char& c : s) if (c >= 'A' && c <= 'Z') c = (char)(c - 'A' + 'a');
    if (s == "return") s = "enter";
    if (s == "control") s = "ctrl";
    if (s == "bksp") s = "backspace";
    if (s == "del") s = "delete";
    for (int i = 0; i < NEXA_G3_NKEYS; i++) {
        if (s == __nexa_g3_key_names[i]) return i;
    }
    return -1;
}

static void __nexa_g3_key_snapshot(void) {
    for (int i = 0; i < NEXA_G3_NKEYS; i++) {
        __nexa_g3.k_prev[i] = __nexa_g3.k_now[i];
        __nexa_g3.k_now[i] = (unsigned char)__nexa_g3_key_down(__nexa_g3_key_names[i]);
    }
}

// The mouse-button names gfx answers to. A name none of them recognises is 0
// rather than an error, which is gfx's rule too.
static int __nexa_g3_mouse_index(const std::string& name) {
    std::string s = name;
    for (char& c : s) if (c >= 'A' && c <= 'Z') c = (char)(c - 'A' + 'a');
    if (s == "left" || s == "l" || s == "lmb") return 0;
    if (s == "right" || s == "r" || s == "rmb") return 1;
    if (s == "middle" || s == "m" || s == "mmb") return 2;
    return -1;
}

// Where the cursor is, in window pixels, and which buttons are down. Both are
// asked of the backend rather than tracked, wherever the backend can answer.
static void __nexa_g3_read_mouse(void) {
    if (!__nexa_g3.ready) return;
#if defined(_WIN32)
    POINT p;
    if (GetCursorPos(&p) && ScreenToClient(__nexa_g3.hwnd, &p)) {
        __nexa_g3.mx = p.x;
        __nexa_g3.my = p.y;
    }
#elif defined(__linux__) && !defined(NEXA_WASM)
    if (!__nexa_g3.dpy) return;
    Window root = 0, child = 0;
    int rx = 0, ry = 0, wx = 0, wy = 0;
    unsigned int mask = 0;
    if (XQueryPointer(__nexa_g3.dpy, __nexa_g3.win, &root, &child, &rx, &ry, &wx, &wy, &mask)) {
        __nexa_g3.mx = wx;
        __nexa_g3.my = wy;
        __nexa_g3.mb[0] = (mask & Button1Mask) ? 1 : 0;
        __nexa_g3.mb[1] = (mask & Button3Mask) ? 1 : 0;
        __nexa_g3.mb[2] = (mask & Button2Mask) ? 1 : 0;
    }
#elif defined(__APPLE__) && !defined(NEXA_WASM)
    if (!__nexa_g3_nswin || !__nexa_g3_nsview) return;
    NSPoint p = [__nexa_g3_nswin mouseLocationOutsideOfEventStream];
    NSPoint v = [__nexa_g3_nsview convertPoint:p fromView:nil];
    NSRect b = [__nexa_g3_nsview bounds];
    // Cocoa's origin is bottom-left and every other backend's is top-left.
    __nexa_g3.mx = (int)v.x;
    __nexa_g3.my = (int)(b.size.height - v.y);
    NSUInteger held = [NSEvent pressedMouseButtons];
    __nexa_g3.mb[0] = (held & (1 << 0)) ? 1 : 0;
    __nexa_g3.mb[1] = (held & (1 << 1)) ? 1 : 0;
    __nexa_g3.mb[2] = (held & (1 << 2)) ? 1 : 0;
#endif
    // Windows reads its buttons where it reads its keys, and wasm has them
    // from the listeners; neither needs a line here.
#if defined(_WIN32)
    __nexa_g3.mb[0] = (GetAsyncKeyState(VK_LBUTTON) & 0x8000) ? 1 : 0;
    __nexa_g3.mb[1] = (GetAsyncKeyState(VK_RBUTTON) & 0x8000) ? 1 : 0;
    __nexa_g3.mb[2] = (GetAsyncKeyState(VK_MBUTTON) & 0x8000) ? 1 : 0;
#endif
}

// --- submitting the batch ---------------------------------------------------
//
// The two halves of the split this module is built around. Above this line,
// one description of the geometry; below it, the two ways a machine will take
// it.

#if defined(NEXA_WASM)

// WebGL keeps no camera of its own, so the matrices go in as a uniform at
// draw time. Only the viewport can be set here.
static void __nexa_g3_platform_camera(void) {
    glViewport(0, 0, __nexa_g3.w, __nexa_g3.h);
}

static void __nexa_g3_batch_submit(void) {
    if (!__nexa_g3_prog || !__nexa_g3_vbo) return;
    if (__nexa_g3_two_sided) glDisable(NEXA_GL_CULL_FACE);

    float mvp[16];
    __nexa_g3_mul4(mvp, __nexa_g3.proj, __nexa_g3.view);

    glUseProgram(__nexa_g3_prog);
    glUniformMatrix4fv(__nexa_g3_u_mvp, 1, 0, mvp);
    glBindBuffer(NEXA_GL_ARRAY_BUFFER, __nexa_g3_vbo);
    glBufferData(NEXA_GL_ARRAY_BUFFER,
                 (__nexa_GLsizeiptr)(__nexa_g3_vn * 6 * (int)sizeof(float)),
                 __nexa_g3_vb, NEXA_GL_DYNAMIC_DRAW);

    const __nexa_GLsizei stride = (__nexa_GLsizei)(6 * sizeof(float));
    if (__nexa_g3_a_pos >= 0) {
        glEnableVertexAttribArray((__nexa_GLuint)__nexa_g3_a_pos);
        glVertexAttribPointer((__nexa_GLuint)__nexa_g3_a_pos, 3, NEXA_GL_FLOAT, 0, stride, (const void*)0);
    }
    if (__nexa_g3_a_col >= 0) {
        glEnableVertexAttribArray((__nexa_GLuint)__nexa_g3_a_col);
        glVertexAttribPointer((__nexa_GLuint)__nexa_g3_a_col, 3, NEXA_GL_FLOAT, 0, stride,
                              (const void*)(3 * sizeof(float)));
    }
    glDrawArrays(NEXA_GL_TRIANGLES, 0, __nexa_g3_vn);

    if (__nexa_g3_two_sided) glEnable(NEXA_GL_CULL_FACE);
}

#else

// The fixed-function pipeline takes the camera as two matrices and keeps it,
// and takes the geometry a vertex at a time.
static void __nexa_g3_platform_camera(void) {
    if (!__nexa_gl.loaded) return;
    __nexa_gl.Viewport(0, 0, __nexa_g3.w, __nexa_g3.h);
    __nexa_gl.MatrixMode(NEXA_GL_PROJECTION);
    __nexa_gl.LoadMatrixf(__nexa_g3.proj);
    __nexa_gl.MatrixMode(NEXA_GL_MODELVIEW);
    __nexa_gl.LoadMatrixf(__nexa_g3.view);
}

static void __nexa_g3_batch_submit(void) {
    if (!__nexa_gl.loaded) return;
    if (__nexa_g3_two_sided) __nexa_gl.Disable(NEXA_GL_CULL_FACE);
    __nexa_gl.Begin(NEXA_GL_TRIANGLES);
    for (int i = 0; i < __nexa_g3_vn; i++) {
        const float* v = &__nexa_g3_vb[i * 6];
        __nexa_gl.Color3ub((__nexa_GLubyte)(v[3] * 255.0f + 0.5f),
                           (__nexa_GLubyte)(v[4] * 255.0f + 0.5f),
                           (__nexa_GLubyte)(v[5] * 255.0f + 0.5f));
        __nexa_gl.Vertex3f(v[0], v[1], v[2]);
    }
    __nexa_gl.End();
    if (__nexa_g3_two_sided) __nexa_gl.Enable(NEXA_GL_CULL_FACE);
}

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
    // The wheel reports one frame and the next poll replaces it, so it is
    // cleared before the events that fill it are drained rather than after.
    __nexa_g3.wheel_y = 0;
    __nexa_g3.wheel_x = 0;
    __nexa_g3_platform_poll();
    __nexa_g3_read_mouse();
    __nexa_g3_key_snapshot();
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
    __nexa_g3_batch_begin(1);
    __nexa_g3_batch_vert((float)x1, (float)y1, (float)z1, (float)r, (float)g, (float)b);
    __nexa_g3_batch_vert((float)x2, (float)y2, (float)z2, (float)r, (float)g, (float)b);
    __nexa_g3_batch_vert((float)x3, (float)y3, (float)z3, (float)r, (float)g, (float)b);
    __nexa_g3_batch_end();
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

    __nexa_g3_batch_begin(0);
    for (int f = 0; f < 6; f++) {
        float k = shade[f];
        for (int i = 0; i < 6; i++) {
            int c = face[f][i];
            __nexa_g3_batch_vert(x + sx[c] * h, y + sy[c] * h, z + sz[c] * h,
                                 (float)r * k, (float)g * k, (float)b * k);
        }
    }
    __nexa_g3_batch_end();
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

// --- the input calls --------------------------------------------------------

static int __nexa_gfx3d_key(const std::string& name) {
    return __nexa_g3_key_down(name);
}

// Went down between the last two polls. A name outside the table above has
// no row to compare and answers 0, the way an unrecognised mouse button does.
static int __nexa_gfx3d_pressed(const std::string& name) {
    if (!__nexa_g3.ready) return 0;
    int i = __nexa_g3_key_slot(name);
    if (i < 0) return 0;
    return (__nexa_g3.k_now[i] && !__nexa_g3.k_prev[i]) ? 1 : 0;
}

static int __nexa_gfx3d_released(const std::string& name) {
    if (!__nexa_g3.ready) return 0;
    int i = __nexa_g3_key_slot(name);
    if (i < 0) return 0;
    return (!__nexa_g3.k_now[i] && __nexa_g3.k_prev[i]) ? 1 : 0;
}

static int __nexa_gfx3d_mouse(const std::string& name) {
    if (!__nexa_g3.ready || !__nexa_g3_focused()) return 0;
    int i = __nexa_g3_mouse_index(name);
    if (i < 0) return 0;
    return __nexa_g3.mb[i] ? 1 : 0;
}

static int __nexa_gfx3d_mouse_x(void) { return __nexa_g3.ready ? __nexa_g3.mx : 0; }
static int __nexa_gfx3d_mouse_y(void) { return __nexa_g3.ready ? __nexa_g3.my : 0; }

static int __nexa_gfx3d_wheel(void)   { return __nexa_g3.ready ? __nexa_g3.wheel_y : 0; }
static int __nexa_gfx3d_wheel_x(void) { return __nexa_g3.ready ? __nexa_g3.wheel_x : 0; }

// Consumes what it returns, like gfx.typed: empty when nothing was typed,
// and empty again on a second call in the same frame.
static std::string __nexa_gfx3d_typed(void) {
    if (!__nexa_g3.ready) return std::string("");
    std::string out;
    out.swap(__nexa_g3.typed);
    return out;
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
