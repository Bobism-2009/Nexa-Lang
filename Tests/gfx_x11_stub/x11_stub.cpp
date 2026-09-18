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
static int nexa_stub_map_state = -1;         /* < 0: XGetWindowAttributes fails */
static int nexa_stub_origin_x = 0;
static int nexa_stub_origin_y = 0;

/* Interned atoms, so that a property can be told apart by name. A real server
   hands out distinct non-zero ids and so does this. */
enum { NEXA_STUB_ATOMS = 32, NEXA_STUB_ATOM_LEN = 48 };
static char nexa_stub_atom_names[NEXA_STUB_ATOMS][NEXA_STUB_ATOM_LEN];
static int nexa_stub_atom_count = 0;

static const char* nexa_stub_atom_name(Atom a) {
    if (a == 0 || (int)a > nexa_stub_atom_count) return "";
    return nexa_stub_atom_names[a - 1];
}

/* The last XChangeProperty, and the map/unmap/move/send log. */
static Atom nexa_stub_prop_atom = 0;
static Atom nexa_stub_prop_type = 0;
static int nexa_stub_prop_format = 0;
static int nexa_stub_prop_count = 0;
static long nexa_stub_prop_data[8];

/* The last XSendEvent. A ClientMessage to the root window is how a program
   asks the window manager for a _NET_WM_STATE change, and the message is the
   whole of the request: nothing comes back and no property is set. */
static int nexa_stub_msg_type = 0;
static Atom nexa_stub_msg_message_type = 0;
static int nexa_stub_msg_format = 0;
static long nexa_stub_msg_data[5];
static Window nexa_stub_msg_window = 0;
static Window nexa_stub_msg_target = 0;
static long nexa_stub_msg_mask = 0;
static int nexa_stub_msg_propagate = 0;

/* How the window was asked for, and the resources that came with it. The gfx
   runtime picks a visual before it creates a window, so what it asked the
   server for is the only visible half of gfx.transparent's X11 side: a visual
   cannot be changed afterwards and nothing reads one back. */
static int nexa_stub_argb_visual = 1;
static int nexa_stub_win_depth = 0;
static int nexa_stub_win_class = 0;
static VisualID nexa_stub_win_visual_id = 0;
static unsigned long nexa_stub_win_mask = 0;
static Colormap nexa_stub_win_colormap = 0;
static unsigned long nexa_stub_win_border_pixel = 0;
static int nexa_stub_cmaps_made = 0;
static int nexa_stub_cmaps_freed = 0;
static int nexa_stub_gcs_made = 0;
static int nexa_stub_gcs_freed = 0;

/* The last XCreateImage's depth and the first few pixels of the last
   XPutImage, which is where the alpha the runtime chose becomes visible. */
enum { NEXA_STUB_PIXELS = 16 };
static int nexa_stub_image_depth = 0;
static unsigned long nexa_stub_image_px[NEXA_STUB_PIXELS];

/* The depth-32 TrueColor visual the fake server offers, when it offers one. A
   real Visual is opaque to callers; all the runtime does with this one is hand
   it back to XCreateWindow and XCreateImage. */
static Visual nexa_stub_argb = { 0x20, TrueColor, 0xFF0000UL, 0xFF00UL, 0xFFUL, 8, 256 };

enum { NEXA_STUB_CALLS = 32 };
static int nexa_stub_calls[NEXA_STUB_CALLS];
static int nexa_stub_call_n = 0;
static int nexa_stub_moved_x = 0;
static int nexa_stub_moved_y = 0;

static void nexa_stub_log_call(int what) {
    if (nexa_stub_call_n < NEXA_STUB_CALLS) nexa_stub_calls[nexa_stub_call_n++] = what;
}
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
    nexa_stub_map_state = -1;
    nexa_stub_origin_x = 0;
    nexa_stub_origin_y = 0;
    nexa_stub_prop_atom = 0;
    nexa_stub_prop_type = 0;
    nexa_stub_prop_format = 0;
    nexa_stub_prop_count = 0;
    memset(nexa_stub_prop_data, 0, sizeof(nexa_stub_prop_data));
    nexa_stub_msg_type = 0;
    nexa_stub_msg_message_type = 0;
    nexa_stub_msg_format = 0;
    memset(nexa_stub_msg_data, 0, sizeof(nexa_stub_msg_data));
    nexa_stub_msg_window = 0;
    nexa_stub_msg_target = 0;
    nexa_stub_msg_mask = 0;
    nexa_stub_msg_propagate = 0;
    nexa_stub_call_n = 0;
    nexa_stub_moved_x = 0;
    nexa_stub_moved_y = 0;
    nexa_stub_argb_visual = 1;
    nexa_stub_win_depth = 0;
    nexa_stub_win_class = 0;
    nexa_stub_win_visual_id = 0;
    nexa_stub_win_mask = 0;
    nexa_stub_win_colormap = 0;
    nexa_stub_win_border_pixel = 0;
    nexa_stub_cmaps_made = 0;
    nexa_stub_cmaps_freed = 0;
    nexa_stub_gcs_made = 0;
    nexa_stub_gcs_freed = 0;
    nexa_stub_image_depth = 0;
    memset(nexa_stub_image_px, 0, sizeof(nexa_stub_image_px));
}

void nexa_x11_stub_set_argb_visual(int available) {
    nexa_stub_argb_visual = available ? 1 : 0;
}

int nexa_x11_stub_window_depth(void) { return nexa_stub_win_depth; }
int nexa_x11_stub_window_class(void) { return nexa_stub_win_class; }
unsigned long nexa_x11_stub_window_visual_id(void) { return (unsigned long)nexa_stub_win_visual_id; }
unsigned long nexa_x11_stub_window_mask(void) { return nexa_stub_win_mask; }
unsigned long nexa_x11_stub_window_colormap(void) { return (unsigned long)nexa_stub_win_colormap; }
unsigned long nexa_x11_stub_window_border_pixel(void) { return nexa_stub_win_border_pixel; }
int nexa_x11_stub_colormaps_made(void) { return nexa_stub_cmaps_made; }
int nexa_x11_stub_colormaps_freed(void) { return nexa_stub_cmaps_freed; }
int nexa_x11_stub_gcs_made(void) { return nexa_stub_gcs_made; }
int nexa_x11_stub_gcs_freed(void) { return nexa_stub_gcs_freed; }
int nexa_x11_stub_image_depth(void) { return nexa_stub_image_depth; }

unsigned long nexa_x11_stub_image_pixel(int i) {
    if (i < 0 || i >= NEXA_STUB_PIXELS) return 0;
    return nexa_stub_image_px[i];
}

void nexa_x11_stub_set_mapped(int map_state) { nexa_stub_map_state = map_state; }

void nexa_x11_stub_set_origin(int x, int y) {
    nexa_stub_origin_x = x;
    nexa_stub_origin_y = y;
}

const char* nexa_x11_stub_property_name(void) {
    return nexa_stub_atom_name(nexa_stub_prop_atom);
}

int nexa_x11_stub_property_type_matches(void) {
    return nexa_stub_prop_type != 0 && nexa_stub_prop_type == nexa_stub_prop_atom;
}

int nexa_x11_stub_property_format(void) { return nexa_stub_prop_format; }
int nexa_x11_stub_property_count(void) { return nexa_stub_prop_count; }

long nexa_x11_stub_property_word(int i) {
    if (i < 0 || i >= (int)(sizeof(nexa_stub_prop_data) / sizeof(nexa_stub_prop_data[0]))) return 0;
    return nexa_stub_prop_data[i];
}

void nexa_x11_stub_calls_clear(void) { nexa_stub_call_n = 0; }
int nexa_x11_stub_call_count(void) { return nexa_stub_call_n; }

int nexa_x11_stub_call(int i) {
    if (i < 0 || i >= nexa_stub_call_n) return 0;
    return nexa_stub_calls[i];
}

int nexa_x11_stub_move_x(void) { return nexa_stub_moved_x; }
int nexa_x11_stub_move_y(void) { return nexa_stub_moved_y; }

int nexa_x11_stub_message_is_client(void) { return nexa_stub_msg_type == ClientMessage; }

const char* nexa_x11_stub_message_name(void) {
    return nexa_stub_atom_name(nexa_stub_msg_message_type);
}

int nexa_x11_stub_message_format(void) { return nexa_stub_msg_format; }

long nexa_x11_stub_message_word(int i) {
    if (i < 0 || i >= (int)(sizeof(nexa_stub_msg_data) / sizeof(nexa_stub_msg_data[0]))) return 0;
    return nexa_stub_msg_data[i];
}

const char* nexa_x11_stub_message_word_name(int i) {
    return nexa_stub_atom_name((Atom)nexa_x11_stub_message_word(i));
}

unsigned long nexa_x11_stub_message_window(void) { return (unsigned long)nexa_stub_msg_window; }
unsigned long nexa_x11_stub_message_target(void) { return (unsigned long)nexa_stub_msg_target; }
long nexa_x11_stub_message_mask(void) { return nexa_stub_msg_mask; }
int nexa_x11_stub_message_propagate(void) { return nexa_stub_msg_propagate; }

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
    (void)d;
    nexa_stub_msg_target = w;
    nexa_stub_msg_propagate = p ? 1 : 0;
    nexa_stub_msg_mask = m;
    nexa_stub_msg_type = e ? e->type : 0;
    nexa_stub_msg_message_type = 0;
    nexa_stub_msg_format = 0;
    nexa_stub_msg_window = 0;
    memset(nexa_stub_msg_data, 0, sizeof(nexa_stub_msg_data));
    if (e && e->type == ClientMessage) {
        nexa_stub_msg_message_type = e->xclient.message_type;
        nexa_stub_msg_format = e->xclient.format;
        nexa_stub_msg_window = e->xclient.window;
        memcpy(nexa_stub_msg_data, e->xclient.data.l, sizeof(nexa_stub_msg_data));
    }
    nexa_stub_log_call(NEXA_STUB_CALL_SEND);
    return 0;
}
int XSelectInput(Display* d, Window w, long m) { (void)d; (void)w; (void)m; return 0; }
Window XCreateSimpleWindow(Display* d, Window parent, int x, int y,
                           unsigned int w, unsigned int h, unsigned int bw,
                           unsigned long border, unsigned long background) {
    (void)d; (void)parent; (void)x; (void)y; (void)w; (void)h; (void)bw;
    (void)border; (void)background;
    /* A simple window copies its parent's depth and visual, which is what the
       zeroes recorded here mean: nothing of its own was asked for. */
    nexa_stub_win_depth = 0;
    nexa_stub_win_class = 0;
    nexa_stub_win_visual_id = 0;
    nexa_stub_win_mask = 0;
    nexa_stub_win_colormap = 0;
    nexa_stub_win_border_pixel = 0;
    return nexa_stub_display_works ? (Window)NEXA_STUB_WINDOW : (Window)0;
}
Window XCreateWindow(Display* d, Window parent, int x, int y,
                     unsigned int w, unsigned int h, unsigned int bw,
                     int depth, unsigned int c_class, Visual* visual,
                     unsigned long valuemask, XSetWindowAttributes* attrs) {
    (void)d; (void)parent; (void)x; (void)y; (void)w; (void)h; (void)bw;
    nexa_stub_win_depth = depth;
    nexa_stub_win_class = (int)c_class;
    nexa_stub_win_visual_id = visual ? visual->visualid : 0;
    nexa_stub_win_mask = valuemask;
    /* Only what the mask says is read, the way a real server reads it: an
       attribute left out of the mask is not a zero, it is absent. */
    nexa_stub_win_colormap = (attrs && (valuemask & CWColormap)) ? attrs->colormap : 0;
    nexa_stub_win_border_pixel =
        (attrs && (valuemask & CWBorderPixel)) ? attrs->border_pixel : 0;
    return nexa_stub_display_works ? (Window)NEXA_STUB_WINDOW : (Window)0;
}
Colormap XCreateColormap(Display* d, Window w, Visual* visual, int alloc) {
    (void)d; (void)w; (void)visual; (void)alloc;
    nexa_stub_cmaps_made++;
    return (Colormap)0x2301;
}
int XFreeColormap(Display* d, Colormap cmap) {
    (void)d; (void)cmap;
    nexa_stub_cmaps_freed++;
    return 0;
}
GC XCreateGC(Display* d, Drawable dr, unsigned long valuemask, void* values) {
    (void)d; (void)dr; (void)valuemask; (void)values;
    nexa_stub_gcs_made++;
    /* Non-null and distinct from the default GC, which is 0 here. */
    static int fake_gc;
    return (GC)&fake_gc;
}
int XFreeGC(Display* d, GC gc) {
    (void)d; (void)gc;
    nexa_stub_gcs_freed++;
    return 0;
}
int XDestroyWindow(Display* d, Window w) { (void)d; (void)w; return 0; }
int XMapWindow(Display* d, Window w) {
    (void)d; (void)w;
    nexa_stub_log_call(NEXA_STUB_CALL_MAP);
    return 0;
}
int XUnmapWindow(Display* d, Window w) {
    (void)d; (void)w;
    nexa_stub_log_call(NEXA_STUB_CALL_UNMAP);
    return 0;
}
int XMoveWindow(Display* d, Window w, int x, int y) {
    (void)d; (void)w;
    nexa_stub_moved_x = x;
    nexa_stub_moved_y = y;
    nexa_stub_log_call(NEXA_STUB_CALL_MOVE);
    return 0;
}
Bool XTranslateCoordinates(Display* d, Window src, Window dst,
                           int sx, int sy, int* dx, int* dy, Window* child) {
    (void)d; (void)src; (void)dst;
    if (dx) *dx = sx + nexa_stub_origin_x;
    if (dy) *dy = sy + nexa_stub_origin_y;
    if (child) *child = 0;
    return 0;
}
int XResizeWindow(Display* d, Window w, unsigned int w2, unsigned int h) {
    (void)d; (void)w; (void)w2; (void)h; return 0;
}
int XStoreName(Display* d, Window w, const char* n) { (void)d; (void)w; (void)n; return 0; }
Atom XInternAtom(Display* d, const char* n, Bool o) {
    (void)d; (void)o;
    if (!n) return 0;
    for (int i = 0; i < nexa_stub_atom_count; i++) {
        if (strcmp(nexa_stub_atom_names[i], n) == 0) return (Atom)(i + 1);
    }
    if (nexa_stub_atom_count >= NEXA_STUB_ATOMS) return 0;
    size_t len = strlen(n);
    if (len >= NEXA_STUB_ATOM_LEN) len = NEXA_STUB_ATOM_LEN - 1;
    memcpy(nexa_stub_atom_names[nexa_stub_atom_count], n, len);
    nexa_stub_atom_names[nexa_stub_atom_count][len] = 0;
    return (Atom)(++nexa_stub_atom_count);
}
Status XSetWMProtocols(Display* d, Window w, Atom* p, int c) {
    (void)d; (void)w; (void)p; (void)c; return 0;
}
int XChangeProperty(Display* d, Window w, Atom pr, Atom t, int f, int m,
                    const unsigned char* data, int n) {
    (void)d; (void)w; (void)m;
    nexa_stub_prop_atom = pr;
    nexa_stub_prop_type = t;
    nexa_stub_prop_format = f;
    nexa_stub_prop_count = n;
    memset(nexa_stub_prop_data, 0, sizeof(nexa_stub_prop_data));
    /* Format 32 means an array of long on the wire as far as Xlib callers are
       concerned, which is what the tests read back. */
    if (data && f == 32 && n > 0 &&
        n <= (int)(sizeof(nexa_stub_prop_data) / sizeof(nexa_stub_prop_data[0]))) {
        memcpy(nexa_stub_prop_data, data, sizeof(long) * (size_t)n);
    }
    return 0;
}
/* The invisible cursor gfx.cursor(0) makes. Handing back a fixed non-zero id
   is enough for the runtime to believe it has one and to free it again. */
Pixmap XCreateBitmapFromData(Display* d, Drawable dr, const char* data,
                             unsigned int w, unsigned int h) {
    (void)d; (void)dr; (void)data; (void)w; (void)h; return (Pixmap)0x2201;
}
Cursor XCreatePixmapCursor(Display* d, Pixmap source, Pixmap mask,
                           XColor* fg, XColor* bg, unsigned int x, unsigned int y) {
    (void)d; (void)source; (void)mask; (void)fg; (void)bg; (void)x; (void)y;
    return (Cursor)0x2202;
}
int XDefineCursor(Display* d, Window w, Cursor c) { (void)d; (void)w; (void)c; return 0; }
int XUndefineCursor(Display* d, Window w) { (void)d; (void)w; return 0; }
int XFreeCursor(Display* d, Cursor c) { (void)d; (void)c; return 0; }
int XFreePixmap(Display* d, Pixmap p) { (void)d; (void)p; return 0; }
Status XGetWindowAttributes(Display* d, Window w, XWindowAttributes* a) {
    (void)d; (void)w;
    if (a) memset(a, 0, sizeof(*a));
    /* Fails unless a test has said what the window looks like -- which is the
       honest answer for a server that never created one. */
    if (!a || nexa_stub_map_state < 0) return 0;
    a->map_state = nexa_stub_map_state;
    return 1;
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
    nexa_stub_image_depth = (int)depth;
    return img;
}
int XDestroyImage(XImage* img) { free(img); return 0; }
int XPutImage(Display* d, Drawable dr, GC gc, XImage* img,
              int sx, int sy, int dx, int dy, unsigned int w, unsigned int h) {
    (void)d; (void)dr; (void)gc; (void)sx; (void)sy; (void)dx; (void)dy;
    (void)w; (void)h;
    /* Keep the first few pixels as they went out. The runtime lays them out
       for this machine's byte order, so a test reads them back as whole 32-bit
       words rather than as four bytes in a particular order. */
    memset(nexa_stub_image_px, 0, sizeof(nexa_stub_image_px));
    if (img && img->data) {
        int n = img->width * img->height;
        if (n > NEXA_STUB_PIXELS) n = NEXA_STUB_PIXELS;
        for (int i = 0; i < n; i++) {
            unsigned int v = 0;
            memcpy(&v, (unsigned char*)img->data + i * 4, 4);
            nexa_stub_image_px[i] = (unsigned long)v;
        }
    }
    return 0;
}
GC XDefaultGC(Display* d, int s) { (void)d; (void)s; return 0; }
int XDefaultScreen(Display* d) { (void)d; return 0; }
Visual* XDefaultVisual(Display* d, int s) { (void)d; (void)s; return 0; }
int XDefaultDepth(Display* d, int s) { (void)d; (void)s; return 24; }
Window XRootWindow(Display* d, int s) { (void)d; (void)s; return 0; }
unsigned long XBlackPixel(Display* d, int s) { (void)d; (void)s; return 0; }
unsigned long XWhitePixel(Display* d, int s) { (void)d; (void)s; return 0xFFFFFFUL; }
/* The one visual the fake server has beyond the screen's own: depth 32
   TrueColor, which is the one with an alpha channel. Anything else is not on
   offer, and neither is this one once a test has said so. */
Status XMatchVisualInfo(Display* d, int screen, int depth, int c_class, XVisualInfo* out) {
    (void)d;
    if (!out) return 0;
    if (depth != 32 || c_class != TrueColor || !nexa_stub_argb_visual) return 0;
    memset(out, 0, sizeof(*out));
    out->visual = &nexa_stub_argb;
    out->visualid = nexa_stub_argb.visualid;
    out->screen = screen;
    out->depth = 32;
    out->c_class = TrueColor;
    out->red_mask = nexa_stub_argb.red_mask;
    out->green_mask = nexa_stub_argb.green_mask;
    out->blue_mask = nexa_stub_argb.blue_mask;
    out->colormap_size = nexa_stub_argb.map_entries;
    out->bits_per_rgb = nexa_stub_argb.bits_per_rgb;
    return 1;
}
int XSetWMNormalHints(Display* d, Window w, XSizeHints* h) { (void)d; (void)w; (void)h; return 0; }
XSizeHints* XAllocSizeHints(void) { return (XSizeHints*)calloc(1, sizeof(XSizeHints)); }
