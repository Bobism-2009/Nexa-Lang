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

#include <cmath>
#include <cstdio>
#include <time.h>
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

// A fingerprint of the pixels in a box, to tell two drawings apart.
static unsigned long long region(int x, int y, int w, int h) {
    unsigned long long f = 1469598103934665603ULL;
    for (int j = y; j < y + h; j++)
        for (int i = x; i < x + w; i++) f = (f ^ pixel(i, j)) * 1099511628211ULL;
    return f;
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

// Every style draws every widget, and a style changes how widgets look, not
// what they do.
static void style_cases() {
    fresh();
    const char* names[] = {"modern", "fluent"};
    check_str("the style is modern to begin with", __nexa_ui_style_get(), "modern");
    for (int i = 0; i < 2; i++) {
        __nexa_ui_style_set(names[i]);
        check_str((std::string("style ") + names[i] + " reads back").c_str(), __nexa_ui_style_get(), names[i]);
    }
    __nexa_ui_style_set("no-such-style");
    check_str("an unknown style name leaves the style as it was", __nexa_ui_style_get(), "fluent");
    __nexa_ui_theme_set("dark");
    check_str("a theme leaves the style alone", __nexa_ui_style_get(), "fluent");
    __nexa_ui_style_set("modern");
    check_str("a style leaves the theme alone", __nexa_ui_theme_get(), "dark");
    __nexa_ui_theme_set("light");

    std::vector<std::string> items = {"One", "Two"};
    for (int i = 0; i < 2; i++) {
        std::string st = names[i];
        auto label = [&](const char* what) { return st + ": " + what; };
        __nexa_ui_style_set(st);
        __nexa_gfx_poll();
        __nexa_ui_background();
        unsigned int bg = pixel(399, 299);
        __nexa_ui_panel(10, 10, 180, 120, "Panel");
        check(label("a panel is drawn").c_str(), inked(12, 60, 170, 60, bg) > 3000);
        __nexa_ui_button(210, 10, 100, 30, "OK", "primary");
        check(label("a button is drawn").c_str(), inked(210, 10, 100, 30, bg) > 1500);
        __nexa_ui_checkbox(210, 50, "", false);
        unsigned long long off = region(210, 50, 20, 20);
        __nexa_ui_background();
        __nexa_ui_checkbox(210, 50, "", true);
        check(label("a checked box differs from an empty one").c_str(), region(210, 50, 20, 20) != off);
        __nexa_ui_background();
        __nexa_ui_radio(210, 80, "", 0, 1);
        off = region(210, 80, 20, 20);
        __nexa_ui_background();
        __nexa_ui_radio(210, 80, "", 1, 1);
        check(label("a chosen radio differs from the others").c_str(), region(210, 80, 20, 20) != off);
        __nexa_ui_background();
        __nexa_ui_toggle(210, 110, "", false);
        off = region(210, 110, 40, 20);
        __nexa_ui_background();
        __nexa_ui_toggle(210, 110, "", true);
        check(label("a toggle shows which way it is").c_str(), region(210, 110, 40, 20) != off);
        __nexa_ui_background();
        __nexa_ui_slider(10, 150, 200, 50, 0, 100);
        check(label("a slider is drawn").c_str(), inked(10, 150, 200, 20, bg) > 100);
        __nexa_ui_progress(10, 180, 200, 0.5);
        check(label("a progress bar fills from the left").c_str(), pixel(30, 184) != bg && pixel(190, 184) != pixel(30, 184));
        __nexa_ui_textbox(10, 200, 200, "text", "");
        check(label("a text box is drawn").c_str(), inked(10, 200, 200, 30, bg) > 1000);
        __nexa_ui_dropdown(220, 200, 150, items, 0);
        check(label("a dropdown is drawn").c_str(), inked(220, 200, 150, 30, bg) > 1000);
        __nexa_ui_separator(10, 260, 200);
        check(label("a separator is drawn").c_str(), inked(10, 260, 200, 1, bg) > 150);
        __nexa_gfx_present();

        // what the widgets do is the same in every style
        check(label("a button with no click is not clicked").c_str(), !button_frame(50, 50));
        tap(80, 60);
        check(label("a tap on a button is a click").c_str(), button_frame(50, 50));
        bool on = false;
        for (int f = 0; f < 3; f++) {
            if (f == 1) tap(28, 208);
            __nexa_gfx_poll();
            __nexa_ui_background();
            on = __nexa_ui_checkbox(20, 200, "Option", on);
            __nexa_gfx_present();
        }
        check(label("a tap on a checkbox checks it").c_str(), on);
    }

    // fluent takes Windows 11's own colours for the light and dark themes
    __nexa_ui_style_set("fluent");
    __nexa_ui_theme_set("light");
    __nexa_ui_background();
    check_int("fluent: light is Windows' Mica grey", (long)pixel(200, 150), 0xF3F3F3L);
    __nexa_ui_theme_set("dark");
    __nexa_ui_background();
    check_int("fluent: dark is Windows' dark Mica", (long)pixel(200, 150), 0x202020L);
    __nexa_ui_theme_set("light");
    __nexa_ui_background();
    __nexa_ui_button(10, 10, 100, 32, "OK", "primary");
    check_int("fluent: the primary button is Windows' accent blue", (long)pixel(20, 20), 0x005FB8L);
    __nexa_ui_accent_set(200, 0, 0);
    __nexa_ui_button(10, 10, 100, 32, "OK", "primary");
    check_int("fluent: ui.accent still sets the accent", (long)pixel(20, 20), 0xC80000L);
    __nexa_ui_theme_set("light");
    __nexa_ui_background();
    __nexa_ui_checkbox(10, 60, "", false);
    check("fluent: a checkbox is 20 px", pixel(10 + 19, 60 + 10) != 0xF3F3F3 && pixel(10 + 21, 60 + 10) == 0xF3F3F3);
    __nexa_ui_style_set("modern");
    __nexa_gfx_close();
}

// --- the app widgets ---

static void app_frame_begin() {
    __nexa_gfx_poll();
    __nexa_ui_background();
}

static void tabs_cases() {
    fresh();
    std::vector<std::string> labels = {"One", "Two", "Three"};
    int sel = 0;
    auto frame = [&]() {
        app_frame_begin();
        sel = __nexa_ui_tabs(10, 10, 380, labels, sel);
        __nexa_gfx_present();
    };
    frame();
    check_int("tabs: the chosen tab stays chosen", sel, 0);
    int second = 10 + (int)std::ceil(__nexa_ui_width("One", 1, 15)) + 32 + 10;
    tap(second, 25);
    frame();
    check_int("tabs: clicking a tab chooses it", sel, 1);
    tap(390, 25);
    frame();
    check_int("tabs: a click past the last tab chooses nothing", sel, 1);
    __nexa_gfx_close();
}

static void list_cases() {
    fresh();
    std::vector<std::string> items;
    for (int i = 0; i < 30; i++) items.push_back("Item " + std::to_string(i));
    int sel = -1;
    int ih = __nexa_ui_line_h(0, 15) + 12;
    unsigned int id = __nexa_ui_id(12, 10, 10);
    auto frame = [&]() {
        app_frame_begin();
        sel = __nexa_ui_list(10, 10, 200, 200, items, sel);
        __nexa_gfx_present();
    };
    frame();
    check_int("list: nothing is selected to begin with", sel, -1);
    tap(60, 10 + 4 + 2 * ih + ih / 2);
    frame();
    check_int("list: clicking a row selects it", sel, 2);
    nexa_x11_stub_push_keysym(XK_Down);
    nexa_x11_stub_push_keysym(XK_Down);
    frame();
    check_int("list: Down moves the selection, once a click gave it the keyboard", sel, 4);
    nexa_x11_stub_push_keysym(XK_End);
    frame();
    check_int("list: End goes to the last row", sel, 29);
    check("list: and scrolls it into view", __nexa_ui.scrolls[id] > 0);
    nexa_x11_stub_push_keysym(XK_Home);
    frame();
    check_int("list: Home goes to the first", sel, 0);
    check("list: and scrolls back", __nexa_ui.scrolls[id] == 0);
    // the wheel, over the list
    nexa_x11_stub_set_pointer(60, 100, 0);
    nexa_x11_stub_push_button(1, 5);
    nexa_x11_stub_push_button(0, 5);
    frame();
    frame();
    float off = __nexa_ui.scrolls[id];
    check("list: the wheel scrolls it", off > 0);
    tap(60, 10 + 4 + ih / 2);
    frame();
    check_int("list: a click after scrolling picks the row under the mouse", sel, (int)((ih / 2 + off) / ih));
    // the wheel elsewhere leaves it
    nexa_x11_stub_set_pointer(300, 100, 0);
    nexa_x11_stub_push_button(1, 4);
    nexa_x11_stub_push_button(0, 4);
    frame();
    frame();
    check("list: the wheel away from it leaves it", __nexa_ui.scrolls[id] == off);
    // dragging the scrollbar to the bottom
    nexa_x11_stub_set_pointer(203, 20, 1);
    frame();
    nexa_x11_stub_set_pointer(203, 290, 1);
    frame();
    nexa_x11_stub_set_pointer(203, 290, 0);
    frame();
    check_int("list: dragging its bar scrolls to the end", (long)__nexa_ui.scrolls[id], 30 * ih + 8 - 200);
    __nexa_gfx_close();
}

static void table_cases() {
    fresh();
    std::vector<std::string> headers = {"Name", "Year"};
    std::vector<std::vector<std::string>> rows = {{"Ada", "1815"}, {"Alan", "1912"}, {"Grace", "1906"}};
    int sel = -1;
    int ih = __nexa_ui_line_h(0, 15) + 12, hh = ih + 2;
    auto frame = [&]() {
        app_frame_begin();
        sel = __nexa_ui_table(10, 10, 300, 200, headers, rows, sel);
        __nexa_gfx_present();
    };
    frame();
    tap(100, 10 + 5);
    frame();
    check_int("table: a click on the header selects nothing", sel, -1);
    tap(100, 10 + hh + 2 + ih + ih / 2);
    frame();
    check_int("table: a click on a row selects it", sel, 1);
    nexa_x11_stub_push_keysym(XK_Up);
    frame();
    check_int("table: Up moves the selection", sel, 0);
    nexa_x11_stub_push_keysym(XK_Up);
    frame();
    check_int("table: and stops at the first row", sel, 0);
    check("table: its cells are drawn", inked(20, 10 + hh + 2 + ih, 280, ih * 2, __nexa_ui_themes[0].input) > 200);
    __nexa_gfx_close();
}

static void menu_cases() {
    fresh();
    std::vector<std::string> file = {"New\tCtrl+N", "-", "Quit"};
    std::vector<std::string> help = {"About"};
    int picked = -1, hpicked = -1, under = 0;
    int h = __nexa_ui_line_h(0, 15) + 14, ih = __nexa_ui_line_h(0, 15) + 12;
    int fx = 10, hx = 10 + (int)std::ceil(__nexa_ui_width("File", 0, 15)) + 24;
    auto frame = [&]() {
        app_frame_begin();
        if (__nexa_ui_button(10, 60, 120, 30, "Under", "primary")) under++;
        __nexa_ui_menubar(0, 0, 400);
        int a = __nexa_ui_menu(fx, 0, "File", file);
        int b = __nexa_ui_menu(hx, 0, "Help", help);
        if (a >= 0) picked = a;
        if (b >= 0) hpicked = b;
        __nexa_gfx_present();
    };
    frame();
    tap(fx + 10, h / 2);
    frame();
    frame();
    check("menu: a click on its title opens it", __nexa_ui.open == __nexa_ui_id(15, fx, 0));
    int oy = h + 2;
    tap(fx + 40, oy + 4 + ih + 4);  // the separator
    frame();
    check_int("menu: a click on a separator picks nothing", picked, -1);
    check("menu: and leaves it open", __nexa_ui.open == __nexa_ui_id(15, fx, 0));
    tap(fx + 40, oy + 4 + ih + 9 + ih / 2);  // "Quit", over the button
    frame();
    check_int("menu: a click on an item picks it", picked, 2);
    check("menu: and closes it", __nexa_ui.open == 0);
    check_int("menu: the button beneath was not clicked", under, 0);
    // open File, then slide across to Help
    tap(fx + 10, h / 2);
    frame();
    frame();
    nexa_x11_stub_set_pointer(hx + 10, h / 2, 0);
    frame();
    check("menu: with a menu open, moving onto the next title opens that one",
          __nexa_ui.open == __nexa_ui_id(15, hx, 0));
    nexa_x11_stub_push_keysym(XK_Escape);
    frame();
    check("menu: Escape closes it", __nexa_ui.open == 0);
    check_int("menu: and picks nothing", hpicked, -1);
    // a second click on an open menu's title closes it
    tap(hx + 10, h / 2);
    frame();
    frame();
    check("menu: (open again)", __nexa_ui.open == __nexa_ui_id(15, hx, 0));
    tap(hx + 10, h / 2);
    frame();
    frame();
    check("menu: a second click on its title closes it", __nexa_ui.open == 0);
    __nexa_gfx_close();
}

static void dialog_cases() {
    fresh();
    std::vector<std::string> buttons = {"Delete", "Cancel"};
    int showing = 1, answer = -2, under = 0;
    auto frame = [&]() {
        app_frame_begin();
        if (__nexa_ui_button(10, 10, 100, 30, "Under", "primary")) under++;
        if (showing) {
            int r = __nexa_ui_dialog("Delete?", "It cannot be undone.", buttons);
            if (r >= 0) { answer = r; showing = 0; }
        }
        __nexa_gfx_present();
    };
    frame();
    frame();
    check("dialog: it dims the page", pixel(390, 290) != __nexa_ui_themes[0].bg);
    tap(50, 20);
    frame();
    frame();
    check_int("dialog: the page behind it takes no clicks", under, 0);
    check_int("dialog: and it stays up until answered", answer, -2);
    int bx = __nexa_ui.dlg.bx[1], by = __nexa_ui.dlg.by;
    tap(bx + 10, by + 10);
    frame();
    check_int("dialog: a click on a button answers with it", answer, 1);
    frame();
    frame();
    tap(50, 20);
    frame();
    check_int("dialog: once answered, the page takes clicks again", under, 1);
    showing = 1;
    answer = -2;
    frame();
    nexa_x11_stub_push_keysym(XK_Return);
    frame();
    check_int("dialog: Enter answers with the first button", answer, 0);
    showing = 1;
    answer = -2;
    frame();
    frame();
    nexa_x11_stub_push_keysym(XK_Escape);
    frame();
    check_int("dialog: Escape answers with the last", answer, 1);
    __nexa_gfx_close();
}

static void tooltip_cases() {
    fresh();
    auto frame = [&]() {
        app_frame_begin();
        __nexa_ui_button(10, 10, 100, 30, "Help", "primary");
        __nexa_ui_tooltip("Shows the manual");
        __nexa_ui_button(10, 60, 100, 30, "Plain", "primary");
        __nexa_gfx_present();
    };
    nexa_x11_stub_set_pointer(50, 20, 0);
    frame();
    frame();
    check("tooltip: not at once", !__nexa_ui.tip_show);
    struct timespec ts = {0, 600 * 1000000L};
    nanosleep(&ts, nullptr);
    __nexa_gfx_poll();
    __nexa_ui_background();
    __nexa_ui_button(10, 10, 100, 30, "Help", "primary");
    __nexa_ui_tooltip("Shows the manual");
    check("tooltip: after the mouse rests on its widget a moment", __nexa_ui.tip_show == 1);
    check_str("tooltip: with its text", __nexa_ui.tip_text, "Shows the manual");
    __nexa_gfx_present();
    nexa_x11_stub_set_pointer(50, 70, 0);
    frame();
    check("tooltip: not on another widget", !__nexa_ui.tip_show);
    nexa_x11_stub_set_pointer(50, 20, 0);
    frame();
    check("tooltip: and back on its own it waits again", !__nexa_ui.tip_show);
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
    style_cases();
    tabs_cases();
    list_cases();
    table_cases();
    menu_cases();
    dialog_cases();
    tooltip_cases();
    return failures ? 1 : 0;
}
