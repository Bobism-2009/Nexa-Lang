// Semantics cover for gfx.borderless (BOB-49): the X11 half, executed.
//
// Taking a window's frame off is one of the gfx calls whose result cannot be
// read back -- what disappears is drawn by the window manager, outside the
// window, and no X call reports it. So what is checked here is the request:
// the _MOTIF_WM_HINTS property the runtime puts on the window, and the
// unmap/map/move that makes a window manager which reads that hint only at
// framing time pick it up. Tests/gfx_x11_stub stands in for the X server and
// records both.
//
// Only the X11 backend is executed; the Win32 and Cocoa ones do not build on
// this machine, and Tests/gfx_emit_cases.sh is what proves they compile. The
// parts that are not per-platform -- the -1 query, the no-op when the state
// is already what was asked for, and the frame coming back on gfx.open -- are
// the same code on all four.
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

int main() {
    nexa_x11_stub_reset(1);
    // A window that is up and placed at 137,42 on the root.
    nexa_x11_stub_set_mapped(IsViewable);
    nexa_x11_stub_set_origin(137, 42);

    check("a window opens", __nexa_gfx_open("t", 16, 16, 1) == 1);
    check_int("a window starts framed", __nexa_gfx_borderless(-1), 0);

    // --- taking the frame off -----------------------------------------
    nexa_x11_stub_calls_clear();
    check_int("borderless(1) returns the new state", __nexa_gfx_borderless(1), 1);
    check_int("and the state reads back", __nexa_gfx_borderless(-1), 1);

    check("the property is _MOTIF_WM_HINTS",
          std::strcmp(nexa_x11_stub_property_name(), "_MOTIF_WM_HINTS") == 0);
    // Motif's own convention: the hint's type is the hint's own atom.
    check("its type is that same atom", nexa_x11_stub_property_type_matches() == 1);
    check_int("32-bit format", nexa_x11_stub_property_format(), 32);
    check_int("five words, the whole MwmHints struct", nexa_x11_stub_property_count(), 5);
    // flags = 2 is MWM_HINTS_DECORATIONS: "the decorations word is the one I
    // am setting". Setting the word without the flag changes nothing.
    check_int("flags say decorations", nexa_x11_stub_property_word(0), 2);
    check_int("decorations = none", nexa_x11_stub_property_word(2), 0);
    check("functions, input_mode and status are left alone",
          nexa_x11_stub_property_word(1) == 0 && nexa_x11_stub_property_word(3) == 0 &&
          nexa_x11_stub_property_word(4) == 0);

    check("unmapped, remapped, then moved",
          nexa_x11_stub_call_count() == 3 &&
          nexa_x11_stub_call(0) == NEXA_STUB_CALL_UNMAP &&
          nexa_x11_stub_call(1) == NEXA_STUB_CALL_MAP &&
          nexa_x11_stub_call(2) == NEXA_STUB_CALL_MOVE);
    // A remap is a fresh placement as far as the window manager is concerned,
    // so the window has to be put back where the user had it.
    check("put back where it was",
          nexa_x11_stub_move_x() == 137 && nexa_x11_stub_move_y() == 42);

    // --- asking for what it already is --------------------------------
    nexa_x11_stub_calls_clear();
    check("asking again touches nothing",
          __nexa_gfx_borderless(1) == 1 && nexa_x11_stub_call_count() == 0);

    // --- putting it back ----------------------------------------------
    nexa_x11_stub_calls_clear();
    check_int("borderless(0) returns 0", __nexa_gfx_borderless(0), 0);
    check("the flag is still decorations, the value is back to on",
          nexa_x11_stub_property_word(0) == 2 && nexa_x11_stub_property_word(2) == 1);
    check_int("and it is remapped again", nexa_x11_stub_call_count(), 3);

    // --- a window that is not on screen -------------------------------
    // Nothing has framed it yet, so there is nothing to make reread the hint.
    nexa_x11_stub_set_mapped(IsUnmapped);
    nexa_x11_stub_calls_clear();
    check("an unmapped window gets the property and no remap",
          __nexa_gfx_borderless(1) == 1 && nexa_x11_stub_call_count() == 0 &&
          nexa_x11_stub_property_word(2) == 0);

    // --- a new window is a framed window ------------------------------
    nexa_x11_stub_set_mapped(IsViewable);
    check("gfx.open puts the frame back",
          __nexa_gfx_open("t", 16, 16, 1) == 1 && __nexa_gfx_borderless(-1) == 0);

    // --- with no window -------------------------------------------------
    __nexa_gfx_close();
    check("a closed window reports 0 and stays 0",
          __nexa_gfx_borderless(-1) == 0 && __nexa_gfx_borderless(1) == 0);

    std::printf("%s\n", failures ? "gfx_borderless_semantics: FAILURES" : "gfx_borderless_semantics ok");
    return failures ? 1 : 0;
}
