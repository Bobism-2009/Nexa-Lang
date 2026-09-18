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
typedef XID Pixmap;
typedef XID Cursor;
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

/* Visual classes, and the window attributes a window on a non-default visual
   has to be given. See XCreateWindow / XMatchVisualInfo below. */
#define StaticGray 0
#define GrayScale 1
#define StaticColor 2
#define PseudoColor 3
#define TrueColor 4
#define DirectColor 5

#define InputOutput 1
#define InputOnly 2

#define CWBackPixmap (1L << 0)
#define CWBackPixel (1L << 1)
#define CWBorderPixmap (1L << 2)
#define CWBorderPixel (1L << 3)
#define CWOverrideRedirect (1L << 9)
#define CWEventMask (1L << 11)
#define CWColormap (1L << 13)
#define CWCursor (1L << 14)

/* XWindowAttributes::map_state */
#define IsUnmapped 0
#define IsUnviewable 1
#define IsViewable 2

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
    unsigned long pixel;
    unsigned short red, green, blue;
    char flags;
    char pad;
} XColor;

/* Only the members the gfx runtime sets. A real XSetWindowAttributes has
   fifteen; XCreateWindow is told by its valuemask which ones to read, and the
   stub records the mask alongside them. */
typedef struct {
    Pixmap background_pixmap;
    unsigned long background_pixel;
    Pixmap border_pixmap;
    unsigned long border_pixel;
    long event_mask;
    Colormap colormap;
    Cursor cursor;
    Bool override_redirect;
} XSetWindowAttributes;

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
extern Window XCreateWindow(Display* d, Window parent, int x, int y,
                            unsigned int w, unsigned int h, unsigned int bw,
                            int depth, unsigned int c_class, Visual* visual,
                            unsigned long valuemask, XSetWindowAttributes* attrs);
extern Colormap XCreateColormap(Display* d, Window w, Visual* visual, int alloc);
extern int XFreeColormap(Display* d, Colormap cmap);
extern GC XCreateGC(Display* d, Drawable dr, unsigned long valuemask, void* values);
extern int XFreeGC(Display* d, GC gc);
extern int XDestroyWindow(Display* d, Window w);
extern int XMapWindow(Display* d, Window w);
extern int XUnmapWindow(Display* d, Window w);
extern int XMoveWindow(Display* d, Window w, int x, int y);
extern Bool XTranslateCoordinates(Display* d, Window src, Window dst,
                                  int sx, int sy, int* dx, int* dy, Window* child);
extern int XResizeWindow(Display* d, Window w, unsigned int w2, unsigned int h);
extern int XStoreName(Display* d, Window w, const char* name);
extern Atom XInternAtom(Display* d, const char* name, Bool only_if_exists);
extern Status XSetWMProtocols(Display* d, Window w, Atom* protocols, int count);
extern int XChangeProperty(Display* d, Window w, Atom prop, Atom type, int format,
                           int mode, const unsigned char* data, int nelements);
extern Pixmap XCreateBitmapFromData(Display* d, Drawable dr, const char* data,
                                    unsigned int w, unsigned int h);
extern Cursor XCreatePixmapCursor(Display* d, Pixmap source, Pixmap mask,
                                  XColor* fg, XColor* bg, unsigned int x, unsigned int y);
extern int XDefineCursor(Display* d, Window w, Cursor c);
extern int XUndefineCursor(Display* d, Window w);
extern int XFreeCursor(Display* d, Cursor c);
extern int XFreePixmap(Display* d, Pixmap p);
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

/* The window as the fake server sees it. Both are off by default, which is
 * the state the input tests want: XGetWindowAttributes fails, the way it does
 * for a window that was never created.
 *   nexa_x11_stub_set_mapped   XGetWindowAttributes succeeds and reports this
 *                              map_state (IsViewable / IsUnmapped)
 *   nexa_x11_stub_set_origin   what XTranslateCoordinates reports for the
 *                              window's own 0,0 in root coordinates
 */
extern void nexa_x11_stub_set_mapped(int map_state);
extern void nexa_x11_stub_set_origin(int x, int y);

/* Whether the fake server has a depth-32 TrueColor visual to offer. On by
 * default, which is what a compositing desktop looks like; turning it off is
 * how a test drives the fallback in gfx.open -- the screen where an alpha
 * channel is not on offer and nothing is said about it. */
extern void nexa_x11_stub_set_argb_visual(int available);

/* The window as it was asked for. XCreateSimpleWindow records a depth of 0 and
 * no visual, the way a window that borrows the screen's does; XCreateWindow
 * records what it was handed.
 *   _window_depth     the depth argument
 *   _window_class     InputOutput / InputOnly
 *   _window_visual_id the VisualID of the visual, or 0 for none
 *   _window_mask      the XCreateWindow valuemask
 *   _window_colormap  the colormap in the attributes, when CWColormap is set
 *   _window_border_pixel  the border pixel, when CWBorderPixel is set. Leaving
 *                     it out of the mask is a BadMatch on a non-default
 *                     visual, so "was it named at all" is the thing to check.
 *   _colormaps_made / _colormaps_freed   XCreateColormap / XFreeColormap
 *   _gcs_made / _gcs_freed               XCreateGC / XFreeGC. A GC must share
 *                     its drawable's depth, so a 32-bit window needs its own.
 *   _image_depth      the depth of the last XCreateImage, which has to agree
 *                     with the window's.
 *   _image_pixel(i)   the i'th 32-bit word of the last XPutImage's data, so a
 *                     test can read the alpha the runtime put on the wire. */
extern int nexa_x11_stub_window_depth(void);
extern int nexa_x11_stub_window_class(void);
extern unsigned long nexa_x11_stub_window_visual_id(void);
extern unsigned long nexa_x11_stub_window_mask(void);
extern unsigned long nexa_x11_stub_window_colormap(void);
extern unsigned long nexa_x11_stub_window_border_pixel(void);
extern int nexa_x11_stub_colormaps_made(void);
extern int nexa_x11_stub_colormaps_freed(void);
extern int nexa_x11_stub_gcs_made(void);
extern int nexa_x11_stub_gcs_freed(void);
extern int nexa_x11_stub_image_depth(void);
extern unsigned long nexa_x11_stub_image_pixel(int i);

/* Atom ids the fake server hands out for interned names. They start above
 * every predefined atom in X11/Xatom.h so that an id which is XA_ATOM is
 * XA_ATOM and not the fourth name some test happened to intern. */
#define NEXA_STUB_ATOM_BASE 1024

/* What the runtime asked the server to do, for tests that assert on the
 * request rather than on a pixel. The property accessors describe one
 * XChangeProperty; the message accessors the last XSendEvent; the call log
 * records XChangeProperty / XMapWindow / XUnmapWindow / XMoveWindow /
 * XSendEvent in the order they arrived. All are cleared by
 * nexa_x11_stub_reset.
 *
 * A single gfx call can write more than one property -- gfx.borderless writes
 * the Motif hint, and the _NET_WM_STATE that has to survive its remap -- so
 * the writes are all kept and the readers report one of them:
 *   _select(name)  report the last write of that property, and say whether
 *                  there was one; _select(0) goes back to the last write of
 *                  any property, which is where every reader starts
 *   _writes()      how many writes have been recorded since the last clear
 *   _type()        the property's type atom, for the ones whose type is not
 *                  their own atom -- a list of atoms is XA_ATOM
 *   _word_name(i)  the i'th word read back as an atom name, which is how the
 *                  states in a _NET_WM_STATE list are told apart */
extern const char* nexa_x11_stub_property_name(void);
extern int nexa_x11_stub_property_type_matches(void);
extern unsigned long nexa_x11_stub_property_type(void);
extern int nexa_x11_stub_property_format(void);
extern int nexa_x11_stub_property_count(void);
extern long nexa_x11_stub_property_word(int i);
extern const char* nexa_x11_stub_property_word_name(int i);
extern int nexa_x11_stub_property_writes(void);
extern int nexa_x11_stub_property_select(const char* name);

/* The last XSendEvent. _NET_WM_STATE changes are asked for with a
 * ClientMessage to the root window and nothing else -- no property, no reply
 * -- so the message is the whole of the request.
 *   _is_client    the event was a ClientMessage at all
 *   _name         the name of the message_type atom
 *   _word(i)      data.l[i]
 *   _word_name(i) that word read back as an atom name, which is how the state
 *                 in data.l[1] is told apart
 *   _window       the window the message is about (not the one it was sent to)
 *   _target       the window XSendEvent was pointed at -- the root
 *   _mask, _propagate  the rest of the XSendEvent call */
extern int nexa_x11_stub_message_is_client(void);
extern const char* nexa_x11_stub_message_name(void);
extern int nexa_x11_stub_message_format(void);
extern long nexa_x11_stub_message_word(int i);
extern const char* nexa_x11_stub_message_word_name(int i);
extern unsigned long nexa_x11_stub_message_window(void);
extern unsigned long nexa_x11_stub_message_target(void);
extern long nexa_x11_stub_message_mask(void);
extern int nexa_x11_stub_message_propagate(void);

#define NEXA_STUB_CALL_MAP 1
#define NEXA_STUB_CALL_UNMAP 2
#define NEXA_STUB_CALL_MOVE 3
#define NEXA_STUB_CALL_SEND 4
#define NEXA_STUB_CALL_PROP 5
extern void nexa_x11_stub_calls_clear(void);
extern int nexa_x11_stub_call_count(void);
extern int nexa_x11_stub_call(int i);
extern int nexa_x11_stub_move_x(void);
extern int nexa_x11_stub_move_y(void);

#ifdef __cplusplus
}
#endif

#endif
