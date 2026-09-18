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
// That remap is also what BOB-52 was about: a withdrawn window loses the
// window-manager states it was in, so the ones the program asked for have to
// be said again around it. That half is checked here too, in the program that
// gfx.ontop has been sliced out of -- which is the case the fix has to work
// in. Tests/gfx_overlay_semantics.cpp drives the same ground with the real
// calls, in the order a program would make them.
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

    check("the hint first, then unmapped, remapped and moved",
          nexa_x11_stub_call_count() == 4 &&
          nexa_x11_stub_call(0) == NEXA_STUB_CALL_PROP &&
          nexa_x11_stub_call(1) == NEXA_STUB_CALL_UNMAP &&
          nexa_x11_stub_call(2) == NEXA_STUB_CALL_MAP &&
          nexa_x11_stub_call(3) == NEXA_STUB_CALL_MOVE);
    // A remap is a fresh placement as far as the window manager is concerned,
    // so the window has to be put back where the user had it.
    check("put back where it was",
          nexa_x11_stub_move_x() == 137 && nexa_x11_stub_move_y() == 42);
    // A window that is in no window-manager state has none to carry across the
    // remap, so nothing but the hint goes on it and no message is sent. This
    // is the shape of every program that never asks for two of these at once.
    check("nothing else is said about a plain window",
          nexa_x11_stub_property_writes() == 1 &&
          nexa_x11_stub_property_select("_NET_WM_STATE") == 0);
    nexa_x11_stub_property_select(0);

    // --- asking for what it already is --------------------------------
    nexa_x11_stub_calls_clear();
    check("asking again touches nothing",
          __nexa_gfx_borderless(1) == 1 && nexa_x11_stub_call_count() == 0);

    // --- putting it back ----------------------------------------------
    nexa_x11_stub_calls_clear();
    check_int("borderless(0) returns 0", __nexa_gfx_borderless(0), 0);
    check("the flag is still decorations, the value is back to on",
          nexa_x11_stub_property_word(0) == 2 && nexa_x11_stub_property_word(2) == 1);
    check_int("and it is remapped again", nexa_x11_stub_call_count(), 4);

    // --- the state the remap would otherwise drop ----------------------
    // Withdrawing a window is how a program loses its _NET_WM_STATE: EWMH has
    // the window manager forget the states of a window it stops managing, so
    // the remap has to say them again or the on-top request quietly dies --
    // and __nexa_g.ontop would still read 1, so asking for it again would be a
    // no-op and the program would have no way back (BOB-52).
    //
    // gfx.ontop is not in this program: the runtime is sliced to the builtins
    // a program names (BOB-25) and this driver's names only gfx.borderless, so
    // __nexa_gfx_ontop does not exist to be called. The flag does, always, and
    // that is why gfx.borderless reads the flag rather than calling its
    // neighbour. Setting it here is the same state gfx.ontop(1) would leave.
    __nexa_g.ontop = 1;
    nexa_x11_stub_calls_clear();
    check_int("borderless(1) on a window that is on top", __nexa_gfx_borderless(1), 1);
    check("the hint, the state while withdrawn, the remap, then the state again",
          nexa_x11_stub_call_count() == 6 &&
          nexa_x11_stub_call(0) == NEXA_STUB_CALL_PROP &&
          nexa_x11_stub_call(1) == NEXA_STUB_CALL_UNMAP &&
          nexa_x11_stub_call(2) == NEXA_STUB_CALL_PROP &&
          nexa_x11_stub_call(3) == NEXA_STUB_CALL_MAP &&
          nexa_x11_stub_call(4) == NEXA_STUB_CALL_MOVE &&
          nexa_x11_stub_call(5) == NEXA_STUB_CALL_SEND);
    // The property is written between the unmap and the map on purpose: a
    // window manager that reads a window's states when it takes the window on
    // reads them then, and a client is allowed to set them while withdrawn.
    check("the second write is _NET_WM_STATE", nexa_x11_stub_property_select("_NET_WM_STATE") == 1);
    // A list of atoms says so by taking XA_ATOM as its type; the Motif hint
    // above is the other convention, where the type is the property's own.
    check("a list of atoms, one of them",
          nexa_x11_stub_property_type() == XA_ATOM &&
          nexa_x11_stub_property_format() == 32 &&
          nexa_x11_stub_property_count() == 1);
    check("and the state in the list is ABOVE",
          std::strcmp(nexa_x11_stub_property_word_name(0), "_NET_WM_STATE_ABOVE") == 0);
    check("the hint is still on the window as well",
          nexa_x11_stub_property_select("_MOTIF_WM_HINTS") == 1 &&
          nexa_x11_stub_property_word(2) == 0);
    nexa_x11_stub_property_select(0);
    // And said once more as a message, for the window manager that had already
    // deleted the property on its way out of managing the window -- which it
    // is free to do after the write above and before the map.
    check("the message adds ABOVE, about our window, to the root",
          nexa_x11_stub_message_is_client() == 1 &&
          std::strcmp(nexa_x11_stub_message_name(), "_NET_WM_STATE") == 0 &&
          nexa_x11_stub_message_word(0) == _NET_WM_STATE_ADD &&
          std::strcmp(nexa_x11_stub_message_word_name(1), "_NET_WM_STATE_ABOVE") == 0 &&
          nexa_x11_stub_message_word(3) == 1 &&
          nexa_x11_stub_message_window() == (unsigned long)__nexa_g.win &&
          nexa_x11_stub_message_target() == (unsigned long)DefaultRootWindow(__nexa_g.dpy));
    // The flag is left exactly as it was: this call is about the frame, and a
    // program that reads gfx.ontop back after it gets the answer it gave.
    check_int("and the window is still, as far as the program asked, on top",
              __nexa_g.ontop, 1);
    __nexa_g.ontop = 0;
    __nexa_gfx_borderless(0);

    // --- a window that is not on screen -------------------------------
    // Nothing has framed it yet, so there is nothing to make reread the hint.
    nexa_x11_stub_set_mapped(IsUnmapped);
    nexa_x11_stub_calls_clear();
    check("an unmapped window gets the property and no remap",
          __nexa_gfx_borderless(1) == 1 && nexa_x11_stub_call_count() == 1 &&
          nexa_x11_stub_call(0) == NEXA_STUB_CALL_PROP &&
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
