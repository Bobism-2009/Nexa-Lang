#!/bin/sh
# Cover for std/ui: widgets, themes and smooth text on the gfx window.
#
#   emit       what NexaC writes for a std/ui program: the widgets, the hooks
#              in gfx.poll()/gfx.present(), stb_truetype and the font with its
#              licence -- and that a gfx program without ui carries none of it
#   build      the generated program compiles with no diagnostics from ui
#   headless   with no display, ui.open is 0 and every widget is quiet
#   semantics  Tests/ui_semantics.cpp drives the runtime against the X11
#              stand-in: clicks, taps between frames, typing and the editing
#              keys, sliders, dropdowns, themes and text
#
# Usage: Tests/ui_cases.sh [path-to-NexaC]      (run from the repo root)

set -u

NEXAC="${1:-./NexaC}"
if [ ! -x "$NEXAC" ]; then
    echo "FAIL: NexaC not found or not executable: $NEXAC"
    exit 1
fi
NEXAC=$(cd "$(dirname "$NEXAC")" && pwd)/$(basename "$NEXAC")
HERE=$(cd "$(dirname "$0")" && pwd)

WORK=$(mktemp -d)
trap 'rm -rf "$WORK"' EXIT INT TERM

fails=0
say_ok() { echo "ok $1"; }
say_fail() {
    echo "FAIL $1"
    fails=$((fails + 1))
}

pick_cxx() {
    if [ -n "${NEXA_CXX:-}" ]; then echo "$NEXA_CXX"; return; fi
    for c in clang++ g++ c++; do
        if command -v "$c" > /dev/null 2>&1; then echo "$c"; return; fi
    done
    echo ""
}

# A program that reaches every ui call.
cat > "$WORK/all.nxa" <<'NXA'
#include <std/io>
#include <std/ui>
fn main() {
    if (ui.open("ui", 320, 240) == 0) {
        io.println("open=0");
    }
    ui.theme("dark");
    io.println("theme=" + ui.theme());
    ui.accent(200, 60, 120);
    ui.rounding(4);
    ui.font_size(14);
    gfx.poll();
    ui.background();
    ui.panel(4, 4, 300, 220, "Panel");
    ui.separator(10, 40, 280);
    let w = ui.text(10, 50, "text") + ui.text(10, 50, 42, 18) + ui.text(10, 50, "c", 12, 1, 2, 3);
    w = w + ui.heading(10, 70, "h") + ui.caption(10, 90, "c") + ui.text_width("x") + ui.text_height();
    let b = ui.button(10, 110, 80, 30, "OK") || ui.button(100, 110, 80, 30, "No", "ghost");
    let c = ui.checkbox(10, 150, "c", false);
    let t = ui.toggle(10, 170, "t", true);
    let r = ui.radio(10, 190, "r", 0, 1);
    let s = ui.slider(120, 150, 100, 5, 0, 10);
    ui.progress(120, 180, 100, 0.5);
    let name = ui.textbox(120, 200, 100, "x", "placeholder");
    let d = ui.dropdown(200, 10, 100, ["a", "b"], 0);
    gfx.present();
    io.println("widgets=" + b + "," + c + "," + t + "," + r + "," + s + "," + name + "," + d);
    io.println("done");
}
NXA

# --- emit -------------------------------------------------------------------

if ! "$NEXAC" "$WORK/all.nxa" --source "$WORK/all.cpp" > "$WORK/emit.log" 2>&1; then
    say_fail "emit: NexaC could not transpile a program using every ui call"
    sed 's/^/  /' "$WORK/emit.log"
    exit 1
fi
for want in "__nexa_ui_after_poll();" "__nexa_ui_before_present();" "STB_TRUETYPE_IMPLEMENTATION" \
            "__nexa_ui_font_regular" "__nexa_ui_font_semibold" "SIL Open Font License" \
            "__nexa_ui_dropdown(" "__nexa_ui_evq"; do
    if grep -qF "$want" "$WORK/all.cpp"; then
        say_ok "emit: the ui program carries $want"
    else
        say_fail "emit: the ui program is missing $want"
    fi
done

printf '#include <std/gfx>\nfn main() {\n    gfx.open("g", 8, 8, 1);\n    gfx.poll();\n    gfx.present();\n}\n' > "$WORK/gfx.nxa"
if "$NEXAC" "$WORK/gfx.nxa" --source "$WORK/gfx.cpp" > /dev/null 2>&1; then
    if grep -qE '__nexa_ui|stbtt_' "$WORK/gfx.cpp"; then
        say_fail "emit: a gfx program without std/ui carries ui code"
    else
        say_ok "emit: a gfx program without std/ui carries none of it"
    fi
else
    say_fail "emit: NexaC could not transpile a plain gfx program"
fi

# --- build, headless, semantics (Linux: the X11 stand-in) -----------------------

CXX=$(pick_cxx)
if [ "$(uname -s)" != "Linux" ] || [ -z "$CXX" ]; then
    echo "SKIP build/headless/semantics: they build against the X11 stand-in, on Linux"
else
    # -Wno-unused-function for gfx's slices, as in the other gfx suites; any
    # other diagnostic from the generated program is a failure.
    if ! "$CXX" -std=c++17 -O1 -Wall -Wextra -Wno-unused-function -I "$HERE/gfx_x11_stub" \
            "$WORK/all.cpp" "$HERE/gfx_x11_stub/x11_stub.cpp" -o "$WORK/all" > "$WORK/build.log" 2>&1; then
        say_fail "build: the ui program does not build"
        grep -E 'error' "$WORK/build.log" | head -n 5 | sed 's/^/  /'
    elif grep -qE '(warning|error):' "$WORK/build.log"; then
        say_fail "build: the compiler had something to say about the ui program"
        grep -E '(warning|error):' "$WORK/build.log" | head -n 5 | sed 's/^/  /'
    else
        say_ok "build: the ui program compiles clean under -Wall -Wextra"
        if "$WORK/all" > "$WORK/run.out" 2>&1 && grep -qx "open=0" "$WORK/run.out" &&
                grep -qx "theme=dark" "$WORK/run.out" && grep -qx "done" "$WORK/run.out"; then
            say_ok "headless: with no display ui.open is 0 and every widget is quiet"
        else
            say_fail "headless: the ui program did not run quietly without a display"
            sed 's/^/  /' "$WORK/run.out"
        fi
    fi

    if ! "$CXX" -std=c++17 -O1 -I "$HERE/gfx_x11_stub" -DNEXA_GEN="\"$WORK/all.cpp\"" \
            "$HERE/ui_semantics.cpp" "$HERE/gfx_x11_stub/x11_stub.cpp" \
            -o "$WORK/semantics" > "$WORK/sem.log" 2>&1; then
        say_fail "semantics: could not build the driver"
        grep -E 'error' "$WORK/sem.log" | head -n 5 | sed 's/^/  /'
    else
        out=$("$WORK/semantics" 2>&1)
        status=$?
        printf '%s\n' "$out" | grep '^ok ' | sed 's/^ok /ok semantics: /'
        if [ $status -ne 0 ]; then
            printf '%s\n' "$out" | grep -v '^ok ' | sed 's/^/  /'
            fails=$((fails + 1))
        fi
    fi
fi

if [ $fails -eq 0 ]; then
    echo "ui ok"
    exit 0
fi
echo "ui: $fails failure(s)"
exit 1
