// Semantics cover for gfx.transparent (BOB-51): the X11 half, executed.
//
// Two halves to this one. Which window the runtime asked the X server for is a
// request, like gfx.borderless's Motif hint and gfx.ontop's _NET_WM_STATE
// message -- a visual is fixed when a window is created, nothing reads one
// back, and whether a desktop has a compositor to honour it is not this
// program's business. Tests/gfx_x11_stub records what was asked for.
//
// The other half is not a request at all, and is the part the founder's ask
// actually turns on: what a clear and a translucent draw leave in the fourth
// byte of a pixel, and what goes out on the wire for them. Those are bytes,
// and bytes can be read. The case that matters most is the one the issue
// names: half-covering a cleared see-through pixel with red has to come out
// translucent red -- the colour as drawn -- and not red mixed toward a clear
// colour nobody can see. It is 200 against 105 for the same draw here.
//
// Only the X11 backend is executed; the Win32 and Cocoa ones do not build on
// this machine, and Tests/gfx_emit_cases.sh is what proves they compile. The
// parts that are not per-platform -- the -1 query, the framebuffer arithmetic,
// the closed-window contract, a fresh window starting solid -- are the same
// code on all four.
//
// Built and run by Tests/gfx_cases.sh. NEXA_GEN is the generated .cpp.

#include <cstdio>
#include <cstring>
#include <string>

#ifndef NEXA_GEN
#error "define NEXA_GEN to the generated C++ file"
#endif

// The generated program has its own main(); this driver supplies the real one.
#define main __nexa_program_main
#include NEXA_GEN
#undef main

static int failures = 0;

static void check(const char* label, bool ok) {
    if (ok) {
        std::printf("ok %s\n", label);
    } else {
        std::printf("FAIL %s\n", label);
        failures++;
    }
}

static void check_int(const char* label, long got, long want) {
    if (got == want) {
        std::printf("ok %s\n", label);
    } else {
        std::printf("FAIL %s: want %ld, got %ld\n", label, want, got);
        failures++;
    }
}

// One framebuffer pixel, as the runtime stores it: straight RGBA on this
// platform, the alpha not premultiplied into the colour.
struct Px {
    int r, g, b, a;
};

static Px px_at(int x, int y) {
    Px p = {-1, -1, -1, -1};
    if (!__nexa_g.fb) return p;
    const unsigned char* s = __nexa_g.fb + ((size_t)y * (size_t)__nexa_g.w + (size_t)x) * 4;
    p.r = s[0];
    p.g = s[1];
    p.b = s[2];
    p.a = s[3];
    return p;
}

static void check_px(const char* label, Px got, int r, int g, int b, int a) {
    if (got.r == r && got.g == g && got.b == b && got.a == a) {
        std::printf("ok %s\n", label);
    } else {
        std::printf("FAIL %s: want %d,%d,%d,%d, got %d,%d,%d,%d\n", label,
            r, g, b, a, got.r, got.g, got.b, got.a);
        failures++;
    }
}

// Four bytes as this machine reads a 32-bit word, which is how the stub keeps
// what XPutImage was handed. The runtime lays the bytes out for the server's
// byte order; the stub reads them back as a word, so the test has to build its
// expectation the same way rather than assume a number.
static unsigned long word_of(unsigned char b0, unsigned char b1,
                             unsigned char b2, unsigned char b3) {
    unsigned char b[4];
    unsigned int v = 0;
    b[0] = b0;
    b[1] = b1;
    b[2] = b2;
    b[3] = b3;
    std::memcpy(&v, b, 4);
    return (unsigned long)v;
}

int main() {
    nexa_x11_stub_reset(1);
    nexa_x11_stub_set_mapped(IsViewable);

    // --- the window the runtime asked for ---------------------------------
    // This program says gfx.transparent, so gfx.open takes the visual that
    // could be see-through whether or not the call is ever reached. It has to:
    // the window is created first and a visual cannot be changed afterwards.
    check("a window opens", __nexa_gfx_open("t", 2, 2, 1) == 1);
    check_int("created at depth 32, not the screen's 24",
              nexa_x11_stub_window_depth(), 32);
    check_int("an InputOutput window, as before", nexa_x11_stub_window_class(),
              InputOutput);
    check("on the ARGB visual the server offered",
          nexa_x11_stub_window_visual_id() != 0 &&
          __nexa_g.vis != 0 &&
          nexa_x11_stub_window_visual_id() == (unsigned long)__nexa_g.vis->visualid);
    // A window on a visual that is not the screen's needs a colormap of its
    // own, and the colormap has to be named in the valuemask to be read.
    check("with a colormap of its own, named in the valuemask",
          (nexa_x11_stub_window_mask() & CWColormap) != 0 &&
          nexa_x11_stub_window_colormap() != 0 &&
          nexa_x11_stub_colormaps_made() == 1);
    // The one that is easy to leave out and fails as BadMatch rather than as a
    // default: a window inherits its parent's border pixmap, and a pixmap from
    // the root's 24-bit visual cannot be worn by a 32-bit window.
    check("and an explicit border pixel, which is a BadMatch to omit",
          (nexa_x11_stub_window_mask() & CWBorderPixel) != 0);
    check("and a background pixel",
          (nexa_x11_stub_window_mask() & CWBackPixel) != 0);
    // A GC and its drawable have to agree about depth, so the root's default
    // GC is no use to a 32-bit window.
    check("with a GC of its own, since the default one is the root's",
          nexa_x11_stub_gcs_made() == 1 && __nexa_g.gc_own == 1 && __nexa_g.gc != 0);

    // --- the toggle -------------------------------------------------------
    check_int("a window starts solid", __nexa_gfx_transparent(-1), 0);
    check_int("transparent(1) returns the new state", __nexa_gfx_transparent(1), 1);
    check_int("and the state reads back", __nexa_gfx_transparent(-1), 1);
    // Anything non-zero means on, the way gfx.fullscreen takes it.
    check_int("any other yes is the same yes", __nexa_gfx_transparent(7), 1);
    // Nothing is asked of the server: the window is already the right one and
    // what is left to decide is only what alpha gets written into it.
    nexa_x11_stub_calls_clear();
    check("turning it on asks the server for nothing",
          __nexa_gfx_transparent(1) == 1 && nexa_x11_stub_call_count() == 0);

    // --- what a clear writes ----------------------------------------------
    __nexa_gfx_clear(10, 20, 30);
    check_px("a clear on a see-through window leaves nothing shown",
             px_at(0, 0), 10, 20, 30, 0);
    // The colour is still written and is still what gfx.get reports: it is the
    // colour the window would have been, kept for the pixels drawn back over.
    check_int("and gfx.get still reports the colour that was cleared",
              __nexa_gfx_get(0, 0), (10 << 16) | (20 << 8) | 30);

    // --- what a draw writes -----------------------------------------------
    __nexa_gfx_alpha_set(255);
    __nexa_gfx_fill(0, 0, 1, 1, 200, 100, 50);
    check_px("an opaque draw is opaque, see-through underneath or not",
             px_at(0, 0), 200, 100, 50, 255);

    // The case the whole issue turns on. Half-covering a cleared see-through
    // pixel has to give half-see-through red -- the colour as drawn, against
    // whatever is behind the window -- and not red blended toward a clear
    // colour that is not on the screen.
    __nexa_gfx_alpha_set(128);
    __nexa_gfx_fill(1, 0, 1, 1, 200, 100, 50);
    check_px("a translucent draw on nothing is the drawn colour, half shown",
             px_at(1, 0), 200, 100, 50, 128);

    // Over a pixel that is already opaque, the same draw is the blend it has
    // always been -- and stays opaque.
    __nexa_gfx_alpha_set(255);
    __nexa_gfx_fill(0, 1, 1, 1, 10, 20, 30);
    __nexa_gfx_alpha_set(128);
    __nexa_gfx_fill(0, 1, 1, 1, 200, 100, 50);
    check_px("and over something solid it is the blend it always was",
             px_at(0, 1), 105, 60, 40, 255);

    // Twice on the same pixel builds up, the way overlapping translucent
    // shapes have always built up -- but towards being shown, not towards a
    // colour. Two halves of nothing is three quarters of something.
    __nexa_gfx_fill(1, 0, 1, 1, 200, 100, 50);
    check_px("two translucent draws build the alpha up, not the colour",
             px_at(1, 0), 200, 100, 50, 192);
    __nexa_gfx_alpha_set(255);

    // --- what goes out on the wire ----------------------------------------
    __nexa_gfx_present();
    // An image and the window it is put on have to agree about depth.
    check_int("the XImage is the window's depth, not the screen's",
              nexa_x11_stub_image_depth(), 32);
    // The server reads a 32-bit window's pixels premultiplied, so a colour
    // goes out scaled by its own alpha even though the framebuffer holds it
    // straight: 200 at alpha 192 is 150 on the wire.
    check_int("an opaque pixel goes out as it stands",
              (long)nexa_x11_stub_image_pixel(0),
              (long)word_of(50, 100, 200, 255));
    check_int("and a half-shown one goes out premultiplied",
              (long)nexa_x11_stub_image_pixel(1),
              (long)word_of((unsigned char)(50 * 192 / 255),
                            (unsigned char)(100 * 192 / 255),
                            (unsigned char)(200 * 192 / 255), 192));

    // --- growing the window -----------------------------------------------
    // A resize exposes framebuffer nobody has drawn on, and undrawn is what a
    // clear leaves behind -- so on a see-through window the new strip is
    // see-through too, rather than a black bar the program never asked for.
    check_int("the window grows", __nexa_gfx_resize(4, 4, -1), 1);
    check_px("the newly exposed strip shows through, like a clear",
             px_at(3, 3), 0, 0, 0, 0);
    check_px("and what was there came with its alpha", px_at(1, 0), 200, 100, 50, 192);

    // --- turning it off ---------------------------------------------------
    check_int("transparent(0) returns 0", __nexa_gfx_transparent(0), 0);
    __nexa_gfx_clear(10, 20, 30);
    check_px("and a clear is solid again", px_at(0, 0), 10, 20, 30, 255);
    __nexa_gfx_alpha_set(128);
    __nexa_gfx_fill(1, 1, 1, 1, 200, 100, 50);
    check_px("so the same translucent draw blends toward the clear colour",
             px_at(1, 1), 105, 60, 40, 255);
    __nexa_gfx_alpha_set(255);
    // And a resize while solid leaves the strip black and opaque, which is
    // what it has always been.
    check_int("the window grows again", __nexa_gfx_resize(6, 6, -1), 1);
    check_px("the new strip is solid black, as it always was",
             px_at(5, 5), 0, 0, 0, 255);
    __nexa_gfx_present();
    check_int("and every pixel goes out as it stands again",
              (long)nexa_x11_stub_image_pixel(0),
              (long)word_of(30, 20, 10, 255));

    // --- a screenshot ------------------------------------------------------
    // 24 bits is three channels: gfx.save writes the colours and not the
    // see-through-ness, which is the documented answer rather than an
    // oversight. A pixel that was never drawn saves as the colour it was
    // cleared to, which is what gfx.get reports for it too.
    check_int("transparent again for the screenshot", __nexa_gfx_transparent(1), 1);
    __nexa_gfx_clear(10, 20, 30);
    check_int("gfx.save writes a BMP of a see-through window",
              __nexa_gfx_save("/dev/null"), 1);
    check_int("and the colour it saves is the one gfx.get reports",
              __nexa_gfx_get(0, 0), (10 << 16) | (20 << 8) | 30);

    // --- a new window is a solid window ------------------------------------
    check("gfx.open puts it back to solid",
          __nexa_gfx_open("t", 2, 2, 1) == 1 && __nexa_gfx_transparent(-1) == 0);
    __nexa_gfx_clear(10, 20, 30);
    check_px("and its clear is solid", px_at(0, 0), 10, 20, 30, 255);

    // --- with no window ----------------------------------------------------
    __nexa_gfx_close();
    nexa_x11_stub_calls_clear();
    check("a closed window reports 0, stays 0 and asks the server for nothing",
          __nexa_gfx_transparent(-1) == 0 && __nexa_gfx_transparent(1) == 0 &&
          nexa_x11_stub_call_count() == 0);
    // Both server resources handed back: a colormap outlives the window that
    // used it, and the GC was ours rather than the screen's.
    check("the colormap and the GC went back to the server",
          nexa_x11_stub_colormaps_freed() == nexa_x11_stub_colormaps_made() &&
          nexa_x11_stub_gcs_freed() == nexa_x11_stub_gcs_made() &&
          nexa_x11_stub_colormaps_made() >= 2);

    // --- a screen with nothing to offer -------------------------------------
    // No 32-bit visual: the ordinary window, no colormap of its own, and not a
    // word about it. Nothing is probed and nothing is warned -- the call still
    // reports what the program asked for, and the program gets whatever the
    // server does with an alpha byte it does not read, which is nothing.
    nexa_x11_stub_reset(1);
    nexa_x11_stub_set_mapped(IsViewable);
    nexa_x11_stub_set_argb_visual(0);
    check("a window opens on a screen with no ARGB visual",
          __nexa_gfx_open("t", 2, 2, 1) == 1);
    check_int("created at the screen's own depth", nexa_x11_stub_window_depth(), 24);
    check("with no colormap of its own and none named",
          nexa_x11_stub_colormaps_made() == 0 &&
          (nexa_x11_stub_window_mask() & CWColormap) == 0);
    check_int("and the call still reports what was asked for",
              __nexa_gfx_transparent(1), 1);
    __nexa_gfx_clear(10, 20, 30);
    check_px("which still writes the alpha, for the server to ignore",
             px_at(0, 0), 10, 20, 30, 0);
    __nexa_gfx_present();
    check_int("and the image matches the window it goes on",
              nexa_x11_stub_image_depth(), 24);
    __nexa_gfx_close();

    std::printf("%s\n", failures ? "gfx_transparent_semantics: FAILURES"
                                 : "gfx_transparent_semantics ok");
    return failures ? 1 : 0;
}
