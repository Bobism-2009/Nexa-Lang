// Semantics cover for std/ui: what the widgets hand back when the user does
// something, and what they put in the framebuffer.
//
// Like gfx_input_semantics.cpp, this includes the C++ NexaC generates for a
// std/ui program and drives the runtime directly, with Tests/gfx_x11_stub
// standing in for the X server: it places the pointer, queues button and key
// events, and runs frames the way a program does -- gfx.poll(), the widgets,
// gfx.present(). The X11 branch of the event loops is the one exercised; the
// claims about widgets hold on every backend, since all four feed the same
// counters.
//
// Built and run by Tests/ui_cases.sh. NEXA_GEN is the generated .cpp.

#include <cstdio>
#include <string>
#include <vector>

#ifndef NEXA_GEN
#error "define NEXA_GEN to the generated C++ file"
#endif

#define main __nexa_program_main
#include NEXA_GEN
#undef main

static int failures = 0;

static void check(const char* label, bool ok) {
    std::printf(ok ? "ok %s\n" : "FAIL %s\n", label);
    if (!ok) failures++;
}
static void check_int(const char* label, long got, long want) {
    if (got == want) {
        std::printf("ok %s\n", label);
    } else {
        std::printf("FAIL %s: want %ld, got %ld\n", label, want, got);
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

static unsigned int pixel(int x, int y) {
    const unsigned char* p = __nexa_g.fb + (y * __nexa_g.w + x) * 4;
    return ((unsigned int)p[0] << 16) | ((unsigned int)p[1] << 8) | p[2];  // RGBA off Windows
}

// Pixels in a box that are not the background colour.
static int inked(int x, int y, int w, int h, unsigned int bg) {
    int n = 0;
    for (int j = y; j < y + h; j++)
        for (int i = x; i < x + w; i++)
            if (pixel(i, j) != bg) n++;
    return n;
}

static void fresh() {
    nexa_x11_stub_reset(1);
    __nexa_ui = __nexa_ui_State();
    __nexa_ui_ev_down = __nexa_ui_ev_up = 0;
    __nexa_ui_evq.clear();
    __nexa_ui_open("ui test", 400, 300);
    nexa_x11_stub_set_pointer(-50, -50, 0);
    __nexa_gfx_poll();
}

// A click that starts and ends between two polls, the way a touchpad tap or
// a synthetic click arrives: the button is never seen down by sampling.
static void tap(int x, int y) {
    nexa_x11_stub_set_pointer(x, y, 0);
    nexa_x11_stub_push_button(1, 1);
    nexa_x11_stub_push_button(0, 1);
}

static void theme_cases() {
    fresh();
    const char* names[] = {"light", "dark", "midnight", "paper", "contrast"};
    const unsigned int bg[] = {0xF3F4F6, 0x1B1C20, 0x0B1120, 0xF4EFE5, 0x000000};
    for (int i = 0; i < 5; i++) {
        __nexa_ui_theme_set(names[i]);
        __nexa_ui_background();
        std::string label = std::string("theme ") + names[i] + " paints its background";
        check_int(label.c_str(), (long)pixel(200, 150), (long)bg[i]);
        check_str((std::string("theme ") + names[i] + " reads back").c_str(), __nexa_ui_theme_get(), names[i]);
    }
    __nexa_ui_theme_set("no-such-theme");
    check_str("an unknown theme name leaves the theme as it was", __nexa_ui_theme_get(), "contrast");
    __nexa_gfx_close();
}

static void text_cases() {
    fresh();
    __nexa_ui_theme_set("light");
    __nexa_ui_background();
    int w = __nexa_ui_text(10, 10, "Hello, world", 0, -1, -1, -1);
    check("text reports a width", w > 60 && w < 120);
    check_int("text_width measures what text draws", __nexa_ui_text_width("Hello, world", 0), w);
    check("text puts ink where it says", inked(10, 10, w, __nexa_ui_text_height(0), 0xF3F4F6) > 80);
    check("nothing is drawn past its width", inked(10 + w + 2, 10, 40, 20, 0xF3F4F6) == 0);
    check("a larger size is wider", __nexa_ui_text_width("Hello", 30) > __nexa_ui_text_width("Hello", 15));
    check("a heading is semibold and larger", __nexa_ui_heading(10, 40, "Hello", 0) > __nexa_ui_text_width("Hello", 0));
    check("kerning tightens AV", __nexa_ui_text_width("AV", 40) <
                                  __nexa_ui_text_width("A", 40) + __nexa_ui_text_width("V", 40));
    check("an empty string is zero wide", __nexa_ui_text_width("", 0) == 0);
    __nexa_gfx_close();
}

static bool button_frame(int x, int y) {
    __nexa_gfx_poll();
    __nexa_ui_background();
    bool b = __nexa_ui_button(x, y, 100, 30, "OK", "primary");
    __nexa_gfx_present();
    return b;
}

static void button_cases() {
    fresh();
    check("a button with no click is not clicked", !button_frame(50, 50));
    tap(80, 60);
    check("a tap between two frames is a click", button_frame(50, 50));
    check("a click counts on one frame only", !button_frame(50, 50));
    tap(10, 10);
    check("a click outside it is not its click", !button_frame(50, 50));

    // Held down over it, then let go somewhere else: a desktop calls that off.
    nexa_x11_stub_set_pointer(80, 60, 1);
    check("pressing is not yet a click", !button_frame(50, 50));
    nexa_x11_stub_set_pointer(300, 250, 1);
    check("dragging off is not a click", !button_frame(50, 50));
    nexa_x11_stub_set_pointer(300, 250, 0);
    check("letting go elsewhere is not a click", !button_frame(50, 50));

    // Press and release with the pointer sampled down in between.
    nexa_x11_stub_set_pointer(80, 60, 1);
    button_frame(50, 50);
    nexa_x11_stub_set_pointer(80, 60, 0);
    check("a held click counts on release", button_frame(50, 50));
    __nexa_gfx_close();
}

static void toggle_cases() {
    fresh();
    bool on = false;
    auto frame = [&]() {
        __nexa_gfx_poll();
        __nexa_ui_background();
        on = __nexa_ui_checkbox(20, 20, "Option", on);
        __nexa_gfx_present();
    };
    frame();
    check("a checkbox keeps its state", !on);
    tap(28, 28);
    frame();
    check("clicking the box checks it", on);
    tap(60, 28);  // on the label
    frame();
    check("clicking the label unchecks it", !on);

    int q = 0;
    tap(28, 80);
    __nexa_gfx_poll();
    q = __nexa_ui_radio(20, 50, "A", q, 0);
    q = __nexa_ui_radio(20, 72, "B", q, 1);
    __nexa_gfx_present();
    check_int("a radio click chooses its value", q, 1);
    __nexa_gfx_close();
}

static void slider_cases() {
    fresh();
    int v = 50;
    auto frame = [&]() {
        __nexa_gfx_poll();
        v = __nexa_ui_slider(20, 20, 200, v, 0, 100);
        __nexa_gfx_present();
    };
    frame();
    check_int("an untouched slider keeps its value", v, 50);
    nexa_x11_stub_set_pointer(300, 30, 1);
    frame();
    check_int("a press that starts off the slider does not drag it", v, 50);
    nexa_x11_stub_set_pointer(300, 30, 0);
    frame();
    nexa_x11_stub_set_pointer(120, 30, 1);  // press on it, then drag
    frame();
    nexa_x11_stub_set_pointer(230, 30, 1);
    frame();
    check_int("dragged past the end, it stops at max", v, 100);
    nexa_x11_stub_set_pointer(0, 30, 1);
    frame();
    check_int("dragged past the start, it stops at min", v, 0);
    nexa_x11_stub_set_pointer(120, 30, 1);
    frame();
    check_int("in the middle, the middle", v, 50);
    nexa_x11_stub_set_pointer(120, 30, 0);
    frame();
    nexa_x11_stub_set_pointer(200, 30, 0);
    frame();
    check_int("let go, moving the mouse moves nothing", v, 50);
    __nexa_gfx_close();
}

static void textbox_cases() {
    fresh();
    std::string text;
    auto frame = [&]() {
        __nexa_gfx_poll();
        text = __nexa_ui_textbox(20, 20, 200, text, "type here");
        __nexa_gfx_present();
    };
    frame();
    nexa_x11_stub_push_key("abc");
    frame();
    check_str("typing into an unfocused box does nothing", text, "");
    tap(100, 30);
    frame();
    nexa_x11_stub_push_key("abc");
    frame();
    check_str("a clicked box takes typing", text, "abc");
    nexa_x11_stub_push_keysym(XK_BackSpace);
    frame();
    check_str("backspace deletes one character", text, "ab");
    nexa_x11_stub_push_keysym(XK_BackSpace);
    nexa_x11_stub_push_keysym(XK_BackSpace);
    nexa_x11_stub_push_key("xy");
    frame();
    check_str("two quick backspaces are two", text, "xy");
    nexa_x11_stub_push_keysym(XK_Home);
    nexa_x11_stub_push_key(">");
    frame();
    check_str("home goes to the start", text, ">xy");
    nexa_x11_stub_push_keysym(XK_End);
    nexa_x11_stub_push_key("!");
    frame();
    check_str("end goes to the end", text, ">xy!");
    nexa_x11_stub_push_keysym(XK_Left);
    nexa_x11_stub_push_keysym(XK_Left);
    nexa_x11_stub_push_keysym(XK_Delete);
    frame();
    check_str("left, left, delete", text, ">x!");
    // Latin-1 text arrives as UTF-8; backspace takes the whole character.
    nexa_x11_stub_push_keysym(XK_End);
    nexa_x11_stub_push_key("\xe9");
    frame();
    check_str("latin-1 is typed as utf-8", text, ">x!\xc3\xa9");
    nexa_x11_stub_push_keysym(XK_BackSpace);
    frame();
    check_str("backspace takes a whole utf-8 character", text, ">x!");
    nexa_x11_stub_push_keysym(XK_Return);
    frame();
    // Read the way a program reads it: after its own gfx.poll(), which is
    // before gfx.present() polls again and publishes the next frame's.
    nexa_x11_stub_push_key("zzz");
    __nexa_gfx_poll();
    text = __nexa_ui_textbox(20, 20, 200, text, "type here");
    std::string seen = __nexa_gfx_typed();
    __nexa_gfx_present();
    check_str("enter finishes editing", text, ">x!");
    check_str("ui leaves typed text for gfx.typed", seen, "zzz");
    __nexa_gfx_close();
}

static void dropdown_cases() {
    fresh();
    std::vector<std::string> items = {"One", "Two", "Three"};
    int sel = 0, under = 0;
    auto frame = [&]() {
        __nexa_gfx_poll();
        __nexa_ui_background();
        if (__nexa_ui_button(20, 70, 200, 30, "Beneath", "secondary")) under++;
        sel = __nexa_ui_dropdown(20, 20, 200, items, sel);
        __nexa_gfx_present();
    };
    int h = __nexa_ui_line_h(0, 15) + 16;
    int ih = __nexa_ui_line_h(0, 15) + 12;
    int list_top = 20 + h + 4 + 4;
    frame();
    tap(100, 30);
    frame();
    frame();
    check("clicking a dropdown opens it", __nexa_ui.open != 0);
    unsigned int bg = __nexa_ui_themes[0].bg;
    check("its list is drawn over what is beneath", pixel(100, list_top + ih + ih / 2) != bg);
    tap(100, list_top + 2 * ih + ih / 2);  // "Three", over the button
    frame();
    frame();
    check_int("clicking an item chooses it", sel, 2);
    check("choosing closes the list", __nexa_ui.open == 0);
    check_int("the button beneath the list was not clicked", under, 0);
    tap(100, 85);
    frame();
    check_int("with the list closed, the button is reachable", under, 1);
    tap(100, 30);
    frame();
    frame();
    tap(380, 280);
    frame();
    frame();
    check("a click elsewhere closes the list", __nexa_ui.open == 0);
    check_int("and chooses nothing", sel, 2);
    __nexa_gfx_close();
}

int main() {
    theme_cases();
    text_cases();
    button_cases();
    toggle_cases();
    slider_cases();
    textbox_cases();
    dropdown_cases();
    return failures ? 1 : 0;
}
