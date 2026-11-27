// Key translation, dumped for every keycode under sixteen modifier states.
// Tests/gfx_x11_wire_cases.sh builds it twice -- once against libX11 (-DREAL)
// and once against NexaC's own X11 client, cut out of a generated program
// into wire.inc -- runs both on the same server and compares what they print.
#ifdef REAL
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/keysym.h>
#include <cstdio>
#include <cstring>
#else
#include "wire.inc"
#endif

int main() {
    Display* d = XOpenDisplay(nullptr);
    if (!d) { std::puts("no display"); return 1; }
    // Shift, Lock, Control, each Mod, AltGr's Mod5, Num Lock's Mod2, and a
    // second group, alone and with Shift where the pair means something.
    const unsigned states[] = {0, ShiftMask, LockMask, ShiftMask | LockMask, ControlMask,
                               ControlMask | ShiftMask, 1u << 4, (1u << 4) | ShiftMask,
                               (1u << 4) | LockMask, 1u << 3, 1u << 7, (1u << 3) | ShiftMask,
                               1u << 5, 1u << 6, (1u << 6) | ShiftMask, 0x2000};
    for (int kc = 8; kc < 256; kc++) {
        for (unsigned st : states) {
            XKeyEvent e;
            std::memset(&e, 0, sizeof(e));
            e.type = KeyPress;
            e.display = d;
            e.keycode = (unsigned)kc;
            e.state = st;
            char buf[16] = {0};
            KeySym ks = 0;
            int n = XLookupString(&e, buf, sizeof(buf), &ks, nullptr);
            std::printf("kc %3d st %04x -> ks %06lx n %d", kc, st, (unsigned long)ks, n);
            for (int i = 0; i < n; i++) std::printf(" %02x", (unsigned char)buf[i]);
            std::printf("\n");
        }
        XKeyEvent e;
        std::memset(&e, 0, sizeof(e));
        e.display = d;
        e.keycode = (unsigned)kc;
        std::printf("kc %3d keysym0 %06lx keysym1 %06lx\n", kc, (unsigned long)XLookupKeysym(&e, 0),
                    (unsigned long)XLookupKeysym(&e, 1));
    }
    // The names gfx.key looks keys up by.
    const KeySym want[] = {XK_Escape, XK_space, XK_Return, XK_Up, XK_Down, XK_Left, XK_Right,
                           XK_Shift_L, XK_Shift_R, XK_Control_L, XK_Control_R, XK_Alt_L, XK_Alt_R,
                           XK_Meta_L, XK_Meta_R, XK_Tab, XK_BackSpace, XK_Delete, XK_F11,
                           'a', 'z', 'A', 'Z', '0', '9', 'q', 'Q'};
    for (KeySym k : want) std::printf("keysym %06lx -> kc %d\n", (unsigned long)k, (int)XKeysymToKeycode(d, k));
    XCloseDisplay(d);
    return 0;
}
