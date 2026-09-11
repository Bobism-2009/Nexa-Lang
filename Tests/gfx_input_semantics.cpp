// Semantics cover for the gfx input upgrades (BOB-21): gfx.wheel, gfx.wheel_x,
// gfx.released, gfx.typed.
//
// A gfx program cannot test these by itself. Every one of them reports
// something the *user* did, so a headless test has to be the one supplying the
// events. This driver includes the C++ that NexaC generates for a gfx program
// and drives its runtime directly, with Tests/gfx_x11_stub standing in for the
// X server and feeding it real XEvents. That makes the X11 branch of
// gfx.poll() something the suite executes rather than merely compiles.
//
// Three claims here are backend-independent, and hold for Win32, Cocoa and
// wasm too, because all four backends funnel into the same bookkeeping:
//   * the notch accumulator publishes whole notches and carries the remainder,
//   * the typed-text queue is printable-only, UTF-8, capped, and consuming,
//   * unfocused and closed windows report nothing.
// The backend-specific halves (which native event carries a scroll, what sign
// it has) are only covered for X11; the other three backends do not build on
// this machine.
//
// Built and run by Tests/gfx_input_cases.sh. NEXA_GEN is the generated .cpp.

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

static void check_int(const char* label, int got, int want) {
    if (got == want) {
        std::printf("ok %s\n", label);
    } else {
        std::printf("FAIL %s: want %d, got %d\n", label, want, got);
        failures++;
    }
}

static void check_str(const char* label, const std::string& got, const std::string& want) {
    if (got == want) {
        std::printf("ok %s\n", label);
    } else {
        std::printf("FAIL %s: want \"%s\", got \"%s\"\n", label, want.c_str(), got.c_str());
        failures++;
    }
}

// Opens a window on the fake server and clears the input state.
static void fresh_window() {
    nexa_x11_stub_reset(1);
    __nexa_gfx_open("test", 8, 8, 1);
    __nexa_gfx_poll();
}

static void wheel_cases() {
    fresh_window();

    // X11 sends scroll as button clicks: 4 up, 5 down, 6 left, 7 right.
    nexa_x11_stub_push_button(1, 4);
    nexa_x11_stub_push_button(1, 4);
    nexa_x11_stub_push_button(1, 4);
    __nexa_gfx_poll();
    check_int("wheel accumulates a frame of scroll-up", __nexa_gfx_wheel(), 3);

    __nexa_gfx_poll();
    check_int("wheel is per-frame, not sticky", __nexa_gfx_wheel(), 0);

    nexa_x11_stub_push_button(1, 5);
    nexa_x11_stub_push_button(1, 5);
    __nexa_gfx_poll();
    check_int("scrolling down is negative", __nexa_gfx_wheel(), -2);

    // Scrolling up and down within one frame cancels, like any other delta.
    nexa_x11_stub_push_button(1, 4);
    nexa_x11_stub_push_button(1, 5);
    __nexa_gfx_poll();
    check_int("opposing notches in one frame cancel", __nexa_gfx_wheel(), 0);

    nexa_x11_stub_push_button(1, 7);
    __nexa_gfx_poll();
    check_int("wheel_x is positive to the right", __nexa_gfx_wheel_x(), 1);
    check_int("horizontal scroll leaves wheel() alone", __nexa_gfx_wheel(), 0);

    nexa_x11_stub_push_button(1, 6);
    nexa_x11_stub_push_button(1, 6);
    __nexa_gfx_poll();
    check_int("wheel_x is negative to the left", __nexa_gfx_wheel_x(), -2);

    // The release half of a wheel click must not double-count it.
    nexa_x11_stub_push_button(1, 4);
    nexa_x11_stub_push_button(0, 4);
    __nexa_gfx_poll();
    check_int("one wheel click counts once", __nexa_gfx_wheel(), 1);

    // A real mouse button is not a wheel.
    nexa_x11_stub_push_button(1, 1);
    nexa_x11_stub_push_button(1, 3);
    __nexa_gfx_poll();
    check_int("left/right clicks are not scrolling", __nexa_gfx_wheel(), 0);
    check_int("left/right clicks are not horizontal scrolling", __nexa_gfx_wheel_x(), 0);

    __nexa_gfx_close();
}

static void wheel_fraction_cases() {
    // A trackpad reports a fraction of a notch per event. Published whole
    // notches would round those to nothing forever, so the remainder carries.
    fresh_window();

    __nexa_gfx_wheel_add(0.0, 0.4);
    __nexa_gfx_poll();
    check_int("a fraction of a notch reports nothing yet", __nexa_gfx_wheel(), 0);

    __nexa_gfx_wheel_add(0.0, 0.4);
    __nexa_gfx_poll();
    check_int("still short of a notch", __nexa_gfx_wheel(), 0);

    __nexa_gfx_wheel_add(0.0, 0.4);
    __nexa_gfx_poll();
    check_int("the carried remainder completes a notch", __nexa_gfx_wheel(), 1);

    // 1.2 was published as 1, so 0.2 is left over: two more tenths is still
    // short, and the sign of a leftover must not flip a later scroll.
    __nexa_gfx_wheel_add(0.0, 0.2);
    __nexa_gfx_poll();
    check_int("the leftover 0.2 did not become a second notch", __nexa_gfx_wheel(), 0);

    __nexa_gfx_wheel_add(0.0, -2.75);
    __nexa_gfx_poll();
    check_int("a negative delta truncates toward zero", __nexa_gfx_wheel(), -2);

    __nexa_gfx_close();
}

static void typed_cases() {
    fresh_window();

    nexa_x11_stub_push_key("hi");
    __nexa_gfx_poll();
    check_str("typed reports the characters typed", __nexa_gfx_typed(), "hi");
    check_str("typed consumes the queue", __nexa_gfx_typed(), "");

    // Several key events in one frame arrive as one string, in order.
    nexa_x11_stub_push_key("a");
    nexa_x11_stub_push_key("b");
    nexa_x11_stub_push_key("c");
    __nexa_gfx_poll();
    check_str("a frame of keys is one string, in order", __nexa_gfx_typed(), "abc");

    __nexa_gfx_poll();
    check_str("typed is empty when nothing was typed", __nexa_gfx_typed(), "");

    // Shift is the layout's business: XLookupString already applied it, and
    // gfx.typed passes the character through rather than the key name.
    nexa_x11_stub_push_key("A");
    __nexa_gfx_poll();
    check_str("typed reports case, not key names", __nexa_gfx_typed(), "A");

    // Printable input only: Enter, Tab, Escape and Backspace are keys, not text.
    nexa_x11_stub_push_key("\r");
    nexa_x11_stub_push_key("\n");
    nexa_x11_stub_push_key("\t");
    nexa_x11_stub_push_key("\033");
    nexa_x11_stub_push_key("\b");
    nexa_x11_stub_push_key("\177");
    nexa_x11_stub_push_key("x");
    __nexa_gfx_poll();
    check_str("control characters are not typed text", __nexa_gfx_typed(), "x");

    // Space is printable and must survive that filter.
    nexa_x11_stub_push_key(" ");
    __nexa_gfx_poll();
    check_str("space is typed text", __nexa_gfx_typed(), " ");

    // XLookupString hands back Latin-1; gfx.typed promises UTF-8.
    nexa_x11_stub_push_key("\xe9");             // U+00E9 LATIN SMALL LETTER E WITH ACUTE
    __nexa_gfx_poll();
    check_str("Latin-1 becomes UTF-8", __nexa_gfx_typed(), "\xc3\xa9");

    // The C1 range is where Latin-1 keeps its controls; those are not text.
    nexa_x11_stub_push_key("\x85");
    nexa_x11_stub_push_key("y");
    __nexa_gfx_poll();
    check_str("C1 controls are not typed text", __nexa_gfx_typed(), "y");

    __nexa_gfx_close();
}

static void typed_cap_cases() {
    // A program that scrolls or types for a long time between polls must not be
    // able to grow the queue without bound.
    fresh_window();
    for (int i = 0; i < 200; i++) nexa_x11_stub_push_key("0123456789");
    __nexa_gfx_poll();
    std::string s = __nexa_gfx_typed();
    check("the typed queue is capped", s.size() <= __nexa_gfx_type_cap);
    check("the cap keeps what fits rather than dropping the lot", s.size() > 0);
    __nexa_gfx_close();
}

static void released_cases() {
    fresh_window();

    nexa_x11_stub_set_key(XK_space, 1);
    __nexa_gfx_poll();
    check_int("a key going down is pressed", __nexa_gfx_pressed("space"), 1);
    check_int("a key going down is not released", __nexa_gfx_released("space"), 0);
    check_int("a key going down is held", __nexa_gfx_key("space"), 1);

    __nexa_gfx_poll();
    check_int("holding a key is not a fresh release", __nexa_gfx_released("space"), 0);

    nexa_x11_stub_set_key(XK_space, 0);
    __nexa_gfx_poll();
    check_int("letting a key up is released", __nexa_gfx_released("space"), 1);
    check_int("letting a key up is not pressed", __nexa_gfx_pressed("space"), 0);
    check_int("letting a key up clears the held state", __nexa_gfx_key("space"), 0);

    __nexa_gfx_poll();
    check_int("released is an edge, not a level", __nexa_gfx_released("space"), 0);

    // Same key-name set as gfx.key/gfx.pressed, aliases included.
    nexa_x11_stub_set_key(XK_Return, 1);
    __nexa_gfx_poll();
    nexa_x11_stub_set_key(XK_Return, 0);
    __nexa_gfx_poll();
    check_int("released takes the same names as pressed", __nexa_gfx_released("enter"), 1);
    check_int("released takes the aliases too", __nexa_gfx_released("return"), 1);
    check_int("released is case-insensitive", __nexa_gfx_released("ENTER"), 1);

    check_int("an unknown key name releases nothing", __nexa_gfx_released("nosuchkey"), 0);
    check_int("an empty key name releases nothing", __nexa_gfx_released(""), 0);

    __nexa_gfx_close();
}

static void focus_cases() {
    // Keys and mouse are already read only while the window has focus; scroll
    // and typed text join them, and neither is queued up to land in the
    // program's lap the moment focus comes back.
    fresh_window();

    nexa_x11_stub_set_focus(0);
    nexa_x11_stub_push_button(1, 4);
    nexa_x11_stub_push_key("abc");
    __nexa_gfx_poll();
    check_int("an unfocused window reports no scrolling", __nexa_gfx_wheel(), 0);
    check_str("an unfocused window reports no typed text", __nexa_gfx_typed(), "");

    nexa_x11_stub_set_focus(1);
    __nexa_gfx_poll();
    check_int("focus does not deliver scrolling that happened while away", __nexa_gfx_wheel(), 0);
    check_str("focus does not deliver text typed while away", __nexa_gfx_typed(), "");

    // And input still works after focus comes back.
    nexa_x11_stub_push_button(1, 4);
    nexa_x11_stub_push_key("z");
    __nexa_gfx_poll();
    check_int("scrolling works again once focused", __nexa_gfx_wheel(), 1);
    check_str("typing works again once focused", __nexa_gfx_typed(), "z");

    __nexa_gfx_close();
}

static void closed_window_cases() {
    // Every gfx function is a safe no-op on a closed window.
    fresh_window();
    nexa_x11_stub_push_button(1, 4);
    nexa_x11_stub_push_key("abc");
    __nexa_gfx_poll();
    __nexa_gfx_close();

    check_int("a closed window reports no scrolling", __nexa_gfx_wheel(), 0);
    check_int("a closed window reports no horizontal scrolling", __nexa_gfx_wheel_x(), 0);
    check_int("a closed window releases nothing", __nexa_gfx_released("space"), 0);
    check_str("a closed window reports no typed text", __nexa_gfx_typed(), "");
    __nexa_gfx_poll();
    check_int("polling a closed window is a no-op", __nexa_gfx_wheel(), 0);

    // A window that never opened at all behaves the same way.
    nexa_x11_stub_reset(0);
    check_int("a window that never opened reports no scrolling", __nexa_gfx_wheel(), 0);
    check_int("a window that never opened releases nothing", __nexa_gfx_released("space"), 0);
    check_str("a window that never opened reports no typed text", __nexa_gfx_typed(), "");
}

static void reopen_cases() {
    // Reopening must not hand the new window the old one's pending input.
    fresh_window();
    nexa_x11_stub_push_button(1, 4);
    nexa_x11_stub_push_key("stale");
    __nexa_gfx_poll();
    __nexa_gfx_close();

    fresh_window();
    check_int("a reopened window starts with no scrolling", __nexa_gfx_wheel(), 0);
    check_str("a reopened window starts with no typed text", __nexa_gfx_typed(), "");
    __nexa_gfx_close();
}

int main() {
    wheel_cases();
    wheel_fraction_cases();
    typed_cases();
    typed_cap_cases();
    released_cases();
    focus_cases();
    closed_window_cases();
    reopen_cases();

    if (failures == 0) {
        std::printf("gfx_input_semantics ok\n");
        return 0;
    }
    std::printf("gfx_input_semantics: %d failure(s)\n", failures);
    return 1;
}
