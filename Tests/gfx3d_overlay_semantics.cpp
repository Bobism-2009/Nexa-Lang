// Semantics cover for std/gfx drawn over a std/gfx3d window: the CPU half.
//
// The overlay has two halves and only one of them can be checked here. The
// GL half -- uploading gfx's framebuffer and blending it over the frame at
// gfx3d.present -- needs a window and a driver, and nothing in this suite has
// either. The other half is where the decisions are: that gfx is handed a
// framebuffer the size of the 3D window, that it starts see-through and goes
// back to see-through at every gfx3d.clear, that it follows a resize, that it
// is given back at gfx3d.close, and that a program whose gfx has a window of
// its own keeps it. None of that touches GL, so the bridge's hooks are called
// directly and gfx3d.open never is: no window is made.
//
// Built and run by Tests/gfx3d_cases.sh. NEXA_GEN is the generated .cpp of a
// program that draws with both modules, so the bridge is in it.

#include <cstdio>

#ifndef NEXA_GEN
#error "define NEXA_GEN to the generated C++ file"
#endif

#define main __nexa_program_main
#include NEXA_GEN
#undef main

static int failures = 0;

static void check(const char* what, long got, long want) {
    if (got != want) {
        std::printf("FAIL %s: want %ld, got %ld\n", what, want, got);
        failures++;
    }
}

// The alpha byte is the fourth whatever the colour order, which differs
// between Windows and everywhere else.
static int alpha_at(int x, int y) {
    return __nexa_g.fb[((size_t)y * (size_t)__nexa_g.w + (size_t)x) * 4 + 3];
}

int main() {
    // Before a 3D window, gfx has nowhere to draw and says so.
    check("width before any window", __nexa_gfx_width(), 0);

    __nexa_g3_overlay_open(320, 200);
    check("overlay flag", __nexa_g.overlay, 1);
    check("width is the window's", __nexa_gfx_width(), 320);
    check("height is the window's", __nexa_gfx_height(), 200);
    check("scale is 1", __nexa_gfx_scale(), 1);
    check("starts see-through", alpha_at(5, 5), 0);

    __nexa_gfx_fill(10, 10, 20, 20, 255, 0, 0);
    check("opaque fill is opaque", alpha_at(15, 15), 255);
    check("outside it still clear", alpha_at(50, 50), 0);

    __nexa_gfx_alpha_set(160);
    __nexa_gfx_fill(100, 100, 10, 10, 0, 0, 0);
    check("translucent fill alpha", alpha_at(105, 105), 160);
    __nexa_gfx_alpha_set(255);

    // gfx.clear in overlay mode: whole layer back to see-through.
    __nexa_gfx_clear(40, 80, 120);
    check("gfx.clear -> see-through", alpha_at(15, 15), 0);

    // A new frame clears whatever was drawn.
    __nexa_gfx_fill(0, 0, 5, 5, 1, 2, 3);
    __nexa_g3_overlay_frame(320, 200);
    check("next frame starts clear", alpha_at(2, 2), 0);

    // The window was resized: the layer follows.
    __nexa_g3_overlay_frame(640, 480);
    check("resize width", __nexa_gfx_width(), 640);
    check("resize height", __nexa_gfx_height(), 480);
    __nexa_gfx_fill(600, 440, 10, 10, 9, 9, 9);
    check("draw in the new area", alpha_at(605, 445), 255);

    __nexa_g3_overlay_close();
    check("closed: flag", __nexa_g.overlay, 0);
    check("closed: width", __nexa_gfx_width(), 0);
    check("closed: fb freed", __nexa_g.fb == nullptr, 1);

    // gfx already has a window of its own: the 3D window must not take its
    // framebuffer. Faked with ready=1 rather than opening one.
    __nexa_g.ready = 1;
    unsigned char mine[4] = {0, 0, 0, 0};
    __nexa_g.fb = mine;
    __nexa_g3_overlay_open(320, 200);
    check("gfx window: not taken over", __nexa_g.overlay, 0);
    check("gfx window: fb untouched", __nexa_g.fb == mine, 1);
    __nexa_g.fb = nullptr;
    __nexa_g.ready = 0;


    if (failures == 0) {
        std::printf("gfx3d overlay semantics ok\n");
        return 0;
    }
    std::printf("gfx3d overlay semantics: %d failure(s)\n", failures);
    return 1;
}
