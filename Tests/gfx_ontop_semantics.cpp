// Semantics cover for gfx.ontop (BOB-50): the X11 half, executed.
//
// Staying above other programs is the window manager's job, and there is no X
// call that asks it whether it is doing it -- a window manager is even allowed
// to refuse. So what is checked here is the request: the _NET_WM_STATE
// ClientMessage the runtime sends to the root window, down to which state atom
// it names and whether it is adding or removing it. Tests/gfx_x11_stub stands
// in for the X server and records it.
//
// Only the X11 backend is executed; the Win32 and Cocoa ones do not build on
// this machine, and Tests/gfx_emit_cases.sh is what proves they compile. The
// parts that are not per-platform -- the -1 query, the no-op when the state is
// already what was asked for, the state surviving a fullscreen round trip, and
// a fresh window starting in the ordinary stacking order -- are the same code
// on all four.
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

// The one message the runtime is expected to have just sent: a _NET_WM_STATE
// ClientMessage about our own window, addressed to the root, adding or
// removing `state`.
static void check_wm_state(const char* label, int add, const char* state) {
    char buf[128];
    std::snprintf(buf, sizeof(buf), "%s: one message, and it is a ClientMessage", label);
    check(buf, nexa_x11_stub_call_count() == 1 &&
               nexa_x11_stub_call(0) == NEXA_STUB_CALL_SEND &&
               nexa_x11_stub_message_is_client() == 1);
    std::snprintf(buf, sizeof(buf), "%s: message_type is _NET_WM_STATE", label);
    check(buf, std::strcmp(nexa_x11_stub_message_name(), "_NET_WM_STATE") == 0);
    std::snprintf(buf, sizeof(buf), "%s: 32-bit format, so data.l is what is read", label);
    check(buf, nexa_x11_stub_message_format() == 32);
    // data.l[0] is the verb. EWMH calls them _NET_WM_STATE_REMOVE (0) and
    // _NET_WM_STATE_ADD (1); getting this backwards is a toggle that turns
    // itself off.
    std::snprintf(buf, sizeof(buf), "%s: data.l[0] is %s", label,
        add ? "_NET_WM_STATE_ADD" : "_NET_WM_STATE_REMOVE");
    check(buf, nexa_x11_stub_message_word(0) ==
               (add ? _NET_WM_STATE_ADD : _NET_WM_STATE_REMOVE));
    std::snprintf(buf, sizeof(buf), "%s: data.l[1] is %s", label, state);
    check(buf, std::strcmp(nexa_x11_stub_message_word_name(1), state) == 0);
    // l[2] is the second state of the change-two-at-once form, which this is
    // not, and l[3] is the source indication: 1 means a normal application.
    std::snprintf(buf, sizeof(buf), "%s: no second state, and the source is an application", label);
    check(buf, nexa_x11_stub_message_word(2) == 0 && nexa_x11_stub_message_word(3) == 1);
    // The message is *about* our window and *sent to* the root, because the
    // window manager is the root window's owner and the only thing that can
    // act on it. Sending it to the window itself is the quiet way to have
    // nothing happen at all.
    std::snprintf(buf, sizeof(buf), "%s: about our window, sent to the root", label);
    check(buf, nexa_x11_stub_message_window() == (unsigned long)__nexa_g.win &&
               nexa_x11_stub_message_target() == (unsigned long)DefaultRootWindow(__nexa_g.dpy));
    std::snprintf(buf, sizeof(buf), "%s: redirect and notify, not propagated", label);
    check(buf, (nexa_x11_stub_message_mask() &
                (SubstructureRedirectMask | SubstructureNotifyMask)) ==
               (SubstructureRedirectMask | SubstructureNotifyMask) &&
               nexa_x11_stub_message_propagate() == 0);
}

int main() {
    nexa_x11_stub_reset(1);
    nexa_x11_stub_set_mapped(IsViewable);

    check("a window opens", __nexa_gfx_open("t", 16, 16, 1) == 1);
    check_int("a window starts in the ordinary stacking order", __nexa_gfx_ontop(-1), 0);

    // --- going on top ---------------------------------------------------
    nexa_x11_stub_calls_clear();
    check_int("ontop(1) returns the new state", __nexa_gfx_ontop(1), 1);
    check_int("and the state reads back", __nexa_gfx_ontop(-1), 1);
    check_wm_state("on", 1, "_NET_WM_STATE_ABOVE");

    // --- asking, rather than telling --------------------------------------
    nexa_x11_stub_calls_clear();
    check("a negative argument reports and touches nothing",
          __nexa_gfx_ontop(-1) == 1 && nexa_x11_stub_call_count() == 0);

    // --- asking for what it already is ------------------------------------
    nexa_x11_stub_calls_clear();
    check("asking again touches nothing",
          __nexa_gfx_ontop(1) == 1 && nexa_x11_stub_call_count() == 0);
    // Anything non-zero means on, the same way gfx.fullscreen takes it.
    nexa_x11_stub_calls_clear();
    check("and any other yes is the same yes",
          __nexa_gfx_ontop(7) == 1 && nexa_x11_stub_call_count() == 0);

    // --- coming back down -------------------------------------------------
    nexa_x11_stub_calls_clear();
    check_int("ontop(0) returns 0", __nexa_gfx_ontop(0), 0);
    check_wm_state("off", 0, "_NET_WM_STATE_ABOVE");

    // --- the fullscreen round trip ----------------------------------------
    // Fullscreen is the neighbour that could cancel this: on Windows it names
    // the window it inserts itself after, and HWND_NOTOPMOST is exactly the
    // instruction to stop being on top. Here on X11 the two states are
    // independent EWMH states, so what is checked is that fullscreen sends its
    // own message and leaves this one's state alone.
    check_int("on again, for the round trip", __nexa_gfx_ontop(1), 1);
    nexa_x11_stub_calls_clear();
    check_int("fullscreen goes on", __nexa_gfx_fullscreen(1), 1);
    check_wm_state("fullscreen on", 1, "_NET_WM_STATE_FULLSCREEN");
    check_int("and says nothing about being on top", __nexa_gfx_ontop(-1), 1);
    nexa_x11_stub_calls_clear();
    check_int("fullscreen comes off", __nexa_gfx_fullscreen(0), 0);
    check_wm_state("fullscreen off", 0, "_NET_WM_STATE_FULLSCREEN");
    check_int("and the window is still on top", __nexa_gfx_ontop(-1), 1);
    // Held as state, not merely reported: asking for it again is still a
    // no-op, which it would not be if the round trip had quietly cleared it.
    nexa_x11_stub_calls_clear();
    check("the state survived rather than being restated",
          __nexa_gfx_ontop(1) == 1 && nexa_x11_stub_call_count() == 0);

    // --- a new window is an ordinary window -------------------------------
    check("gfx.open puts it back in the ordinary order",
          __nexa_gfx_open("t", 16, 16, 1) == 1 && __nexa_gfx_ontop(-1) == 0);

    // --- with no window ---------------------------------------------------
    __nexa_gfx_close();
    nexa_x11_stub_calls_clear();
    check("a closed window reports 0, stays 0 and asks the server for nothing",
          __nexa_gfx_ontop(-1) == 0 && __nexa_gfx_ontop(1) == 0 &&
          nexa_x11_stub_call_count() == 0);

    std::printf("%s\n", failures ? "gfx_ontop_semantics: FAILURES" : "gfx_ontop_semantics ok");
    return failures ? 1 : 0;
}
