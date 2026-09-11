/* Xlib stand-in for the headless gfx tests (BOB-21).
 *
 * Every entry point fails or does nothing, so a gfx program linked against this
 * behaves exactly as it does on a machine with no display: gfx.open() returns 0
 * and every other gfx call is a no-op. That is the state Tests/gfx_input_cases.sh
 * asserts on. See X11/Xlib.h in this directory.
 */
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <cstdlib>
#include <cstring>


/* --- fake server state ---------------------------------------------------- */

enum { NEXA_STUB_WINDOW = 0x2101, NEXA_STUB_EVENTS = 64, NEXA_STUB_KEYS = 240 };

static int nexa_stub_display_works = 0;
static int nexa_stub_focused = 1;
static XEvent nexa_stub_queue[NEXA_STUB_EVENTS];
static char nexa_stub_text[NEXA_STUB_EVENTS][64];
static int nexa_stub_head = 0;
static int nexa_stub_tail = 0;
static char nexa_stub_keymap[32];
static KeySym nexa_stub_keysyms[NEXA_STUB_KEYS];
static int nexa_stub_keysym_count = 0;

/* A stable, collision-free keycode per keysym, handed out on first request.
   Codes start at 8 because 0 means "no such key" to XKeysymToKeycode callers. */
static int nexa_stub_code_for(KeySym ks) {
    for (int i = 0; i < nexa_stub_keysym_count; i++) {
        if (nexa_stub_keysyms[i] == ks) return 8 + i;
    }
    if (nexa_stub_keysym_count >= NEXA_STUB_KEYS) return 0;
    nexa_stub_keysyms[nexa_stub_keysym_count] = ks;
    return 8 + nexa_stub_keysym_count++;
}

static XEvent* nexa_stub_alloc(int type) {
    int next = (nexa_stub_tail + 1) % NEXA_STUB_EVENTS;
    if (next == nexa_stub_head) return 0;      /* full; drop */
    XEvent* e = &nexa_stub_queue[nexa_stub_tail];
    memset(e, 0, sizeof(*e));
    e->type = type;
    nexa_stub_text[nexa_stub_tail][0] = 0;
    nexa_stub_tail = next;
    return e;
}

void nexa_x11_stub_reset(int display_works) {
    nexa_stub_display_works = display_works;
    nexa_stub_focused = 1;
    nexa_stub_head = 0;
    nexa_stub_tail = 0;
    nexa_stub_keysym_count = 0;
    memset(nexa_stub_keymap, 0, sizeof(nexa_stub_keymap));
}

void nexa_x11_stub_set_focus(int focused) { nexa_stub_focused = focused ? 1 : 0; }

void nexa_x11_stub_push_button(int press, unsigned int button) {
    XEvent* e = nexa_stub_alloc(press ? ButtonPress : ButtonRelease);
    if (e) e->xbutton.button = button;
}

void nexa_x11_stub_push_key(const char* latin1_text) {
    int slot = nexa_stub_tail;
    XEvent* e = nexa_stub_alloc(KeyPress);
    if (!e) return;
    size_t n = latin1_text ? strlen(latin1_text) : 0;
    if (n > sizeof(nexa_stub_text[0]) - 1) n = sizeof(nexa_stub_text[0]) - 1;
    if (n) memcpy(nexa_stub_text[slot], latin1_text, n);
    nexa_stub_text[slot][n] = 0;
    e->xkey.keycode = (unsigned int)slot;      /* XLookupString finds the text again */
}

void nexa_x11_stub_set_key(KeySym ks, int down) {
    int code = nexa_stub_code_for(ks);
    if (!code) return;
    if (down) nexa_stub_keymap[code / 8] |= (char)(1 << (code % 8));
    else nexa_stub_keymap[code / 8] &= (char)~(1 << (code % 8));
}

/* --- Xlib ----------------------------------------------------------------- */

Display* XOpenDisplay(const char* name) {
    (void)name;
    if (!nexa_stub_display_works) return 0;
    static int fake;
    return (Display*)&fake;
}
int XCloseDisplay(Display* d) { (void)d; return 0; }
int XInitThreads(void) { return 1; }
int XFlush(Display* d) { (void)d; return 0; }
int XSync(Display* d, Bool discard) { (void)d; (void)discard; return 0; }
int XPending(Display* d) { (void)d; return nexa_stub_head != nexa_stub_tail; }
int XNextEvent(Display* d, XEvent* e) {
    (void)d;
    if (!e) return 0;
    if (nexa_stub_head == nexa_stub_tail) { memset(e, 0, sizeof(*e)); return 0; }
    *e = nexa_stub_queue[nexa_stub_head];
    nexa_stub_head = (nexa_stub_head + 1) % NEXA_STUB_EVENTS;
    return 0;
}
Status XSendEvent(Display* d, Window w, Bool p, long m, XEvent* e) {
    (void)d; (void)w; (void)p; (void)m; (void)e; return 0;
}
int XSelectInput(Display* d, Window w, long m) { (void)d; (void)w; (void)m; return 0; }
Window XCreateSimpleWindow(Display* d, Window parent, int x, int y,
                           unsigned int w, unsigned int h, unsigned int bw,
                           unsigned long border, unsigned long background) {
    (void)d; (void)parent; (void)x; (void)y; (void)w; (void)h; (void)bw;
    (void)border; (void)background;
    return nexa_stub_display_works ? (Window)NEXA_STUB_WINDOW : (Window)0;
}
int XDestroyWindow(Display* d, Window w) { (void)d; (void)w; return 0; }
int XMapWindow(Display* d, Window w) { (void)d; (void)w; return 0; }
int XResizeWindow(Display* d, Window w, unsigned int w2, unsigned int h) {
    (void)d; (void)w; (void)w2; (void)h; return 0;
}
int XStoreName(Display* d, Window w, const char* n) { (void)d; (void)w; (void)n; return 0; }
Atom XInternAtom(Display* d, const char* n, Bool o) { (void)d; (void)n; (void)o; return 0; }
Status XSetWMProtocols(Display* d, Window w, Atom* p, int c) {
    (void)d; (void)w; (void)p; (void)c; return 0;
}
int XChangeProperty(Display* d, Window w, Atom pr, Atom t, int f, int m,
                    const unsigned char* data, int n) {
    (void)d; (void)w; (void)pr; (void)t; (void)f; (void)m; (void)data; (void)n; return 0;
}
Status XGetWindowAttributes(Display* d, Window w, XWindowAttributes* a) {
    (void)d; (void)w; if (a) memset(a, 0, sizeof(*a)); return 0;
}
Bool XQueryPointer(Display* d, Window w, Window* root, Window* child,
                   int* rx, int* ry, int* wx, int* wy, unsigned int* mask) {
    (void)d; (void)w;
    if (root) *root = 0;
    if (child) *child = 0;
    if (rx) *rx = 0;
    if (ry) *ry = 0;
    if (wx) *wx = 0;
    if (wy) *wy = 0;
    if (mask) *mask = 0;
    return 0;
}
int XGetInputFocus(Display* d, Window* focus, int* revert) {
    (void)d;
    if (focus) *focus = nexa_stub_focused ? (Window)NEXA_STUB_WINDOW : (Window)0;
    if (revert) *revert = RevertToParent;
    return 0;
}
int XQueryKeymap(Display* d, char keys[32]) {
    (void)d;
    if (keys) memcpy(keys, nexa_stub_keymap, 32);
    return 0;
}
KeyCode XKeysymToKeycode(Display* d, KeySym ks) {
    (void)d;
    return (KeyCode)nexa_stub_code_for(ks);
}
int XLookupString(XKeyEvent* e, char* buf, int n, KeySym* ks, void* status) {
    (void)status;
    if (ks) *ks = 0;
    if (!e || !buf || n <= 0) return 0;
    unsigned int slot = e->keycode;
    if (slot >= (unsigned int)NEXA_STUB_EVENTS) return 0;
    int len = (int)strlen(nexa_stub_text[slot]);
    if (len > n) len = n;
    memcpy(buf, nexa_stub_text[slot], (size_t)len);
    return len;
}
XImage* XCreateImage(Display* d, Visual* v, unsigned int depth, int format,
                     int offset, char* data, unsigned int w, unsigned int h,
                     int pad, int bpl) {
    XImage* img;
    (void)d; (void)v; (void)format; (void)offset; (void)pad;
    img = (XImage*)calloc(1, sizeof(XImage));
    if (!img) return 0;
    img->width = (int)w;
    img->height = (int)h;
    img->depth = (int)depth;
    img->data = data;
    img->bits_per_pixel = 32;
    img->bytes_per_line = bpl ? bpl : (int)(w * 4);
    return img;
}
int XDestroyImage(XImage* img) { free(img); return 0; }
int XPutImage(Display* d, Drawable dr, GC gc, XImage* img,
              int sx, int sy, int dx, int dy, unsigned int w, unsigned int h) {
    (void)d; (void)dr; (void)gc; (void)img; (void)sx; (void)sy;
    (void)dx; (void)dy; (void)w; (void)h;
    return 0;
}
GC XDefaultGC(Display* d, int s) { (void)d; (void)s; return 0; }
int XDefaultScreen(Display* d) { (void)d; return 0; }
Visual* XDefaultVisual(Display* d, int s) { (void)d; (void)s; return 0; }
int XDefaultDepth(Display* d, int s) { (void)d; (void)s; return 24; }
Window XRootWindow(Display* d, int s) { (void)d; (void)s; return 0; }
unsigned long XBlackPixel(Display* d, int s) { (void)d; (void)s; return 0; }
unsigned long XWhitePixel(Display* d, int s) { (void)d; (void)s; return 0xFFFFFFUL; }
int XSetWMNormalHints(Display* d, Window w, XSizeHints* h) { (void)d; (void)w; (void)h; return 0; }
XSizeHints* XAllocSizeHints(void) { return (XSizeHints*)calloc(1, sizeof(XSizeHints)); }
