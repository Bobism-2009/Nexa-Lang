#!/bin/sh
# gfx.pads / pad / pad_pressed / pad_axis: gamepads.
#
# No test machine has a gamepad, so the Linux reader is given a joystick the
# test makes itself. Tests/gfx_pad_test.nxa writes the kernel's joystick events
# into a file and appends to it between polls; this builds the program's C++
# with NEXA_GFX_PAD_PATH pointed at that file instead of /dev/input/js%d, so
# everything from the read() on is the code a real pad reaches: the event
# parsing, the xpad layout, the dead zone, the triggers, the d-pad, and the
# pressed edge between two gfx.poll()s. Built against the X11 stand-in in
# Tests/gfx_x11_stub, as the other gfx layers are; no window is opened.
#
#   sliced   a gfx program that reads no pad carries none of the reader.
#   reads    the test program, against its expected output.
#
# Linux only; skipped, loudly, without a C++ compiler.
#
# Usage: Tests/gfx_pad_cases.sh [path-to-NexaC]     (run from the repo root)

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

printf '#include <std/gfx>\nfn main() {\n    gfx.open("x", 8, 8, 1);\n    gfx.poll();\n    gfx.close();\n}\n' > "$WORK/nopad.nxa"
if ! "$NEXAC" "$WORK/nopad.nxa" --source "$WORK/nopad.cpp" > "$WORK/t.log" 2>&1; then
    echo "FAIL sliced: NexaC could not transpile a gfx program"
    fails=$((fails + 1))
elif grep -q '__nexa_gfx_pad\|__nexa_pads\|XInputGetState\|NEXA_GFX_PAD_PATH' "$WORK/nopad.cpp"; then
    echo "FAIL sliced: a program that reads no gamepad carries the reader"
    fails=$((fails + 1))
else
    echo "ok sliced: no pad call, no pad code"
fi

CXX="${NEXA_CXX:-}"
if [ -z "$CXX" ]; then
    for c in clang++ g++ c++; do
        if command -v "$c" > /dev/null 2>&1; then CXX=$c; break; fi
    done
fi

if [ -z "$CXX" ]; then
    echo "skip reads: no C++ compiler (set NEXA_CXX to force one)"
elif [ "$(uname -s)" != "Linux" ]; then
    echo "skip reads: the joystick device is Linux's"
elif ! "$NEXAC" "$HERE/gfx_pad_test.nxa" --source "$WORK/pad.cpp" > "$WORK/t.log" 2>&1; then
    echo "FAIL reads: NexaC could not transpile the test program"
    sed 's/^/  /' "$WORK/t.log"
    fails=$((fails + 1))
elif ! "$CXX" -std=c++17 -O1 '-DNEXA_GFX_PAD_PATH="_nexa_pad/js%d"' -I "$HERE/gfx_x11_stub" "$WORK/pad.cpp" \
        "$HERE/gfx_x11_stub/x11_stub.cpp" -o "$WORK/pad" > "$WORK/build.log" 2>&1; then
    echo "FAIL reads: could not build the test program"
    sed 's/^/  /' "$WORK/build.log" | tail -8
    fails=$((fails + 1))
else
    (cd "$WORK" && ./pad > out.txt 2>&1)
    if diff -u --strip-trailing-cr "$HERE/gfx_pad_test.expected" "$WORK/out.txt" > "$WORK/diff.txt"; then
        echo "ok reads: buttons, sticks, triggers, d-pad and the pressed edge"
    else
        echo "FAIL reads: the pad read differently"
        sed 's/^/  /' "$WORK/diff.txt"
        fails=$((fails + 1))
    fi
fi

if [ $fails -eq 0 ]; then
    echo "gfx_pad ok"
    exit 0
fi
echo "gfx_pad: $fails failure(s)"
exit 1
