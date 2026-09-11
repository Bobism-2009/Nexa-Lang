#!/bin/sh
# std/gfx cover (BOB-22).
#
# std/gfx is the one module whose tests cannot simply be run: on Linux the
# window backend links X11 statically, so a box without the X11 development
# libraries cannot even build a gfx program, and a box without a display cannot
# open a window. The cover is therefore layered, cheapest and most portable
# first, so that every machine runs as much of it as it can:
#
#   codegen   Transpile only (--source). Pins the call each gfx.* form emits:
#             the defaults NexaC fills in for omitted arguments, and which
#             overload gfx.blit resolves to. These are exactly the decisions
#             that live in the transpiler rather than the runtime, so a
#             behavioural test cannot see them -- gfx.open("t", 4, 4) and
#             gfx.open("t", 4, 4, 12) draw the same window. Never invokes the
#             C++ compiler, so it runs anywhere NexaC itself runs.
#
#   headless  Build and run Tests/gfx_headless_test.nxa, which never opens a
#             window: the closed-window return values, text metrics and image
#             decoding. Needs a C++ toolchain that can link a gfx binary, but
#             no display. Skipped when the link fails.
#
#   window    Build and run Tests/gfx_open_close_test.nxa, which opens a real
#             window and reads pixels back. Needs a display; skipped without
#             one.
#
# Arity and unknown-method diagnostics live in Tests/Lang/errors/gfx_*.nxa,
# where the run_tests.sh error phase already checks that NexaC does the
# complaining and clang never gets to speak.
#
# Skips are reported but do not fail the run. Note that run_tests.sh only
# prints a suite's output when it fails, so run this directly to see which
# layers a machine actually ran:
#
#   sh Tests/gfx_cases.sh            (from the repo root)
#
# Usage: Tests/gfx_cases.sh [path-to-NexaC]

set -u

NEXAC="${1:-./NexaC}"
if [ ! -x "$NEXAC" ]; then
    echo "FAIL: NexaC not found or not executable: $NEXAC"
    exit 1
fi
NEXAC=$(cd "$(dirname "$NEXAC")" && pwd)/$(basename "$NEXAC")

SUITE=$(cd "$(dirname "$0")" && pwd)

WORK=$(mktemp -d)
trap 'rm -rf "$WORK"' EXIT INT TERM

fails=0
skips=0

# --- codegen layer ----------------------------------------------------------

# The gfx runtime is pasted into the generated file ahead of main, and it calls
# plenty of __nexa_gfx_* helpers itself. Only main is the transpiler's output
# for the program under test, so that is all we match against.
main_body() {
    awk '/^int main\(/ { inmain = 1 } inmain { print } inmain && /^\}/ { exit }' "$1"
}

# expect_emit <label> <grep-ERE> <body>
# Asserts main contains a line matching the pattern after transpiling `body`.
expect_emit() {
    label=$1
    want=$2
    body=$3

    printf '#include <std/gfx>\nfn main() {\n%s\n}\n' "$body" > "$WORK/case.nxa"
    if ! "$NEXAC" "$WORK/case.nxa" --source "$WORK/case.cpp" > "$WORK/case.log" 2>&1; then
        echo "FAIL $label: NexaC could not transpile"
        sed 's/^/  /' "$WORK/case.log"
        fails=$((fails + 1))
        return
    fi
    main_body "$WORK/case.cpp" > "$WORK/case.main"
    if ! grep -Eq "$want" "$WORK/case.main"; then
        echo "FAIL $label: no generated line in main matches /$want/"
        sed 's/^/  /' "$WORK/case.main"
        fails=$((fails + 1))
        return
    fi
    echo "ok $label"
}

echo "-- codegen: the call each gfx.* form emits"

# Optional arguments: the value NexaC substitutes when the caller omits one is
# part of the documented default, and only the generated source shows it.
expect_emit "open defaults to scale 12" \
    '__nexa_gfx_open\("t", 4, 4, 12\)' '    gfx.open("t", 4, 4);'
expect_emit "open passes an explicit scale" \
    '__nexa_gfx_open\("t", 4, 4, 3\)' '    gfx.open("t", 4, 4, 3);'
# -1 is the "keep the current scale" sentinel, not a scale.
expect_emit "resize keeps the scale with -1" \
    '__nexa_gfx_resize\(8, 8, -1\)' '    gfx.resize(8, 8);'
expect_emit "resize passes an explicit scale" \
    '__nexa_gfx_resize\(8, 8, 2\)' '    gfx.resize(8, 8, 2);'
# Omitted text scale reads the default at the point of the call, so changing
# gfx.text_size() between two gfx.text() calls has to move the second one.
expect_emit "text without a scale reads the default" \
    '__nexa_gfx_text\(0, 0, "a", 1, 2, 3, __nexa_gfx_text_scale\(\)\)' \
    '    gfx.text(0, 0, "a", 1, 2, 3);'
expect_emit "text passes an explicit scale" \
    '__nexa_gfx_text\(0, 0, "a", 1, 2, 3, 4\)' \
    '    gfx.text(0, 0, "a", 1, 2, 3, 4);'
expect_emit "text_width defaults its scale" \
    '__nexa_gfx_text_width\("a", -1\)' '    let w: int = gfx.text_width("a");'
expect_emit "text_width passes an explicit scale" \
    '__nexa_gfx_text_width\("a", 2\)' '    let w: int = gfx.text_width("a", 2);'
expect_emit "text_height defaults its scale" \
    '__nexa_gfx_text_height\("a", -1\)' '    let h: int = gfx.text_height("a");'
expect_emit "text_height passes an explicit scale" \
    '__nexa_gfx_text_height\("a", 2\)' '    let h: int = gfx.text_height("a", 2);'

# Get/set pairs split on argument count, not on name.
expect_emit "text_size with no argument gets" \
    '__nexa_gfx_text_scale\(\)' '    let n: int = gfx.text_size();'
expect_emit "text_size with an argument sets" \
    '__nexa_gfx_text_size_set\(3\)' '    gfx.text_size(3);'
expect_emit "title with no argument gets" \
    '__nexa_gfx_title_get\(\)' '    let s: string = gfx.title();'
expect_emit "title with an argument sets" \
    '__nexa_gfx_title_set\("hi"\)' '    gfx.title("hi");'
# -1 means "report the state" rather than "set it to false".
expect_emit "fullscreen with no argument queries" \
    '__nexa_gfx_fullscreen\(-1\)' '    let f: int = gfx.fullscreen();'
expect_emit "fullscreen with an argument sets" \
    '__nexa_gfx_fullscreen\(0\)' '    gfx.fullscreen(0);'
expect_emit "audio defaults to 44100 Hz" \
    '__nexa_gfx_audio\(44100\)' '    gfx.audio();'
expect_emit "audio passes an explicit rate" \
    '__nexa_gfx_audio\(22050\)' '    gfx.audio(22050);'
expect_emit "opendialog defaults to an empty filter" \
    '__nexa_gfx_opendialog\(std::string\(\)\)' '    let p: string = gfx.opendialog();'
expect_emit "opendialog passes an explicit filter" \
    '__nexa_gfx_opendialog\("\*\.png"\)' '    let p: string = gfx.opendialog("*.png");'
# openfile is documented as an alias, so it must reach the same runtime call.
expect_emit "openfile is an alias for opendialog" \
    '__nexa_gfx_opendialog\("\*\.png"\)' '    let p: string = gfx.openfile("*.png");'

# gfx.blit is the only overloaded call: four argument counts feed nine runtime
# parameters, and a wrong slot silently draws the wrong rectangle rather than
# failing to compile. The unused slots are 0, which the runtime reads as
# "whole image" / "native size".
expect_emit "blit whole image at native size" \
    '__nexa_gfx_blit\(1, 2, 7, 0, 0, 0, 0, 0, 0\)' '    gfx.blit(1, 2, 7);'
expect_emit "blit whole image scaled fills dw,dh" \
    '__nexa_gfx_blit\(1, 2, 7, 3, 4, 0, 0, 0, 0\)' '    gfx.blit(1, 2, 7, 3, 4);'
expect_emit "blit source rect fills sx,sy,sw,sh" \
    '__nexa_gfx_blit\(1, 2, 7, 0, 0, 3, 4, 5, 6\)' '    gfx.blit(1, 2, 7, 3, 4, 5, 6);'
expect_emit "blit source rect scaled fills both" \
    '__nexa_gfx_blit\(1, 2, 7, 8, 9, 3, 4, 5, 6\)' '    gfx.blit(1, 2, 7, 3, 4, 5, 6, 8, 9);'
# The handle/path overload is picked from the inferred type of the third
# argument, so a string variable has to resolve the same way a literal does.
expect_emit "blit takes a path literal" \
    '__nexa_gfx_blit_path\(1, 2, "a\.png", 0, 0, 0, 0, 0, 0\)' \
    '    gfx.blit(1, 2, "a.png");'
expect_emit "blit takes a path in a variable" \
    '__nexa_gfx_blit_path\(1, 2, __nexa_var_[0-9]+, 0, 0, 0, 0, 0, 0\)' \
    '    let p: string = "a.png";
    gfx.blit(1, 2, p);'
expect_emit "blit takes a path with a source rect" \
    '__nexa_gfx_blit_path\(1, 2, "a\.png", 0, 0, 3, 4, 5, 6\)' \
    '    gfx.blit(1, 2, "a.png", 3, 4, 5, 6);'

# The fixed-arity calls: one each, so that renaming a runtime entry point
# without updating the transpiler fails here rather than at link time.
expect_emit "close" '__nexa_gfx_close\(\)' '    gfx.close();'
expect_emit "poll" '__nexa_gfx_poll\(\)' '    gfx.poll();'
expect_emit "present" '__nexa_gfx_present\(\)' '    gfx.present();'
expect_emit "closed" '__nexa_gfx_closed\(\)' '    let c: int = gfx.closed();'
expect_emit "width" '__nexa_gfx_width\(\)' '    let n: int = gfx.width();'
expect_emit "height" '__nexa_gfx_height\(\)' '    let n: int = gfx.height();'
expect_emit "scale" '__nexa_gfx_scale\(\)' '    let n: int = gfx.scale();'
expect_emit "clear" '__nexa_gfx_clear\(1, 2, 3\)' '    gfx.clear(1, 2, 3);'
expect_emit "plot" '__nexa_gfx_plot\(1, 2, 3, 4, 5\)' '    gfx.plot(1, 2, 3, 4, 5);'
expect_emit "get" '__nexa_gfx_get\(1, 2\)' '    let p: int = gfx.get(1, 2);'
expect_emit "fill" '__nexa_gfx_fill\(1, 2, 3, 4, 5, 6, 7\)' \
    '    gfx.fill(1, 2, 3, 4, 5, 6, 7);'
expect_emit "line" '__nexa_gfx_line\(1, 2, 3, 4, 5, 6, 7\)' \
    '    gfx.line(1, 2, 3, 4, 5, 6, 7);'
expect_emit "key" '__nexa_gfx_key\("w"\)' '    let k: int = gfx.key("w");'
expect_emit "pressed" '__nexa_gfx_pressed\("w"\)' '    let k: int = gfx.pressed("w");'
expect_emit "mouse_x" '__nexa_gfx_mouse_x\(\)' '    let n: int = gfx.mouse_x();'
expect_emit "mouse_y" '__nexa_gfx_mouse_y\(\)' '    let n: int = gfx.mouse_y();'
expect_emit "mouse" '__nexa_gfx_mouse\("left"\)' '    let n: int = gfx.mouse("left");'
expect_emit "drop" '__nexa_gfx_drop\(\)' '    let s: string = gfx.drop();'
expect_emit "image" '__nexa_gfx_image\("a\.png"\)' '    let i: int = gfx.image("a.png");'
expect_emit "decode" '__nexa_gfx_decode\("xx"\)' '    let i: int = gfx.decode("xx");'
expect_emit "image_w" '__nexa_gfx_image_w\(1\)' '    let n: int = gfx.image_w(1);'
expect_emit "image_h" '__nexa_gfx_image_h\(1\)' '    let n: int = gfx.image_h(1);'
expect_emit "sample" '__nexa_gfx_sample\(1\)' '    let n: int = gfx.sample(1);'
expect_emit "audio_queued" '__nexa_gfx_audio_queued\(\)' \
    '    let n: int = gfx.audio_queued();'
# audio_flush returns nothing, so it is wrapped to stay usable as an expression.
expect_emit "audio_flush is usable as an expression" \
    '__nexa_gfx_audio_flush\(\), 0' '    let n: int = gfx.audio_flush();'

# --- behavioural layers -----------------------------------------------------

# expect_program <label> <source.nxa> <expected-stdout-file>
# Builds and runs a test program and diffs its stdout. A build failure is
# reported as a skip, not a failure: on a machine without the X11 development
# libraries no gfx program links at all, and that is a property of the machine
# rather than of NexaC.
expect_program() {
    label=$1
    src=$2
    want=$3

    log=$("$NEXAC" "$src" -o "$WORK/prog" 2>&1)
    if [ $? -ne 0 ] || [ ! -x "$WORK/prog" ]; then
        echo "SKIP $label: this machine cannot build a gfx program"
        printf '%s\n' "$log" | grep -E 'error|Error' | head -n 3 | sed 's/^/       /'
        skips=$((skips + 1))
        return 1
    fi
    # Generated code must be clean; a warning here is a codegen bug.
    diags=$(printf '%s\n' "$log" | grep -E '\.cpp:[0-9]+:[0-9]+: (warning|error):')
    if [ -n "$diags" ]; then
        echo "FAIL $label: the C++ compiler had something to say about generated code"
        printf '%s\n' "$diags" | sed 's/^/  /'
        fails=$((fails + 1))
        return 0
    fi
    got=$("$WORK/prog" 2>&1)
    rc=$?
    if [ $rc -ne 0 ]; then
        echo "FAIL $label: program exited $rc"
        printf '%s\n' "$got" | sed 's/^/  /'
        fails=$((fails + 1))
        return 0
    fi
    if [ "$got" != "$(cat "$want")" ]; then
        echo "FAIL $label: output differs from $(basename "$want") (-want +got):"
        cat "$want" > "$WORK/want"
        printf '%s\n' "$got" > "$WORK/got"
        diff -u "$WORK/want" "$WORK/got" | tail -n +3 | sed 's/^/  /'
        fails=$((fails + 1))
        return 0
    fi
    echo "ok $label"
    return 0
}

echo "-- headless: std/gfx with no window"
# Run from the repo root so the test finds Tests/gfx_2x2.png.
gfx_builds=1
if ! expect_program "gfx_headless_test" \
        "$SUITE/gfx_headless_test.nxa" "$SUITE/gfx_headless_test.expected"; then
    gfx_builds=0
fi

echo "-- window: std/gfx against a real window"
if [ "$gfx_builds" -eq 0 ]; then
    echo "SKIP gfx_open_close_test: no gfx build on this machine"
    skips=$((skips + 1))
elif [ -z "${DISPLAY:-}" ] && [ -z "${WAYLAND_DISPLAY:-}" ]; then
    echo "SKIP gfx_open_close_test: no display (DISPLAY/WAYLAND_DISPLAY unset)"
    skips=$((skips + 1))
else
    # This one reports through its exit code rather than a golden stdout.
    log=$("$NEXAC" "$SUITE/gfx_open_close_test.nxa" -o "$WORK/win" 2>&1)
    if [ $? -ne 0 ] || [ ! -x "$WORK/win" ]; then
        echo "SKIP gfx_open_close_test: build failed"
        skips=$((skips + 1))
    else
        out=$("$WORK/win" 2>&1)
        rc=$?
        if [ $rc -ne 0 ] || [ "$out" != "ok" ]; then
            echo "FAIL gfx_open_close_test: exited $rc"
            printf '%s\n' "$out" | sed 's/^/  /'
            fails=$((fails + 1))
        else
            echo "ok gfx_open_close_test"
        fi
    fi
fi

# --- report -----------------------------------------------------------------

if [ $fails -eq 0 ]; then
    if [ $skips -gt 0 ]; then
        echo "gfx ok ($skips layer(s) skipped: see above)"
    else
        echo "gfx ok"
    fi
    exit 0
fi
echo "gfx: $fails failure(s)"
exit 1
