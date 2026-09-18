// Semantics cover for the overlay recipe (BOB-52): gfx.borderless, gfx.ontop
// and gfx.fullscreen in one program, on X11, executed.
//
// Each of those calls has a cover of its own, and each of those covers drives
// its call alone. What is here is what they cannot see: the three together.
// On X11 gfx.borderless takes the window down and puts it back up, because a
// window manager that reads _MOTIF_WM_HINTS only when it frames a window has
// to be made to frame it again -- and EWMH has a window manager forget the
// states of a window it stops managing, so that round trip is where an on-top
// or fullscreen request goes if nothing says it a second time. It is also a
// trap that does not open again: __nexa_g.ontop would still read 1 afterwards,
// so gfx.ontop(1) would agree there was nothing to do.
//
// What is checked is therefore the composition, in the order a program writes
// it, and the requests it leaves on the wire. Tests/gfx_x11_stub stands in for
// the X server and records them. Only the X11 backend is executed; the other
// three are compiled by Tests/gfx_emit_cases.sh.
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

// Where in the call log the i'th call of a kind is, or -1. The remap is a
// fixed sequence -- hint, unmap, states, map, move, messages -- but which
// messages are in it depends on what the window was in, so the log is read by
// kind rather than by counting positions out by hand.
static int nth_call(int kind, int n) {
    for (int i = 0; i < nexa_x11_stub_call_count(); i++) {
        if (nexa_x11_stub_call(i) == kind && n-- == 0) return i;
    }
    return -1;
}

// One of the _NET_WM_STATE_ADD messages the remap sends. Only the last message
// is kept in full, so this is the check for it; the ones before it are counted
// in the call log and read back from the property, which has them all.
static void check_add_message(const char* label, const char* state) {
    char buf[160];
    std::snprintf(buf, sizeof(buf), "%s: a _NET_WM_STATE ClientMessage", label);
    check(buf, nexa_x11_stub_message_is_client() == 1 &&
               std::strcmp(nexa_x11_stub_message_name(), "_NET_WM_STATE") == 0 &&
               nexa_x11_stub_message_format() == 32);
    std::snprintf(buf, sizeof(buf), "%s: adding %s", label, state);
    check(buf, nexa_x11_stub_message_word(0) == _NET_WM_STATE_ADD &&
               std::strcmp(nexa_x11_stub_message_word_name(1), state) == 0 &&
               nexa_x11_stub_message_word(2) == 0 &&
               nexa_x11_stub_message_word(3) == 1);
    std::snprintf(buf, sizeof(buf), "%s: about our window, sent to the root", label);
    check(buf, nexa_x11_stub_message_window() == (unsigned long)__nexa_g.win &&
               nexa_x11_stub_message_target() == (unsigned long)DefaultRootWindow(__nexa_g.dpy) &&
               (nexa_x11_stub_message_mask() &
                (SubstructureRedirectMask | SubstructureNotifyMask)) ==
               (SubstructureRedirectMask | SubstructureNotifyMask));
}

// The _NET_WM_STATE property the remap writes while the window is withdrawn:
// the states it lists, in order, as a list of atoms.
static void check_state_property(const char* label, const char* first, const char* second) {
    char buf[160];
    int count = second ? 2 : 1;
    std::snprintf(buf, sizeof(buf), "%s: _NET_WM_STATE was written", label);
    check(buf, nexa_x11_stub_property_select("_NET_WM_STATE") == 1);
    std::snprintf(buf, sizeof(buf), "%s: a list of %d atom(s)", label, count);
    // XA_ATOM is how a property says its words are atom ids rather than
    // numbers; the Motif hint next to it is the other convention, where the
    // type is the property's own atom.
    check(buf, nexa_x11_stub_property_type() == XA_ATOM &&
               nexa_x11_stub_property_format() == 32 &&
               nexa_x11_stub_property_count() == count);
    std::snprintf(buf, sizeof(buf), "%s: it lists %s%s%s", label, first,
        second ? " and " : "", second ? second : "");
    check(buf, std::strcmp(nexa_x11_stub_property_word_name(0), first) == 0 &&
               (!second ||
                std::strcmp(nexa_x11_stub_property_word_name(1), second) == 0));
    nexa_x11_stub_property_select(0);
}

int main() {
    nexa_x11_stub_reset(1);
    nexa_x11_stub_set_mapped(IsViewable);
    nexa_x11_stub_set_origin(64, 96);

    check("a window opens", __nexa_gfx_open("t", 16, 16, 1) == 1);

    // --- the recipe, in the order that used to lose ----------------------
    // gfx.ontop(1); gfx.borderless(1); -- an overlay, written the way it
    // reads. The second call is the one that used to throw the first away.
    check_int("ontop first", __nexa_gfx_ontop(1), 1);
    nexa_x11_stub_calls_clear();
    check_int("then borderless", __nexa_gfx_borderless(1), 1);

    check("the hint goes on before the window comes down",
          nth_call(NEXA_STUB_CALL_PROP, 0) < nth_call(NEXA_STUB_CALL_UNMAP, 0) &&
          nth_call(NEXA_STUB_CALL_UNMAP, 0) >= 0);
    // Between the unmap and the map: EWMH lets a client set the states of a
    // window it has withdrawn, and a window manager reads them when it takes
    // the window on at map time.
    check("and the states while the window is down",
          nth_call(NEXA_STUB_CALL_UNMAP, 0) < nth_call(NEXA_STUB_CALL_PROP, 1) &&
          nth_call(NEXA_STUB_CALL_PROP, 1) < nth_call(NEXA_STUB_CALL_MAP, 0));
    check_state_property("on top", "_NET_WM_STATE_ABOVE", 0);
    // And once more as a message, which is the half that reaches a window
    // manager that had already deleted the property on its way out.
    check("one message, after the remap",
          nth_call(NEXA_STUB_CALL_SEND, 0) > nth_call(NEXA_STUB_CALL_MAP, 0) &&
          nth_call(NEXA_STUB_CALL_SEND, 1) == -1);
    check_add_message("on top", "_NET_WM_STATE_ABOVE");
    check("the window is put back where it was",
          nexa_x11_stub_move_x() == 64 && nexa_x11_stub_move_y() == 96);

    // --- and the state the program reads back is still true ----------------
    // The trap this closes: __nexa_g.ontop never stopped saying 1, so a
    // program that noticed the window was no longer on top and asked again
    // got a no-op. Asking again is still a no-op -- and now that is right,
    // because the request is standing.
    nexa_x11_stub_calls_clear();
    check("gfx.ontop still reads 1, and asking again is still nothing to do",
          __nexa_gfx_ontop(-1) == 1 && __nexa_gfx_ontop(1) == 1 &&
          nexa_x11_stub_call_count() == 0);

    // --- the other order -------------------------------------------------
    // borderless(1); ontop(1); is the order that worked by luck. It still
    // does, and the borderless call in it says nothing about states, because
    // there were none to carry.
    check_int("back to a framed window", __nexa_gfx_borderless(0), 0);
    check_int("and out of the topmost pile", __nexa_gfx_ontop(0), 0);
    nexa_x11_stub_calls_clear();
    check_int("borderless first this time", __nexa_gfx_borderless(1), 1);
    check("nothing is said about states it was not in",
          nexa_x11_stub_property_writes() == 1 &&
          nexa_x11_stub_property_select("_NET_WM_STATE") == 0 &&
          nth_call(NEXA_STUB_CALL_SEND, 0) == -1);
    nexa_x11_stub_property_select(0);
    nexa_x11_stub_calls_clear();
    check_int("then ontop, which is a live toggle", __nexa_gfx_ontop(1), 1);
    check("one message and nothing else",
          nexa_x11_stub_call_count() == 1 &&
          nexa_x11_stub_call(0) == NEXA_STUB_CALL_SEND);
    check_add_message("ontop after borderless", "_NET_WM_STATE_ABOVE");

    // --- both states at once ----------------------------------------------
    // A fullscreen overlay: the remap has two states to carry, and both go in
    // the one property and get a message each.
    check_int("fullscreen as well", __nexa_gfx_fullscreen(1), 1);
    nexa_x11_stub_calls_clear();
    check_int("the frame comes back while both are on", __nexa_gfx_borderless(0), 0);
    check_state_property("both", "_NET_WM_STATE_ABOVE", "_NET_WM_STATE_FULLSCREEN");
    check("two messages, both after the remap",
          nth_call(NEXA_STUB_CALL_SEND, 0) > nth_call(NEXA_STUB_CALL_MAP, 0) &&
          nth_call(NEXA_STUB_CALL_SEND, 1) > nth_call(NEXA_STUB_CALL_SEND, 0) &&
          nth_call(NEXA_STUB_CALL_SEND, 2) == -1);
    // The last one is the one the stub kept in full; the first is the ABOVE
    // the property above already showed.
    check_add_message("fullscreen", "_NET_WM_STATE_FULLSCREEN");
    check("and both states still read back as the program left them",
          __nexa_gfx_ontop(-1) == 1 && __nexa_gfx_fullscreen(-1) == 1 &&
          __nexa_gfx_borderless(-1) == 0);

    // --- a window that is not on screen ------------------------------------
    // Nothing framed it, so there is no remap -- and so nothing to put back
    // either: the states the program asked for are still on their way to a
    // window manager that has not taken the window on yet.
    nexa_x11_stub_set_mapped(IsUnmapped);
    nexa_x11_stub_calls_clear();
    check_int("borderless on a window that is down", __nexa_gfx_borderless(1), 1);
    check("only the hint is written",
          nexa_x11_stub_call_count() == 1 &&
          nexa_x11_stub_call(0) == NEXA_STUB_CALL_PROP &&
          std::strcmp(nexa_x11_stub_property_name(), "_MOTIF_WM_HINTS") == 0);

    // --- a new window is a plain window ------------------------------------
    nexa_x11_stub_set_mapped(IsViewable);
    check("gfx.open starts over on all three",
          __nexa_gfx_open("t", 16, 16, 1) == 1 &&
          __nexa_gfx_borderless(-1) == 0 && __nexa_gfx_ontop(-1) == 0 &&
          __nexa_gfx_fullscreen(-1) == 0);

    // --- with no window -----------------------------------------------------
    __nexa_gfx_close();
    nexa_x11_stub_calls_clear();
    check("a closed window asks the server for nothing",
          __nexa_gfx_borderless(1) == 0 && __nexa_gfx_ontop(1) == 0 &&
          __nexa_gfx_fullscreen(1) == 0 && nexa_x11_stub_call_count() == 0);

    std::printf("%s\n", failures ? "gfx_overlay_semantics: FAILURES" : "gfx_overlay_semantics ok");
    return failures ? 1 : 0;
}
