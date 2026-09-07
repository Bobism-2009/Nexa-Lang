#pragma once

#include <string>

namespace nexa {

inline std::string gfxRuntimeCpp() {
    return R"NEXA_GFX(
#include <string>
#include <cstdint>
#include <cstring>
#include <cstdlib>
#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#include <emscripten/html5.h>
#elif defined(_WIN32)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#elif defined(__APPLE__)
#import <Cocoa/Cocoa.h>
#include <CoreGraphics/CoreGraphics.h>
#elif defined(__linux__)
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/keysym.h>
#endif

struct __nexa_Gfx {
    int w;
    int h;
    int scale;
    int closed;
    int ready;
    int mx;
    int my;
    int min;
    int mlb;
    int mmb;
    int mrb;
    int text_scale;
    unsigned char* fb;
    std::string title;
#ifdef _WIN32
    HWND hwnd;
    BITMAPINFO bmi;
#endif
#if defined(__linux__) && !defined(__EMSCRIPTEN__)
    Display* dpy;
    Window win;
    GC gc;
    XImage* img;
    Visual* vis;
    unsigned char* xbuf;
    int xbw;
    int xbh;
    int wm_delete;
#endif
#ifdef __EMSCRIPTEN__
    int keys[512];
#endif
};

static __nexa_Gfx __nexa_g = {};

static int __nexa_gfx_map_mouse(int px, int py, int cw, int ch, int* ox, int* oy) {
    if (cw < 1 || ch < 1 || __nexa_g.w < 1 || __nexa_g.h < 1) return 0;
    if (px < 0 || py < 0 || px >= cw || py >= ch) return 0;
    int x = px * __nexa_g.w / cw;
    int y = py * __nexa_g.h / ch;
    if (x >= __nexa_g.w) x = __nexa_g.w - 1;
    if (y >= __nexa_g.h) y = __nexa_g.h - 1;
    if (x < 0) x = 0;
    if (y < 0) y = 0;
    *ox = x;
    *oy = y;
    return 1;
}

static void __nexa_gfx_mouse_refresh();

static void __nexa_gfx_mouse_apply(int x, int y, int inside, int left, int middle, int right) {
    if (inside) {
        __nexa_g.mx = x;
        __nexa_g.my = y;
        __nexa_g.min = 1;
        __nexa_g.mlb = left ? 1 : 0;
        __nexa_g.mmb = middle ? 1 : 0;
        __nexa_g.mrb = right ? 1 : 0;
    } else {
        __nexa_g.min = 0;
        __nexa_g.mlb = 0;
        __nexa_g.mmb = 0;
        __nexa_g.mrb = 0;
    }
}

#ifdef __APPLE__
@interface __NexaGfxDelegate : NSObject <NSWindowDelegate>
@end
@implementation __NexaGfxDelegate
- (BOOL)windowShouldClose:(id)sender {
    (void)sender;
    __nexa_g.closed = 1;
    return YES;
}
@end

@interface __NexaGfxView : NSView
@end
@implementation __NexaGfxView
- (BOOL)isOpaque { return YES; }
- (BOOL)acceptsFirstResponder { return YES; }
- (void)drawRect:(NSRect)dirtyRect {
    (void)dirtyRect;
    if (!__nexa_g.fb || __nexa_g.w < 1 || __nexa_g.h < 1) return;
    CGColorSpaceRef cs = CGColorSpaceCreateDeviceRGB();
    if (!cs) return;
    CGContextRef bmp = CGBitmapContextCreate(
        __nexa_g.fb, (size_t)__nexa_g.w, (size_t)__nexa_g.h, 8,
        (size_t)__nexa_g.w * 4, cs,
        (CGBitmapInfo)kCGImageAlphaNoneSkipLast | (CGBitmapInfo)kCGBitmapByteOrder32Big);
    CGColorSpaceRelease(cs);
    if (!bmp) return;
    CGImageRef img = CGBitmapContextCreateImage(bmp);
    CGContextRelease(bmp);
    if (!img) return;
    CGContextRef ctx = [[NSGraphicsContext currentContext] CGContext];
    CGContextSaveGState(ctx);
    CGContextSetInterpolationQuality(ctx, kCGInterpolationNone);
    NSRect b = [self bounds];
    CGContextTranslateCTM(ctx, 0, b.size.height);
    CGContextScaleCTM(ctx, 1.0, -1.0);
    CGContextDrawImage(ctx, CGRectMake(0, 0, b.size.width, b.size.height), img);
    CGContextRestoreGState(ctx);
    CGImageRelease(img);
}
@end

static NSWindow* __nexa_gfx_nswin = nil;
static __NexaGfxView* __nexa_gfx_nsview = nil;
static __NexaGfxDelegate* __nexa_gfx_delegate = nil;
#endif

static void __nexa_gfx_free() {
    delete[] __nexa_g.fb;
    __nexa_g.fb = nullptr;
#if defined(__linux__) && !defined(__EMSCRIPTEN__)
    if (__nexa_g.img) {
        __nexa_g.img->data = nullptr;
        XDestroyImage(__nexa_g.img);
        __nexa_g.img = nullptr;
    }
    delete[] __nexa_g.xbuf;
    __nexa_g.xbuf = nullptr;
    __nexa_g.xbw = 0;
    __nexa_g.xbh = 0;
    if (__nexa_g.dpy && __nexa_g.win) {
        XDestroyWindow(__nexa_g.dpy, __nexa_g.win);
        __nexa_g.win = 0;
    }
    if (__nexa_g.dpy) {
        XCloseDisplay(__nexa_g.dpy);
        __nexa_g.dpy = nullptr;
    }
    __nexa_g.vis = nullptr;
#endif
#ifdef _WIN32
    if (__nexa_g.hwnd) {
        DestroyWindow(__nexa_g.hwnd);
        __nexa_g.hwnd = nullptr;
    }
#endif
#ifdef __APPLE__
    if (__nexa_gfx_nswin) {
        [__nexa_gfx_nswin setDelegate:nil];
        [__nexa_gfx_nswin close];
        __nexa_gfx_nswin = nil;
    }
    __nexa_gfx_nsview = nil;
    __nexa_gfx_delegate = nil;
#endif
    __nexa_g.ready = 0;
    __nexa_g.min = 0;
    __nexa_g.mlb = 0;
    __nexa_g.mmb = 0;
    __nexa_g.mrb = 0;
}

#ifdef _WIN32
static LRESULT CALLBACK __nexa_gfx_wndproc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    if (msg == WM_CLOSE) {
        __nexa_g.closed = 1;
        DestroyWindow(hwnd);
        return 0;
    }
    if (msg == WM_DESTROY) {
        if (__nexa_g.hwnd == hwnd) __nexa_g.hwnd = nullptr;
        return 0;
    }
    if (msg == WM_ERASEBKGND) return 1;
    if (msg == WM_PAINT) {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hwnd, &ps);
        if (__nexa_g.fb && __nexa_g.w > 0 && __nexa_g.h > 0) {
            RECT rc;
            GetClientRect(hwnd, &rc);
            StretchDIBits(hdc, 0, 0, rc.right, rc.bottom, 0, 0, __nexa_g.w, __nexa_g.h,
                __nexa_g.fb, &__nexa_g.bmi, DIB_RGB_COLORS, SRCCOPY);
        }
        EndPaint(hwnd, &ps);
        return 0;
    }
    return DefWindowProcA(hwnd, msg, wParam, lParam);
}
#endif

#ifdef __EMSCRIPTEN__
static EM_BOOL __nexa_gfx_ekey(int type, const EmscriptenKeyboardEvent* e, void*) {
    int down = (type == EMSCRIPTEN_EVENT_KEYDOWN) ? 1 : 0;
    int code = (int)e->keyCode;
    if (code >= 0 && code < 512) __nexa_g.keys[code] = down;
    if (code == 27 && !down) __nexa_g.closed = 1;
    return EM_TRUE;
}

static EM_BOOL __nexa_gfx_emouse(int type, const EmscriptenMouseEvent* e, void*) {
    if (type == EMSCRIPTEN_EVENT_MOUSELEAVE) {
        __nexa_gfx_mouse_apply(0, 0, 0, 0, 0, 0);
        return EM_TRUE;
    }
    double css_w = 0, css_h = 0;
    emscripten_get_element_css_size("#canvas", &css_w, &css_h);
    int cw = (int)css_w;
    int ch = (int)css_h;
    if (cw < 1) cw = __nexa_g.w;
    if (ch < 1) ch = __nexa_g.h;
    int ox = 0, oy = 0;
    int inside = __nexa_gfx_map_mouse((int)e->targetX, (int)e->targetY, cw, ch, &ox, &oy);
    unsigned short bt = e->buttons;
    __nexa_gfx_mouse_apply(ox, oy, inside, (bt & 1) != 0, (bt & 4) != 0, (bt & 2) != 0);
    return EM_TRUE;
}
#endif

static const int __nexa_gfx_max = 4096;

static void __nexa_gfx_clamp_whs(int* w, int* h, int* scale, int defaultScale) {
    if (*w < 1) *w = 1;
    if (*h < 1) *h = 1;
    if (*w > __nexa_gfx_max) *w = __nexa_gfx_max;
    if (*h > __nexa_gfx_max) *h = __nexa_gfx_max;
    if (*scale < 1) *scale = defaultScale < 1 ? 12 : defaultScale;
    if (*scale > 64) *scale = 64;
}

static void __nexa_gfx_drop_x11_image() {
#if defined(__linux__) && !defined(__EMSCRIPTEN__)
    if (__nexa_g.img) {
        __nexa_g.img->data = nullptr;
        XDestroyImage(__nexa_g.img);
        __nexa_g.img = nullptr;
    }
    delete[] __nexa_g.xbuf;
    __nexa_g.xbuf = nullptr;
    __nexa_g.xbw = 0;
    __nexa_g.xbh = 0;
#endif
}

static void __nexa_gfx_apply_window_size(int w, int h, int scale) {
#ifdef __EMSCRIPTEN__
    EM_ASM(({
        var c = Module['canvas'] || document.getElementById('canvas');
        if (!c) return;
        c.width = $0;
        c.height = $1;
        c.style.width = ($0 * $2) + 'px';
        c.style.height = ($1 * $2) + 'px';
    }), w, h, scale);
#elif defined(_WIN32)
    if (!__nexa_g.hwnd) return;
    __nexa_g.bmi.bmiHeader.biWidth = w;
    __nexa_g.bmi.bmiHeader.biHeight = -h;
    DWORD style = (DWORD)GetWindowLongA(__nexa_g.hwnd, GWL_STYLE);
    RECT wr = {0, 0, w * scale, h * scale};
    AdjustWindowRect(&wr, style, FALSE);
    SetWindowPos(__nexa_g.hwnd, nullptr, 0, 0, wr.right - wr.left, wr.bottom - wr.top,
        SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE);
    InvalidateRect(__nexa_g.hwnd, nullptr, FALSE);
#elif defined(__APPLE__)
    if (!__nexa_gfx_nswin) return;
    NSSize sz = NSMakeSize((CGFloat)(w * scale), (CGFloat)(h * scale));
    [__nexa_gfx_nswin setContentSize:sz];
    if (__nexa_gfx_nsview) {
        [__nexa_gfx_nsview setFrame:NSMakeRect(0, 0, sz.width, sz.height)];
        [__nexa_gfx_nsview setNeedsDisplay:YES];
    }
#elif defined(__linux__)
    __nexa_gfx_drop_x11_image();
    if (__nexa_g.dpy && __nexa_g.win) {
        XResizeWindow(__nexa_g.dpy, __nexa_g.win, (unsigned)(w * scale), (unsigned)(h * scale));
        XFlush(__nexa_g.dpy);
    }
#endif
}

static int __nexa_gfx_open(const std::string& title, int w, int h, int scale) {
    __nexa_gfx_clamp_whs(&w, &h, &scale, 12);
    __nexa_gfx_free();
    __nexa_g.w = w;
    __nexa_g.h = h;
    __nexa_g.scale = scale;
    __nexa_g.closed = 0;
    __nexa_g.mx = 0;
    __nexa_g.my = 0;
    __nexa_g.min = 0;
    __nexa_g.mlb = 0;
    __nexa_g.mmb = 0;
    __nexa_g.mrb = 0;
    if (__nexa_g.text_scale < 1) __nexa_g.text_scale = 1;
    __nexa_g.title = title;
    __nexa_g.fb = new unsigned char[(size_t)w * (size_t)h * 4];
    std::memset(__nexa_g.fb, 0, (size_t)w * (size_t)h * 4);
#ifdef __EMSCRIPTEN__
    std::memset(__nexa_g.keys, 0, sizeof(__nexa_g.keys));
    emscripten_set_keydown_callback(EMSCRIPTEN_EVENT_TARGET_WINDOW, 0, 1, __nexa_gfx_ekey);
    emscripten_set_keyup_callback(EMSCRIPTEN_EVENT_TARGET_WINDOW, 0, 1, __nexa_gfx_ekey);
    EM_ASM(({
        var c = Module['canvas'] || document.getElementById('canvas');
        if (!c) {
            c = document.createElement('canvas');
            c.id = 'canvas';
            document.body.appendChild(c);
        }
        Module['canvas'] = c;
        c.width = $0;
        c.height = $1;
        c.style.width = ($0 * $2) + 'px';
        c.style.height = ($1 * $2) + 'px';
        c.style.imageRendering = 'pixelated';
        c.style.background = '#000';
        document.title = UTF8ToString($3);
    }), w, h, scale, title.c_str());
    emscripten_set_mousemove_callback("#canvas", 0, 1, __nexa_gfx_emouse);
    emscripten_set_mousedown_callback("#canvas", 0, 1, __nexa_gfx_emouse);
    emscripten_set_mouseup_callback("#canvas", 0, 1, __nexa_gfx_emouse);
    emscripten_set_mouseleave_callback("#canvas", 0, 1, __nexa_gfx_emouse);
    __nexa_g.ready = 1;
    return 1;
#elif defined(_WIN32)
    WNDCLASSA wc = {};
    wc.lpfnWndProc = __nexa_gfx_wndproc;
    wc.hInstance = GetModuleHandleA(nullptr);
    wc.lpszClassName = "NexaGfx";
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)GetStockObject(BLACK_BRUSH);
    RegisterClassA(&wc);
    RECT wr = {0, 0, w * scale, h * scale};
    AdjustWindowRect(&wr, WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX | WS_THICKFRAME, FALSE);
    __nexa_g.hwnd = CreateWindowExA(0, "NexaGfx", title.c_str(),
        WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX | WS_THICKFRAME | WS_VISIBLE,
        CW_USEDEFAULT, CW_USEDEFAULT, wr.right - wr.left, wr.bottom - wr.top,
        nullptr, nullptr, wc.hInstance, nullptr);
    std::memset(&__nexa_g.bmi, 0, sizeof(__nexa_g.bmi));
    __nexa_g.bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    __nexa_g.bmi.bmiHeader.biWidth = w;
    __nexa_g.bmi.bmiHeader.biHeight = -h;
    __nexa_g.bmi.bmiHeader.biPlanes = 1;
    __nexa_g.bmi.bmiHeader.biBitCount = 32;
    __nexa_g.bmi.bmiHeader.biCompression = BI_RGB;
    __nexa_g.ready = __nexa_g.hwnd ? 1 : 0;
    if (!__nexa_g.hwnd) __nexa_g.closed = 1;
    return __nexa_g.ready;
#elif defined(__APPLE__)
    @autoreleasepool {
        [NSApplication sharedApplication];
        [NSApp setActivationPolicy:NSApplicationActivationPolicyRegular];
        static int launched = 0;
        if (!launched) {
            [NSApp finishLaunching];
            launched = 1;
        }
        NSRect content = NSMakeRect(0, 0, (CGFloat)(w * scale), (CGFloat)(h * scale));
        NSUInteger style = NSWindowStyleMaskTitled | NSWindowStyleMaskClosable |
                           NSWindowStyleMaskMiniaturizable | NSWindowStyleMaskResizable;
        __nexa_gfx_nswin = [[NSWindow alloc] initWithContentRect:content
            styleMask:style backing:NSBackingStoreBuffered defer:NO];
        [__nexa_gfx_nswin setReleasedWhenClosed:NO];
        [__nexa_gfx_nswin setTitle:[NSString stringWithUTF8String:title.c_str()]];
        __nexa_gfx_nsview = [[__NexaGfxView alloc] initWithFrame:content];
        [__nexa_gfx_nswin setContentView:__nexa_gfx_nsview];
        __nexa_gfx_delegate = [[__NexaGfxDelegate alloc] init];
        [__nexa_gfx_nswin setDelegate:__nexa_gfx_delegate];
        [__nexa_gfx_nswin center];
        [__nexa_gfx_nswin makeKeyAndOrderFront:nil];
        [NSApp activateIgnoringOtherApps:YES];
        __nexa_g.ready = __nexa_gfx_nswin ? 1 : 0;
        if (!__nexa_g.ready) __nexa_g.closed = 1;
        return __nexa_g.ready;
    }
#elif defined(__linux__)
    XInitThreads();
    __nexa_g.dpy = XOpenDisplay(nullptr);
    if (!__nexa_g.dpy) {
        __nexa_g.closed = 1;
        return 0;
    }
    int scr = DefaultScreen(__nexa_g.dpy);
    __nexa_g.vis = DefaultVisual(__nexa_g.dpy, scr);
    __nexa_g.win = XCreateSimpleWindow(__nexa_g.dpy, RootWindow(__nexa_g.dpy, scr),
        80, 80, (unsigned)(w * scale), (unsigned)(h * scale), 1,
        BlackPixel(__nexa_g.dpy, scr), BlackPixel(__nexa_g.dpy, scr));
    XStoreName(__nexa_g.dpy, __nexa_g.win, title.c_str());
    Atom wm = XInternAtom(__nexa_g.dpy, "WM_DELETE_WINDOW", False);
    XSetWMProtocols(__nexa_g.dpy, __nexa_g.win, &wm, 1);
    __nexa_g.wm_delete = (int)wm;
    XSelectInput(__nexa_g.dpy, __nexa_g.win,
        ExposureMask | KeyPressMask | KeyReleaseMask | StructureNotifyMask |
        PointerMotionMask | ButtonPressMask | ButtonReleaseMask);
    __nexa_g.gc = DefaultGC(__nexa_g.dpy, scr);
    XMapWindow(__nexa_g.dpy, __nexa_g.win);
    XFlush(__nexa_g.dpy);
    __nexa_g.ready = 1;
    return 1;
#else
    (void)title;
    __nexa_g.closed = 1;
    return 0;
#endif
}

static int __nexa_gfx_resize(int w, int h, int scale) {
    if (!__nexa_g.ready || !__nexa_g.fb) return 0;
    __nexa_gfx_clamp_whs(&w, &h, &scale, __nexa_g.scale);
    if (w != __nexa_g.w || h != __nexa_g.h) {
        unsigned char* nfb = new unsigned char[(size_t)w * (size_t)h * 4];
        std::memset(nfb, 0, (size_t)w * (size_t)h * 4);
        int cw = w < __nexa_g.w ? w : __nexa_g.w;
        int ch = h < __nexa_g.h ? h : __nexa_g.h;
        for (int y = 0; y < ch; y++) {
            std::memcpy(nfb + (size_t)y * (size_t)w * 4,
                __nexa_g.fb + (size_t)y * (size_t)__nexa_g.w * 4, (size_t)cw * 4);
        }
        delete[] __nexa_g.fb;
        __nexa_g.fb = nfb;
    }
    __nexa_g.w = w;
    __nexa_g.h = h;
    __nexa_g.scale = scale;
    if (__nexa_g.mx >= w) __nexa_g.mx = w > 0 ? w - 1 : 0;
    if (__nexa_g.my >= h) __nexa_g.my = h > 0 ? h - 1 : 0;
    __nexa_gfx_apply_window_size(w, h, scale);
    return 1;
}

static int __nexa_gfx_width() {
    return __nexa_g.ready ? __nexa_g.w : 0;
}

static int __nexa_gfx_height() {
    return __nexa_g.ready ? __nexa_g.h : 0;
}

static int __nexa_gfx_scale() {
    return __nexa_g.ready ? __nexa_g.scale : 0;
}

static std::string __nexa_gfx_title_get() {
    return __nexa_g.title;
}

static int __nexa_gfx_title_set(const std::string& s) {
    __nexa_g.title = s;
    if (!__nexa_g.ready) return 0;
#ifdef __EMSCRIPTEN__
    EM_ASM({ document.title = UTF8ToString($0); }, s.c_str());
    return 1;
#elif defined(_WIN32)
    if (!__nexa_g.hwnd) return 0;
    return SetWindowTextA(__nexa_g.hwnd, s.c_str()) ? 1 : 0;
#elif defined(__APPLE__)
    if (!__nexa_gfx_nswin) return 0;
    [__nexa_gfx_nswin setTitle:[NSString stringWithUTF8String:s.c_str()]];
    return 1;
#elif defined(__linux__)
    if (!__nexa_g.dpy || !__nexa_g.win) return 0;
    XStoreName(__nexa_g.dpy, __nexa_g.win, s.c_str());
    XFlush(__nexa_g.dpy);
    return 1;
#else
    return 0;
#endif
}

static void __nexa_gfx_close() {
    __nexa_g.closed = 1;
    __nexa_gfx_free();
}

static void __nexa_gfx_poll() {
    if (!__nexa_g.ready) return;
#ifdef _WIN32
    MSG msg;
    while (PeekMessageA(&msg, nullptr, 0, 0, PM_REMOVE)) {
        if (msg.message == WM_QUIT) __nexa_g.closed = 1;
        TranslateMessage(&msg);
        DispatchMessageA(&msg);
    }
    if (__nexa_g.hwnd && !IsWindow(__nexa_g.hwnd)) __nexa_g.closed = 1;
#elif defined(__APPLE__)
    @autoreleasepool {
        NSEvent* ev;
        while ((ev = [NSApp nextEventMatchingMask:NSEventMaskAny
            untilDate:[NSDate distantPast]
            inMode:NSDefaultRunLoopMode
            dequeue:YES])) {
            [NSApp sendEvent:ev];
        }
    }
#elif defined(__linux__)
    while (__nexa_g.dpy && XPending(__nexa_g.dpy)) {
        XEvent ev;
        XNextEvent(__nexa_g.dpy, &ev);
        if (ev.type == ClientMessage && (int)ev.xclient.data.l[0] == __nexa_g.wm_delete) __nexa_g.closed = 1;
        if (ev.type == DestroyNotify) __nexa_g.closed = 1;
    }
#endif
    __nexa_gfx_mouse_refresh();
}

static int __nexa_gfx_closed() {
    return __nexa_g.closed;
}

static void __nexa_gfx_mouse_refresh() {
    if (!__nexa_g.ready) return;
#ifdef _WIN32
    if (!__nexa_g.hwnd) return;
    POINT p;
    if (!GetCursorPos(&p)) return;
    if (!ScreenToClient(__nexa_g.hwnd, &p)) return;
    RECT rc;
    if (!GetClientRect(__nexa_g.hwnd, &rc)) return;
    int ox = 0, oy = 0;
    int inside = __nexa_gfx_map_mouse((int)p.x, (int)p.y, (int)rc.right, (int)rc.bottom, &ox, &oy);
    int swap = GetSystemMetrics(SM_SWAPBUTTON);
    int leftVk = swap ? VK_RBUTTON : VK_LBUTTON;
    int rightVk = swap ? VK_LBUTTON : VK_RBUTTON;
    int left = (GetAsyncKeyState(leftVk) & 0x8000) ? 1 : 0;
    int right = (GetAsyncKeyState(rightVk) & 0x8000) ? 1 : 0;
    int middle = (GetAsyncKeyState(VK_MBUTTON) & 0x8000) ? 1 : 0;
    __nexa_gfx_mouse_apply(ox, oy, inside, left, middle, right);
#elif defined(__APPLE__)
    if (!__nexa_gfx_nswin) return;
    NSPoint s = [NSEvent mouseLocation];
    NSRect wr = [__nexa_gfx_nswin frame];
    NSRect cr = [__nexa_gfx_nswin contentRectForFrameRect:wr];
    int cw = (int)cr.size.width;
    int ch = (int)cr.size.height;
    int px = (int)(s.x - cr.origin.x);
    int py = (int)(cr.size.height - (s.y - cr.origin.y));
    int ox = 0, oy = 0;
    int inside = __nexa_gfx_map_mouse(px, py, cw, ch, &ox, &oy);
    NSUInteger bt = [NSEvent pressedMouseButtons];
    __nexa_gfx_mouse_apply(ox, oy, inside, (bt & 1) != 0, (bt & 4) != 0, (bt & 2) != 0);
#elif defined(__linux__)
    if (!__nexa_g.dpy || !__nexa_g.win) return;
    Window root = 0, child = 0;
    int rx = 0, ry = 0, wx = 0, wy = 0;
    unsigned mask = 0;
    if (!XQueryPointer(__nexa_g.dpy, __nexa_g.win, &root, &child, &rx, &ry, &wx, &wy, &mask)) return;
    XWindowAttributes wa;
    if (!XGetWindowAttributes(__nexa_g.dpy, __nexa_g.win, &wa)) return;
    int ox = 0, oy = 0;
    int inside = __nexa_gfx_map_mouse(wx, wy, wa.width, wa.height, &ox, &oy);
    __nexa_gfx_mouse_apply(ox, oy, inside,
        (mask & Button1Mask) != 0, (mask & Button2Mask) != 0, (mask & Button3Mask) != 0);
#endif
}

static int __nexa_gfx_mouse_x() {
    if (!__nexa_g.ready) return 0;
    __nexa_gfx_mouse_refresh();
    return __nexa_g.mx;
}

static int __nexa_gfx_mouse_y() {
    if (!__nexa_g.ready) return 0;
    __nexa_gfx_mouse_refresh();
    return __nexa_g.my;
}

static int __nexa_gfx_mouse(const std::string& name) {
    if (!__nexa_g.ready) return 0;
    __nexa_gfx_mouse_refresh();
    std::string s = name;
    for (char& c : s) if (c >= 'A' && c <= 'Z') c = (char)(c - 'A' + 'a');
    if (s == "left" || s == "lmb" || s == "l") return __nexa_g.mlb;
    if (s == "right" || s == "rmb" || s == "r") return __nexa_g.mrb;
    if (s == "middle" || s == "mmb" || s == "m") return __nexa_g.mmb;
    return 0;
}

static void __nexa_gfx_put(int i, unsigned char R, unsigned char G, unsigned char B) {
#ifdef _WIN32
    __nexa_g.fb[i + 0] = B;
    __nexa_g.fb[i + 1] = G;
    __nexa_g.fb[i + 2] = R;
    __nexa_g.fb[i + 3] = 255;
#else
    __nexa_g.fb[i + 0] = R;
    __nexa_g.fb[i + 1] = G;
    __nexa_g.fb[i + 2] = B;
    __nexa_g.fb[i + 3] = 255;
#endif
}

static void __nexa_gfx_clear(int r, int g, int b) {
    if (!__nexa_g.fb) return;
    unsigned char R = (unsigned char)(r < 0 ? 0 : (r > 255 ? 255 : r));
    unsigned char G = (unsigned char)(g < 0 ? 0 : (g > 255 ? 255 : g));
    unsigned char B = (unsigned char)(b < 0 ? 0 : (b > 255 ? 255 : b));
    int n = __nexa_g.w * __nexa_g.h;
    for (int i = 0; i < n; i++) __nexa_gfx_put(i * 4, R, G, B);
}

static void __nexa_gfx_plot(int x, int y, int r, int g, int b) {
    if (!__nexa_g.fb) return;
    if (x < 0 || y < 0 || x >= __nexa_g.w || y >= __nexa_g.h) return;
    unsigned char R = (unsigned char)(r < 0 ? 0 : (r > 255 ? 255 : r));
    unsigned char G = (unsigned char)(g < 0 ? 0 : (g > 255 ? 255 : g));
    unsigned char B = (unsigned char)(b < 0 ? 0 : (b > 255 ? 255 : b));
    __nexa_gfx_put((y * __nexa_g.w + x) * 4, R, G, B);
}

static int __nexa_gfx_get(int x, int y) {
    if (!__nexa_g.fb || !__nexa_g.ready) return -1;
    if (x < 0 || y < 0 || x >= __nexa_g.w || y >= __nexa_g.h) return -1;
    const unsigned char* p = __nexa_g.fb + ((size_t)y * (size_t)__nexa_g.w + (size_t)x) * 4;
#ifdef _WIN32
    int r = p[2];
    int g = p[1];
    int b = p[0];
#else
    int r = p[0];
    int g = p[1];
    int b = p[2];
#endif
    return (r << 16) | (g << 8) | b;
}

static unsigned char __nexa_gfx_u8(int v) {
    if (v < 0) return 0;
    if (v > 255) return 255;
    return (unsigned char)v;
}

static void __nexa_gfx_fill(int x, int y, int w, int h, int r, int g, int b) {
    if (!__nexa_g.fb) return;
    if (w < 0) { x += w; w = -w; }
    if (h < 0) { y += h; h = -h; }
    if (w < 1 || h < 1) return;
    int x0 = x;
    int y0 = y;
    int x1 = x + w;
    int y1 = y + h;
    if (x0 < 0) x0 = 0;
    if (y0 < 0) y0 = 0;
    if (x1 > __nexa_g.w) x1 = __nexa_g.w;
    if (y1 > __nexa_g.h) y1 = __nexa_g.h;
    if (x0 >= x1 || y0 >= y1) return;
    unsigned char R = __nexa_gfx_u8(r);
    unsigned char G = __nexa_gfx_u8(g);
    unsigned char B = __nexa_gfx_u8(b);
    for (int yy = y0; yy < y1; yy++) {
        int row = yy * __nexa_g.w;
        for (int xx = x0; xx < x1; xx++) {
            __nexa_gfx_put((row + xx) * 4, R, G, B);
        }
    }
}

static int __nexa_gfx_outcode(int x, int y) {
    int c = 0;
    if (x < 0) c |= 1;
    else if (x >= __nexa_g.w) c |= 2;
    if (y < 0) c |= 4;
    else if (y >= __nexa_g.h) c |= 8;
    return c;
}

static int __nexa_gfx_clip_line(int* x0, int* y0, int* x1, int* y1) {
    if (__nexa_g.w < 1 || __nexa_g.h < 1) return 0;
    int xmin = 0, ymin = 0, xmax = __nexa_g.w - 1, ymax = __nexa_g.h - 1;
    int c0 = __nexa_gfx_outcode(*x0, *y0);
    int c1 = __nexa_gfx_outcode(*x1, *y1);
    for (;;) {
        if (!(c0 | c1)) return 1;
        if (c0 & c1) return 0;
        int c = c0 ? c0 : c1;
        int dx = *x1 - *x0;
        int dy = *y1 - *y0;
        long long x = 0, y = 0;
        if (c & 8) {
            x = (long long)*x0 + (dy != 0 ? (long long)dx * (ymax - *y0) / dy : 0);
            y = ymax;
        } else if (c & 4) {
            x = (long long)*x0 + (dy != 0 ? (long long)dx * (ymin - *y0) / dy : 0);
            y = ymin;
        } else if (c & 2) {
            y = (long long)*y0 + (dx != 0 ? (long long)dy * (xmax - *x0) / dx : 0);
            x = xmax;
        } else {
            y = (long long)*y0 + (dx != 0 ? (long long)dy * (xmin - *x0) / dx : 0);
            x = xmin;
        }
        if (x < -2147483647ll) x = -2147483647ll;
        if (x > 2147483647ll) x = 2147483647ll;
        if (y < -2147483647ll) y = -2147483647ll;
        if (y > 2147483647ll) y = 2147483647ll;
        if (c == c0) {
            *x0 = (int)x;
            *y0 = (int)y;
            c0 = __nexa_gfx_outcode(*x0, *y0);
        } else {
            *x1 = (int)x;
            *y1 = (int)y;
            c1 = __nexa_gfx_outcode(*x1, *y1);
        }
    }
}

static void __nexa_gfx_line(int x0, int y0, int x1, int y1, int r, int g, int b) {
    if (!__nexa_g.fb) return;
    if (!__nexa_gfx_clip_line(&x0, &y0, &x1, &y1)) return;
    unsigned char R = __nexa_gfx_u8(r);
    unsigned char G = __nexa_gfx_u8(g);
    unsigned char B = __nexa_gfx_u8(b);
    int dx = x1 - x0;
    if (dx < 0) dx = -dx;
    int dy = y1 - y0;
    if (dy < 0) dy = -dy;
    int sx = x0 < x1 ? 1 : -1;
    int sy = y0 < y1 ? 1 : -1;
    int err = dx - dy;
    int x = x0;
    int y = y0;
    for (;;) {
        if (x >= 0 && y >= 0 && x < __nexa_g.w && y < __nexa_g.h) {
            __nexa_gfx_put((y * __nexa_g.w + x) * 4, R, G, B);
        }
        if (x == x1 && y == y1) break;
        int e2 = err + err;
        if (e2 > -dy) { err -= dy; x += sx; }
        if (e2 < dx) { err += dx; y += sy; }
    }
}

// 5x7, columns left-to-right, bit 0 = top. Printable ASCII 32..126.
static const unsigned char __nexa_gfx_font5x7[95][5] = {
    {0x00,0x00,0x00,0x00,0x00}, {0x00,0x00,0x5F,0x00,0x00}, {0x00,0x07,0x00,0x07,0x00},
    {0x14,0x7F,0x14,0x7F,0x14}, {0x24,0x2A,0x7F,0x2A,0x12}, {0x23,0x13,0x08,0x64,0x62},
    {0x36,0x49,0x56,0x20,0x50}, {0x00,0x08,0x07,0x03,0x00}, {0x00,0x1C,0x22,0x41,0x00},
    {0x00,0x41,0x22,0x1C,0x00}, {0x2A,0x1C,0x7F,0x1C,0x2A}, {0x08,0x08,0x3E,0x08,0x08},
    {0x00,0x80,0x70,0x30,0x00}, {0x08,0x08,0x08,0x08,0x08}, {0x00,0x00,0x60,0x60,0x00},
    {0x20,0x10,0x08,0x04,0x02}, {0x3E,0x51,0x49,0x45,0x3E}, {0x00,0x42,0x7F,0x40,0x00},
    {0x72,0x49,0x49,0x49,0x46}, {0x21,0x41,0x49,0x4D,0x33}, {0x18,0x14,0x12,0x7F,0x10},
    {0x27,0x45,0x45,0x45,0x39}, {0x3C,0x4A,0x49,0x49,0x31}, {0x41,0x21,0x11,0x09,0x07},
    {0x36,0x49,0x49,0x49,0x36}, {0x46,0x49,0x49,0x29,0x1E}, {0x00,0x00,0x14,0x00,0x00},
    {0x00,0x40,0x34,0x00,0x00}, {0x00,0x08,0x14,0x22,0x41}, {0x14,0x14,0x14,0x14,0x14},
    {0x00,0x41,0x22,0x14,0x08}, {0x02,0x01,0x59,0x09,0x06}, {0x3E,0x41,0x5D,0x59,0x4E},
    {0x7C,0x12,0x11,0x12,0x7C}, {0x7F,0x49,0x49,0x49,0x36}, {0x3E,0x41,0x41,0x41,0x22},
    {0x7F,0x41,0x41,0x22,0x1C}, {0x7F,0x49,0x49,0x49,0x41}, {0x7F,0x09,0x09,0x09,0x01},
    {0x3E,0x41,0x41,0x51,0x73}, {0x7F,0x08,0x08,0x08,0x7F}, {0x00,0x41,0x7F,0x41,0x00},
    {0x20,0x40,0x41,0x3F,0x01}, {0x7F,0x08,0x14,0x22,0x41}, {0x7F,0x40,0x40,0x40,0x40},
    {0x7F,0x02,0x1C,0x02,0x7F}, {0x7F,0x04,0x08,0x10,0x7F}, {0x3E,0x41,0x41,0x41,0x3E},
    {0x7F,0x09,0x09,0x09,0x06}, {0x3E,0x41,0x51,0x21,0x5E}, {0x7F,0x09,0x19,0x29,0x46},
    {0x26,0x49,0x49,0x49,0x32}, {0x01,0x01,0x7F,0x01,0x01}, {0x3F,0x40,0x40,0x40,0x3F},
    {0x1F,0x20,0x40,0x20,0x1F}, {0x3F,0x40,0x38,0x40,0x3F}, {0x63,0x14,0x08,0x14,0x63},
    {0x03,0x04,0x78,0x04,0x03}, {0x61,0x51,0x49,0x45,0x43}, {0x00,0x7F,0x41,0x41,0x41},
    {0x02,0x04,0x08,0x10,0x20}, {0x00,0x41,0x41,0x41,0x7F}, {0x04,0x02,0x01,0x02,0x04},
    {0x40,0x40,0x40,0x40,0x40}, {0x00,0x03,0x07,0x08,0x00}, {0x20,0x54,0x54,0x54,0x78},
    {0x7F,0x48,0x44,0x44,0x38}, {0x38,0x44,0x44,0x44,0x20}, {0x38,0x44,0x44,0x48,0x7F},
    {0x38,0x54,0x54,0x54,0x18}, {0x08,0x7E,0x09,0x01,0x02}, {0x18,0xA4,0xA4,0xA4,0x7C},
    {0x7F,0x08,0x04,0x04,0x78}, {0x00,0x44,0x7D,0x40,0x00}, {0x40,0x80,0x84,0x7D,0x00},
    {0x7F,0x10,0x28,0x44,0x00}, {0x00,0x41,0x7F,0x40,0x00}, {0x7C,0x04,0x78,0x04,0x78},
    {0x7C,0x08,0x04,0x04,0x78}, {0x38,0x44,0x44,0x44,0x38}, {0xFC,0x18,0x24,0x24,0x18},
    {0x18,0x24,0x24,0x18,0xFC}, {0x7C,0x08,0x04,0x04,0x08}, {0x48,0x54,0x54,0x54,0x24},
    {0x04,0x3F,0x44,0x40,0x20}, {0x3C,0x40,0x40,0x20,0x7C}, {0x1C,0x20,0x40,0x20,0x1C},
    {0x3C,0x40,0x30,0x40,0x3C}, {0x44,0x28,0x10,0x28,0x44}, {0x1C,0xA0,0xA0,0xA0,0x7C},
    {0x44,0x64,0x54,0x4C,0x44}, {0x00,0x08,0x36,0x41,0x00}, {0x00,0x00,0x77,0x00,0x00},
    {0x00,0x41,0x36,0x08,0x00}, {0x08,0x04,0x08,0x10,0x08}
};

static void __nexa_gfx_glyph(int x, int y, int ch, unsigned char R, unsigned char G, unsigned char B, int scale) {
    if (ch < 32 || ch > 126) ch = '?';
    if (scale < 1) scale = 1;
    const unsigned char* col = __nexa_gfx_font5x7[ch - 32];
    for (int i = 0; i < 5; i++) {
        unsigned char bits = col[i];
        for (int j = 0; j < 8; j++) {
            if (!(bits & (1u << j))) continue;
            for (int sy = 0; sy < scale; sy++) {
                int py = y + j * scale + sy;
                if (py < 0 || py >= __nexa_g.h) continue;
                int row = py * __nexa_g.w;
                for (int sx = 0; sx < scale; sx++) {
                    int px = x + i * scale + sx;
                    if (px < 0 || px >= __nexa_g.w) continue;
                    __nexa_gfx_put((row + px) * 4, R, G, B);
                }
            }
        }
    }
}

static int __nexa_gfx_text_scale() {
    int s = __nexa_g.text_scale;
    if (s < 1) s = 1;
    if (s > 64) s = 64;
    return s;
}

static int __nexa_gfx_text_size_set(int n) {
    if (n < 1) n = 1;
    if (n > 64) n = 64;
    __nexa_g.text_scale = n;
    return n;
}

static int __nexa_gfx_clamp_text_scale(int scale) {
    if (scale < 1) scale = __nexa_gfx_text_scale();
    if (scale > 64) scale = 64;
    return scale;
}

static void __nexa_gfx_text_dims(const std::string& s, int scale, int* out_w, int* out_h) {
    scale = __nexa_gfx_clamp_text_scale(scale);
    const int adv = 6 * scale;
    const int lh = 8 * scale;
    const int tab = 24 * scale;
    if (s.empty()) {
        if (out_w) *out_w = 0;
        if (out_h) *out_h = 0;
        return;
    }
    int cx = 0;
    int maxw = 0;
    int lines = 1;
    for (size_t i = 0; i < s.size(); i++) {
        unsigned char ch = (unsigned char)s[i];
        if (ch == '\n') {
            if (cx > maxw) maxw = cx;
            cx = 0;
            lines++;
            continue;
        }
        if (ch == '\t') {
            int next = ((cx / tab) + 1) * tab;
            cx = next;
            continue;
        }
        cx += adv;
    }
    if (cx > maxw) maxw = cx;
    if (out_w) *out_w = maxw;
    if (out_h) *out_h = lines * lh;
}

static int __nexa_gfx_text_width(const std::string& s, int scale) {
    int w = 0, h = 0;
    __nexa_gfx_text_dims(s, scale, &w, &h);
    return w;
}

static int __nexa_gfx_text_height(const std::string& s, int scale) {
    int w = 0, h = 0;
    __nexa_gfx_text_dims(s, scale, &w, &h);
    return h;
}

static int __nexa_gfx_text(int x, int y, const std::string& s, int r, int g, int b, int scale) {
    if (!__nexa_g.fb) return 0;
    if (scale < 1) scale = __nexa_gfx_text_scale();
    if (scale > 64) scale = 64;
    unsigned char R = __nexa_gfx_u8(r);
    unsigned char G = __nexa_gfx_u8(g);
    unsigned char B = __nexa_gfx_u8(b);
    int cx = x;
    int cy = y;
    int maxw = 0;
    const int adv = 6 * scale;
    const int lh = 8 * scale;
    const int tab = 24 * scale;
    for (size_t i = 0; i < s.size(); i++) {
        unsigned char ch = (unsigned char)s[i];
        if (ch == '\n') {
            int w = cx - x;
            if (w > maxw) maxw = w;
            cx = x;
            cy += lh;
            continue;
        }
        if (ch == '\t') {
            int cell = cx - x;
            if (cell < 0) cell = 0;
            int next = ((cell / tab) + 1) * tab;
            cx = x + next;
            continue;
        }
        __nexa_gfx_glyph(cx, cy, (int)ch, R, G, B, scale);
        cx += adv;
    }
    int w = cx - x;
    if (w > maxw) maxw = w;
    return maxw;
}

#if defined(__linux__) && !defined(__EMSCRIPTEN__)
static void __nexa_gfx_x11_present() {
    if (!__nexa_g.dpy || !__nexa_g.win || !__nexa_g.fb) return;
    XWindowAttributes wa;
    if (!XGetWindowAttributes(__nexa_g.dpy, __nexa_g.win, &wa)) return;
    int dw = wa.width > 0 ? wa.width : (__nexa_g.w * __nexa_g.scale);
    int dh = wa.height > 0 ? wa.height : (__nexa_g.h * __nexa_g.scale);
    if (dw < 1) dw = 1;
    if (dh < 1) dh = 1;
    if (dw > 8192) dw = 8192;
    if (dh > 8192) dh = 8192;
    if (!__nexa_g.xbuf || __nexa_g.xbw != dw || __nexa_g.xbh != dh) {
        if (__nexa_g.img) {
            __nexa_g.img->data = nullptr;
            XDestroyImage(__nexa_g.img);
            __nexa_g.img = nullptr;
        }
        delete[] __nexa_g.xbuf;
        __nexa_g.xbuf = new unsigned char[(size_t)dw * (size_t)dh * 4];
        __nexa_g.xbw = dw;
        __nexa_g.xbh = dh;
        int scr = DefaultScreen(__nexa_g.dpy);
        int depth = DefaultDepth(__nexa_g.dpy, scr);
        __nexa_g.img = XCreateImage(__nexa_g.dpy, __nexa_g.vis ? __nexa_g.vis : DefaultVisual(__nexa_g.dpy, scr),
            (unsigned)depth, ZPixmap, 0, (char*)__nexa_g.xbuf, (unsigned)dw, (unsigned)dh, 32, 0);
        if (!__nexa_g.img) return;
    }
    const int sw = __nexa_g.w;
    const int sh = __nexa_g.h;
    const int lsb = (__nexa_g.img->byte_order != MSBFirst);
    for (int y = 0; y < dh; y++) {
        int sy = y * sh / dh;
        if (sy >= sh) sy = sh - 1;
        for (int x = 0; x < dw; x++) {
            int sx = x * sw / dw;
            if (sx >= sw) sx = sw - 1;
            const unsigned char* s = __nexa_g.fb + (size_t)(sy * sw + sx) * 4;
            unsigned char* d = __nexa_g.xbuf + (size_t)(y * dw + x) * 4;
            if (lsb) {
                d[0] = s[2];
                d[1] = s[1];
                d[2] = s[0];
                d[3] = 255;
            } else {
                d[0] = 255;
                d[1] = s[0];
                d[2] = s[1];
                d[3] = s[2];
            }
        }
    }
    XPutImage(__nexa_g.dpy, __nexa_g.win, __nexa_g.gc, __nexa_g.img, 0, 0, 0, 0,
        (unsigned)dw, (unsigned)dh);
    XFlush(__nexa_g.dpy);
}
#endif

static void __nexa_gfx_present() {
    if (!__nexa_g.ready || !__nexa_g.fb) return;
#ifdef __EMSCRIPTEN__
    EM_ASM(({
        var c = Module['canvas'] || document.getElementById('canvas');
        if (!c) return;
        var ww = $0;
        var hh = $1;
        if (c.width !== ww) c.width = ww;
        if (c.height !== hh) c.height = hh;
        var ctx = c.getContext('2d');
        var img = ctx.createImageData(ww, hh);
        var src = HEAPU8.subarray($2, $2 + ww * hh * 4);
        img.data.set(src);
        ctx.putImageData(img, 0, 0);
    }), __nexa_g.w, __nexa_g.h, (int)(uintptr_t)__nexa_g.fb);
#elif defined(_WIN32)
    if (__nexa_g.hwnd) InvalidateRect(__nexa_g.hwnd, nullptr, FALSE);
    __nexa_gfx_poll();
#elif defined(__APPLE__)
    if (__nexa_gfx_nsview) [__nexa_gfx_nsview setNeedsDisplay:YES];
    if (__nexa_gfx_nswin) [__nexa_gfx_nswin displayIfNeeded];
    __nexa_gfx_poll();
#elif defined(__linux__)
    __nexa_gfx_x11_present();
    __nexa_gfx_poll();
#endif
}

#ifdef __APPLE__
static int __nexa_gfx_mac_held(unsigned short kc) {
    return CGEventSourceKeyState(kCGEventSourceStateHIDSystemState, kc) ? 1 : 0;
}
#endif

static int __nexa_gfx_vk(const std::string& name) {
    if (name.empty()) return 0;
    std::string s = name;
    for (char& c : s) if (c >= 'A' && c <= 'Z') c = (char)(c - 'A' + 'a');
#ifdef _WIN32
    if (s.size() == 1) {
        char c = s[0];
        if (c >= '0' && c <= '9') return GetAsyncKeyState((int)c) & 0x8000 ? 1 : 0;
        if (c >= 'a' && c <= 'z') return GetAsyncKeyState((int)(c - 'a' + 'A')) & 0x8000 ? 1 : 0;
    }
    if (s == "escape") return GetAsyncKeyState(VK_ESCAPE) & 0x8000 ? 1 : 0;
    if (s == "space") return GetAsyncKeyState(VK_SPACE) & 0x8000 ? 1 : 0;
    if (s == "enter") return GetAsyncKeyState(VK_RETURN) & 0x8000 ? 1 : 0;
    if (s == "up") return GetAsyncKeyState(VK_UP) & 0x8000 ? 1 : 0;
    if (s == "down") return GetAsyncKeyState(VK_DOWN) & 0x8000 ? 1 : 0;
    if (s == "left") return GetAsyncKeyState(VK_LEFT) & 0x8000 ? 1 : 0;
    if (s == "right") return GetAsyncKeyState(VK_RIGHT) & 0x8000 ? 1 : 0;
#elif defined(__EMSCRIPTEN__)
    auto down = [&](int code) -> int {
        return (code >= 0 && code < 512) ? __nexa_g.keys[code] : 0;
    };
    if (s.size() == 1) {
        char c = s[0];
        if (c >= '0' && c <= '9') return down(48 + (c - '0'));
        if (c >= 'a' && c <= 'z') return down(65 + (c - 'a'));
    }
    if (s == "escape") return down(27);
    if (s == "space") return down(32);
    if (s == "enter") return down(13);
    if (s == "up") return down(38);
    if (s == "down") return down(40);
    if (s == "left") return down(37);
    if (s == "right") return down(39);
#elif defined(__APPLE__)
    static const unsigned char letters[26] = {
        0x00, 0x0B, 0x08, 0x02, 0x0E, 0x03, 0x05, 0x04, 0x22, 0x26,
        0x28, 0x25, 0x2E, 0x2D, 0x1F, 0x23, 0x0C, 0x0F, 0x01, 0x11,
        0x20, 0x09, 0x0D, 0x07, 0x10, 0x06
    };
    static const unsigned char digits[10] = {
        0x1D, 0x12, 0x13, 0x14, 0x15, 0x17, 0x16, 0x1A, 0x1C, 0x19
    };
    if (s.size() == 1) {
        char c = s[0];
        if (c >= '0' && c <= '9') return __nexa_gfx_mac_held(digits[c - '0']);
        if (c >= 'a' && c <= 'z') return __nexa_gfx_mac_held(letters[c - 'a']);
    }
    if (s == "escape") return __nexa_gfx_mac_held(0x35);
    if (s == "space") return __nexa_gfx_mac_held(0x31);
    if (s == "enter") return __nexa_gfx_mac_held(0x24);
    if (s == "up") return __nexa_gfx_mac_held(0x7E);
    if (s == "down") return __nexa_gfx_mac_held(0x7D);
    if (s == "left") return __nexa_gfx_mac_held(0x7B);
    if (s == "right") return __nexa_gfx_mac_held(0x7C);
#elif defined(__linux__)
    if (!__nexa_g.dpy) return 0;
    char keys[32];
    XQueryKeymap(__nexa_g.dpy, keys);
    auto held = [&](KeySym ks) -> int {
        KeyCode kc = XKeysymToKeycode(__nexa_g.dpy, ks);
        if (!kc) return 0;
        return (keys[kc / 8] & (1 << (kc % 8))) ? 1 : 0;
    };
    if (s.size() == 1) {
        char c = s[0];
        if (held((KeySym)c) || held((KeySym)(c - 'a' + 'A'))) return 1;
        return 0;
    }
    if (s == "escape") return held(XK_Escape);
    if (s == "space") return held(XK_space);
    if (s == "enter") return held(XK_Return);
    if (s == "up") return held(XK_Up);
    if (s == "down") return held(XK_Down);
    if (s == "left") return held(XK_Left);
    if (s == "right") return held(XK_Right);
#endif
    return 0;
}

static int __nexa_gfx_key(const std::string& name) {
    if (!__nexa_g.ready) return 0;
    return __nexa_gfx_vk(name);
}
)NEXA_GFX";
}

}  // namespace nexa
