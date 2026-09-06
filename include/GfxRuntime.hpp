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
    unsigned char* fb;
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
#endif

static int __nexa_gfx_open(const std::string& title, int w, int h, int scale) {
    if (w < 1) w = 1;
    if (h < 1) h = 1;
    if (w > 2048) w = 2048;
    if (h > 2048) h = 2048;
    if (scale < 1) scale = 12;
    if (scale > 64) scale = 64;
    __nexa_gfx_free();
    __nexa_g.w = w;
    __nexa_g.h = h;
    __nexa_g.scale = scale;
    __nexa_g.closed = 0;
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
    XSelectInput(__nexa_g.dpy, __nexa_g.win, ExposureMask | KeyPressMask | KeyReleaseMask | StructureNotifyMask);
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
}

static int __nexa_gfx_closed() {
    return __nexa_g.closed;
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
