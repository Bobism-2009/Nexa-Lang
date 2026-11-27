#pragma once
// NexaC's own X11 client: the forty-odd Xlib calls std/gfx and std/ui make,
// spoken straight down the socket in the core protocol, with nothing linked.
//
// A Linux gfx or ui executable used to carry a static libX11 (and libxcb,
// libXau, libXdmcp) so that it ran where libX11.so is not installed: 1.1MB of
// every such program, for a library of which the runtime calls a few dozen
// requests. These are those requests. Same names, same types, same answers,
// so the runtime's X11 code is the same text whichever is underneath; the
// program is built with NEXA_X11_WIRE when this one is (see NexaC.cpp), and
// with <X11/Xlib.h> otherwise -- "x11": "system", a shared library, and the
// test suites, which compile the generated C++ against Tests/gfx_x11_stub.
//
// What it does and does not do:
//   - connects over the abstract or the filesystem Unix socket, or TCP for a
//     DISPLAY with a host (ssh -X), with the MIT-MAGIC-COOKIE-1 in
//     $XAUTHORITY or ~/.Xauthority when there is one;
//   - speaks in the client's own byte order, which the server then uses for
//     everything it sends back, so nothing is swapped;
//   - keeps Xlib's key translation (KeyBind.c): the core keyboard map, Shift,
//     Lock as Caps or Shift Lock, Num Lock on the keypad, Mode_switch, and
//     Latin-1 text with Control applied -- what XLookupString gives a program
//     that opens no input method, which the runtime never did;
//   - drops protocol errors, where Xlib's default handler prints and exits;
//     a lost connection becomes a DestroyNotify, so the window reads as
//     closed rather than the process ending;
//   - is single-threaded, as every caller in the runtime is.

#include <string>

namespace nexa {

inline std::string x11WireRuntimeCpp() {
    return R"NEXA_X11W(
#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cerrno>
#include <poll.h>
#include <unistd.h>
#include <netdb.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <sys/socket.h>
#include <sys/uio.h>
#include <sys/un.h>

// Every call of Xlib's that the runtime makes is here, and a program reaches
// only some of them.
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-function"

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
typedef struct __nexa_xgc* GC;

#define False 0
#define True 1
#define None 0L
#define NoSymbol 0L
#define AllocNone 0
#define XYPixmap 1
#define ZPixmap 2
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
#define MappingNotify 34
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
#define ShiftMask (1 << 0)
#define LockMask (1 << 1)
#define ControlMask (1 << 2)
#define Button1 1
#define Button1Mask (1 << 8)
#define Button2Mask (1 << 9)
#define Button3Mask (1 << 10)
#define PropModeReplace 0
#define RevertToParent 2
#define LSBFirst 0
#define MSBFirst 1
#define XA_ATOM ((Atom)4)
#define XA_CARDINAL ((Atom)6)
#define XA_STRING ((Atom)31)
#define XA_WM_NAME ((Atom)39)

#define XK_space 0x0020
#define XK_BackSpace 0xff08
#define XK_Tab 0xff09
#define XK_Clear 0xff0b
#define XK_Return 0xff0d
#define XK_Escape 0xff1b
#define XK_Delete 0xffff
#define XK_Home 0xff50
#define XK_Left 0xff51
#define XK_Up 0xff52
#define XK_Right 0xff53
#define XK_Down 0xff54
#define XK_End 0xff57
#define XK_Mode_switch 0xff7e
#define XK_Num_Lock 0xff7f
#define XK_KP_Space 0xff80
#define XK_KP_Tab 0xff89
#define XK_KP_Enter 0xff8d
#define XK_KP_Home 0xff95
#define XK_KP_Left 0xff96
#define XK_KP_Right 0xff98
#define XK_KP_End 0xff9c
#define XK_KP_Delete 0xff9f
#define XK_KP_Multiply 0xffaa
#define XK_KP_9 0xffb9
#define XK_KP_Equal 0xffbd
#define XK_F11 0xffc8
#define XK_Shift_L 0xffe1
#define XK_Shift_R 0xffe2
#define XK_Control_L 0xffe3
#define XK_Control_R 0xffe4
#define XK_Caps_Lock 0xffe5
#define XK_Shift_Lock 0xffe6
#define XK_Meta_L 0xffe7
#define XK_Meta_R 0xffe8
#define XK_Alt_L 0xffe9
#define XK_Alt_R 0xffea
#define XK_VoidSymbol 0xffffff

typedef struct {
    VisualID visualid;
    int c_class;
    unsigned long red_mask, green_mask, blue_mask;
    int bits_per_rgb;
    int map_entries;
    int depth;
} Visual;

typedef struct _XDisplay Display;

typedef struct {
    int type; unsigned long serial; Bool send_event; Display* display; Window window;
    Window root; Window subwindow; Time time; int x, y; int x_root, y_root;
    unsigned int state; unsigned int keycode; Bool same_screen;
} XKeyEvent;
typedef struct {
    int type; unsigned long serial; Bool send_event; Display* display; Window window;
    Window root; Window subwindow; Time time; int x, y; int x_root, y_root;
    unsigned int state; unsigned int button; Bool same_screen;
} XButtonEvent;
typedef struct {
    int type; unsigned long serial; Bool send_event; Display* display; Window window;
    Atom message_type; int format;
    union { char b[20]; short s[10]; long l[5]; } data;
} XClientMessageEvent;
typedef struct {
    int type; unsigned long serial; Bool send_event; Display* display; Window event; Window window;
} XDestroyWindowEvent;
typedef struct {
    int type; unsigned long serial; Bool send_event; Display* display; Window window;
    int x, y, width, height; int count;
} XExposeEvent;
typedef struct {
    int type; unsigned long serial; Bool send_event; Display* display; Window event; Window window;
    int x, y, width, height; int border_width; Window above; Bool override_redirect;
} XConfigureEvent;
typedef struct {
    int type; unsigned long serial; Bool send_event; Display* display; Window window;
} XAnyEvent;
typedef union _XEvent {
    int type;
    XAnyEvent xany;
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
    Colormap colormap;
} XWindowAttributes;

typedef struct {
    Visual* visual;
    VisualID visualid;
    int screen;
    int depth;
    int c_class;
    unsigned long red_mask, green_mask, blue_mask;
    int colormap_size;
    int bits_per_rgb;
} XVisualInfo;

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
} XImage;

struct _XDisplay {
    int fd;
    int dead;
    unsigned long seq;
    unsigned long id_base, id_inc, id_next;
    size_t max_req;
    int image_byte_order, bitmap_unit, bitmap_bit_order, bitmap_pad;
    int min_kc, max_kc;
    int fmt_bpp[33], fmt_pad[33];
    int screen;
    Window root;
    Colormap cmap;
    unsigned long white, black;
    int root_depth;
    Visual* root_visual;
    Visual* visuals;
    int nvisuals;
    GC default_gc;
    KeySym* keys;
    int per;
    int xkb, xkb_major, xkb_event, xkb_ntypes, xkb_first, xkb_nkeys;
    unsigned char* xkb_map;
    const unsigned char** xkb_types;
    const unsigned char** xkb_keys;
    unsigned num_lock, mode_switch;
    KeySym lock_meaning;
    XEvent* q;
    int qhead, qn, qcap;
    unsigned char* in;
    size_t inn, incap;
    size_t outn;
    unsigned char out[16384];
};

// --- the socket -------------------------------------------------------------

static void __nexa_x_event(Display* d, const unsigned char* w);
static void __nexa_x_keymap_drop(Display* d);

static void __nexa_x_die(Display* d) {
    if (d->dead) return;
    d->dead = 1;
    if (d->fd >= 0) close(d->fd);
    d->fd = -1;
    d->outn = 0;
    // What Xlib would have exited on. The runtime closes the window on a
    // DestroyNotify, so the program sees a closed window and carries on.
    unsigned char w[32] = {DestroyNotify};
    __nexa_x_event(d, w);
}

static int __nexa_x_send(Display* d, const struct iovec* iov, int n) {
    struct iovec v[3];
    std::memcpy(v, iov, sizeof(struct iovec) * (size_t)n);
    int i = 0;
    while (i < n && !d->dead) {
        struct msghdr m;
        std::memset(&m, 0, sizeof(m));
        m.msg_iov = v + i;
        m.msg_iovlen = (size_t)(n - i);
        ssize_t k = sendmsg(d->fd, &m, MSG_NOSIGNAL);
        if (k < 0) {
            if (errno == EINTR) continue;
            if (errno == EAGAIN) { struct pollfd p = {d->fd, POLLOUT, 0}; poll(&p, 1, -1); continue; }
            __nexa_x_die(d);
            return 0;
        }
        while (i < n && (size_t)k >= v[i].iov_len) { k -= (ssize_t)v[i].iov_len; i++; }
        if (i < n) { v[i].iov_base = (char*)v[i].iov_base + k; v[i].iov_len -= (size_t)k; }
    }
    return !d->dead;
}

static void __nexa_x_flush(Display* d) {
    if (!d->outn || d->dead) { d->outn = 0; return; }
    struct iovec v = {d->out, d->outn};
    d->outn = 0;
    __nexa_x_send(d, &v, 1);
}

// A request of `words` 4-byte units, zeroed, in the output buffer; the caller
// fills in everything after the opcode, the byte after it and the length.
static unsigned char* __nexa_x_req(Display* d, int op, int b1, size_t words) {
    const size_t n = words * 4;
    if (d->outn + n > sizeof(d->out)) __nexa_x_flush(d);
    unsigned char* p = d->out + d->outn;
    d->outn += n;
    std::memset(p, 0, n);
    p[0] = (unsigned char)op;
    p[1] = (unsigned char)b1;
    const unsigned short len = (unsigned short)words;
    std::memcpy(p + 2, &len, 2);
    d->seq++;
    return p;
}

static void __nexa_x_put16(unsigned char* p, unsigned v) { unsigned short s = (unsigned short)v; std::memcpy(p, &s, 2); }
static void __nexa_x_put32(unsigned char* p, unsigned long v) { unsigned int w = (unsigned int)v; std::memcpy(p, &w, 4); }
static unsigned __nexa_x_get16(const unsigned char* p) { unsigned short s; std::memcpy(&s, p, 2); return s; }
static unsigned long __nexa_x_get32(const unsigned char* p) { unsigned int w; std::memcpy(&w, p, 4); return w; }
static int __nexa_x_gets16(const unsigned char* p) { short s; std::memcpy(&s, p, 2); return s; }

// A request whose body is too big for the buffer -- an image, a long
// property -- goes out from where it lies, after what is already queued.
static void __nexa_x_req_big(Display* d, const unsigned char* head, size_t hn,
                             const void* body, size_t bn) {
    static const unsigned char zeros[4] = {0, 0, 0, 0};
    __nexa_x_flush(d);
    struct iovec v[3] = {{(void*)head, hn}, {(void*)body, bn}, {(void*)zeros, (4 - (bn & 3)) & 3}};
    __nexa_x_send(d, v, v[2].iov_len ? 3 : 2);
    d->seq++;
}

static unsigned long __nexa_x_id(Display* d) {
    return d->id_base | ((d->id_next++ * d->id_inc));
}

// --- reading: events to the queue, the awaited reply to the caller ----------

static void __nexa_x_event(Display* d, const unsigned char* w) {
    int type = w[0] & 0x7f;
    if (d->xkb > 0 && type == d->xkb_event) {
        // XKB's NewKeyboardNotify or MapNotify: a client that uses XKB is
        // told this way, and not with MappingNotify, that the keyboard changed.
        if (w[1] <= 1) __nexa_x_keymap_drop(d);
        return;
    }
    if (type == MappingNotify && w[4] != 2) {
        // The keyboard changed under us: the map is read again on next use.
        __nexa_x_keymap_drop(d);
    }
    if (d->qn == d->qcap) {
        int cap = d->qcap ? d->qcap * 2 : 64;
        XEvent* q = (XEvent*)std::malloc(sizeof(XEvent) * (size_t)cap);
        if (!q) return;
        for (int i = 0; i < d->qn; i++) q[i] = d->q[(d->qhead + i) % d->qcap];
        std::free(d->q);
        d->q = q;
        d->qhead = 0;
        d->qcap = cap;
    }
    XEvent* e = &d->q[(d->qhead + d->qn++) % d->qcap];
    std::memset(e, 0, sizeof(*e));
    e->type = type;
    e->xany.serial = __nexa_x_get16(w + 2);
    e->xany.send_event = (w[0] & 0x80) ? 1 : 0;
    e->xany.display = d;
    switch (type) {
        case KeyPress: case KeyRelease: case ButtonPress: case ButtonRelease: case MotionNotify:
            e->xkey.keycode = w[1];  // the button for a button event, at the same place
            e->xkey.time = __nexa_x_get32(w + 4);
            e->xkey.root = __nexa_x_get32(w + 8);
            e->xkey.window = __nexa_x_get32(w + 12);
            e->xkey.subwindow = __nexa_x_get32(w + 16);
            e->xkey.x_root = __nexa_x_gets16(w + 20);
            e->xkey.y_root = __nexa_x_gets16(w + 22);
            e->xkey.x = __nexa_x_gets16(w + 24);
            e->xkey.y = __nexa_x_gets16(w + 26);
            e->xkey.state = __nexa_x_get16(w + 28);
            e->xkey.same_screen = w[30];
            break;
        case Expose:
            e->xexpose.window = __nexa_x_get32(w + 4);
            e->xexpose.x = (int)__nexa_x_get16(w + 8);
            e->xexpose.y = (int)__nexa_x_get16(w + 10);
            e->xexpose.width = (int)__nexa_x_get16(w + 12);
            e->xexpose.height = (int)__nexa_x_get16(w + 14);
            e->xexpose.count = (int)__nexa_x_get16(w + 16);
            break;
        case DestroyNotify:
            e->xdestroywindow.event = __nexa_x_get32(w + 4);
            e->xdestroywindow.window = __nexa_x_get32(w + 8);
            break;
        case ConfigureNotify:
            e->xconfigure.event = __nexa_x_get32(w + 4);
            e->xconfigure.window = __nexa_x_get32(w + 8);
            e->xconfigure.above = __nexa_x_get32(w + 12);
            e->xconfigure.x = __nexa_x_gets16(w + 16);
            e->xconfigure.y = __nexa_x_gets16(w + 18);
            e->xconfigure.width = (int)__nexa_x_get16(w + 20);
            e->xconfigure.height = (int)__nexa_x_get16(w + 22);
            e->xconfigure.border_width = (int)__nexa_x_get16(w + 24);
            e->xconfigure.override_redirect = w[26];
            break;
        case ClientMessage:
            e->xclient.format = w[1];
            e->xclient.window = __nexa_x_get32(w + 4);
            e->xclient.message_type = __nexa_x_get32(w + 8);
            if (w[1] == 32) {
                for (int i = 0; i < 5; i++) e->xclient.data.l[i] = (long)(int)__nexa_x_get32(w + 12 + 4 * i);
            } else if (w[1] == 16) {
                for (int i = 0; i < 10; i++) e->xclient.data.s[i] = (short)__nexa_x_get16(w + 12 + 2 * i);
            } else {
                std::memcpy(e->xclient.data.b, w + 12, 20);
            }
            break;
        default:
            e->xany.window = __nexa_x_get32(w + 4);
            break;
    }
}

// Reads what the server has sent: all of it that is waiting, or, with
// `block`, at least one more chunk. 0 once the connection is gone.
static int __nexa_x_fill(Display* d, int block) {
    if (d->dead) return 0;
    if (!block) {
        struct pollfd p = {d->fd, POLLIN, 0};
        if (poll(&p, 1, 0) <= 0) return 1;
    }
    if (d->incap - d->inn < 4096) {
        size_t cap = d->incap ? d->incap * 2 : 16384;
        unsigned char* b = (unsigned char*)std::realloc(d->in, cap);
        if (!b) { __nexa_x_die(d); return 0; }
        d->in = b;
        d->incap = cap;
    }
    for (;;) {
        ssize_t k = recv(d->fd, d->in + d->inn, d->incap - d->inn, 0);
        if (k > 0) { d->inn += (size_t)k; return 1; }
        if (k < 0 && errno == EINTR) continue;
        if (k < 0 && errno == EAGAIN) return 1;
        __nexa_x_die(d);
        return 0;
    }
}

// Takes whole packets off the front of the input. With `seq`, stops at that
// request's reply or error: 1 and the reply in *out (the caller frees it),
// or 0 for an error. Without (`seq` 0), queues events and drops the rest.
static int __nexa_x_drain(Display* d, unsigned long seq, unsigned char** out) {
    size_t at = 0;
    int found = -1;
    while (d->inn - at >= 32) {
        const unsigned char* p = d->in + at;
        size_t len = 32;
        if (p[0] == 1 || (p[0] & 0x7f) == 35) len += 4 * (size_t)__nexa_x_get32(p + 4);
        if (d->inn - at < len) break;
        if (p[0] == 0 || p[0] == 1) {
            if (seq && __nexa_x_get16(p + 2) == (seq & 0xffff)) {
                if (p[0] == 1) {
                    *out = (unsigned char*)std::malloc(len);
                    if (*out) std::memcpy(*out, p, len);
                    found = *out ? 1 : 0;
                } else {
                    found = 0;
                }
                at += len;
                break;
            }
        } else if ((p[0] & 0x7f) != 35) {
            __nexa_x_event(d, p);
        }
        at += len;
    }
    if (at) {
        std::memmove(d->in, d->in + at, d->inn - at);
        d->inn -= at;
    }
    return found;
}

// Sends what is queued and waits for the reply to request `seq`.
static unsigned char* __nexa_x_reply(Display* d, unsigned long seq) {
    __nexa_x_flush(d);
    unsigned char* r = nullptr;
    for (;;) {
        int got = __nexa_x_drain(d, seq, &r);
        if (got >= 0) return got ? r : nullptr;
        if (!__nexa_x_fill(d, 1)) return nullptr;
    }
}

// --- connecting -------------------------------------------------------------

static int __nexa_x_connect_unix(int n) {
    char path[64];
    std::snprintf(path, sizeof(path), "/tmp/.X11-unix/X%d", n);
    for (int abstract = 1; abstract >= 0; abstract--) {
        int fd = socket(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC, 0);
        if (fd < 0) return -1;
        struct sockaddr_un a;
        std::memset(&a, 0, sizeof(a));
        a.sun_family = AF_UNIX;
        const size_t len = std::strlen(path);
        std::memcpy(a.sun_path + abstract, path, len);
        socklen_t sl = (socklen_t)(offsetof(struct sockaddr_un, sun_path) + abstract + len + (abstract ? 0 : 1));
        if (connect(fd, (struct sockaddr*)&a, sl) == 0) return fd;
        close(fd);
    }
    return -1;
}

static int __nexa_x_connect_tcp(const char* host, int n) {
    char port[16];
    std::snprintf(port, sizeof(port), "%d", 6000 + n);
    struct addrinfo hints;
    std::memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    struct addrinfo* res = nullptr;
    if (getaddrinfo(host, port, &hints, &res) != 0) return -1;
    int fd = -1;
    for (struct addrinfo* ai = res; ai; ai = ai->ai_next) {
        fd = socket(ai->ai_family, ai->ai_socktype | SOCK_CLOEXEC, ai->ai_protocol);
        if (fd < 0) continue;
        if (connect(fd, ai->ai_addr, ai->ai_addrlen) == 0) {
            int one = 1;
            setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, &one, sizeof(one));
            break;
        }
        close(fd);
        fd = -1;
    }
    freeaddrinfo(res);
    return fd;
}

// The MIT-MAGIC-COOKIE-1 for display `n` in the authority file: an entry for
// this machine (or for any machine) wins; otherwise the first for display n.
static int __nexa_x_cookie(int n, unsigned char cookie[16]) {
    char path[4096];
    const char* xa = std::getenv("XAUTHORITY");
    if (xa && *xa) std::snprintf(path, sizeof(path), "%s", xa);
    else {
        const char* home = std::getenv("HOME");
        if (!home) return 0;
        std::snprintf(path, sizeof(path), "%s/.Xauthority", home);
    }
    FILE* f = std::fopen(path, "rb");
    if (!f) return 0;
    char host[256] = {0};
    gethostname(host, sizeof(host) - 1);
    char num[16];
    std::snprintf(num, sizeof(num), "%d", n);
    int best = 0;
    for (;;) {
        unsigned char h[2];
        if (std::fread(h, 1, 2, f) != 2) break;
        const unsigned family = (unsigned)(h[0] << 8 | h[1]);
        char field[4][256];
        unsigned flen[4];
        int ok = 1;
        for (int i = 0; i < 4 && ok; i++) {
            if (std::fread(h, 1, 2, f) != 2) { ok = 0; break; }
            flen[i] = (unsigned)(h[0] << 8 | h[1]);
            if (flen[i] >= sizeof(field[i]) || std::fread(field[i], 1, flen[i], f) != flen[i]) ok = 0;
            else field[i][flen[i]] = 0;
        }
        if (!ok) break;
        if (std::strcmp(field[2], "MIT-MAGIC-COOKIE-1") != 0 || flen[3] != 16) continue;
        if (flen[1] && std::strcmp(field[1], num) != 0) continue;
        const int here = family == 65535 || (family == 256 && flen[0] && std::strcmp(field[0], host) == 0);
        if (best < (here ? 2 : 1)) {
            best = here ? 2 : 1;
            std::memcpy(cookie, field[3], 16);
        }
        if (best == 2) break;
    }
    std::fclose(f);
    return best != 0;
}

static int XInitThreads(void) { return 1; }

static int XCloseDisplay(Display* d);

static Display* XOpenDisplay(const char* name) {
    const char* disp = name ? name : std::getenv("DISPLAY");
    if (!disp || !*disp) return nullptr;
    const char* colon = std::strrchr(disp, ':');
    if (!colon || colon[1] < '0' || colon[1] > '9') return nullptr;
    char host[256];
    size_t hn = (size_t)(colon - disp);
    if (hn >= sizeof(host)) return nullptr;
    std::memcpy(host, disp, hn);
    host[hn] = 0;
    char* end = nullptr;
    const int n = (int)std::strtol(colon + 1, &end, 10);
    const int screen = (end && *end == '.') ? (int)std::strtol(end + 1, nullptr, 10) : 0;
    if (std::strncmp(host, "unix/", 5) == 0 || std::strcmp(host, "unix") == 0) host[0] = 0;
    const int fd = host[0] ? __nexa_x_connect_tcp(host, n) : __nexa_x_connect_unix(n);
    if (fd < 0) return nullptr;

    Display* d = (Display*)std::calloc(1, sizeof(Display));
    if (!d) { close(fd); return nullptr; }
    d->fd = fd;

    unsigned char cookie[16];
    const int auth = __nexa_x_cookie(n, cookie);
    unsigned char hello[12 + 20 + 16];
    std::memset(hello, 0, sizeof(hello));
    const unsigned one = 1;
    hello[0] = *(const unsigned char*)&one ? 'l' : 'B';
    __nexa_x_put16(hello + 2, 11);
    size_t hlen = 12;
    if (auth) {
        __nexa_x_put16(hello + 6, 18);
        __nexa_x_put16(hello + 8, 16);
        std::memcpy(hello + 12, "MIT-MAGIC-COOKIE-1", 18);
        std::memcpy(hello + 32, cookie, 16);
        hlen = sizeof(hello);
    }
    struct iovec v = {hello, hlen};
    if (!__nexa_x_send(d, &v, 1)) { XCloseDisplay(d); return nullptr; }
    while (d->inn < 8) if (!__nexa_x_fill(d, 1)) { XCloseDisplay(d); return nullptr; }
    const size_t total = 8 + 4 * (size_t)__nexa_x_get16(d->in + 6);
    while (d->inn < total) if (!__nexa_x_fill(d, 1)) { XCloseDisplay(d); return nullptr; }
    const unsigned char* s = d->in;
    if (s[0] != 1 || total < 40) { XCloseDisplay(d); return nullptr; }

    d->id_base = __nexa_x_get32(s + 12);
    const unsigned long mask = __nexa_x_get32(s + 16);
    d->id_inc = mask & (~mask + 1);
    d->id_next = 1;
    d->max_req = 4 * (size_t)__nexa_x_get16(s + 26);
    const int nscreens = s[28], nformats = s[29];
    d->image_byte_order = s[30];
    d->bitmap_bit_order = s[31];
    d->bitmap_unit = s[32];
    d->bitmap_pad = s[33];
    d->min_kc = s[34];
    d->max_kc = s[35];
    size_t at = 40 + ((__nexa_x_get16(s + 24) + 3) & ~3u);
    for (int i = 0; i < nformats && at + 8 <= total; i++, at += 8) {
        if (s[at] <= 32) { d->fmt_bpp[s[at]] = s[at + 1]; d->fmt_pad[s[at]] = s[at + 2]; }
    }
    d->screen = screen < nscreens ? screen : 0;
    for (int sc = 0; sc < nscreens && at + 40 <= total; sc++) {
        const unsigned char* r = s + at;
        const int ndepths = r[39];
        const int mine = sc == d->screen;
        if (mine) {
            d->root = __nexa_x_get32(r);
            d->cmap = __nexa_x_get32(r + 4);
            d->white = __nexa_x_get32(r + 8);
            d->black = __nexa_x_get32(r + 12);
            d->root_depth = r[38];
        }
        const unsigned long root_visual = __nexa_x_get32(r + 32);
        at += 40;
        for (int k = 0; k < ndepths && at + 8 <= total; k++) {
            const int depth = s[at];
            const int nv = (int)__nexa_x_get16(s + at + 2);
            at += 8;
            if (mine && nv > 0) {
                Visual* grown = (Visual*)std::realloc(d->visuals, sizeof(Visual) * (size_t)(d->nvisuals + nv));
                if (!grown) { XCloseDisplay(d); return nullptr; }
                d->visuals = grown;
            }
            for (int j = 0; j < nv && at + 24 <= total; j++, at += 24) {
                if (!mine) continue;
                Visual* vis = &d->visuals[d->nvisuals++];
                vis->visualid = __nexa_x_get32(s + at);
                vis->c_class = s[at + 4];
                vis->bits_per_rgb = s[at + 5];
                vis->map_entries = (int)__nexa_x_get16(s + at + 6);
                vis->red_mask = __nexa_x_get32(s + at + 8);
                vis->green_mask = __nexa_x_get32(s + at + 12);
                vis->blue_mask = __nexa_x_get32(s + at + 16);
                vis->depth = depth;
            }
        }
        if (mine) {
            for (int j = 0; j < d->nvisuals; j++) if (d->visuals[j].visualid == root_visual) d->root_visual = &d->visuals[j];
        }
    }
    d->inn = 0;
    if (!d->root || !d->root_visual) { XCloseDisplay(d); return nullptr; }
    return d;
}

static int XCloseDisplay(Display* d) {
    if (!d) return 0;
    __nexa_x_flush(d);
    if (d->fd >= 0) close(d->fd);
    std::free(d->visuals);
    __nexa_x_keymap_drop(d);
    std::free(d->q);
    std::free(d->in);
    std::free(d);
    return 0;
}

// --- events -----------------------------------------------------------------

static int XFlush(Display* d) { __nexa_x_flush(d); return 1; }

static int XPending(Display* d) {
    __nexa_x_flush(d);
    __nexa_x_fill(d, 0);
    __nexa_x_drain(d, 0, nullptr);
    return d->qn;
}

static int XNextEvent(Display* d, XEvent* e) {
    for (;;) {
        __nexa_x_drain(d, 0, nullptr);
        if (d->qn) break;
        __nexa_x_flush(d);
        if (!__nexa_x_fill(d, 1) && !d->qn) {
            std::memset(e, 0, sizeof(*e));
            e->type = DestroyNotify;
            return 0;
        }
    }
    *e = d->q[d->qhead];
    d->qhead = (d->qhead + 1) % d->qcap;
    d->qn--;
    return 0;
}

static int XSync(Display* d, Bool discard) {
    unsigned char* r = (__nexa_x_req(d, 43, 0, 1), __nexa_x_reply(d, d->seq));
    std::free(r);
    if (discard) d->qn = 0;
    return 1;
}

static Status XSendEvent(Display* d, Window w, Bool propagate, long mask, XEvent* e) {
    unsigned char* p = __nexa_x_req(d, 25, propagate ? 1 : 0, 11);
    __nexa_x_put32(p + 4, w);
    __nexa_x_put32(p + 8, (unsigned long)mask);
    unsigned char* ev = p + 12;
    ev[0] = (unsigned char)e->type;
    if (e->type == ClientMessage) {
        ev[1] = (unsigned char)e->xclient.format;
        __nexa_x_put32(ev + 4, e->xclient.window);
        __nexa_x_put32(ev + 8, e->xclient.message_type);
        if (e->xclient.format == 32) {
            for (int i = 0; i < 5; i++) __nexa_x_put32(ev + 12 + 4 * i, (unsigned long)e->xclient.data.l[i]);
        } else if (e->xclient.format == 16) {
            for (int i = 0; i < 10; i++) __nexa_x_put16(ev + 12 + 2 * i, (unsigned)e->xclient.data.s[i]);
        } else {
            std::memcpy(ev + 12, e->xclient.data.b, 20);
        }
    }
    return 1;
}

// --- windows ----------------------------------------------------------------

static void __nexa_x_change_attrs(Display* d, Window w, unsigned long bit, unsigned long value) {
    unsigned char* p = __nexa_x_req(d, 2, 0, 4);
    __nexa_x_put32(p + 4, w);
    __nexa_x_put32(p + 8, bit);
    __nexa_x_put32(p + 12, value);
}

static int XSelectInput(Display* d, Window w, long mask) {
    __nexa_x_change_attrs(d, w, CWEventMask, (unsigned long)mask);
    return 1;
}

static Window XCreateWindow(Display* d, Window parent, int x, int y, unsigned int w, unsigned int h,
                            unsigned int bw, int depth, unsigned int c_class, Visual* visual,
                            unsigned long valuemask, XSetWindowAttributes* a) {
    // In bit order, which is the order the protocol wants the values in. Of
    // the fifteen, the eight this header's XSetWindowAttributes carries.
    unsigned long vals[15];
    int nv = 0;
    valuemask &= CWBackPixmap | CWBackPixel | CWBorderPixmap | CWBorderPixel |
                 CWOverrideRedirect | CWEventMask | CWColormap | CWCursor;
    if (!a) valuemask = 0;
    if (valuemask & CWBackPixmap) vals[nv++] = a->background_pixmap;
    if (valuemask & CWBackPixel) vals[nv++] = a->background_pixel;
    if (valuemask & CWBorderPixmap) vals[nv++] = a->border_pixmap;
    if (valuemask & CWBorderPixel) vals[nv++] = a->border_pixel;
    if (valuemask & CWOverrideRedirect) vals[nv++] = a->override_redirect ? 1 : 0;
    if (valuemask & CWEventMask) vals[nv++] = (unsigned long)a->event_mask;
    if (valuemask & CWColormap) vals[nv++] = a->colormap;
    if (valuemask & CWCursor) vals[nv++] = a->cursor;
    const Window id = __nexa_x_id(d);
    unsigned char* p = __nexa_x_req(d, 1, depth, 8 + (size_t)nv);
    __nexa_x_put32(p + 4, id);
    __nexa_x_put32(p + 8, parent);
    __nexa_x_put16(p + 12, (unsigned)x);
    __nexa_x_put16(p + 14, (unsigned)y);
    __nexa_x_put16(p + 16, w);
    __nexa_x_put16(p + 18, h);
    __nexa_x_put16(p + 20, bw);
    __nexa_x_put16(p + 22, c_class);
    __nexa_x_put32(p + 24, visual ? visual->visualid : 0);
    __nexa_x_put32(p + 28, valuemask);
    for (int i = 0; i < nv; i++) __nexa_x_put32(p + 32 + 4 * i, vals[i]);
    return id;
}

static Window XCreateSimpleWindow(Display* d, Window parent, int x, int y, unsigned int w, unsigned int h,
                                  unsigned int bw, unsigned long border, unsigned long background) {
    XSetWindowAttributes a;
    std::memset(&a, 0, sizeof(a));
    a.background_pixel = background;
    a.border_pixel = border;
    return XCreateWindow(d, parent, x, y, w, h, bw, 0, 0, nullptr, CWBackPixel | CWBorderPixel, &a);
}

static void __nexa_x_simple(Display* d, int op, XID id) {
    unsigned char* p = __nexa_x_req(d, op, 0, 2);
    __nexa_x_put32(p + 4, id);
}

static int XDestroyWindow(Display* d, Window w) { __nexa_x_simple(d, 4, w); return 1; }
static int XMapWindow(Display* d, Window w) { __nexa_x_simple(d, 8, w); return 1; }
static int XUnmapWindow(Display* d, Window w) { __nexa_x_simple(d, 10, w); return 1; }

static void __nexa_x_configure(Display* d, Window w, unsigned mask, long a, long b) {
    unsigned char* p = __nexa_x_req(d, 12, 0, 5);
    __nexa_x_put32(p + 4, w);
    __nexa_x_put16(p + 8, mask);
    __nexa_x_put32(p + 12, (unsigned long)a);
    __nexa_x_put32(p + 16, (unsigned long)b);
}

static int XMoveWindow(Display* d, Window w, int x, int y) { __nexa_x_configure(d, w, 0x3, x, y); return 1; }
static int XResizeWindow(Display* d, Window w, unsigned int ww, unsigned int h) {
    __nexa_x_configure(d, w, 0xC, (long)ww, (long)h);
    return 1;
}

static Colormap XCreateColormap(Display* d, Window w, Visual* visual, int alloc) {
    const Colormap id = __nexa_x_id(d);
    unsigned char* p = __nexa_x_req(d, 78, alloc, 4);
    __nexa_x_put32(p + 4, id);
    __nexa_x_put32(p + 8, w);
    __nexa_x_put32(p + 12, visual ? visual->visualid : 0);
    return id;
}

static int XFreeColormap(Display* d, Colormap c) { __nexa_x_simple(d, 79, c); return 1; }

static GC XCreateGC(Display* d, Drawable dr, unsigned long valuemask, void* values) {
    (void)valuemask;
    (void)values;
    const XID id = __nexa_x_id(d);
    unsigned char* p = __nexa_x_req(d, 55, 0, 4);
    __nexa_x_put32(p + 4, id);
    __nexa_x_put32(p + 8, dr);
    return (GC)(unsigned long)id;
}

static int XFreeGC(Display* d, GC gc) { __nexa_x_simple(d, 60, (XID)(unsigned long)gc); return 1; }

static int XDefaultScreen(Display* d) { return d->screen; }
static Window XRootWindow(Display* d, int s) { (void)s; return d->root; }
static Visual* XDefaultVisual(Display* d, int s) { (void)s; return d->root_visual; }
static int XDefaultDepth(Display* d, int s) { (void)s; return d->root_depth; }
static unsigned long XBlackPixel(Display* d, int s) { (void)s; return d->black; }
static unsigned long XWhitePixel(Display* d, int s) { (void)s; return d->white; }
static GC XDefaultGC(Display* d, int s) {
    (void)s;
    if (!d->default_gc) d->default_gc = XCreateGC(d, d->root, 0, nullptr);
    return d->default_gc;
}

#define DefaultScreen(d) XDefaultScreen(d)
#define DefaultVisual(d, s) XDefaultVisual((d), (s))
#define DefaultDepth(d, s) XDefaultDepth((d), (s))
#define DefaultGC(d, s) XDefaultGC((d), (s))
#define RootWindow(d, s) XRootWindow((d), (s))
#define DefaultRootWindow(d) XRootWindow((d), XDefaultScreen(d))
#define BlackPixel(d, s) XBlackPixel((d), (s))
#define WhitePixel(d, s) XWhitePixel((d), (s))

static Status XMatchVisualInfo(Display* d, int screen, int depth, int c_class, XVisualInfo* vi) {
    (void)screen;
    for (int i = 0; i < d->nvisuals; i++) {
        Visual* v = &d->visuals[i];
        if (v->depth != depth || v->c_class != c_class) continue;
        vi->visual = v;
        vi->visualid = v->visualid;
        vi->screen = d->screen;
        vi->depth = depth;
        vi->c_class = c_class;
        vi->red_mask = v->red_mask;
        vi->green_mask = v->green_mask;
        vi->blue_mask = v->blue_mask;
        vi->colormap_size = v->map_entries;
        vi->bits_per_rgb = v->bits_per_rgb;
        return 1;
    }
    return 0;
}

// --- properties and atoms ---------------------------------------------------

static Atom XInternAtom(Display* d, const char* name, Bool only_if_exists) {
    const size_t n = std::strlen(name);
    if (n > 1024) return 0;
    unsigned char* p = __nexa_x_req(d, 16, only_if_exists ? 1 : 0, 2 + (n + 3) / 4);
    __nexa_x_put16(p + 4, (unsigned)n);
    std::memcpy(p + 8, name, n);
    unsigned char* r = __nexa_x_reply(d, d->seq);
    if (!r) return 0;
    const Atom a = __nexa_x_get32(r + 8);
    std::free(r);
    return a;
}

static int XChangeProperty(Display* d, Window w, Atom prop, Atom type, int format, int mode,
                           const unsigned char* data, int nelements) {
    if (nelements < 0 || (format != 8 && format != 16 && format != 32)) return 0;
    const size_t unit = (size_t)format / 8;
    const size_t bytes = unit * (size_t)nelements;
    unsigned char* body = (unsigned char*)std::malloc(bytes + 1);
    if (!body) return 0;
    // Xlib's format-32 data is an array of long, whatever a long is; the wire
    // wants 32 bits a word. Format 16 is an array of short, which already is.
    for (int i = 0; i < nelements; i++) {
        if (format == 32) __nexa_x_put32(body + 4 * i, (unsigned long)((const long*)data)[i]);
        else if (format == 16) __nexa_x_put16(body + 2 * i, (unsigned)((const short*)data)[i]);
        else body[i] = data[i];
    }
    const size_t words = 6 + (bytes + 3) / 4;
    if (words > 65535 || words * 4 > d->max_req) { std::free(body); return 0; }
    unsigned char head[24];
    std::memset(head, 0, sizeof(head));
    head[0] = 18;
    head[1] = (unsigned char)mode;
    __nexa_x_put16(head + 2, (unsigned)words);
    __nexa_x_put32(head + 4, w);
    __nexa_x_put32(head + 8, prop);
    __nexa_x_put32(head + 12, type);
    head[16] = (unsigned char)format;
    __nexa_x_put32(head + 20, (unsigned long)nelements);
    if (words * 4 <= sizeof(d->out)) {
        unsigned char* p = __nexa_x_req(d, 18, mode, words);
        std::memcpy(p + 4, head + 4, 20);
        std::memcpy(p + 24, body, bytes);
    } else {
        __nexa_x_req_big(d, head, 24, body, bytes);
    }
    std::free(body);
    return 1;
}

static int XStoreName(Display* d, Window w, const char* name) {
    return XChangeProperty(d, w, XA_WM_NAME, XA_STRING, 8, PropModeReplace,
                           (const unsigned char*)name, (int)std::strlen(name));
}

static Status XSetWMProtocols(Display* d, Window w, Atom* protocols, int count) {
    const Atom wp = XInternAtom(d, "WM_PROTOCOLS", False);
    if (!wp) return 0;
    long* l = (long*)std::malloc(sizeof(long) * (size_t)(count > 0 ? count : 1));
    if (!l) return 0;
    for (int i = 0; i < count; i++) l[i] = (long)protocols[i];
    XChangeProperty(d, w, wp, XA_ATOM, 32, PropModeReplace, (const unsigned char*)l, count);
    std::free(l);
    return 1;
}

// --- cursors ----------------------------------------------------------------

static int XFreePixmap(Display* d, Pixmap p) { __nexa_x_simple(d, 54, p); return 1; }
static int XFreeCursor(Display* d, Cursor c) { __nexa_x_simple(d, 95, c); return 1; }
static int XDefineCursor(Display* d, Window w, Cursor c) { __nexa_x_change_attrs(d, w, CWCursor, c); return 1; }
static int XUndefineCursor(Display* d, Window w) { __nexa_x_change_attrs(d, w, CWCursor, 0); return 1; }

static Pixmap XCreateBitmapFromData(Display* d, Drawable dr, const char* data, unsigned int w, unsigned int h) {
    if (!w || !h || w > 4096 || h > 4096) return 0;
    const Pixmap id = __nexa_x_id(d);
    unsigned char* p = __nexa_x_req(d, 53, 1, 4);
    __nexa_x_put32(p + 4, id);
    __nexa_x_put32(p + 8, dr);
    __nexa_x_put16(p + 12, w);
    __nexa_x_put16(p + 14, h);
    const GC gc = XCreateGC(d, id, 0, nullptr);
    // XBM data is a byte at a time, least significant bit first, rows padded
    // to a byte; the server's bitmaps are rows padded to its scanline pad, in
    // its bit order. One bit at a time is plenty for a cursor.
    const int pad = d->bitmap_pad ? d->bitmap_pad : 32;
    const size_t inrow = (w + 7) / 8;
    const size_t row = ((w + (unsigned)pad - 1) / (unsigned)pad) * (unsigned)pad / 8;
    unsigned char* bits = (unsigned char*)std::calloc(1, row * h + 4);
    if (!bits) return 0;
    for (unsigned y = 0; y < h; y++) {
        for (unsigned x = 0; x < w; x++) {
            if (!((((const unsigned char*)data)[y * inrow + x / 8] >> (x % 8)) & 1)) continue;
            bits[y * row + x / 8] |= (unsigned char)(d->bitmap_bit_order == MSBFirst ? 0x80 >> (x % 8) : 1 << (x % 8));
        }
    }
    unsigned char head[24];
    std::memset(head, 0, sizeof(head));
    head[0] = 72;
    head[1] = XYPixmap;
    __nexa_x_put16(head + 2, (unsigned)(6 + (row * h + 3) / 4));
    __nexa_x_put32(head + 4, id);
    __nexa_x_put32(head + 8, (unsigned long)gc);
    __nexa_x_put16(head + 12, w);
    __nexa_x_put16(head + 14, h);
    head[21] = 1;
    __nexa_x_req_big(d, head, 24, bits, row * h);
    std::free(bits);
    XFreeGC(d, gc);
    return id;
}

static Cursor XCreatePixmapCursor(Display* d, Pixmap source, Pixmap mask, XColor* fg, XColor* bg,
                                  unsigned int x, unsigned int y) {
    const Cursor id = __nexa_x_id(d);
    unsigned char* p = __nexa_x_req(d, 93, 0, 8);
    __nexa_x_put32(p + 4, id);
    __nexa_x_put32(p + 8, source);
    __nexa_x_put32(p + 12, mask);
    __nexa_x_put16(p + 16, fg->red);
    __nexa_x_put16(p + 18, fg->green);
    __nexa_x_put16(p + 20, fg->blue);
    __nexa_x_put16(p + 22, bg->red);
    __nexa_x_put16(p + 24, bg->green);
    __nexa_x_put16(p + 26, bg->blue);
    __nexa_x_put16(p + 28, x);
    __nexa_x_put16(p + 30, y);
    return id;
}

// --- questions --------------------------------------------------------------

static Visual* __nexa_x_visual(Display* d, unsigned long id) {
    for (int i = 0; i < d->nvisuals; i++) if (d->visuals[i].visualid == id) return &d->visuals[i];
    return nullptr;
}

// GetWindowAttributes and GetGeometry, both sent before either answer is
// waited for: Xlib asks one and then the other, two round trips a frame.
static Status XGetWindowAttributes(Display* d, Window w, XWindowAttributes* wa) {
    __nexa_x_simple(d, 3, w);
    const unsigned long s1 = d->seq;
    __nexa_x_simple(d, 14, w);
    const unsigned long s2 = d->seq;
    unsigned char* a = __nexa_x_reply(d, s1);
    unsigned char* g = __nexa_x_reply(d, s2);
    Status ok = 0;
    if (a && g) {
        wa->visual = __nexa_x_visual(d, __nexa_x_get32(a + 8));
        wa->c_class = (int)__nexa_x_get16(a + 12);
        wa->map_state = a[26];
        wa->colormap = __nexa_x_get32(a + 28);
        wa->depth = g[1];
        wa->root = __nexa_x_get32(g + 8);
        wa->x = __nexa_x_gets16(g + 12);
        wa->y = __nexa_x_gets16(g + 14);
        wa->width = (int)__nexa_x_get16(g + 16);
        wa->height = (int)__nexa_x_get16(g + 18);
        wa->border_width = (int)__nexa_x_get16(g + 20);
        ok = 1;
    }
    std::free(a);
    std::free(g);
    return ok;
}

static Bool XTranslateCoordinates(Display* d, Window src, Window dst, int sx, int sy,
                                  int* dx, int* dy, Window* child) {
    unsigned char* p = __nexa_x_req(d, 40, 0, 4);
    __nexa_x_put32(p + 4, src);
    __nexa_x_put32(p + 8, dst);
    __nexa_x_put16(p + 12, (unsigned)sx);
    __nexa_x_put16(p + 14, (unsigned)sy);
    unsigned char* r = __nexa_x_reply(d, d->seq);
    if (!r) return 0;
    const Bool same = r[1];
    *child = __nexa_x_get32(r + 8);
    *dx = __nexa_x_gets16(r + 12);
    *dy = __nexa_x_gets16(r + 14);
    std::free(r);
    return same;
}

static Bool XQueryPointer(Display* d, Window w, Window* root, Window* child, int* rx, int* ry,
                          int* wx, int* wy, unsigned int* mask) {
    __nexa_x_simple(d, 38, w);
    unsigned char* r = __nexa_x_reply(d, d->seq);
    if (!r) return 0;
    const Bool same = r[1];
    *root = __nexa_x_get32(r + 8);
    *child = __nexa_x_get32(r + 12);
    *rx = __nexa_x_gets16(r + 16);
    *ry = __nexa_x_gets16(r + 18);
    *wx = same ? __nexa_x_gets16(r + 20) : 0;
    *wy = same ? __nexa_x_gets16(r + 22) : 0;
    *mask = __nexa_x_get16(r + 24);
    std::free(r);
    return same;
}

static int XGetInputFocus(Display* d, Window* focus, int* revert) {
    __nexa_x_req(d, 43, 0, 1);
    unsigned char* r = __nexa_x_reply(d, d->seq);
    *focus = r ? __nexa_x_get32(r + 8) : 0;
    *revert = r ? r[1] : 0;
    std::free(r);
    return 1;
}

static int XQueryKeymap(Display* d, char keys[32]) {
    __nexa_x_req(d, 44, 0, 1);
    unsigned char* r = __nexa_x_reply(d, d->seq);
    if (r) std::memcpy(keys, r + 8, 32);
    else std::memset(keys, 0, 32);
    std::free(r);
    return 1;
}

// --- the keyboard -----------------------------------------------------------
//
// What libX11 does, which is XKB wherever the server has it (every X server
// of the last twenty years): the server's own key types say which modifiers
// pick which level, so Shift with Caps Lock gives a lowercase letter, AltGr
// reaches the third level, and a second layout arrives as a group in the
// event's state. This is XKBBind.c's XkbTranslateKeyCode and
// XkbTranslateKeySym over a map fetched once with XkbGetMap. A server
// without XKB gets the core protocol's rules, KeyBind.c's.

static void __nexa_x_keymap_drop(Display* d) {
    std::free(d->keys);
    d->keys = nullptr;
    std::free(d->xkb_map);
    d->xkb_map = nullptr;
    std::free(d->xkb_types);
    d->xkb_types = nullptr;
    std::free(d->xkb_keys);
    d->xkb_keys = nullptr;
}

static unsigned char* __nexa_x_query_ext(Display* d, const char* name) {
    const size_t n = std::strlen(name);
    unsigned char* p = __nexa_x_req(d, 98, 0, 2 + (n + 3) / 4);
    __nexa_x_put16(p + 4, (unsigned)n);
    std::memcpy(p + 8, name, n);
    return __nexa_x_reply(d, d->seq);
}

// XkbGetMap for the key types and the symbols, kept whole; the types and
// keys point into it. 0 if the reply is not one this can read.
static int __nexa_x_xkb_load(Display* d) {
    unsigned char* p = __nexa_x_req(d, d->xkb_major, 8, 7);
    __nexa_x_put16(p + 4, 0x100);  // the core keyboard
    __nexa_x_put16(p + 6, 1 | 2);  // XkbKeyTypesMask | XkbKeySymsMask, whole
    unsigned char* r = __nexa_x_reply(d, d->seq);
    if (!r) return 0;
    const size_t len = 32 + 4 * (size_t)__nexa_x_get32(r + 4);
    const int ntypes = r[15], first = r[17], nkeys = r[20];
    const unsigned char** types = (const unsigned char**)std::malloc(sizeof(*types) * (size_t)(ntypes ? ntypes : 1));
    const unsigned char** keys = (const unsigned char**)std::malloc(sizeof(*keys) * (size_t)(nkeys ? nkeys : 1));
    size_t at = 40;
    int ok = types && keys && r[14] == 0 && (__nexa_x_get16(r + 12) & 3) == 3;
    for (int i = 0; ok && i < ntypes; i++) {
        if (at + 8 > len) { ok = 0; break; }
        types[i] = r + at;
        const size_t nmap = r[at + 5];
        at += 8 + nmap * 8 + (r[at + 6] ? nmap * 4 : 0);
    }
    for (int i = 0; ok && i < nkeys; i++) {
        if (at + 8 > len) { ok = 0; break; }
        keys[i] = r + at;
        at += 8 + 4 * (size_t)__nexa_x_get16(r + at + 6);
    }
    if (!ok || at > len) {
        std::free(r);
        std::free(types);
        std::free(keys);
        return 0;
    }
    d->xkb_map = r;
    d->xkb_types = types;
    d->xkb_ntypes = ntypes;
    d->xkb_keys = keys;
    d->xkb_first = first;
    d->xkb_nkeys = nkeys;
    return 1;
}

// The core map, and which modifier bits Num Lock, Mode_switch and Lock are on.
static int __nexa_x_core_load(Display* d) {
    unsigned char* p = __nexa_x_req(d, 101, 0, 2);
    p[4] = (unsigned char)d->min_kc;
    p[5] = (unsigned char)(d->max_kc - d->min_kc + 1);
    const unsigned long s1 = d->seq;
    __nexa_x_req(d, 119, 0, 1);
    const unsigned long s2 = d->seq;
    unsigned char* km = __nexa_x_reply(d, s1);
    unsigned char* mm = __nexa_x_reply(d, s2);
    int ok = 0;
    if (km && km[1]) {
        const int per = km[1];
        const size_t n = (size_t)(d->max_kc - d->min_kc + 1) * (size_t)per;
        d->keys = (KeySym*)std::malloc(sizeof(KeySym) * n);
        if (d->keys) {
            for (size_t i = 0; i < n; i++) d->keys[i] = __nexa_x_get32(km + 32 + 4 * i);
            d->per = per;
            ok = 1;
        }
    }
    d->num_lock = d->mode_switch = 0;
    d->lock_meaning = NoSymbol;
    if (ok && mm) {
        const int kpm = mm[1];
        for (int m = 0; m < 8; m++) {
            for (int k = 0; k < kpm; k++) {
                const int kc = mm[32 + m * kpm + k];
                if (kc < d->min_kc || kc > d->max_kc) continue;
                const KeySym* syms = &d->keys[(size_t)(kc - d->min_kc) * (size_t)d->per];
                for (int j = 0; j < d->per; j++) {
                    if (m == 1) {
                        if (syms[j] == XK_Caps_Lock) d->lock_meaning = XK_Caps_Lock;
                        else if (syms[j] == XK_Shift_Lock && d->lock_meaning != XK_Caps_Lock) d->lock_meaning = XK_Shift_Lock;
                    } else if (m >= 3) {
                        if (syms[j] == XK_Num_Lock) d->num_lock |= 1u << m;
                        if (syms[j] == XK_Mode_switch) d->mode_switch |= 1u << m;
                    }
                }
            }
        }
    }
    std::free(km);
    std::free(mm);
    return ok;
}

// Read on first use and again after a MappingNotify. XKB is asked for once:
// using it is what makes the server put the group into key events' state.
static int __nexa_x_keymap(Display* d) {
    if (d->keys || d->xkb_map) return 1;
    if (d->dead) return 0;
    if (d->xkb == 0) {
        d->xkb = -1;
        unsigned char* q = __nexa_x_query_ext(d, "XKEYBOARD");
        if (q && q[8]) {
            const int major = q[9];
            unsigned char* p = __nexa_x_req(d, major, 0, 2);
            __nexa_x_put16(p + 4, 1);
            unsigned char* u = __nexa_x_reply(d, d->seq);
            if (u && u[1]) {
                d->xkb = 1;
                d->xkb_major = major;
                d->xkb_event = q[10];
                // XkbSelectEvents: NewKeyboardNotify and MapNotify, every
                // detail of both, so that a remapped key is read afresh.
                unsigned char* e = __nexa_x_req(d, major, 1, 4);
                __nexa_x_put16(e + 4, 0x100);
                __nexa_x_put16(e + 6, 3);
                __nexa_x_put16(e + 10, 3);
                __nexa_x_put16(e + 12, 0xff);
                __nexa_x_put16(e + 14, 0xff);
            }
            std::free(u);
        }
        std::free(q);
    }
    if (d->xkb > 0 && __nexa_x_xkb_load(d)) return 1;
    return __nexa_x_core_load(d);
}

static void __nexa_x_case(KeySym s, KeySym* lower, KeySym* upper) {
    *lower = *upper = s;
    if (s >= 'A' && s <= 'Z') *lower = s + 0x20;
    else if (s >= 'a' && s <= 'z') *upper = s - 0x20;
    else if ((s >= 0xc0 && s <= 0xd6) || (s >= 0xd8 && s <= 0xde)) *lower = s + 0x20;
    else if ((s >= 0xe0 && s <= 0xf6) || (s >= 0xf8 && s <= 0xfe)) *upper = s - 0x20;
}

static const unsigned char* __nexa_x_xkb_key(Display* d, unsigned kc) {
    if ((int)kc < d->xkb_first || (int)kc >= d->xkb_first + d->xkb_nkeys) return nullptr;
    const unsigned char* k = d->xkb_keys[kc - (unsigned)d->xkb_first];
    return (k[4] & 0x0f) ? k : nullptr;
}

static const unsigned char* __nexa_x_xkb_type(Display* d, const unsigned char* k, int group) {
    return k[group] < d->xkb_ntypes ? d->xkb_types[k[group]] : nullptr;
}

static KeySym __nexa_x_xkb_sym(const unsigned char* k, size_t i) {
    return i < __nexa_x_get16(k + 6) ? __nexa_x_get32(k + 8 + 4 * i) : NoSymbol;
}

// XkbTranslateKeyCode: the keysym the state picks, and the modifiers that
// picking it used up.
static int __nexa_x_xkb_translate(Display* d, unsigned kc, unsigned state, unsigned* used, KeySym* out) {
    *used = 0;
    *out = NoSymbol;
    const unsigned char* k = __nexa_x_xkb_key(d, kc);
    if (!k) return 0;
    const int ngroups = k[4] & 0x0f;
    int group = (int)((state >> 13) & 3);
    if (group >= ngroups) {
        if ((k[4] & 0xc0) == 0x40) group = ngroups - 1;
        else if ((k[4] & 0xc0) == 0x80) { group = (k[4] >> 4) & 3; if (group >= ngroups) group = 0; }
        else group %= ngroups;
    }
    const unsigned char* t = __nexa_x_xkb_type(d, k, group);
    if (!t) return 0;
    size_t col = (size_t)group * k[5];
    unsigned preserve = 0;
    const int nmap = t[5];
    for (int i = 0; i < nmap; i++) {
        const unsigned char* e = t + 8 + 8 * i;
        if (e[0] && (state & t[0]) == e[1]) {
            col += e[2];
            if (t[6]) preserve = t[8 + 8 * nmap + 4 * i];
            break;
        }
    }
    *out = __nexa_x_xkb_sym(k, col);
    *used = t[0] & ~preserve;
    return *out != NoSymbol;
}

// XKeycodeToKeysym's "column": group and level, with the core protocol's
// promise of two symbols in each of the first two groups.
static KeySym __nexa_x_kc2ks(Display* d, unsigned kc, int col) {
    if (!__nexa_x_keymap(d) || col < 0) return NoSymbol;
    if (d->xkb_map) {
        const unsigned char* k = __nexa_x_xkb_key(d, kc);
        if (!k || col > 3) return NoSymbol;
        const int group = col >> 1;
        int level = col & 1;
        if (group >= (k[4] & 0x0f)) return NoSymbol;
        const unsigned char* t = __nexa_x_xkb_type(d, k, group);
        const int width = t ? t[4] : 0;
        if (level >= width) {
            if (group > 1 || width != 1 || level != 1) return NoSymbol;
            level = 0;
        }
        return __nexa_x_xkb_sym(k, (size_t)group * k[5] + (size_t)level);
    }
    int per = d->per;
    if ((col >= per && col > 3) || (int)kc < d->min_kc || (int)kc > d->max_kc) return NoSymbol;
    const KeySym* syms = &d->keys[(size_t)(kc - (unsigned)d->min_kc) * (size_t)d->per];
    if (col < 4) {
        if (col > 1) {
            while (per > 2 && syms[per - 1] == NoSymbol) per--;
            if (per < 3) col -= 2;
        }
        if (per <= (col | 1) || syms[col | 1] == NoSymbol) {
            KeySym lo, up;
            __nexa_x_case(syms[col & ~1], &lo, &up);
            if (!(col & 1)) return lo;
            return up == lo ? NoSymbol : up;
        }
    }
    return syms[col];
}

static KeyCode XKeysymToKeycode(Display* d, KeySym ks) {
    if (!__nexa_x_keymap(d)) return 0;
    if (d->xkb_map) {
        for (size_t j = 0;; j++) {
            int any = 0;
            for (int i = d->min_kc; i <= d->max_kc; i++) {
                const unsigned char* k = __nexa_x_xkb_key(d, (unsigned)i);
                if (!k || j >= __nexa_x_get16(k + 6)) continue;
                any = 1;
                if (__nexa_x_xkb_sym(k, j) == ks) return (KeyCode)i;
            }
            if (!any) return 0;
        }
    }
    for (int j = 0; j < d->per; j++)
        for (int i = d->min_kc; i <= d->max_kc; i++)
            if (__nexa_x_kc2ks(d, (unsigned)i, j) == ks) return (KeyCode)i;
    return 0;
}

static KeySym XLookupKeysym(XKeyEvent* e, int index) {
    return __nexa_x_kc2ks(e->display, e->keycode, index);
}

// KeyBind.c's XTranslateKey, for a server with no XKB.
static KeySym __nexa_x_core_translate(Display* d, unsigned kc, unsigned mods) {
    if ((int)kc < d->min_kc || (int)kc > d->max_kc) return NoSymbol;
    const KeySym* syms = &d->keys[(size_t)(kc - (unsigned)d->min_kc) * (size_t)d->per];
    int per = d->per;
    while (per > 2 && syms[per - 1] == NoSymbol) per--;
    if (per > 2 && (mods & d->mode_switch)) { syms += 2; per -= 2; }
    KeySym sym, lo, up;
    if ((mods & d->num_lock) && per > 1 &&
        ((syms[1] >= 0xff80 && syms[1] <= 0xffbd) || (syms[1] >= 0x11000000 && syms[1] <= 0x1100ffff))) {
        sym = ((mods & ShiftMask) || ((mods & LockMask) && d->lock_meaning == XK_Shift_Lock)) ? syms[0] : syms[1];
    } else if (!(mods & ShiftMask) && (!(mods & LockMask) || d->lock_meaning == NoSymbol)) {
        if (per == 1 || syms[1] == NoSymbol) __nexa_x_case(syms[0], &sym, &up);
        else sym = syms[0];
    } else if (!(mods & LockMask) || d->lock_meaning != XK_Caps_Lock) {
        if (per == 1 || (up = syms[1]) == NoSymbol) __nexa_x_case(syms[0], &lo, &up);
        sym = up;
    } else {
        KeySym s = (per == 1 || syms[1] == NoSymbol) ? syms[0] : syms[1];
        __nexa_x_case(s, &lo, &up);
        if (!(mods & ShiftMask) && s != syms[0] && (s != up || lo == up)) __nexa_x_case(syms[0], &lo, &up);
        sym = up;
    }
    return sym == XK_VoidSymbol ? NoSymbol : sym;
}

// A keysym as a Latin-1 character, as libX11 converts one for a program in
// the C locale: Latin-1 and its Unicode keysyms, the dead accents that have
// a spacing form there, and the function keys that stand for an ASCII
// control. -1 for everything else.
static int __nexa_x_latin1(KeySym s) {
    if ((s & 0xffffff00) == 0xfe00) {
        switch (s) {
            case 0xfe50: return 0x60;  // dead_grave
            case 0xfe51: return 0xb4;  // dead_acute
            case 0xfe52: return 0x5e;  // dead_circumflex
            case 0xfe53: return 0x7e;  // dead_tilde
            case 0xfe54: return 0xaf;  // dead_macron
            case 0xfe57: return 0xa8;  // dead_diaeresis
            case 0xfe58: return 0xb0;  // dead_abovering
            case 0xfe5b: return 0xb8;  // dead_cedilla
            default: return -1;
        }
    }
    if ((s & 0xffffff00) == 0xff00) {
        if ((s >= XK_BackSpace && s <= XK_Clear) || s == XK_Return || s == XK_Escape || s == XK_KP_Tab ||
            s == XK_KP_Enter || (s >= XK_KP_Multiply && s <= XK_KP_9) || s == XK_KP_Equal || s == XK_Delete)
            return (int)(s & 0x7f);
        return s == XK_KP_Space ? ' ' : -1;
    }
    if ((s & 0xff000000) == 0x01000000) s &= 0xffffff;
    if ((s >= 0x20 && s <= 0x7e) || (s >= 0xa0 && s <= 0xff)) return (int)s;
    return -1;
}

static int XLookupString(XKeyEvent* e, char* buf, int nbytes, KeySym* ks, void* status) {
    (void)status;
    Display* d = e->display;
    KeySym dummy;
    if (!ks) ks = &dummy;
    *ks = NoSymbol;
    if (!__nexa_x_keymap(d)) return 0;
    unsigned mods = e->state;
    if (d->xkb_map) {
        unsigned used = 0;
        if (!__nexa_x_xkb_translate(d, e->keycode, e->state, &used, ks)) return 0;
        mods &= ~used;
        if (mods & LockMask) { KeySym lo; __nexa_x_case(*ks, &lo, ks); }
    } else {
        *ks = __nexa_x_core_translate(d, e->keycode, mods);
    }
    int c = *ks ? __nexa_x_latin1(*ks) : -1;
    if (c < 0 || nbytes < 1) return 0;
    if (mods & ControlMask) {
        if ((c >= '@' && c < 0x7f) || c == ' ') c &= 0x1f;
        else if (c == '2') c = 0;
        else if (c >= '3' && c <= '7') c -= '3' - 0x1b;
        else if (c == '8') c = 0x7f;
        else if (c == '/') c = '_' & 0x1f;
    }
    buf[0] = (char)c;
    return 1;
}

// --- images -----------------------------------------------------------------

static XImage* XCreateImage(Display* d, Visual* v, unsigned int depth, int format, int offset, char* data,
                            unsigned int w, unsigned int h, int pad, int bytes_per_line) {
    if (depth == 0 || depth > 32 || format != ZPixmap) return nullptr;
    XImage* img = (XImage*)std::calloc(1, sizeof(XImage));
    if (!img) return nullptr;
    img->width = (int)w;
    img->height = (int)h;
    img->xoffset = offset;
    img->format = format;
    img->data = data;
    img->byte_order = d->image_byte_order;
    img->bitmap_unit = d->bitmap_unit;
    img->bitmap_bit_order = d->bitmap_bit_order;
    img->bitmap_pad = pad;
    img->depth = (int)depth;
    img->bits_per_pixel = d->fmt_bpp[depth] ? d->fmt_bpp[depth] : 32;
    img->bytes_per_line = bytes_per_line ? bytes_per_line
        : (int)((((size_t)w * (size_t)img->bits_per_pixel + (size_t)pad - 1) / (size_t)pad) * (size_t)pad / 8);
    if (v) { img->red_mask = v->red_mask; img->green_mask = v->green_mask; img->blue_mask = v->blue_mask; }
    return img;
}

static int XDestroyImage(XImage* img) {
    if (!img) return 0;
    std::free(img->data);
    std::free(img);
    return 1;
}

// As many whole rows to a request as the server takes, straight from the
// image: Xlib does the same, a strip at a time. A part of a row is copied
// out first, since then the rows are not end to end.
static int XPutImage(Display* d, Drawable dr, GC gc, XImage* img, int sx, int sy, int dx, int dy,
                     unsigned int w, unsigned int h) {
    if (!img || !img->data || !w || !h || d->dead) return 0;
    const int depth = img->depth;
    const int bpp = img->bits_per_pixel;
    const int pad = d->fmt_pad[depth] ? d->fmt_pad[depth] : 32;
    const size_t row = (((size_t)w * (size_t)bpp + (size_t)pad - 1) / (size_t)pad) * (size_t)pad / 8;
    const size_t limit = (d->max_req ? d->max_req : 262140) - 24;
    if (row > limit) return 0;
    const int contiguous = sx == 0 && (size_t)img->bytes_per_line == row;
    size_t rows = limit / row;
    if (rows > h) rows = h;
    unsigned char* tmp = contiguous ? nullptr : (unsigned char*)std::malloc(row * rows);
    if (!contiguous && !tmp) return 0;
    for (unsigned y = 0; y < h && !d->dead;) {
        unsigned n = (unsigned)rows;
        if (n > h - y) n = h - y;
        const unsigned char* src = (const unsigned char*)img->data + (size_t)(sy + (int)y) * (size_t)img->bytes_per_line;
        if (!contiguous) {
            for (unsigned r = 0; r < n; r++)
                std::memcpy(tmp + r * row, src + (size_t)r * (size_t)img->bytes_per_line + (size_t)sx * (size_t)bpp / 8, row);
            src = tmp;
        }
        unsigned char head[24];
        std::memset(head, 0, sizeof(head));
        head[0] = 72;
        head[1] = ZPixmap;
        __nexa_x_put16(head + 2, (unsigned)(6 + (row * n + 3) / 4));
        __nexa_x_put32(head + 4, dr);
        __nexa_x_put32(head + 8, (unsigned long)gc);
        __nexa_x_put16(head + 12, w);
        __nexa_x_put16(head + 14, n);
        __nexa_x_put16(head + 16, (unsigned)dx);
        __nexa_x_put16(head + 18, (unsigned)(dy + (int)y));
        head[21] = (unsigned char)depth;
        __nexa_x_req_big(d, head, 24, src, row * n);
        y += n;
    }
    std::free(tmp);
    return 1;
}

static int XFree(void* p) { std::free(p); return 1; }
#pragma GCC diagnostic pop
)NEXA_X11W";
}

}  // namespace nexa
