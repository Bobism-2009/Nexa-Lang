/* Xlib stand-in for the headless gfx tests (BOB-21).
 *
 * Just enough of Xlib's surface to compile and link the X11 branch of the gfx
 * runtime on a machine with no libx11-dev and no display. It is NOT an X11
 * implementation: XOpenDisplay always fails, which is exactly the state the
 * tests want, because the input bookkeeping under test (the typed-text filter,
 * the wheel accumulator, the key edge detector) is backend-independent and the
 * closed-window no-ops are half of what the tests assert.
 *
 * See Tests/gfx_input_cases.sh. Nothing outside Tests/ includes this.
 */
#ifndef NEXA_TEST_X11_XLIB_H
#define NEXA_TEST_X11_XLIB_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct _XDisplay Display;
typedef unsigned long XID;
typedef XID Window;
typedef XID Drawable;
typedef XID Colormap;
typedef unsigned long Atom;
typedef unsigned long VisualID;
typedef unsigned long Time;
typedef unsigned long KeySym;
typedef unsigned char KeyCode;
typedef int Bool;
typedef int Status;
typedef struct _XGC* GC;
typedef struct { VisualID visualid; int c_class; unsigned long red_mask, green_mask, blue_mask; int bits_per_rgb; int map_entries; } Visual;
typedef struct { int x, y, width, height; } Screen;

#define False 0
#define True 1
#define None 0L
#define AllocNone 0

#define ZPixmap 2
#define XYPixmap 1

#define KeyPress 2
#define KeyRelease 3
#define ButtonPress 4
#define ButtonRelease 5
#define MotionNotify 6
#define Expose 12
#define DestroyNotify 17
#define ConfigureNotify 22
#define ClientMessage 33

#define NoEventMask 0L
#define KeyPressMask (1L << 0)
#define KeyReleaseMask (1L << 1)
#define ButtonPressMask (1L << 2)
#define ButtonReleaseMask (1L << 3)
#define PointerMotionMask (1L << 6)
#define ExposureMask (1L << 15)
#define StructureNotifyMask (1L << 17)
#define SubstructureNotifyMask (1L << 19)
#define SubstructureRedirectMask (1L << 20)

#define Button1Mask (1 << 8)
#define Button2Mask (1 << 9)
#define Button3Mask (1 << 10)

#define PropModeReplace 0
#define RevertToParent 2

#define LSBFirst 0
#define MSBFirst 1

#define _NET_WM_STATE_REMOVE 0
#define _NET_WM_STATE_ADD 1

typedef struct {
    int type;
    unsigned long serial;
    Bool send_event;
    Display* display;
    Window window;
    Window root;
    Window subwindow;
    Time time;
    int x, y;
    int x_root, y_root;
    unsigned int state;
    unsigned int keycode;
    Bool same_screen;
} XKeyEvent;

typedef struct {
    int type;
    unsigned long serial;
    Bool send_event;
    Display* display;
    Window window;
    Window root;
    Window subwindow;
    Time time;
    int x, y;
    int x_root, y_root;
    unsigned int state;
    unsigned int button;
    Bool same_screen;
} XButtonEvent;

typedef struct {
    int type;
    unsigned long serial;
    Bool send_event;
    Display* display;
    Window window;
    Atom message_type;
    int format;
    union {
        char b[20];
        short s[10];
        long l[5];
    } data;
} XClientMessageEvent;

typedef struct {
    int type;
    unsigned long serial;
    Bool send_event;
    Display* display;
    Window event;
    Window window;
} XDestroyWindowEvent;

typedef struct {
    int type;
    unsigned long serial;
    Bool send_event;
    Display* display;
    Window window;
    int x, y, width, height;
    int count;
} XExposeEvent;

typedef struct {
    int type;
    unsigned long serial;
    Bool send_event;
    Display* display;
    Window event;
    Window window;
    int x, y, width, height;
    int border_width;
    Window above;
    Bool override_redirect;
} XConfigureEvent;

typedef union _XEvent {
    int type;
    XKeyEvent xkey;
    XButtonEvent xbutton;
    XClientMessageEvent xclient;
    XDestroyWindowEvent xdestroywindow;
    XExposeEvent xexpose;
    XConfigureEvent xconfigure;
    long pad[24];
} XEvent;

typedef struct {
    int x, y;
    int width, height;
    int border_width;
    int depth;
    Visual* visual;
    Window root;
    int c_class;
    int map_state;
} XWindowAttributes;

typedef struct _XImage {
    int width, height;
    int xoffset;
    int format;
    char* data;
    int byte_order;
    int bitmap_unit;
    int bitmap_bit_order;
    int bitmap_pad;
    int depth;
    int bytes_per_line;
    int bits_per_pixel;
    unsigned long red_mask, green_mask, blue_mask;
    struct funcs { int (*destroy_image)(struct _XImage*); } f;
} XImage;

extern Display* XOpenDisplay(const char* name);
extern int XCloseDisplay(Display* d);
extern int XInitThreads(void);
extern int XFlush(Display* d);
extern int XSync(Display* d, Bool discard);
extern int XPending(Display* d);
extern int XNextEvent(Display* d, XEvent* e);
extern Status XSendEvent(Display* d, Window w, Bool prop, long mask, XEvent* e);
extern int XSelectInput(Display* d, Window w, long mask);
extern Window XCreateSimpleWindow(Display* d, Window parent, int x, int y,
                                  unsigned int w, unsigned int h, unsigned int bw,
                                  unsigned long border, unsigned long background);
extern int XDestroyWindow(Display* d, Window w);
extern int XMapWindow(Display* d, Window w);
extern int XResizeWindow(Display* d, Window w, unsigned int w2, unsigned int h);
extern int XStoreName(Display* d, Window w, const char* name);
extern Atom XInternAtom(Display* d, const char* name, Bool only_if_exists);
extern Status XSetWMProtocols(Display* d, Window w, Atom* protocols, int count);
extern int XChangeProperty(Display* d, Window w, Atom prop, Atom type, int format,
                           int mode, const unsigned char* data, int nelements);
extern Status XGetWindowAttributes(Display* d, Window w, XWindowAttributes* a);
extern Bool XQueryPointer(Display* d, Window w, Window* root, Window* child,
                          int* rx, int* ry, int* wx, int* wy, unsigned int* mask);
extern int XGetInputFocus(Display* d, Window* focus, int* revert);
extern int XQueryKeymap(Display* d, char keys[32]);
extern KeyCode XKeysymToKeycode(Display* d, KeySym ks);
extern int XLookupString(XKeyEvent* e, char* buf, int n, KeySym* ks, void* status);
extern XImage* XCreateImage(Display* d, Visual* v, unsigned int depth, int format,
                            int offset, char* data, unsigned int w, unsigned int h,
                            int pad, int bytes_per_line);
extern int XDestroyImage(XImage* img);
extern int XPutImage(Display* d, Drawable dr, GC gc, XImage* img,
                     int sx, int sy, int dx, int dy, unsigned int w, unsigned int h);
extern GC XDefaultGC(Display* d, int screen);
extern int XDefaultScreen(Display* d);
extern Visual* XDefaultVisual(Display* d, int screen);
extern int XDefaultDepth(Display* d, int screen);
extern Window XRootWindow(Display* d, int screen);
extern unsigned long XBlackPixel(Display* d, int screen);
extern unsigned long XWhitePixel(Display* d, int screen);

#define DefaultScreen(d) XDefaultScreen(d)
#define DefaultVisual(d, s) XDefaultVisual((d), (s))
#define DefaultDepth(d, s) XDefaultDepth((d), (s))
#define DefaultGC(d, s) XDefaultGC((d), (s))
#define RootWindow(d, s) XRootWindow((d), (s))
#define DefaultRootWindow(d) XRootWindow((d), XDefaultScreen(d))
#define BlackPixel(d, s) XBlackPixel((d), (s))
#define WhitePixel(d, s) XWhitePixel((d), (s))

/* --- test control -----------------------------------------------------------
 * Not part of Xlib. Tests/gfx_input_semantics.cpp uses these to put the fake
 * server into a known state and feed the runtime real X events, so the X11
 * branch of gfx.poll() is exercised rather than merely compiled.
 *
 * By default XOpenDisplay fails, which is what a plain gfx program linked
 * against this stub should see. nexa_x11_stub_reset(1) turns the display on.
 */
extern void nexa_x11_stub_reset(int display_works);
extern void nexa_x11_stub_set_focus(int focused);
extern void nexa_x11_stub_push_button(int press, unsigned int button);
extern void nexa_x11_stub_push_key(const char* latin1_text);
extern void nexa_x11_stub_set_key(KeySym ks, int down);

#ifdef __cplusplus
}
#endif

#endif
